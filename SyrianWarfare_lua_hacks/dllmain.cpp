// dllmain.cpp : Defines the entry point for the DLL application.
#include <tchar.h>
#include "stdafx.h"
#include <iostream>
#include <string.h>
#include <strsafe.h>
#include <unordered_set>
#include <map>
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#include "MinHook.h"

#pragma comment(lib, "libMinHook.x86.lib")

DWORD g_threadID;
HINSTANCE g_hModule;
DWORD WINAPI MyThread(LPVOID);

TCHAR debug_string[255] = _T("This is in the injected dll");

typedef lua_State *(*game_lua_open)(void);
typedef void(*game_lua_close)(lua_State *L);
typedef int(*game_luaopen_io)(lua_State *L);
typedef int(*game_lua_dostring)(lua_State *L, const char *str);
typedef int(*game_lua_dofile)(lua_State *L, const char *filename);
typedef int(*game_lua_gettop)(lua_State *L);
typedef const char * (*game_lua_tostring)(lua_State *L, int idx);
typedef void(*game_lua_settop)(lua_State *L, int idx);
typedef int(*game_lua_next)(lua_State *L, int idx);
typedef lua_Number(*game_lua_tonumber)(lua_State *L, int idx);
typedef const void * (*game_lua_topointer)(lua_State *L, int idx);
typedef void(*game_lua_pushstring)(lua_State *L, const char *s);
typedef void(*game_lua_gettable)(lua_State *L, int idx);
typedef void(*game_lua_pushnil)(lua_State *L);
typedef int(*game_lua_isstring)(lua_State *L, int idx);
typedef int(*game_lua_isnumber)(lua_State *L, int idx);
typedef int(*game_lua_type)(lua_State *L, int idx);

game_lua_open orig_lua_open = NULL;
game_lua_close orig_lua_close = NULL;
game_lua_gettop orig_gettop = NULL;
lua_State * game_L = NULL;

std::unordered_set<lua_State *> lua_States;

namespace Lua {
	game_lua_open lua_open = NULL;
	game_lua_close lua_close = NULL;
	game_luaopen_io luaopen_io = NULL;
	game_lua_dostring lua_dostring = NULL;
	game_lua_dofile lua_dofile = NULL;
	game_lua_gettop lua_gettop = NULL;
	game_lua_tostring lua_tostring = NULL;
	game_lua_settop lua_settop = NULL;
	game_lua_next lua_next = NULL;
	game_lua_tonumber lua_tonumber = NULL;
	game_lua_topointer lua_topointer = NULL;

	game_lua_pushstring lua_pushstring = NULL;
	game_lua_gettable lua_gettable = NULL;
	game_lua_pushnil lua_pushnil = NULL;

	game_lua_isstring lua_isstring = NULL;
	game_lua_isnumber lua_isnumber = NULL;
	game_lua_type lua_type = NULL;
};

BOOL WINAPI DllMain(HINSTANCE hDll, DWORD dwReason, LPVOID lpReserved) {
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		OutputDebugString(debug_string);
		// Code to run when the DLL is loaded
		g_hModule = hDll;
		DisableThreadLibraryCalls(hDll);
		CreateThread(NULL, NULL, &MyThread, NULL, NULL, &g_threadID);
		break;

	case DLL_PROCESS_DETACH:
		// Code to run when the DLL is freed
		break;

	case DLL_THREAD_ATTACH:
		// Code to run when a thread is created during the DLL's lifetime
		break;

	case DLL_THREAD_DETACH:
		// Code to run when a thread ends normally.
		break;
	}
	return TRUE;
}

