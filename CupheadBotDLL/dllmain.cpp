#include <Windows.h>
#include <string>
#include <array>
#include <fstream>
#include <d3d11.h>


typedef HRESULT(__stdcall *d3d11_PresentHook)(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);

d3d11_PresentHook p_present_hook = nullptr;

DWORD g_p_present;

IDXGISwapChain* g_p_swapchain;
ID3D11Device* g_p_device;
ID3D11DeviceContext* g_p_device_context;

BYTE* post_detour_buffer = nullptr;


struct DummyWindow
{
	DummyWindow()
	{
		WNDCLASSEXA wc = { 0 };
		wc.cbSize = sizeof(wc);
		wc.style = CS_CLASSDC;
		wc.lpfnWndProc = DefWindowProc;
		wc.hInstance = GetModuleHandleA(NULL);
		wc.lpszClassName = "DX";
		RegisterClassExA(&wc);

		m_hWnd =
			CreateWindowA("DX", 0, WS_OVERLAPPEDWINDOW, 100, 100, 300, 300, GetDesktopWindow(), 0, wc.hInstance, 0);
	}

	~DummyWindow()
	{
		if (m_hWnd)
		{
			DestroyWindow(m_hWnd);
			UnregisterClassA("DX", GetModuleHandleA(NULL));
		}
	}

	HWND m_hWnd = NULL;
};


template<typename T>
T read_memory(DWORD address)
{
	return *((T*)address);
}


template<typename T>
void write_memory(DWORD address, T value)
{
	*((T*)address) = value;
}


template<typename T>
T* point_memory(DWORD address)
{
	return (T*(address));
}


template<typename T>
DWORD protect_memory(DWORD hook_at, DWORD protection, size_t size)
{
	DWORD old_protection;
	VirtualProtect((LPVOID)hook_at, sizeof(T) * size, protection, &old_protection);
	return old_protection;
}


template<typename T>
DWORD protect_memory(DWORD hook_at, DWORD protection)
{
	return protect_memory<T>(hook_at, protection, 1);
}


DWORD get_VF(DWORD class_adr, DWORD func_idx)
{
	DWORD vtable = read_memory<DWORD>(class_adr);
	DWORD hook_adr = vtable + func_idx * sizeof(DWORD);
	return read_memory<DWORD>(hook_adr);
}


/** Change the address of func_idx in class_adr's vtable with new_func.
	Returns the original function address replaced by new_func.
*/
DWORD hook_vtable(DWORD class_adr, DWORD func_idx, DWORD new_func)
{
	DWORD vtable = read_memory<DWORD>(class_adr);
	DWORD hook_at = vtable + func_idx * sizeof(DWORD);

	DWORD old_protection = protect_memory<DWORD>(hook_at, PAGE_READWRITE);
	DWORD original_func = read_memory<DWORD>(hook_at);
	write_memory<DWORD>(hook_at, new_func);
	protect_memory<DWORD>(hook_at, old_protection);

	return original_func;
}


/** Places a JMP hook at hook_at.
	The original bytes replaced are returned and should be restored ASAP. 
	(The hook must be unhooked immediately, since the extra byte after the current function gets replaced ...)
*/
const std::array<BYTE, 5> jump_hook(DWORD hook_at, DWORD new_func)
{
	DWORD old_protection = protect_memory<BYTE[5]>(hook_at, PAGE_EXECUTE_READWRITE);
	
	std::array<BYTE, 5> originals;
	for (size_t i = 0; i < 5; ++i)
		originals[i] = read_memory<BYTE>(hook_at + i);

	DWORD new_offset = new_func - hook_at - 5;
	write_memory<BYTE>(hook_at, 0xE9);  // JMP
	write_memory<DWORD>(hook_at + 1, new_offset);

	protect_memory<BYTE[5]>(hook_at, old_protection);

	return originals;
}


