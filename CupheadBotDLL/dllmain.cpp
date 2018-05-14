#include "imgui\imgui.h"
#include "imgui\imgui_impl_dx11.h"
#include "memory_tools.h"
#include "d3d11_hook.h"


HMODULE g_dll_module;
HWND g_cuphead_window_handle;

bool g_imgui_initialized = false;
bool g_exit_scheduled = false;

typedef LRESULT(CALLBACK *wndproc)(_In_ HWND hwnd, _In_ UINT uMsg, _In_ WPARAM wParam, _In_ LPARAM lParam);
wndproc g_orig_wndproc_handler = nullptr;


// defined in imgui_impl_dx11.cpp
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK window_proc_impl(_In_ HWND hwnd, _In_ UINT uMsg, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam))
		return true;

	return g_orig_wndproc_handler(hwnd, uMsg, wParam, lParam);
}


void hook_input_handler()
{
	// hook Cuphead's input handler and instead use my own version which is used by ImGui
	g_orig_wndproc_handler = (wndproc)GetWindowLongPtr(g_cuphead_window_handle, GWLP_WNDPROC);
	SetWindowLongPtr(g_cuphead_window_handle, GWLP_WNDPROC, (LONG_PTR)&window_proc_impl);
}


void unhook_input_handler()
{
	SetWindowLongPtr(g_cuphead_window_handle, GWLP_WNDPROC, (LONG_PTR)g_orig_wndproc_handler);
	g_orig_wndproc_handler = nullptr;
}


void init_overlay()
{
	ImGui::CreateContext();
	//ImGuiIO &io = ImGui::GetIO();
	g_imgui_initialized = ImGui_ImplDX11_Init(g_cuphead_window_handle, g_p_device, g_p_device_context);

	hook_input_handler();
}


void exit_overlay()
{
	g_imgui_initialized = false;
	unhook_input_handler();
	ImGui_ImplDX11_Shutdown();
	ImGui::DestroyContext();
}


void render_ui()
{
	ImGui::Text("WNDPROC: %x, HOOKED: %x", g_orig_wndproc_handler, &window_proc_impl);
	
	if (ImGui::Button("EXIT")) {                           // Buttons return true when clicked (NB: most widgets return true when edited/activated)
		g_exit_scheduled = true;
	}
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
}


bool present_impl(ID3D11Device* device, ID3D11DeviceContext* device_context, IDXGISwapChain* swap_chain)
{
	if (!g_imgui_initialized) return false;
	if (g_exit_scheduled) {
		// the reason unhooking is done here is because otherwise we wouldn't know what Cuphead's thread is executing
		// this way at least we can control exactly what is happening
		exit_overlay();
		return true;
	}

	ImGui_ImplDX11_NewFrame();

	render_ui();

	//ImGui::ShowDemoWindow();

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	return false;
}


DWORD WINAPI run_bot(LPVOID param = NULL)
{
	g_cuphead_window_handle = FindWindow(NULL, L"Cuphead");
	
	hook_d3d11();

	init_overlay();

	while (!g_exit_scheduled)
		Sleep(500);
	Sleep(200);  // just wait, since we don't know what the thread is currently executing ...
	FreeLibraryAndExitThread(g_dll_module, NULL);

	return 1;
}


//DWORD WINAPI on_exit(LPVOID param = NULL)
//{
//	g_exit_scheduled = true;
//	Sleep(200);
//
//	return 1;
//}


BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
	if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
		g_dll_module = hModule;
		HANDLE thread = CreateThread(NULL, NULL, &run_bot, NULL, NULL, NULL);
		CloseHandle(thread);
	} else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
		//HANDLE thread = CreateThread(NULL, NULL, &on_exit, NULL, NULL, NULL);
		//CloseHandle(thread);
		//on_exit();
	}
    return TRUE;
}