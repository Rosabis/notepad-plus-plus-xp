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

#pragma once

// Notepad++ XP edition
// --------------------
// The whole project is compiled with a recent MSVC toolset (C++20) but targets
// Windows XP (_WIN32_WINNT == 0x0501), so the Windows SDK headers hide every
// API that Vista and later introduced.  Each of those calls is reached through
// the thin wrappers below: the address is resolved once with GetProcAddress and,
// when the API does not exist on the running system, a down-level equivalent is
// used instead.
//
// The C runtime itself is made XP-compatible by linking YY_Thunks_for_WinXP.obj
// (see .github/workflows and BUILD.md): it supplies the Vista+ entry points the
// MSVC static CRT and STL need (FlsAlloc, EncodePointer, SRW locks, WaitOnAddress,
// GetFileInformationByHandleEx used by std::filesystem, and so on).

#include <windows.h>
#include <commctrl.h>
#include <objidl.h>
#include <shellapi.h>
#include <stdlib.h>
#include <string>
#include <cstdarg>
#include <cstdio>
#include <cwchar>

#include "NppGdiPlusIcon.h"

#if defined(_WIN32_WINNT) && (_WIN32_WINNT < 0x0600)
#	define NPP_TARGET_WINXP 1
#endif

// dwmapi.h is entirely guarded by NTDDI_VERSION >= NTDDI_VISTA, so the constant
// has to be provided here when the SDK hides it.
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#	define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

// The RRF_* flags live in winreg.h next to RegGetValueW; make sure they exist even if
// the SDK version in use decides to hide them for a Windows XP target.
#ifndef RRF_RT_REG_NONE
#	define RRF_RT_REG_NONE      0x00000001
#	define RRF_RT_REG_SZ        0x00000002
#	define RRF_RT_REG_EXPAND_SZ 0x00000004
#	define RRF_RT_REG_BINARY    0x00000008
#	define RRF_RT_REG_DWORD     0x00000010
#	define RRF_RT_REG_MULTI_SZ  0x00000020
#	define RRF_RT_REG_QWORD     0x00000040
#	define RRF_RT_ANY           0x0000ffff
#endif

// Constants that only appear in the SDK for a Windows Vista (or newer) target.
#ifndef USER_DEFAULT_SCREEN_DPI
#	define USER_DEFAULT_SCREEN_DPI 96
#endif
#ifndef LOCALE_NAME_SYSTEM_DEFAULT
#	define LOCALE_NAME_SYSTEM_DEFAULT L"!x-sys-default-locale"
#endif
#ifndef LOCALE_NAME_USER_DEFAULT
#	define LOCALE_NAME_USER_DEFAULT nullptr
#endif
#ifndef LOCALE_RETURN_NUMBER
#	define LOCALE_RETURN_NUMBER 0x20000000
#endif

// Miscellaneous constants the SDK only exposes for a newer target. They keep their real
// values: the corresponding API calls are routed through the helpers below, which strip the
// bits an older system would reject with ERROR_INVALID_PARAMETER.
#ifndef SM_CXPADDEDBORDER
#	define SM_CXPADDEDBORDER 92
#endif
#ifndef WM_MOUSEHWHEEL
#	define WM_MOUSEHWHEEL 0x020E
#endif
#ifndef BCN_FIRST
#	define BCN_FIRST (0U - 1250U)
#endif
#ifndef BCN_DROPDOWN
#	define BCN_DROPDOWN (BCN_FIRST + 0x0002)
#endif
#ifndef COPY_FILE_NO_BUFFERING
#	define COPY_FILE_NO_BUFFERING 0x00001000
#endif
#ifndef REPLACEFILE_IGNORE_MERGE_ERRORS
#	define REPLACEFILE_IGNORE_MERGE_ERRORS 0x00000002
#endif
#ifndef REPLACEFILE_IGNORE_ACL_ERRORS
#	define REPLACEFILE_IGNORE_ACL_ERRORS 0x00000004
#endif

// IFileDialog option flags (FOS_*) -- Windows Vista+
#ifndef FOS_PICKFOLDERS
#	define FOS_PICKFOLDERS        0x00000020
#endif
#ifndef FOS_FORCEFILESYSTEM
#	define FOS_FORCEFILESYSTEM    0x00000040
#endif
#ifndef FOS_PATHMUSTEXIST
#	define FOS_PATHMUSTEXIST      0x00000800
#endif
#ifndef FOS_FILEMUSTEXIST
#	define FOS_FILEMUSTEXIST      0x00001000
#endif
#ifndef FOS_ALLOWMULTISELECT
#	define FOS_ALLOWMULTISELECT   0x00000200
#endif
#ifndef FOS_NOTESTFILECREATE
#	define FOS_NOTESTFILECREATE  0x00010000
#endif

// Toolbar common-control messages / custom-draw return flags -- Windows Vista+
#ifndef TB_GETITEMDROPDOWNRECT
#	define TB_GETITEMDROPDOWNRECT (WM_USER + 103)
#endif
#ifndef TBCDRF_USECDCOLORS
#	define TBCDRF_USECDCOLORS    0x00800000
#endif

// Tooltip icon identifier -- Windows Vista+
#ifndef TTI_ERROR_LARGE
#	define TTI_ERROR_LARGE 6
#endif

// Locale sort flag -- Windows Vista+
#ifndef SORT_DIGITSASNUMBERS
#	define SORT_DIGITSASNUMBERS 0x00000008
#endif

