#include <iostream>
#include <vector>
#include "memory_tools.h"


const size_t BUFFER_SIZE = 4096;  // bytes


void log_error(const char* msg)
{
	std::cout << msg << ", ERROR: " << GetLastError() << '\n';
}


DWORD get_pid(const std::string& window_title)
{
	HWND wnd = FindWindow(NULL, window_title.c_str());
	if (!wnd) {
		log_error("FindWindow failed!");
		return 0;
	}
	DWORD pid;
	GetWindowThreadProcessId(wnd, &pid);
	return pid;
}


DWORD get_base_address(HANDLE proc)
{
	HMODULE k32 = GetModuleHandle("kernel32.dll");
	LPVOID func_adr = GetProcAddress(k32, "GetModuleHandleA");
	if (!func_adr)
		func_adr = GetProcAddress(k32, "GetModuleHandleW");

	HANDLE thread = CreateRemoteThread(proc, NULL, NULL, (LPTHREAD_START_ROUTINE)func_adr, NULL, NULL, NULL);
	WaitForSingleObject(thread, INFINITE);

	DWORD base;
	GetExitCodeThread(thread, &base);

	CloseHandle(thread);

	return base;
}


bool equal(const BYTE* buf1, const BYTE* buf2, size_t size)
{
	for (size_t i = 0; i < size; ++i) {
		if (buf1[i] != buf2[i])
			return false;
	}
	return true;
}


/* Returns the next memory page after base_adr that has the PAGE_EXECUTE_READWRITE permission. */
MemoryRegion next_memory_page(HANDLE proc, DWORD base_adr)
{
	MEMORY_BASIC_INFORMATION mem_info;
	while (VirtualQueryEx(proc, (LPVOID)base_adr, &mem_info, sizeof(mem_info)) != 0) {
		if (mem_info.AllocationProtect & PAGE_EXECUTE_READWRITE) {
			printf("%x %d %x\n", mem_info.BaseAddress, mem_info.RegionSize, mem_info.AllocationProtect);
			return MemoryRegion((DWORD)mem_info.BaseAddress, mem_info.RegionSize);
		}
		base_adr += mem_info.RegionSize;
	}
	return MemoryRegion();
}


MemoryRegion first_memory_page(HANDLE proc)
{
	return next_memory_page(proc, 0);
}


/** Function header must have the following pattern:
	push ebp
	mov ebp, esp
*/
bool is_function_header(HANDLE proc, BYTE* buf)
{
	static const BYTE func_header[3] = {
		0x55, 0x8B, 0xEC
	};

	return equal(func_header, buf, 3);
}


/** Returns the base address of the function that matches the given func_header or 0 if it doesn't exist.

	Note: Cuphead uses JIT (just in time) compilation, so make sure the desired
	function has been assembled in memory before running this function.
*/
DWORD find_function(HANDLE proc, BYTE func_header[], size_t header_size)
{
	MemoryRegion page = first_memory_page(proc);
	BYTE buffer[BUFFER_SIZE] = {};

	while (page.valid()) {
		DWORD address = page.base_adr;
		size_t func_header_idx = 0;

		do {
			size_t buffer_size = min(BUFFER_SIZE, page.end() - address);
			read_memory<BYTE>(proc, address, buffer, buffer_size);

			for (size_t i = 0; i < buffer_size; ++i) {
				if (func_header[func_header_idx] == buffer[i]) {
					++func_header_idx;
					if (func_header_idx == header_size)
						return address + i + 1 - header_size;
				}
				else
					func_header_idx = 0;
			}

			address += buffer_size;
		} while (address < page.end());

		page = next_memory_page(proc, page.end() + 1);
	}

	return 0;
}


/** Place a JMP instruction at hook_at address that jumps to jmp_adr.

bytes_to_replace are the number of bytes to be replaced by the new JMP instruction.
(5 bytes for the JMP and bytes_to_replace - 5 bytes for leftover instructions that wouldn't work with the new JMP ...)
*/
void jump_hook(HANDLE proc, DWORD hook_at, DWORD jmp_adr, int bytes_to_replace)
{
	if (bytes_to_replace > 12)
		return;

	DWORD new_offset = jmp_adr - hook_at - 5;  // JMP call addresses are relative to the address of the JMP ... 

	DWORD old_protection = protect_memory<DWORD[3]>(proc, hook_at, PAGE_EXECUTE_READWRITE);
	// JMP ADR = always 5 bytes
	write_memory<BYTE>(proc, hook_at, 0xE9);  // JMP
	write_memory<DWORD>(proc, hook_at + 1, new_offset);

	// replace left over instruction bytes with NOPs
	for (int i = 5; i < bytes_to_replace; ++i) {
		write_memory<BYTE>(proc, hook_at + i, 0x90);  // NOP
	}

	protect_memory<DWORD[3]>(proc, hook_at, old_protection);
}