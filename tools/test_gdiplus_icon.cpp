// Verify the GDI+ PNG icon path used by NppXP::loadIconFromResource.
// Loads Notepad++.exe and, for each icon, decodes the RT_ICON PNG payload
// through the very same NppGdiPlus::createIconFromPngData() helper that ships
// in NppGdiPlusIcon.h, then reports the resulting HICON size.

#include <windows.h>
#include <objidl.h>
#include <cstdio>

#include "NppGdiPlusIcon.h"

struct GRPICONDIR_ENTRY
{
	BYTE  bWidth;
	BYTE  bHeight;
	BYTE  bColorCount;
	BYTE  bReserved;
	WORD  wPlanes;
	WORD  wBitCount;
	DWORD dwBytesInRes;
	WORD  nID;
};

static void TestGdiPlusIcon(HMODULE hExe, int iconID, int cx, int cy)
{
	wprintf(L"\n=== icon %d (requested %dx%d) ===\n", iconID, cx, cy);

	HRSRC hGrp = FindResourceW(hExe, (LPCWSTR)MAKEINTRESOURCE(iconID), (LPCWSTR)RT_GROUP_ICON);
	if (!hGrp) { wprintf(L"  RT_GROUP_ICON not found\n"); return; }

	HGLOBAL hGrpMem = LoadResource(hExe, hGrp);
	const BYTE* pDir = (const BYTE*)LockResource(hGrpMem);
	WORD idCount = *(const WORD*)(pDir + 4);

	// Pick best match for cx x cy
	int best = -1, bestScore = 999999;
	for (WORD i = 0; i < idCount; ++i)
	{
		const GRPICONDIR_ENTRY* e = (const GRPICONDIR_ENTRY*)(pDir + 6 + i * 14);
		int w = e->bWidth ? e->bWidth : 256;
		int h = e->bHeight ? e->bHeight : 256;
		int score = (w == cx && h == cy) ? (abs(w - cx) + abs(h - cy))
		          : (w >= cx && h >= cy) ? 10 + (w - cx) + (h - cy)
		          : 1000 + (cx - w) + (cy - h);
		if (score < bestScore) { bestScore = score; best = i; }
	}
	if (best < 0) { wprintf(L"  no matching entry\n"); return; }

	const GRPICONDIR_ENTRY* e = (const GRPICONDIR_ENTRY*)(pDir + 6 + best * 14);
	wprintf(L"  chosen entry: %dx%d nID=%u\n",
		e->bWidth ? e->bWidth : 256, e->bHeight ? e->bHeight : 256, e->nID);

	HRSRC hIco = FindResourceW(hExe, (LPCWSTR)MAKEINTRESOURCE(e->nID), (LPCWSTR)RT_ICON);
	if (!hIco) { wprintf(L"  RT_ICON not found\n"); return; }

	DWORD icoSize = SizeofResource(hExe, hIco);
	const void* pIco = LockResource(LoadResource(hExe, hIco));
	const BYTE* b = (const BYTE*)pIco;
	wprintf(L"  payload: %lu bytes, magic=%02X %02X %02X %02X %s\n",
		icoSize, b[0], b[1], b[2], b[3],
		(b[0] == 0x89 && b[1] == 0x50) ? L"(PNG)" : L"(BMP)");

	// Legacy user32 path (fails on XP for PNG)
	HICON hUser32 = CreateIconFromResourceEx((PBYTE)pIco, icoSize, TRUE, 0x00030000, cx, cy, LR_DEFAULTSIZE);
	wprintf(L"  [user32] CreateIconFromResourceEx -> %s\n", hUser32 ? L"OK" : L"NULL");
	if (hUser32) { DestroyIcon(hUser32); }

	// GDI+ path
	HICON hGdi = NppGdiPlus::createIconFromPngData(pIco, icoSize);
	wprintf(L"  [GDI+]   createIconFromPngData   -> %s\n", hGdi ? L"OK" : L"NULL");

	if (hGdi)
	{
		ICONINFO ii{};
		if (GetIconInfo(hGdi, &ii))
		{
			BITMAP bm{};
			if (ii.hbmColor && GetObjectW(ii.hbmColor, sizeof(bm), &bm))
				wprintf(L"           HICON bitmap = %ldx%ld, bpp=%d\n", bm.bmWidth, bm.bmHeight, bm.bmBitsPixel);
			if (ii.hbmColor)  DeleteObject(ii.hbmColor);
			if (ii.hbmMask)   DeleteObject(ii.hbmMask);
		}
		DestroyIcon(hGdi);
	}
}

int wmain(int argc, wchar_t* argv[])
{
	const wchar_t* exe = (argc > 1) ? argv[1]
		: L"C:\\Users\\Arthur\\Documents\\GitHub\\notepad-plus-plus-xp\\PowerEditor\\bin\\Notepad++.exe";

	wprintf(L"Loading: %s\n", exe);
	HMODULE hExe = LoadLibraryExW(exe, nullptr, LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE);
	if (!hExe) { wprintf(L"LoadLibraryEx failed: %lu\n", GetLastError()); return 1; }

	// Same icon IDs the runtime log showed, with the sizes Notepad++ asks for
	struct { int id; int cx; } tests[] = {
		{ 501, 16 }, { 502, 16 }, { 503, 16 }, { 505, 16 },
		{ 507, 16 }, { 510, 16 }, { 512, 16 },
		{ 1530, 11 }, { 1533, 11 }, { 1534, 11 }, { 1535, 11 },
	};

	for (const auto& t : tests)
		TestGdiPlusIcon(hExe, t.id, t.cx, t.cx);

	FreeLibrary(hExe);
	return 0;
}