// Progress bar state identifiers -- Windows Vista+
#ifndef PBST_NORMAL
#	define PBST_NORMAL  0x0001
#	define PBST_ERROR   0x0002
#	define PBST_PAUSED  0x0003
#endif

// Progress bar messages -- Windows Vista+
#ifndef PBM_SETSTATE
#	define PBM_SETSTATE 0x0410
#endif

// ListView item state -- Windows Vista+
#ifndef LIS_DEFAULTCOLORS
#	define LIS_DEFAULTCOLORS 0x00000010
#endif

// Button styles -- Windows Vista+
#ifndef BS_SPLITBUTTON
#	define BS_SPLITBUTTON     0x0000000C
#	define BS_DEFSPLITBUTTON 0x0000000D
#endif

// Rebar band mask flags -- Windows Vista+
#ifndef RBBIM_CHEVRONLOCATION
#	define RBBIM_CHEVRONLOCATION 0x00001000
#	define RBBIM_CHEVRONSTATE    0x00002000
#endif

// uxtheme DTTOPTS / DrawThemeTextEx -- Windows Vista+
// The dark-mode code uses DrawThemeTextEx for themed text. The call is resolved through
// GetProcAddress (see NppXP::drawThemeTextEx) so it never becomes a static import, which
// would otherwise break the Windows XP loader (uxtheme.dll on XP lacks the export).
//
// The Windows 10 SDK declares the DTTOPTS struct and DTT_CALLBACK_PROC inside
// <uxtheme.h> unconditionally (only the DrawThemeTextEx *function* prototype is gated
// behind NTDDI_VISTA), so those types are available even when targeting Windows XP. Pull
// the header in so every translation unit that includes this compat layer sees them.
// uxtheme.h has its own include guard, so this is a harmless no-op when the caller already
// included it. Defining the types ourselves would clash with uxtheme.h (C2011/C2116) and is
// therefore avoided.
#ifndef _UXTHEME_H_
#	include <uxtheme.h>
#endif

// Application Recovery and Restart -- Windows Vista+
#ifndef RESTART_NO_CRASH
#	define RESTART_NO_CRASH  1
#	define RESTART_NO_HANG   2
#	define RESTART_NO_PATCH  4
#	define RESTART_NO_REBOOT 8
#endif
#ifndef RESTART_MAX_CMD_LINE
#	define RESTART_MAX_CMD_LINE 1024
#endif

namespace NppXP
{
	namespace detail
	{
		// Stored instead of nullptr once an API is known to be missing, so the
		// lookup is attempted only once.
		inline FARPROC unavailable() noexcept
		{
			return reinterpret_cast<FARPROC>(static_cast<INT_PTR>(-1));
		}

		inline FARPROC resolveOnce(FARPROC& cache, const wchar_t* moduleName, const char* procName) noexcept
		{
			if (cache != nullptr)
				return cache;

			HMODULE module = ::GetModuleHandleW(moduleName);
			if (module == nullptr)
				module = ::LoadLibraryW(moduleName); // dwmapi.dll and friends are not mapped on XP

			cache = (module != nullptr) ? ::GetProcAddress(module, procName) : nullptr;
			if (cache == nullptr)
				cache = NppXP::detail::unavailable();

			return cache;
		}

		inline bool available(FARPROC proc) noexcept
		{
			return proc != nullptr && proc != NppXP::detail::unavailable();
		}
	}


	// Minimal run-time OS check, used to strip flags an older kernel would reject.
	// Deliberately self-contained so that this header has no Notepad++ dependencies.
	inline bool isAtLeastWindows(DWORD majorVersion, DWORD minorVersion) noexcept
	{
		static OSVERSIONINFOW osvi{};			// zero initialised: no guard variable needed
		if (osvi.dwOSVersionInfoSize == 0)
		{
			osvi.dwOSVersionInfoSize = sizeof(osvi);
#pragma warning(suppress: 4996) // GetVersionExW is deprecated since Win8.1 but is the only API available on XP
			if (::GetVersionExW(&osvi) == FALSE)
			{
				osvi.dwMajorVersion = 0;
				osvi.dwMinorVersion = 0;
			}
		}

		return (osvi.dwMajorVersion > majorVersion) ||
			(osvi.dwMajorVersion == majorVersion && osvi.dwMinorVersion >= minorVersion);
	}

	inline bool isAtLeastWin7() noexcept
	{
		return NppXP::isAtLeastWindows(6, 1);
	}

	inline bool isAtLeastWin8() noexcept
	{
		return NppXP::isAtLeastWindows(6, 2);
	}


	// CopyFileEx flags -- COPY_FILE_NO_BUFFERING (0x1000) is Windows 8+ and unknown bits
	// make CopyFileEx fail with ERROR_INVALID_PARAMETER.
	inline DWORD copyFileExFlags(DWORD flags) noexcept
	{
		if ((flags & COPY_FILE_NO_BUFFERING) != 0 && !NppXP::isAtLeastWin8())
			flags &= ~static_cast<DWORD>(COPY_FILE_NO_BUFFERING);

		return flags;
	}


	// ReplaceFile flags -- REPLACEFILE_IGNORE_MERGE_ERRORS / REPLACEFILE_IGNORE_ACL_ERRORS
	// are Windows 7+ and are rejected by older kernels.
	inline BOOL replaceFile(LPCWSTR lpReplacedFileName, LPCWSTR lpReplacementFileName,
		LPCWSTR lpBackupFileName, DWORD dwReplaceFlags, LPVOID lpExclude, LPVOID lpReserved) noexcept
	{
		constexpr DWORD win7Flags = REPLACEFILE_IGNORE_MERGE_ERRORS | REPLACEFILE_IGNORE_ACL_ERRORS;
		if ((dwReplaceFlags & win7Flags) != 0 && !NppXP::isAtLeastWin7())
			dwReplaceFlags &= ~static_cast<DWORD>(win7Flags);

		return ::ReplaceFileW(lpReplacedFileName, lpReplacementFileName, lpBackupFileName,
			dwReplaceFlags, lpExclude, lpReserved);
	}


