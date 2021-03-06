#include "PlayerDataBot.h"


const std::array<PlayerDataBot::Loadout::Weapon, PlayerDataBot::Loadout::N_WEAPONS> PlayerDataBot::Loadout::WEAPON_TABLE = { {
	{ "PEASHOOTER", 1456773641 },
	{ "SPREAD", 1456773649 },
	{ "CHASER", 1460621839 },
	{ "LOBBER", 1467024095 },
	{ "CHARGE", 1466416941 },
	{ "ROUNDABOUT", 1466518900 },
	{ "TRIPLE LASER", 0x58A3110F },
	{ "ARC", 0x56F2E227 },
	{ "EXPLODER", 0x575FF384 }
} };


const std::array<PlayerDataBot::Loadout::Charm, PlayerDataBot::Loadout::N_CHARMS> PlayerDataBot::Loadout::CHARM_TABLE = { {
	{ "HEART", 0x571289E6 },
	{ "COFFEE", 0x571345E2 },
	{ "SMOKE BOMB", 0x57151B56 },
	{ "P. SUGAR", 0x58A299CC },
	{ "TWIN HEART", 0x5971F75B },
	{ "WHETSTONE", 0x5971ACAF },
	{ "PIT SAVER", 0x58A2AF58 }
} };


const std::array<PlayerDataBot::Loadout::Super, PlayerDataBot::Loadout::N_SUPERS> PlayerDataBot::Loadout::SUPER_TABLE = { {
	{ "ENERGY BEAM", 0x56D53D31 },
	{ "INVINCIBILITY", 0x591C13BA },
	{ "GIANT GHOST", 0x577A1293 }
} };


bool operator==(const PlayerDataBot::Loadout::_LoadoutItem& left, const PlayerDataBot::Loadout::_LoadoutItem& right) 
{ 
	return left.id == right.id; 
}


DWORD PlayerDataBot::get_player_data_func_adr = 0;


DWORD PlayerDataBot::get_money_address()
{
	DWORD adr = get_player_data_address();
	if (!adr) return 0;

	adr = read_memory<DWORD>(adr + 0xc);
	adr = read_memory<DWORD>(adr + 0x8);
	return adr + 0x14;
}

DWORD PlayerDataBot::get_loadout_address()
{
	DWORD adr = get_player_data_address();
	if (!adr) return 0;

	adr = read_memory<DWORD>(adr + 0x8);
	return read_memory<DWORD>(adr + 0x8);
}

bool PlayerDataBot::initialized_or_init_signature()
{
	static const BYTE signature[] = {
		0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x08, 0xE8, 0x2D, 0x00, 0x00, 0x00, 0x83, 0xEC, 0x0C, 0x50, 0xE8, 0x4C, 0x00, 0x00, 0x00, 0x83, 0xC4, 0x10, 0xC9, 0xC3
	};
	if (!get_player_data_func_adr)
		get_player_data_func_adr = find_signature(signature, sizeof(signature));
	return get_player_data_func_adr;
}

DWORD PlayerDataBot::get_money()
{
	if (DWORD adr = get_money_address())
		return read_memory<DWORD>(adr);
	return 0;
}

bool PlayerDataBot::set_money(DWORD money)
{
	if (DWORD adr = get_money_address()) {
		write_memory<DWORD>(adr, money);
		return true;
	}
	return false;
}

DWORD PlayerDataBot::get_player_data_address()
{
	if (!initialized_or_init_signature())
		return 0;

	// Call a function that returns the address to the player data structure.
	// The start of the pointer chain depends on the currently loaded save file, but with this there aren't any problems.
	DWORD adr = 0;
	__asm {
		CALL[get_player_data_func_adr]
		mov adr, eax
	}
	return adr;
}

const PlayerDataBot::Loadout::Weapon & PlayerDataBot::Loadout::get_primary_weapon() const
{
	DWORD id = read_memory<DWORD>(loadout_adr + 0x8);
	for (const auto& weapon : WEAPON_TABLE)
		if (weapon.id == id)
			return weapon;
	return WEAPON_TABLE.front();
}

const PlayerDataBot::Loadout::Weapon & PlayerDataBot::Loadout::get_secondary_weapon() const
{
	DWORD id = read_memory<DWORD>(loadout_adr + 0xC);
	for (const auto& weapon : WEAPON_TABLE)
		if (weapon.id == id)
			return weapon;
	return WEAPON_TABLE.front();
}

const PlayerDataBot::Loadout::Super & PlayerDataBot::Loadout::get_super() const
{
	DWORD id = read_memory<DWORD>(loadout_adr + 0x10);
	for (const auto& super : SUPER_TABLE)
		if (super.id == id)
			return super;
	return SUPER_TABLE.front();
}

const PlayerDataBot::Loadout::Charm & PlayerDataBot::Loadout::get_charm() const
{
	DWORD id = read_memory<DWORD>(loadout_adr + 0x14);
	for (const auto& charm : CHARM_TABLE)
		if (charm.id == id)
			return charm;
	return CHARM_TABLE.front();
}
