// Diagnose icon loading failures.
// Loads Notepad++.exe, enumerates RT_GROUP_ICON resources, and tries to load
// icon 501 using each of the three methods used by loadIcon().

#include <windows.h>
#include <cstdio>
#include <vector>

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

static void TestIconLoad(HMODULE hExe, int iconID)
{
    wprintf(L"\n=== Testing icon ID %d (0x%X) ===\n", iconID, iconID);

    // 1. Find RT_GROUP_ICON
    HRSRC hGrp = FindResourceW(hExe, (LPCWSTR)MAKEINTRESOURCE(iconID), (LPCWSTR)RT_GROUP_ICON);
    wprintf(L"[1] FindResourceW(RT_GROUP_ICON): %s\n", hGrp ? L"FOUND" : L"NOT FOUND");
    if (!hGrp) {
        wprintf(L"    LastError: %lu\n", GetLastError());
        return;
    }
    DWORD grpSize = SizeofResource(hExe, hGrp);
    HGLOBAL hGrpMem = LoadResource(hExe, hGrp);
    void* pDir = LockResource(hGrpMem);
    wprintf(L"    Size: %lu bytes\n", grpSize);

    // Parse header
    WORD idReserved = *(const WORD*)((const BYTE*)pDir + 0);
    WORD idType     = *(const WORD*)((const BYTE*)pDir + 2);
    WORD idCount    = *(const WORD*)((const BYTE*)pDir + 4);
    wprintf(L"    Header: reserved=%u type=%u count=%u\n", idReserved, idType, idCount);

    // 2. List all entries
    wprintf(L"[2] Entries:\n");
    int bestIndex = -1;
    int bestScore = 999999;
    const int cx = 16, cy = 16;
    for (WORD i = 0; i < idCount; ++i) {
        const GRPICONDIR_ENTRY* e = (const GRPICONDIR_ENTRY*)((const BYTE*)pDir + 6 + i * 14);
        int w = (e->bWidth == 0) ? 256 : e->bWidth;
        int h = (e->bHeight == 0) ? 256 : e->bHeight;
        wprintf(L"    [%u] w=%d h=%d planes=%u bpp=%u bytes=%lu nID=%u\n",
                i, w, h, e->wPlanes, e->wBitCount, e->dwBytesInRes, e->nID);
        int dw = abs(w - cx), dh = abs(h - cy);
        int score;
        if (w == cx && h == cy) score = 0 + dw + dh;
        else if (w >= cx && h >= cy) score = 10 + dw + dh;
        else score = 1000 + dw + dh;
        if (score < bestScore) { bestScore = score; bestIndex = i; }
    }
    if (bestIndex < 0) { wprintf(L"    No matching entry\n"); return; }
    const GRPICONDIR_ENTRY* best = (const GRPICONDIR_ENTRY*)((const BYTE*)pDir + 6 + bestIndex * 14);
    WORD chosenNID = best->nID;
    wprintf(L"    Best entry: %d, nID=%u (score=%d)\n", bestIndex, chosenNID, bestScore);

    // 3. Find RT_ICON for that nID
    HRSRC hIco = FindResourceW(hExe, (LPCWSTR)MAKEINTRESOURCE(chosenNID), (LPCWSTR)RT_ICON);
    wprintf(L"[3] FindResourceW(RT_ICON, nID=%u): %s\n", chosenNID, hIco ? L"FOUND" : L"NOT FOUND");
    if (!hIco) { wprintf(L"    LastError: %lu\n", GetLastError()); return; }
    DWORD icoSize = SizeofResource(hExe, hIco);
    HGLOBAL hIcoMem = LoadResource(hExe, hIco);
    void* pIco = LockResource(hIcoMem);
    wprintf(L"    Size: %lu bytes\n", icoSize);
    wprintf(L"    First 32 bytes:");
    for (DWORD j = 0; j < 32 && j < icoSize; ++j) wprintf(L" %02X", ((const BYTE*)pIco)[j]);
    wprintf(L"\n");

    // 4. CreateIconFromResourceEx
    HICON hIcon = CreateIconFromResourceEx((PBYTE)pIco, icoSize, TRUE, 0x00030000, cx, cy, LR_DEFAULTSIZE);
    wprintf(L"[4] CreateIconFromResourceEx(16x16): %s\n", hIcon ? L"OK" : L"FAILED");
    if (hIcon) DestroyIcon(hIcon);

    // 5. LoadImage
    hIcon = (HICON)LoadImageW(hExe, (LPCWSTR)MAKEINTRESOURCE(iconID), IMAGE_ICON, cx, cy, LR_DEFAULTSIZE);
    wprintf(L"[5] LoadImage(16x16): %s", hIcon ? L"OK" : L"FAILED");
    if (!hIcon) wprintf(L"  LastError: %lu", GetLastError());
    wprintf(L"\n");
    if (hIcon) DestroyIcon(hIcon);

    // 6. LoadIconWithScaleDown
    HMODULE hComctl = GetModuleHandleW(L"comctl32.dll");
    wprintf(L"[6] comctl32.dll handle: 0x%p\n", hComctl);
    if (hComctl) {
        FARPROC p = GetProcAddress(hComctl, "LoadIconWithScaleDown");
        wprintf(L"    GetProcAddress(\"LoadIconWithScaleDown\"): %s\n", p ? L"FOUND" : L"NOT FOUND");
        if (p) {
            typedef HRESULT (WINAPI *FnLoadIconWithScaleDown)(HINSTANCE, PCWSTR, int, int, HICON*);
            HICON hScale = nullptr;
            HRESULT hr = ((FnLoadIconWithScaleDown)p)(hExe, (PCWSTR)MAKEINTRESOURCE(iconID), cx, cy, &hScale);
            wprintf(L"    LoadIconWithScaleDown(16x16): hr=0x%08lX, hIcon=%s\n",
                    hr, hScale ? L"OK" : L"NULL");
            if (hScale) DestroyIcon(hScale);
        }
    }
}

int wmain(int argc, wchar_t* argv[])
{
    const wchar_t* exePath = (argc > 1) ? argv[1] : L"C:\\Users\\Arthur\\Documents\\GitHub\\notepad-plus-plus-xp\\PowerEditor\\bin\\Notepad++.exe";

    wprintf(L"Loading: %s\n", exePath);
    HMODULE hExe = LoadLibraryExW(exePath, nullptr, LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE);
    if (!hExe) {
        wprintf(L"LoadLibraryEx failed: %lu\n", GetLastError());
        return 1;
    }
    wprintf(L"Loaded OK, handle=0x%p\n\n", hExe);

    // Test a few key icon IDs
    int testIds[] = { 501, 502, 503, 505, 506, 507, 508, 510, 511, 512 };
    for (int id : testIds) {
        TestIconLoad(hExe, id);
    }

    FreeLibrary(hExe);
    return 0;
}