	// kernel32!CompareStringOrdinal -- Windows Vista+
	inline int compareStringOrdinal(LPCWCH lpString1, int cchCount1,
		LPCWCH lpString2, int cchCount2, BOOL bIgnoreCase) noexcept
	{
		using FnCompareStringOrdinal = int (WINAPI*)(LPCWCH, int, LPCWCH, int, BOOL);
		static FARPROC proc = nullptr;
		NppXP::detail::resolveOnce(proc, L"kernel32.dll", "CompareStringOrdinal");
		if (NppXP::detail::available(proc))
			return reinterpret_cast<FnCompareStringOrdinal>(proc)(lpString1, cchCount1, lpString2, cchCount2, bIgnoreCase);

		// Windows XP: ordinal (code unit) comparison, optionally case insensitive
		if (lpString1 == nullptr || lpString2 == nullptr)
		{
			::SetLastError(ERROR_INVALID_PARAMETER);
			return 0;
		}

		const int length1 = (cchCount1 < 0) ? static_cast<int>(::lstrlenW(lpString1)) : cchCount1;
		const int length2 = (cchCount2 < 0) ? static_cast<int>(::lstrlenW(lpString2)) : cchCount2;
		const int common = (length1 < length2) ? length1 : length2;

		for (int i = 0; i < common; ++i)
		{
			wchar_t c1 = lpString1[i];
			wchar_t c2 = lpString2[i];

			if (bIgnoreCase != FALSE)
			{
				if (::LCMapStringW(LOCALE_INVARIANT, LCMAP_UPPERCASE, &lpString1[i], 1, &c1, 1) == 0)
					c1 = lpString1[i];
				if (::LCMapStringW(LOCALE_INVARIANT, LCMAP_UPPERCASE, &lpString2[i], 1, &c2, 1) == 0)
					c2 = lpString2[i];
			}

			if (c1 != c2)
				return (c1 < c2) ? CSTR_LESS_THAN : CSTR_GREATER_THAN;
		}

		if (length1 == length2)
			return CSTR_EQUAL;

		return (length1 < length2) ? CSTR_LESS_THAN : CSTR_GREATER_THAN;
	}


	// kernel32!GetTickCount64 -- Windows Vista+
	// XP only has the 32-bit counter, which wraps after ~49.7 days.
	inline ULONGLONG getTickCount64() noexcept
	{
		using FnGetTickCount64 = ULONGLONG (WINAPI*)();
		static FARPROC proc = nullptr;
		NppXP::detail::resolveOnce(proc, L"kernel32.dll", "GetTickCount64");
		if (NppXP::detail::available(proc))
			return reinterpret_cast<FnGetTickCount64>(proc)();

		return static_cast<ULONGLONG>(::GetTickCount());
	}


	// kernel32!GetFinalPathNameByHandle -- Windows Vista+
	// Returns 0 on XP: callers must fall back to the path they already know.
	inline DWORD getFinalPathNameByHandle(HANDLE hFile, LPWSTR lpszFilePath, DWORD cchFilePath, DWORD dwFlags) noexcept
	{
		using FnGetFinalPathNameByHandle = DWORD (WINAPI*)(HANDLE, LPWSTR, DWORD, DWORD);
		static FARPROC proc = nullptr;
		NppXP::detail::resolveOnce(proc, L"kernel32.dll", "GetFinalPathNameByHandleW");
		if (NppXP::detail::available(proc))
			return reinterpret_cast<FnGetFinalPathNameByHandle>(proc)(hFile, lpszFilePath, cchFilePath, dwFlags);

		return 0;
	}


	// kernel32!QueryFullProcessImageName -- Windows Vista+
	// XP has no such API; the path is obtained through GetModuleFileNameExW from psapi.dll,
	// which lives in the same process and yields the executable image path just the same.
	inline DWORD queryFullProcessImageNameW(HANDLE hProcess, DWORD dwFlags, LPWSTR lpExeName, PDWORD lpdwSize) noexcept
	{
		using FnQueryFullProcessImageNameW = DWORD (WINAPI*)(HANDLE, DWORD, LPWSTR, PDWORD);
		static FARPROC proc = nullptr;
		NppXP::detail::resolveOnce(proc, L"kernel32.dll", "QueryFullProcessImageNameW");
		if (NppXP::detail::available(proc))
			return reinterpret_cast<FnQueryFullProcessImageNameW>(proc)(hProcess, dwFlags, lpExeName, lpdwSize);

		using FnGetModuleFileNameExW = DWORD (WINAPI*)(HANDLE, HMODULE, LPWSTR, DWORD);
		static FARPROC psapiProc = nullptr;
		NppXP::detail::resolveOnce(psapiProc, L"psapi.dll", "GetModuleFileNameExW");
		if (NppXP::detail::available(psapiProc))
		{
			const DWORD size = (lpdwSize != nullptr) ? *lpdwSize : 0;
			const DWORD len = reinterpret_cast<FnGetModuleFileNameExW>(psapiProc)(hProcess, nullptr, lpExeName, size);
			if (len != 0 && lpdwSize != nullptr)
				*lpdwSize = len;
			return len;
		}

		return 0;
}