/* Restores the JMP hook at hook_at with the original bytes returned by jump_hook. */
void jump_unhook(DWORD hook_at, const std::array<BYTE, 5>& originals)
{
	DWORD old_protection = protect_memory<BYTE[5]>(hook_at, PAGE_EXECUTE_READWRITE);

	for (size_t i = 0; i < 5; ++i)
		write_memory<BYTE>(hook_at + i, originals[i]);

	protect_memory<BYTE[5]>(hook_at, old_protection);
}


DWORD detour_hook(DWORD hook_at, DWORD detour, size_t length)
{
	post_detour_buffer = new BYTE[length + 5];
	memcpy(post_detour_buffer, (BYTE*)hook_at, length);
	post_detour_buffer[length] = 0xE9;
	*(DWORD*)(post_detour_buffer + length + 1) = (hook_at + length) - ((DWORD)(post_detour_buffer) + length + 5);

	DWORD old_protection = protect_memory<BYTE[5]>(hook_at, PAGE_EXECUTE_READWRITE);
	write_memory<BYTE>(hook_at, 0xE9);
	write_memory<DWORD>(hook_at + 1, detour - hook_at - 5);
	protect_memory<BYTE[5]>(hook_at, old_protection);

	DWORD _old_prot;
	VirtualProtect(post_detour_buffer, length + 5, PAGE_EXECUTE_READWRITE, &_old_prot);

	return (DWORD)post_detour_buffer;
}


void remove_detour_hook(DWORD hook_at, const BYTE* original, size_t length)
{
	DWORD old_protection = protect_memory<BYTE>(hook_at, PAGE_EXECUTE_READWRITE, length);

	memcpy((void*)hook_at, original, length);

	protect_memory<BYTE>(hook_at, old_protection, length);

	if (post_detour_buffer)
		delete[] post_detour_buffer;
}


HRESULT __stdcall present_callback(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
	pSwapChain->GetDevice(__uuidof(g_p_device), (void**)&g_p_device);
	g_p_device->GetImmediateContext(&g_p_device_context);

	return p_present_hook(pSwapChain, SyncInterval, Flags);
}


void hook_d3d11()
{
	DummyWindow window = DummyWindow();

	auto feature_level = D3D_FEATURE_LEVEL_11_0;
	DXGI_SWAP_CHAIN_DESC swapChainDesc = { 0 };
	swapChainDesc.BufferCount = 1;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.OutputWindow = window.m_hWnd;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	swapChainDesc.Windowed = TRUE;

	IDXGISwapChain* p_swapchain;
	ID3D11Device* p_device;
	ID3D11DeviceContext* p_device_context;

	D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, &feature_level, 1, D3D11_SDK_VERSION, 
								  &swapChainDesc, &p_swapchain, &p_device, NULL, &p_device_context);

	g_p_present = get_VF((DWORD)p_swapchain, 8);

	DWORD post_detour = detour_hook(g_p_present, (DWORD)&present_callback, 5);
	p_present_hook = (d3d11_PresentHook)post_detour;

	p_device->Release();
	p_device_context->Release();
	p_swapchain->Release();
}


DWORD WINAPI run_bot(LPVOID param)
{
	hook_d3d11();

	while (!g_p_device) { Sleep(10); }

	std::ofstream ofs("C:\\Users\\Mare5\\Desktop\\bot.log");
	ofs << "testestset\n";
	ofs << std::hex << g_p_present << '\n';
	ofs << std::hex << g_p_device << '\n' << g_p_device_context << '\n' << g_p_swapchain << '\n';
	ofs.close();

	MessageBoxA(NULL, "TEST", "TEST", MB_OK | MB_TOPMOST);

	return 1;
}


DWORD WINAPI on_exit(LPVOID param)
{
	remove_detour_hook(g_p_present, post_detour_buffer, 5);
	return 1;
}


BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
	if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
		HANDLE thread = CreateThread(NULL, NULL, &run_bot, NULL, NULL, NULL);
		CloseHandle(thread);
	} else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
		// todo do this without crashing the game ;-)
		HANDLE thread = CreateThread(NULL, NULL, &on_exit, NULL, NULL, NULL);
		CloseHandle(thread);
	}
    return TRUE;
}