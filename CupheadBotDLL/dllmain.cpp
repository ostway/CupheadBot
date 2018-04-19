#include <Windows.h>
#include <string>
#include <array>
#include <fstream>


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
DWORD protect_memory(DWORD hook_at, DWORD protection)
{
	DWORD old_protection;
	VirtualProtect((LPVOID)hook_at, sizeof(T), protection, &old_protection);
	return old_protection;
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


DWORD WINAPI run_bot(LPVOID param)
{
	//std::ofstream ofs("C:\\Users\\Mare5\\Desktop\\bot.log");
	//ofs.close();

	MessageBoxA(NULL, "TEST", "TEST", MB_OK | MB_TOPMOST);

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
	} else if (ul_reason_for_call == DLL_PROCESS_DETACH) { }
    return TRUE;
}