	// user32!ChangeWindowMessageFilterEx -- Windows 7+
	// XP has no UIPI (User Interface Privilege Isolation) and therefore no message
	// filtering, so the call is a no-op there. Resolved through GetProcAddress so it
	// never becomes a static import (user32 on XP does not export it, which would
	// otherwise make the loader fail).
	// MSGFLT_ALLOW / MSGFLT_DISALLOW are only declared by the SDK for Vista+. They are
	// exposed as namespaced constants rather than macros so that they do not clobber
	// the local MSGFLT_* const definitions elsewhere (notably winmain.cpp's
	// "#ifndef MSGFLT_ALLOW" block, which also declares MSGFLT_DISALLOW).
	constexpr DWORD msgfltAllow = 1;
	constexpr DWORD msgfltDisallow = 2;

	inline BOOL changeWindowMessageFilterEx(HWND hWnd, UINT message, DWORD action, LPVOID pChangeFilterStruct) noexcept
	{
		using FnChangeWindowMessageFilterEx = BOOL (WINAPI*)(HWND, UINT, DWORD, LPVOID);
		static FARPROC proc = nullptr;
		NppXP::detail::resolveOnce(proc, L"user32.dll", "ChangeWindowMessageFilterEx");
		if (NppXP::detail::available(proc))
			return reinterpret_cast<FnChangeWindowMessageFilterEx>(proc)(hWnd, message, action, pChangeFilterStruct);

		return TRUE; // XP: no UIPI, the message passes through unconditionally
	}


	// kernel32!FindFirstStreamW -- Windows Vista+ on the client SKUs.
	// Windows Server 2003 (and therefore XP x64) exports it, plain 32-bit Windows XP does not,
	// so it must never become a static import. Notepad++ only uses it to probe for NTFS
	// Alternate Data Streams; on XP the call fails with ERROR_CALL_NOT_IMPLEMENTED and callers
	// should use hasFindFirstStream() to tell "no ADS on this file" apart from "cannot tell".
	inline HANDLE findFirstStreamW(LPCWSTR lpFileName, STREAM_INFO_LEVELS infoLevel,
	                               LPVOID lpFindStreamData, DWORD dwFlags) noexcept
	{
		using FnFindFirstStreamW = HANDLE (WINAPI*)(LPCWSTR, STREAM_INFO_LEVELS, LPVOID, DWORD);
		static FARPROC proc = nullptr;
		NppXP::detail::resolveOnce(proc, L"kernel32.dll", "FindFirstStreamW");
		if (NppXP::detail::available(proc))
			return reinterpret_cast<FnFindFirstStreamW>(proc)(lpFileName, infoLevel, lpFindStreamData, dwFlags);

		::SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
		return INVALID_HANDLE_VALUE;
	}


	// False on Windows XP, where the Alternate Data Stream enumeration API does not exist and
	// findFirstStreamW() above can therefore not give a meaningful answer.
	inline bool hasFindFirstStream() noexcept
	{
		static FARPROC proc = nullptr;
		NppXP::detail::resolveOnce(proc, L"kernel32.dll", "FindFirstStreamW");
		return NppXP::detail::available(proc);
	}


	// uxtheme!DrawThemeTextEx -- Windows Vista+
	// Resolved through GetProcAddress so it is never a static import (uxtheme.dll on XP
	// does not export it, which would otherwise make the loader fail). On XP the call
	// degrades to a plain DrawText. HANDLE is used instead of HTHEME so that this header
	// does not depend on uxtheme.h being included by every translation unit.
	inline HRESULT drawThemeTextEx(HANDLE hTheme, HDC hdc, int iPartId, int iStateId,
		LPCWSTR pszText, int cchText, DWORD dwTextFlags, LPRECT pRect, const DTTOPTS* pOptions) noexcept
	{
		using FnDrawThemeTextEx = HRESULT (WINAPI*)(HANDLE, HDC, int, int, LPCWSTR, int, DWORD, LPRECT, const DTTOPTS*);
		static FARPROC proc = nullptr;
		NppXP::detail::resolveOnce(proc, L"uxtheme.dll", "DrawThemeTextEx");
		if (NppXP::detail::available(proc))
			return reinterpret_cast<FnDrawThemeTextEx>(proc)(hTheme, hdc, iPartId, iStateId, pszText, cchText, dwTextFlags, pRect, pOptions);

		if (pRect != nullptr)
			::DrawTextW(hdc, pszText, cchText, pRect, dwTextFlags);
		return S_OK;
	}


	// kernel32!GetProductInfo -- Windows Vista+
	// Returns FALSE on XP, which means "not a Server Core SKU".
	inline BOOL getProductInfo(DWORD dwOSMajorVersion, DWORD dwOSMinorVersion,
		DWORD dwSpMajorVersion, DWORD dwSpMinorVersion, PDWORD pdwReturnedProductType) noexcept
	{
		using FnGetProductInfo = BOOL (WINAPI*)(DWORD, DWORD, DWORD, DWORD, PDWORD);
		static FARPROC proc = nullptr;
		NppXP::detail::resolveOnce(proc, L"kernel32.dll", "GetProductInfo");
		if (NppXP::detail::available(proc))
		{
			return reinterpret_cast<FnGetProductInfo>(proc)(dwOSMajorVersion, dwOSMinorVersion,
				dwSpMajorVersion, dwSpMinorVersion, pdwReturnedProductType);
		}

		return FALSE;
	}


