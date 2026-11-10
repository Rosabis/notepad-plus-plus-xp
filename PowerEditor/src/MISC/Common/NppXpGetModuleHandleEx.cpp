// This file is part of Notepad++ project
// Copyright (C)2026 Arthur <Rosabis>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

// XP-compatible emulation of kernel32!GetModuleHandleExW.
//
// GetModuleHandleExW is a Windows Vista API. The static MSVC CRT/STL reference it,
// but YY-Thunks' "WinXP" object does not provide it, so the linker otherwise falls
// back to kernel32.lib's import -- which does not exist on Windows XP and would make
// the loader fail at start-up. This file supplies a correct-enough emulation; the
// matching import slot is redirected to it by NppXpShims.asm (linked as an object
// file, i.e. before any import library, so the linker prefers our definition).

#include <windows.h>

#ifndef GET_MODULE_HANDLE_EX_FLAG_PIN
#	define GET_MODULE_HANDLE_EX_FLAG_PIN                0x00000001
#	define GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT 0x00000002
#	define GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS       0x00000004
#endif

extern "C" BOOL WINAPI NppXpGetModuleHandleExW(DWORD dwFlags, LPCWSTR lpModuleName, HMODULE* phModule)
{
	if (phModule == nullptr)
	{
		::SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	HMODULE hMod = nullptr;
	if (dwFlags & GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS)
	{
		// The caller passed a code/data address; resolve the module that owns it.
		MEMORY_BASIC_INFORMATION mbi{};
		if (::VirtualQuery(lpModuleName, &mbi, sizeof(mbi)) == 0 || mbi.AllocationBase == nullptr)
		{
			*phModule = nullptr;
			::SetLastError(ERROR_INVALID_PARAMETER);
			return FALSE;
		}
		hMod = static_cast<HMODULE>(mbi.AllocationBase);
	}
	else
	{
		hMod = ::GetModuleHandleW(lpModuleName);
	}

	if (hMod == nullptr)
	{
		*phModule = nullptr;
		return FALSE;
	}

	// Unless the caller asked us not to, add a real reference so the module cannot be
	// unloaded while they hold the handle (mirrors the kernel behaviour).
	if ((dwFlags & GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT) == 0)
	{
		wchar_t path[MAX_PATH] = { 0 };
		if (::GetModuleFileNameW(hMod, path, MAX_PATH) != 0)
		{
			HMODULE hLoaded = ::LoadLibraryW(path);
			if (hLoaded != nullptr)
				hMod = hLoaded;
		}
	}

	*phModule = hMod;
	return TRUE;
}
