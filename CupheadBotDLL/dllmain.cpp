#include "imgui\imgui.h"
#include "imgui\imgui_impl_dx11.h"
#include "memory_tools.h"
#include "d3d11_hook.h"


HWND g_cuphead_window_handle;

bool g_imgui_initialized = false;

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


void present_impl(ID3D11Device* device, ID3D11DeviceContext* device_context, IDXGISwapChain* swap_chain)
{
	if (!g_imgui_initialized) return;

	ImGui_ImplDX11_NewFrame();

	//{
	//	ImGui::Text("WNDPROC: %x, HOOKED: %x", g_orig_wndproc_handler, &window_proc_impl);
	//	
	//	static float f = 0.0f;
	//	static int counter = 0;
	//	ImGui::Text("Hello, world!");                           // Display some text (you can use a format string too)
	//	ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f    

	//	if (ImGui::Button("Button"))                            // Buttons return true when clicked (NB: most widgets return true when edited/activated)
	//		counter++;
	//	ImGui::SameLine();
	//	ImGui::Text("counter = %d", counter);

	//	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
	//}

	ImGui::ShowDemoWindow();

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}


void init_overlay()
{
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	g_imgui_initialized = ImGui_ImplDX11_Init(g_cuphead_window_handle, g_p_device, g_p_device_context);
}


DWORD WINAPI run_bot(LPVOID param)
{
	hook_d3d11();

	g_cuphead_window_handle = FindWindow(NULL, L"Cuphead");

	hook_input_handler();

	init_overlay();

	return 1;
}


DWORD WINAPI on_exit(LPVOID param)
{
	unhook_d3d11();

	ImGui_ImplDX11_Shutdown();
	ImGui::DestroyContext();

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