	// kernel32!LCMapStringEx -- Windows Vista+
	// XP knows nothing about locale names, so the user default LCID is used instead.
	// Locale-name specific flags (LINGUISTIC_IGNOREDIACRITIC, SORT_DIGITSASNUMBERS...)
	// are silently ignored by LCMapStringW, which only degrades the sort order.
	inline int lcMapStringEx(LPCWSTR lpLocaleName, DWORD dwMapFlags, LPCWSTR lpSrcStr, int cchSrc,
		LPWSTR lpDestStr, int cchDest, LPVOID lpVersionInformation, LPVOID lpReserved, LPARAM sortHandle) noexcept
	{
		using FnLCMapStringEx = int (WINAPI*)(LPCWSTR, DWORD, LPCWSTR, int, LPWSTR, int, LPVOID, LPVOID, LPARAM);
		static FARPROC proc = nullptr;
		NppXP::detail::resolveOnce(proc, L"kernel32.dll", "LCMapStringEx");
		if (NppXP::detail::available(proc))
		{
			return reinterpret_cast<FnLCMapStringEx>(proc)(lpLocaleName, dwMapFlags, lpSrcStr, cchSrc,
				lpDestStr, cchDest, lpVersionInformation, lpReserved, sortHandle);
		}

		return ::LCMapStringW(LOCALE_USER_DEFAULT, dwMapFlags, lpSrcStr, cchSrc, lpDestStr, cchDest);
	}


	// kernel32!GetLocaleInfoEx -- Windows Vista+
	// Replacement for the single use Notepad++ had for it: reading the ANSI
	// codepage of the system default locale as a number.
	// LOCALE_RETURN_NUMBER is a Vista addition, hence the string round-trip on XP.
	inline int getSystemDefaultAnsiCodepage() noexcept
	{
		using FnGetLocaleInfoEx = int (WINAPI*)(LPCWSTR, LCTYPE, LPWSTR, int);
		static FARPROC proc = nullptr;
		NppXP::detail::resolveOnce(proc, L"kernel32.dll", "GetLocaleInfoEx");

		int codepage = 0;
		if (NppXP::detail::available(proc))
		{
			if (reinterpret_cast<FnGetLocaleInfoEx>(proc)(LOCALE_NAME_SYSTEM_DEFAULT,
				LOCALE_IDEFAULTANSICODEPAGE | LOCALE_RETURN_NUMBER,
				reinterpret_cast<LPWSTR>(&codepage),
				static_cast<int>(sizeof(int) / sizeof(wchar_t))) != 0)
			{
				return codepage;
			}
		}

		wchar_t buffer[16]{};
		if (::GetLocaleInfoW(LOCALE_SYSTEM_DEFAULT, LOCALE_IDEFAULTANSICODEPAGE, buffer, 16) == 0)
			return 0;

		return static_cast<int>(::wcstol(buffer, nullptr, 10));
	}


	// comctl32!LoadIconWithScaleDown -- Windows Vista+
	// Returns E_NOTIMPL on Windows XP (comctl32 v5.82 lacks the export) so that
	// callers fall back to LoadImage / the manual PNG-aware loader below.
	//
	// RESOLUTION STRATEGY (important):
	// We resolve the function ONLY by name. On Windows Vista+ (comctl32 v6, the
	// version Notepad++ activates through its manifest) the symbol is exported by
	// name and GetProcAddress succeeds. On Windows XP (comctl32 v5.82) the export
	// does not exist and the lookup fails, which is the correct signal to fall back.
	//
	// We deliberately do NOT probe ordinals. The ordinal assigned to
	// LoadIconWithScaleDown is NOT stable across comctl32 builds -- it is 381 on
	// comctl32 v6 but a *different, unrelated function* occupies that same ordinal
	// on other builds (e.g. v5.82 reports 382 as the slot, and the slot contents
	// differ between versions). Calling whatever happens to sit at a guessed ordinal
	// with LoadIconWithScaleDown's signature corrupts the stack and crashes the host
	// (observed as an access violation jumping to a near-null address during tab /
	// toolbar icon loading). Name resolution is the only safe, version-independent way.
	inline HRESULT loadIconWithScaleDown(HINSTANCE hinst, PCWSTR pszName, int cx, int cy, HICON* phico) noexcept
	{
		using FnLoadIconWithScaleDown = HRESULT (WINAPI*)(HINSTANCE, PCWSTR, int, int, HICON*);
		static FARPROC proc = nullptr;

		// Fast path: already resolved (success) -> call it
		if (NppXP::detail::available(proc))
			return reinterpret_cast<FnLoadIconWithScaleDown>(proc)(hinst, pszName, cx, cy, phico);

		// Already known to be missing -> tell the caller to fall back
		if (proc == NppXP::detail::unavailable())
			return E_NOTIMPL;

		HMODULE mod = ::GetModuleHandleW(L"comctl32.dll");
		if (!mod)
			mod = ::LoadLibraryW(L"comctl32.dll");

		if (mod)
			proc = ::GetProcAddress(mod, "LoadIconWithScaleDown");

		if (proc == nullptr)
			proc = NppXP::detail::unavailable();  // permanently unavailable on this OS

		if (NppXP::detail::available(proc))
			return reinterpret_cast<FnLoadIconWithScaleDown>(proc)(hinst, pszName, cx, cy, phico);

		return E_NOTIMPL;
	}