//https://stackoverflow.com/questions/20523044/loop-through-all-lua-global-variables-in-c#20525495
//http://www.lua.org/manual/5.1/manual.html#lua_next
//http://lua-users.org/lists/lua-l/2004-04/msg00201.html
void print_table(lua_State *L, int depth) {
	char debug_string_char[255];

	Lua::lua_pushnil(L); // put a nil key on stack
	while (Lua::lua_next(L, -2) != 0) {
		if (Lua::lua_isstring(L, -1)) {
			sprintf(debug_string_char, "%i STR %s = %s", depth, Lua::lua_tostring(L, -2), Lua::lua_tostring(L, -1));
			OutputDebugStringA(debug_string_char);
		}
		else if (Lua::lua_isnumber(L, -1)) {
			sprintf(debug_string_char, "%i NUM %s = 0x%f", depth, Lua::lua_tostring(L, -2), Lua::lua_tonumber(L, -1));
			OutputDebugStringA(debug_string_char);
		}
		else if (Lua::lua_type(L, -1) == LUA_TTABLE) {
			auto table_name = Lua::lua_tostring(L, -2);
			auto table_pointer = Lua::lua_topointer(L, -1);
			sprintf(debug_string_char, "%i TAB %s = 0x%p", depth, table_name, table_pointer);
			OutputDebugStringA(debug_string_char);
			if (std::string(table_name).compare("_G") == 0 && depth < 2) {
				//OutputDebugString(_T("Found _G"));
			}
			else {
				//Lua::lua_pushstring(L, table_name);
				//Lua::lua_gettable(L, LUA_GLOBALSINDEX);
				//print_table(L, depth+1);
			}
		}
		else if (Lua::lua_type(L, -1) == LUA_TFUNCTION) {
			sprintf(debug_string_char, "%i FUN %s = 0x%p", depth, Lua::lua_tostring(L, -2), Lua::lua_topointer(L, -1));
			OutputDebugStringA(debug_string_char);
		}

		Lua::lua_settop(L, -(1) - 1);
	}
}

lua_State * hooked_lua_open(void) {
	game_L = orig_lua_open();
	lua_States.insert(game_L);
	wsprintf(debug_string, _T("In lua_open, lua_State 0x%p"), game_L);
	OutputDebugString(debug_string);
	return game_L;
}

void hooked_lua_close(lua_State *L) {
	wsprintf(debug_string, _T("In lua_close, lua_State 0x%p"), L);
	OutputDebugString(debug_string);

	lua_States.erase(L);
	orig_lua_close(L);
}

int hooked_lua_gettop(lua_State *L) {
	if (!lua_States.count(L)) { // if state is already recorded, don't debug print
		wsprintf(debug_string, _T("In lua_gettop, lua_State 0x%p"), L);
		OutputDebugString(debug_string);
		lua_States.insert(L);
	}	
	return orig_gettop(L);
}

