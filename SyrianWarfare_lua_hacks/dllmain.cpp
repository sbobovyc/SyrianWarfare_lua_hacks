// dllmain.cpp : Defines the entry point for the DLL application.
#include <tchar.h>
#include "stdafx.h"
#include <iostream>
#include <string.h>
#include <strsafe.h>
#include <unordered_set>  
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
	
	Lua::lua_open = (game_lua_open)GetProcAddress(hLua, "lua_open");
	wsprintf(debug_string, L"lua_open at 0x%p", Lua::lua_open);
	OutputDebugString(debug_string);
	Lua::lua_close = (game_lua_close)GetProcAddress(hLua, "lua_close");
	wsprintf(debug_string, L"lua_close at 0x%p", Lua::lua_close);
	OutputDebugString(debug_string);
	Lua::luaopen_io = (game_luaopen_io)GetProcAddress(hLua, "luaopen_io");
	wsprintf(debug_string, L"luaopen_io at 0x%p", Lua::luaopen_io);
	OutputDebugString(debug_string);
	Lua::lua_dostring = (game_lua_dostring)GetProcAddress(hLua, "lua_dostring");
	wsprintf(debug_string, L"lua_dostring at 0x%p", Lua::lua_dostring);
	OutputDebugString(debug_string);
	Lua::lua_dofile = (game_lua_dofile)GetProcAddress(hLua, "lua_dofile");
	wsprintf(debug_string, L"lua_dofile at 0x%p", Lua::lua_dofile);
	OutputDebugString(debug_string);
	
	Lua::lua_gettop = (game_lua_gettop)GetProcAddress(hLua, "lua_gettop");
	wsprintf(debug_string, L"lua_gettop at 0x%p", Lua::lua_gettop);
	OutputDebugString(debug_string);
	Lua::lua_tostring = (game_lua_tostring)GetProcAddress(hLua, "lua_tostring");
	wsprintf(debug_string, L"lua_tostring at 0x%p", Lua::lua_tostring);
	OutputDebugString(debug_string);	
	Lua::lua_settop = (game_lua_settop)GetProcAddress(hLua, "lua_settop");
	wsprintf(debug_string, L"lua_pop at 0x%p", Lua::lua_settop);
	OutputDebugString(debug_string);	
	Lua::lua_next = (game_lua_next)GetProcAddress(hLua, "lua_next");
	wsprintf(debug_string, L"lua_next at 0x%p", Lua::lua_next);
	OutputDebugString(debug_string);
	Lua::lua_tonumber = (game_lua_tonumber)GetProcAddress(hLua, "lua_tonumber");
	wsprintf(debug_string, L"lua_tonumber at 0x%p", Lua::lua_tonumber);
	OutputDebugString(debug_string);
	Lua::lua_topointer = (game_lua_topointer)GetProcAddress(hLua, "lua_topointer");
	wsprintf(debug_string, L"lua_topointer at 0x%p", Lua::lua_topointer);
	OutputDebugString(debug_string);
	Lua::lua_pushstring = (game_lua_pushstring)GetProcAddress(hLua, "lua_pushstring");
	wsprintf(debug_string, L"lua_pushstring at 0x%p", Lua::lua_pushstring);
	OutputDebugString(debug_string);
	Lua::lua_gettable = (game_lua_gettable)GetProcAddress(hLua, "lua_gettable");
	wsprintf(debug_string, L"lua_gettable at 0x%p", Lua::lua_gettable);
	OutputDebugString(debug_string);
	Lua::lua_pushnil = (game_lua_pushnil)GetProcAddress(hLua, "lua_pushnil");
	wsprintf(debug_string, L"lua_pushnil at 0x%p", Lua::lua_pushnil);
	OutputDebugString(debug_string);
	
	Lua::lua_isstring = (game_lua_isstring)GetProcAddress(hLua, "lua_isstring");
	wsprintf(debug_string, L"lua_isstring at 0x%p", Lua::lua_isstring);
	OutputDebugString(debug_string);
	Lua::lua_isnumber = (game_lua_isnumber)GetProcAddress(hLua, "lua_isnumber");
	wsprintf(debug_string, L"lua_isnumber at 0x%p", Lua::lua_isnumber);
	OutputDebugString(debug_string);
	Lua::lua_type = (game_lua_type)GetProcAddress(hLua, "lua_type");
	wsprintf(debug_string, L"lua_type at 0x%p", Lua::lua_type);
	OutputDebugString(debug_string);
	
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