	// GRPICONDIRENTRY -- layout of each entry inside an RT_GROUP_ICON resource.
	// This differs from ICONDIRENTRY (used in .ico files) in the last field:
	//   GRPICONDIRENTRY ends with WORD  nID (2 bytes)  = 14 bytes total
	//   ICONDIRENTRY   ends with DWORD dwImageOffset (4 bytes) = 16 bytes total
	// We must use this struct when parsing RT_GROUP_ICON resources directly.
#pragma pack(push, 1)
	struct GRPICONDIR_ENTRY
	{
		BYTE  bWidth;
		BYTE  bHeight;
		BYTE  bColorCount;
		BYTE  bReserved;
		WORD  wPlanes;
		WORD  wBitCount;
		DWORD dwBytesInRes;
		WORD  nID;            // resource identifier (NOT a file offset)
	};
#pragma pack(pop)


	// XP-compatible icon loader that understands PNG-compressed resources.
	// This is the final fallback when LoadIconWithScaleDown and LoadImage fail.
	//
	// TWO PROBLEMS IT SOLVES
	//
	// 1. Resource format: we parse the RT_GROUP_ICON directory ourselves rather
	//    than using LookupIconIdFromDirectoryEx, because that API expects
	//    ICONDIRENTRY (.ico file, 16 bytes/entry, trailing DWORD dwImageOffset)
	//    while RT_GROUP_ICON resources store GRPICONDIRENTRY (14 bytes/entry,
	//    trailing WORD nID).  The 2-byte difference misaligns every entry after
	//    the first, yielding wrong matches or invalid resource IDs.
	//
	// 2. PNG-in-ico: Windows XP's user32 cannot decode PNG payloads inside an
	//    RT_ICON entry -- LoadImage and CreateIconFromResourceEx only understand
	//    the legacy BMP (DIB) form.  PNG-in-ico arrived with Windows Vista, and
	//    every icon shipped with Notepad++ 8.9.8 uses it, so on XP *all* of them
	//    would fail to load.
	//
	//    For PNG payloads the bytes are handed to GDI+ (gdiplus.dll, present
	//    since XP SP1) through its flat C API, which decodes PNG fine and can
	//    produce an HICON directly.  GDI+ is loaded lazily with LoadLibrary, so
	//    a system without it simply falls back to the user32 path instead of
	//    failing at load time.  See NppGdiPlusIcon.h.
	//
	// BMP payloads keep going through CreateIconFromResourceEx (user32, Win2000+).
	inline HICON loadIconFromResource(HINSTANCE hinst, PCWSTR pszName, int cx, int cy) noexcept
	{
		// Only handle integer resource IDs (passed as MAKEINTRESOURCE from IconList)
		if (reinterpret_cast<UINT_PTR>(pszName) > 0xFFFF)
			return nullptr;

		const HRSRC hGrpRes = ::FindResourceW(hinst, pszName, reinterpret_cast<LPCWSTR>(RT_GROUP_ICON));
		if (!hGrpRes)
			return nullptr;

		const DWORD grpSize = ::SizeofResource(hinst, hGrpRes);
		const HGLOBAL hGrpMem = ::LoadResource(hinst, hGrpRes);
		if (!hGrpMem || grpSize < sizeof(WORD) * 3)
			return nullptr;

		auto* pDir = static_cast<const BYTE*>(::LockResource(hGrpMem));
		if (!pDir)
			return nullptr;

		// Parse GRPICONDIR header (same layout as ICONDIR: reserved, type, count)
		const WORD idCount = *reinterpret_cast<const WORD*>(pDir + 4);
		if (idCount == 0 || grpSize < static_cast<DWORD>(6u + idCount * 14u)) // 14 = sizeof(GRPICONDIRENTRY)
			return nullptr;

		// Manually find the best-matching entry for cx × cy.
		// Selection logic mirrors what LoadIconWithScaleDown / the shell does:
		//   1. Prefer exact size match
		//   2. Fall back to nearest-larger (upscaling looks better than downscaling)
		//   3. Accept any available size as last resort
		int bestIndex = -1;
		int bestScore = 999999;
		constexpr int PENALTY_EXACT = 0;
		constexpr int PENALTY_SMALLER = 1000;     // downscale penalty
		constexpr int PENALTY_LARGER = 10;         // upscale penalty (preferred over downscale)

		for (WORD i = 0; i < idCount; ++i)
		{
			const auto* pEntry = reinterpret_cast<const GRPICONDIR_ENTRY*>(pDir + 6 + i * 14);
			const int w = (pEntry->bWidth == 0) ? 256 : static_cast<int>(pEntry->bWidth);
			const int h = (pEntry->bHeight == 0) ? 256 : static_cast<int>(pEntry->bHeight);
			const int dw = abs(w - cx);
			const int dh = abs(h - cy);

			int score;
			if (w == cx && h == cy)
				score = PENALTY_EXACT + dw + dh;
			else if (w >= cx && h >= cy)
				score = PENALTY_LARGER + dw + dh;
			else
				score = PENALTY_SMALLER + dw + dh;

			if (score < bestScore)
			{
				bestScore = score;
				bestIndex = static_cast<int>(i);
			}
		}

		if (bestIndex < 0)
			return nullptr;

		// Read the nID from the best-matching GRPICONDIRENTRY
		const auto* bestEntry = reinterpret_cast<const GRPICONDIR_ENTRY*>(pDir + 6 + bestIndex * 14);
		const WORD iconId = bestEntry->nID;

		// Load the actual icon image data (RT_ICON)
		const HRSRC hIcoRes = ::FindResourceW(hinst, MAKEINTRESOURCE(iconId), reinterpret_cast<LPCWSTR>(RT_ICON));
		if (!hIcoRes)
			return nullptr;

		const DWORD icoSize = ::SizeofResource(hinst, hIcoRes);
		const HGLOBAL hIcoMem = ::LoadResource(hinst, hIcoRes);
		if (!hIcoMem || icoSize == 0)
			return nullptr;

		const void* pIcoData = ::LockResource(hIcoMem);
		if (!pIcoData)
			return nullptr;

		// PNG-compressed payload (89 50 4E 47): Windows XP's user32 cannot decode
		// it, so hand the bytes to GDI+ (present since XP SP1) instead.
		// BMP payloads (BITMAPINFOHEADER, 28 00 00 00) go through user32 as usual.
		static constexpr BYTE pngMagic[4] = { 0x89, 0x50, 0x4E, 0x47 };
		if (icoSize >= 4 && ::memcmp(pIcoData, pngMagic, sizeof(pngMagic)) == 0)
		{
			const HICON hPngIcon = NppGdiPlus::createIconFromPngData(pIcoData, icoSize, cx, cy);
			if (hPngIcon != nullptr)
				return hPngIcon;
			// GDI+ unavailable or decode failed -- fall through and let user32 try
		}

		// Legacy BMP payload: CreateIconFromResourceEx (user32 since Win2000/XP)
		return ::CreateIconFromResourceEx(
			static_cast<PBYTE>(const_cast<void*>(pIcoData)), icoSize,
			TRUE, 0x00030000, cx, cy, LR_DEFAULTSIZE);
	}