DWORD WINAPI MyThread(LPVOID) {		

	HMODULE hLua = GetModuleHandle(_T("lua"));
	if (hLua == NULL) {
		OutputDebugString(_T("GetModuleHandle failed"));
		//return 1;
	}

	while (hLua == NULL) {
		hLua = GetModuleHandle(_T("lua"));
		OutputDebugString(_T("GetModuleHandle failed"));
	} 

	wsprintf(debug_string, L"Lua module at 0x%p", hLua);
	OutputDebugString(debug_string);

	std::map<std::string, void *> functionMap;
	functionMap.insert(std::pair<std::string, void *>("lua_open", &Lua::lua_open));
	functionMap.insert(std::pair<std::string, void *>("lua_close", &Lua::lua_close));
	functionMap.insert(std::pair<std::string, void *>("luaopen_io", &Lua::luaopen_io));
	functionMap.insert(std::pair<std::string, void *>("lua_dostring", &Lua::lua_dostring));
	functionMap.insert(std::pair<std::string, void *>("lua_dofile", &Lua::lua_dofile));
	functionMap.insert(std::pair<std::string, void *>("lua_gettop", &Lua::lua_gettop));
	functionMap.insert(std::pair<std::string, void *>("lua_tostring", &Lua::lua_tostring));
	functionMap.insert(std::pair<std::string, void *>("lua_settop", &Lua::lua_settop));
	functionMap.insert(std::pair<std::string, void *>("lua_next", &Lua::lua_next));
	functionMap.insert(std::pair<std::string, void *>("lua_tonumber", &Lua::lua_tonumber));
	functionMap.insert(std::pair<std::string, void *>("lua_topointer", &Lua::lua_topointer));
	functionMap.insert(std::pair<std::string, void *>("lua_pushstring", &Lua::lua_pushstring));
	functionMap.insert(std::pair<std::string, void *>("lua_gettable", &Lua::lua_gettable));
	functionMap.insert(std::pair<std::string, void *>("lua_pushnil", &Lua::lua_pushnil));
	functionMap.insert(std::pair<std::string, void *>("lua_isstring", &Lua::lua_isstring));
	functionMap.insert(std::pair<std::string, void *>("lua_isnumber", &Lua::lua_isnumber));
	functionMap.insert(std::pair<std::string, void *>("lua_type", &Lua::lua_type));

	for (auto& kv: functionMap) {
		FARPROC tmp = GetProcAddress(hLua, kv.first.c_str());
		memcpy(kv.second, &tmp, sizeof(FARPROC));
		wsprintf(debug_string, L"%s at 0x%p", std::wstring(kv.first.begin(), kv.first.end()).c_str(), tmp);
		OutputDebugString(debug_string);
	}

	// Initialize MinHook.
	if (MH_Initialize() != MH_OK)
	{
		OutputDebugString(_T("Initialize failed"));
		return 1;
	}

	// Create a hook 
	if (MH_CreateHook((PVOID *)Lua::lua_open, &hooked_lua_open, reinterpret_cast<LPVOID*>(&orig_lua_open)) != MH_OK)
	{
		OutputDebugString(_T("Create hook failed"));
		return 1;
	}

	// Enable the hook 
	if (MH_EnableHook((PVOID *)Lua::lua_open) != MH_OK)
	{
		OutputDebugString(_T("Enable hook failed"));
		return 1;
	}

	// Create a hook 
	if (MH_CreateHook((PVOID *)Lua::lua_close, &hooked_lua_close, reinterpret_cast<LPVOID*>(&orig_lua_close)) != MH_OK)
	{
		OutputDebugString(_T("Create hook failed"));
		return 1;
	}

	// Enable the hook 
	if (MH_EnableHook((PVOID *)Lua::lua_close) != MH_OK)
	{
		OutputDebugString(_T("Enable hook failed"));
		return 1;
	}

	// Create a hook 
	if (MH_CreateHook((PVOID *)Lua::lua_gettop, &hooked_lua_gettop, reinterpret_cast<LPVOID*>(&orig_gettop)) != MH_OK)
	{
		OutputDebugString(_T("Create hook failed"));
		return 1;
	}

	// Enable the hook 
	if (MH_EnableHook((PVOID *)Lua::lua_gettop) != MH_OK)
	{
		OutputDebugString(_T("Enable hook failed"));
		return 1;
	}
	
	char buff[] = "game.spawnTank(\"tank_t90\", 100, 100, game.getLandHeight(100, 100), 0, \"usa\")";
	int error;
	
	while (1) {
		if (GetAsyncKeyState(VK_F9) & 1)
		{
			OutputDebugString(_T("Pressed F9, exiting"));
			if (MH_DisableHook(&Lua::lua_gettop) != MH_OK)
			{
				OutputDebugString(_T("Failed to unhook lua_gettop"));
			}
			break;
		}
		if (GetAsyncKeyState(VK_F8) & 1)
		{
			wsprintf(debug_string, _T("Current game_L is lua_State 0x%p"), game_L);
			OutputDebugString(debug_string);

			for (auto it : lua_States) {
				wsprintf(debug_string, _T("lua_State 0x%p"), it);
				OutputDebugString(debug_string);

				if (GetAsyncKeyState(VK_SHIFT)) {
					Lua::lua_pushstring(it, "_G");
					Lua::lua_gettable(it, LUA_GLOBALSINDEX);
					print_table(it, 0);
				}
			}
		}
		if (GetAsyncKeyState(VK_F7) & 1)
		{
			if (game_L == NULL && lua_States.size() != 0) {
				game_L = *lua_States.begin();
			}
			if (game_L == NULL) {
				OutputDebugString(_T("No valid lua_State found"));
				continue;
			}
			auto current_state = lua_States.find(game_L);
			auto end_state = lua_States.end();
			wsprintf(debug_string, _T("Current game_L is lua_State 0x%p"), *current_state);
			OutputDebugString(debug_string);
			if (GetAsyncKeyState(VK_SHIFT)) {
				std::advance(current_state, 1);
				if (current_state == end_state) {
					OutputDebugString(_T("Use first state"));
					game_L = *lua_States.begin();
				}
				else {
					OutputDebugString(_T("Increment state"));					
					game_L = *current_state;
				}
			}

		}

		if (GetAsyncKeyState(VK_F6) & 1)
		{
			wsprintf(debug_string, _T("Executing lua_dostring with lua_State 0x%p"), game_L);
			OutputDebugString(debug_string);
			error = Lua::lua_dostring(game_L, buff);
			if(error) {
				wsprintf(debug_string, _T("Error %s"), Lua::lua_tostring(game_L, -1));
				Lua::lua_settop(game_L, -(1)-1); /* pop error message from the stack */
			}
		}
		if (GetAsyncKeyState(VK_F5) & 1)
		{
			wsprintf(debug_string, _T("Execute luaopen_io with lua_State 0x%p"), game_L);
			OutputDebugString(debug_string);
			Lua::luaopen_io(game_L);
		}
	}

	FreeLibraryAndExitThread(g_hModule, 0);
	return 0;
}
