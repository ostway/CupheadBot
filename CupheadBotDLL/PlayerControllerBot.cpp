#include "PlayerControllerBot.h"


const float PlayerControllerBot::DEFAULT_SUPER_COST = 10.0f;

// static members are declared in a class and defined here https://stackoverflow.com/questions/1410563/what-is-the-difference-between-a-definition-and-a-declaration
BasicHookInfo PlayerControllerBot::player_controller_hook = {
	0,
	{ 0x8B, 0xC8, 0x39, 0x09, 0x8B, 0x40, 0x60 }
};
DWORD PlayerControllerBot::player_stats_adr = 0;
DWORD PlayerControllerBot::player_controller_adr = 0;
DWORD PlayerControllerBot::original_base_address_func = 0;


/** Trampoline function used to get the address of the PlayerController and PlayerStats and then going back to the original function.
	The function hook persists from set_base_address_hook to restore_base_address_hook. That means the player controller
	address gets constantly updated.*/
void __declspec(naked) base_address_getter()
{
	__asm 
	{
		mov ecx, [ebp + 8]
		mov [PlayerControllerBot::player_controller_adr], ecx
		MOV [PlayerControllerBot::player_stats_adr], EAX
		mov    ecx, eax														// original code replaced by the hook
		cmp    DWORD PTR[ecx], ecx
		mov    eax, DWORD PTR[eax + 0x60]
		JMP [PlayerControllerBot::original_base_address_func]				// jump back to original code
	}
}

bool PlayerControllerBot::init()
{
	return set_base_address_hook();
}

void PlayerControllerBot::exit()
{
	restore_base_address_hook();
}

bool PlayerControllerBot::set_invincible(bool invincible)
{
	if (!initialized_or_init()) return false;
	write_memory<bool>(player_stats_adr + 0x6c, invincible);
	return true;
}

bool PlayerControllerBot::set_hp(DWORD new_hp)
{
	if (!initialized_or_init()) return false;
	write_memory<DWORD>(player_stats_adr + 0x60, new_hp);
	return true;
}

DWORD PlayerControllerBot::get_max_hp()
{
	if (!initialized_or_init()) return 0;
	return read_memory<DWORD>(player_stats_adr + 0x5c);
}

bool PlayerControllerBot::set_super_cost(float new_cost)
{
	if (!initialized_or_init()) return false;
	write_memory<float>(player_stats_adr + 0x70, new_cost);
	return true;
}

bool PlayerControllerBot::set_super_meter(float val)
{
	if (!initialized_or_init()) return false;
	write_memory<float>(player_stats_adr + 0x68, val);
	return true;
}

float PlayerControllerBot::get_max_super()
{
	if (!initialized_or_init()) return 0;
	return read_memory<float>(player_stats_adr + 0x64);
}

DWORD PlayerControllerBot::get_loadout_address()
{
	if (!initialized_or_init()) return 0;
	return read_memory<DWORD>(player_stats_adr + 0x38);
}

DWORD PlayerControllerBot::get_weapon_manager_address()
{
	if (!initialized_or_init()) return 0;
	return read_memory<DWORD>(player_controller_adr + 0x64);
}

bool PlayerControllerBot::initialized_or_init()
{
	// Once the hook is placed all we can do is wait for it to get executed and fill in the player_controller_adr
	if (!original_base_address_func)
		init();
	return player_controller_adr;
}

bool PlayerControllerBot::set_base_address_hook()
{
	static const BYTE signature[] = {
		0x8B, 0xC8, 0x39, 0x09, 0x8B, 0x40, 0x60, 0x85, 0xC0, 0x0F, 0x9F, 0xC0, 0x0F, 0xB6, 0xC0
	};

	player_controller_hook.hook_at = find_signature(signature, sizeof(signature));
	if (!player_controller_hook.hook_at) return false;

	original_base_address_func = jump_hook(player_controller_hook.hook_at, (DWORD)&base_address_getter, player_controller_hook.original_bytes.size());
	
	return true;
}

bool PlayerControllerBot::restore_base_address_hook()
{
	if (player_controller_hook.hook_at) {
		jump_unhook(player_controller_hook.hook_at, player_controller_hook.original_bytes.data(), player_controller_hook.original_bytes.size());
		player_controller_hook.hook_at = 0;
	}
	return true;
}