	// shell32!SHCreateItemFromParsingName -- Windows Vista+
	// Returns E_NOTIMPL on XP; the Vista file dialog is not available there anyway.
	inline HRESULT shCreateItemFromParsingName(PCWSTR pszPath, IBindCtx* pbc, REFIID riid, void** ppv) noexcept
	{
		using FnSHCreateItemFromParsingName = HRESULT (WINAPI*)(PCWSTR, IBindCtx*, REFIID, void**);
		static FARPROC proc = nullptr;
		NppXP::detail::resolveOnce(proc, L"shell32.dll", "SHCreateItemFromParsingName");
		if (NppXP::detail::available(proc))
			return reinterpret_cast<FnSHCreateItemFromParsingName>(proc)(pszPath, pbc, riid, ppv);

		return E_NOTIMPL;
	}


	// dwmapi!DwmSetWindowAttribute -- Windows Vista+
	// Desktop composition does not exist on XP; report it as such.
	inline HRESULT dwmSetWindowAttribute(HWND hwnd, DWORD dwAttribute, LPCVOID pvAttribute, DWORD cbAttribute) noexcept
	{
		using FnDwmSetWindowAttribute = HRESULT (WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
		static FARPROC proc = nullptr;
		NppXP::detail::resolveOnce(proc, L"dwmapi.dll", "DwmSetWindowAttribute");
		if (NppXP::detail::available(proc))
			return reinterpret_cast<FnDwmSetWindowAttribute>(proc)(hwnd, dwAttribute, pvAttribute, cbAttribute);

		return static_cast<HRESULT>(0x80263001); // DWM_E_COMPOSITIONDISABLED
	}


	// kernel32!RegisterApplicationRestart / UnregisterApplicationRestart -- Windows Vista+
	// Application Restart simply does not exist on XP, so the calls are no-ops there.
	inline HRESULT registerApplicationRestart(PCWSTR pwzCommandline, DWORD dwFlags) noexcept
	{
		using FnRegisterApplicationRestart = HRESULT (WINAPI*)(PCWSTR, DWORD);
		static FARPROC proc = nullptr;
		NppXP::detail::resolveOnce(proc, L"kernel32.dll", "RegisterApplicationRestart");
		if (NppXP::detail::available(proc))
			return reinterpret_cast<FnRegisterApplicationRestart>(proc)(pwzCommandline, dwFlags);

		return E_NOTIMPL;
	}

	inline HRESULT unregisterApplicationRestart() noexcept
	{
		using FnUnregisterApplicationRestart = HRESULT (WINAPI*)();
		static FARPROC proc = nullptr;
		NppXP::detail::resolveOnce(proc, L"kernel32.dll", "UnregisterApplicationRestart");
		if (NppXP::detail::available(proc))
			return reinterpret_cast<FnUnregisterApplicationRestart>(proc)();

		return E_NOTIMPL;
	}

	inline HRESULT getApplicationRestartSettings(HANDLE hProcess, PWSTR pwzCommandline,
		PDWORD pcchCommandline, PDWORD pdwFlags) noexcept
	{
		using FnGetApplicationRestartSettings = HRESULT (WINAPI*)(HANDLE, PWSTR, PDWORD, PDWORD, PDWORD);
		static FARPROC proc = nullptr;
		NppXP::detail::resolveOnce(proc, L"kernel32.dll", "GetApplicationRestartSettings");
		if (NppXP::detail::available(proc))
		{
			return reinterpret_cast<FnGetApplicationRestartSettings>(proc)(
				hProcess, pwzCommandline, pcchCommandline, pdwFlags, nullptr);
		}

		return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
	}


	// user32!Add/RemoveClipboardFormatListener -- Windows Vista+
	// Windows XP has no clipboard change notification, so the calls fail with
	// ERROR_CALL_NOT_IMPLEMENTED rather than referencing a missing import.
	inline BOOL addClipboardFormatListener(HWND hwnd) noexcept
	{
		using FnAddClipboardFormatListener = BOOL (WINAPI*)(HWND);
		static FARPROC proc = nullptr;
		NppXP::detail::resolveOnce(proc, L"user32.dll", "AddClipboardFormatListener");
		if (NppXP::detail::available(proc))
			return reinterpret_cast<FnAddClipboardFormatListener>(proc)(hwnd);

		::SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
		return FALSE;
	}

	inline BOOL removeClipboardFormatListener(HWND hwnd) noexcept
	{
		using FnRemoveClipboardFormatListener = BOOL (WINAPI*)(HWND);
		static FARPROC proc = nullptr;
		NppXP::detail::resolveOnce(proc, L"user32.dll", "RemoveClipboardFormatListener");
		if (NppXP::detail::available(proc))
			return reinterpret_cast<FnRemoveClipboardFormatListener>(proc)(hwnd);

		::SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
		return FALSE;
	}


	// advapi32!RegGetValue -- Windows Vista+
	// Emulated on XP with RegOpenKeyExW/RegQueryValueExW, including the RRF_RT_* type filter.
	inline LSTATUS regGetValueW(HKEY hkey, LPCWSTR lpSubKey, LPCWSTR lpValue, DWORD dwFlags,
		LPDWORD pdwType, PVOID pvData, LPDWORD pcbData) noexcept
	{
		using FnRegGetValueW = LSTATUS (WINAPI*)(HKEY, LPCWSTR, LPCWSTR, DWORD, LPDWORD, PVOID, LPDWORD);
		static FARPROC proc = nullptr;
		NppXP::detail::resolveOnce(proc, L"advapi32.dll", "RegGetValueW");
		if (NppXP::detail::available(proc))
			return reinterpret_cast<FnRegGetValueW>(proc)(hkey, lpSubKey, lpValue, dwFlags, pdwType, pvData, pcbData);

		HKEY hSubKey = hkey;
		if (lpSubKey != nullptr && *lpSubKey != L'\0')
		{
			const LSTATUS openStatus = ::RegOpenKeyExW(hkey, lpSubKey, 0, KEY_READ, &hSubKey);
			if (openStatus != ERROR_SUCCESS)
				return openStatus;
		}

		DWORD valueType = 0;
		DWORD valueSize = (pcbData != nullptr) ? *pcbData : 0;
		const LSTATUS queryStatus = ::RegQueryValueExW(hSubKey, lpValue, nullptr, &valueType,
			static_cast<LPBYTE>(pvData), &valueSize);
		if (hSubKey != hkey)
			::RegCloseKey(hSubKey);

		if (queryStatus != ERROR_SUCCESS)
			return queryStatus;

		// Reject a value whose registry type does not match the requested RRF_RT_* filter
		const DWORD requestedTypes = dwFlags & RRF_RT_ANY;
		if (requestedTypes != RRF_RT_ANY)
		{
			DWORD mask = 0;
			if ((requestedTypes & RRF_RT_REG_SZ) != 0)        mask |= REG_SZ;
			if ((requestedTypes & RRF_RT_REG_EXPAND_SZ) != 0) mask |= REG_EXPAND_SZ;
			if ((requestedTypes & RRF_RT_REG_BINARY) != 0)    mask |= REG_BINARY;
			if ((requestedTypes & RRF_RT_REG_DWORD) != 0)     mask |= REG_DWORD;
			if ((requestedTypes & RRF_RT_REG_MULTI_SZ) != 0)  mask |= REG_MULTI_SZ;
			if ((requestedTypes & RRF_RT_REG_QWORD) != 0)     mask |= REG_QWORD;
			if ((valueType & mask) == 0)
				return ERROR_UNSUPPORTED_TYPE;
		}

		if (pdwType != nullptr)
			*pdwType = valueType;
		if (pcbData != nullptr)
			*pcbData = valueSize;

		return ERROR_SUCCESS;
	}


	// shell32!SHParseDisplayName + shell32!SHOpenFolderAndSelectItems -- Windows Vista+
	// On XP the selection is done by invoking explorer.exe with the /select switch.
	inline HRESULT openFolderAndSelect(const wchar_t* path) noexcept
	{
		// SFGAOF is a plain ULONG; spelled out here so that the shell headers are not needed
		using FnShParseDisplayName = HRESULT (WINAPI*)(PCWSTR, IBindCtx*, void**, ULONG, ULONG*);
		using FnShOpenFolderAndSelectItems = HRESULT (WINAPI*)(const void*, UINT, void* const*, DWORD);

		static FARPROC procParse = nullptr;
		static FARPROC procOpen = nullptr;
		NppXP::detail::resolveOnce(procParse, L"shell32.dll", "SHParseDisplayName");
		NppXP::detail::resolveOnce(procOpen, L"shell32.dll", "SHOpenFolderAndSelectItems");

		if (NppXP::detail::available(procParse) && NppXP::detail::available(procOpen))
		{
			void* pidl = nullptr;
			HRESULT hr = reinterpret_cast<FnShParseDisplayName>(procParse)(path, nullptr, &pidl, 0, nullptr);
			if (SUCCEEDED(hr))
			{
				hr = reinterpret_cast<FnShOpenFolderAndSelectItems>(procOpen)(pidl, 0, nullptr, 0);
				::CoTaskMemFree(pidl);
			}
			return hr;
		}

		std::wstring parameters = L"/select,\"";
		parameters += (path != nullptr) ? path : L"";
		parameters += L"\"";

		const INT_PTR result = reinterpret_cast<INT_PTR>(
			::ShellExecuteW(nullptr, L"open", L"explorer.exe", parameters.c_str(), nullptr, SW_SHOWNORMAL));
		return (result > 32) ? S_OK : E_FAIL;
	}
}
