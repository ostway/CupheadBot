// TODO error handling
// TODO freeze thread?
// Fix random crash when closing Cuphead.exe


#include "CupheadBotUI.h"


HMODULE g_dll_module;


DWORD WINAPI run_bot(LPVOID param = NULL)
{
	CupheadBotUI bot_ui(g_dll_module);

	while (!bot_ui.is_exit_scheduled())
		Sleep(500);
	Sleep(200);  // just wait, since we don't know what the thread is currently executing ...
	FreeLibraryAndExitThread(g_dll_module, NULL);

	return 1;
}


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