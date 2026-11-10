# Upgrading to a newer upstream release

This document explains how to rebase the Windows XP port onto a newer  
[Notepad++](https://github.com/notepad-plus-plus/notepad-plus-plus) release.

Current baseline: **v8.9.8** — ported and verified on a real Windows XP SP3 machine
(starts cleanly, all icons render, no error dialogs).

Last verified: 2026-09-03.

For *building* the XP binary and for the general source compatibility rules, see  
[`BUILD.md`](BUILD.md). This document is only about moving to a new upstream version.

---

## Why this is usually quick

The port was deliberately structured so that XP-specific logic lives in files upstream  
does not have. Those files merge cleanly forever:

| File                                                          | Purpose                                                              |
| ------------------------------------------------------------- | -------------------------------------------------------------------- |
| `src/MISC/Common/NppWinXpCompat.h`                            | Vista+ API wrappers (dynamic `GetProcAddress` + down-level fallback) |
| `src/MISC/Common/NppGdiPlusIcon.h`                            | GDI+ PNG icon decoding (XP cannot decode PNG-in-ico)                 |
| `src/MISC/Common/NppXpShims.asm`                              | Import-slot redirection for `GetModuleHandleExW`                     |
| `src/MISC/Common/NppXpGetModuleHandleEx.cpp`                  | The replacement body for the above                                   |
| `src/WinControls/OpenSaveFileDialog/LegacyFileDialog.{h,cpp}` | Classic file dialogs for XP                                          |
| `tools/`                                                      | `checkXpBinary.py` and the icon diagnostics                          |

What *does* conflict is a fixed, small set:

- `PowerEditor/visual.net/notepadPlus.Cpp.props` and `notepadPlus.vcxproj`
- `.github/workflows/CI_build.yml`
- source files where a Vista+ call was swapped for a `NppXP::` wrapper

A healthy sign: business code should only show one-line substitutions like  
`NppXP::compareStringOrdinal(...)`. If `#if _WIN32_WINNT < 0x0600` blocks start  
spreading through feature code, the abstraction is leaking and the next upgrade  
will hurt.

---

## Procedure

### 1. Fetch and merge

```bash
git fetch upstream --tags
git merge upstream/v8.9.9          # use the target version tag
```

### 2. Resolve project-file conflicts first

This is the only step that needs real care.

When `.vcxproj` / `.props` conflict, **keep our XP settings** and merge in upstream's  
additions:

- `<MinimumRequiredVersion>5.01</MinimumRequiredVersion>`
- `<OptimizeReferences>true</OptimizeReferences>` (required for correctness, see step 5)
- `<CETCompat>false</CETCompat>` / `<ImageHasSafeExceptionHandlers>false</ImageHasSafeExceptionHandlers>`
- the `NppYYThunksObj` path at the front of `AdditionalDependencies`
- the MASM `masm.props` / `masm.targets` imports and the `.asm` item
- `WIN32;_WIN32_WINNT=_WIN32_WINNT_WINXP;WINVER=0x0501;NTDDI_VERSION=NTDDI_WINXPSP3`

Upstream very often adds new `.cpp` files. Missing one shows up as an unresolved  
extern at link time, so after merging, diff the file lists:

```bash
git diff upstream/v8.9.8 upstream/v8.9.9 --name-only -- '*.vcxproj'
```

### 3. Build — the compiler errors *are* your to-do list

Because `_WIN32_WINNT == 0x0501`, the SDK hides every Vista+ prototype. Any newly  
introduced Vista+ call fails to compile. That is exactly what you want: a precise,  
exhaustive list of what needs wrapping, far better than guessing after linking.

### 4. Fix each one — two patterns only

**(a) The API has an XP equivalent** → add a wrapper to `NppWinXpCompat.h`:

```cpp
inline HRESULT someNewVistaCall(...args...) noexcept
{
    using Fn = HRESULT (WINAPI*)(...);
    static FARPROC proc = nullptr;
    NppXP::detail::resolveOnce(proc, L"somedll.dll", "SomeNewVistaCall");
    if (NppXP::detail::available(proc))
        return reinterpret_cast<Fn>(proc)(...args...);

    // Windows XP fallback
    return E_NOTIMPL;
}
```

Then swap the call site to `NppXP::someNewVistaCall(...)`.

> **Never probe export ordinals.** `LoadIconWithScaleDown` is exported by ordinal only  
> in some comctl32 builds, and the ordinal differs per version. Guessing it made the  
> process call an unrelated function and crash at `0x0000000B`. Resolve **by name** only;  
> if that fails, return `E_NOTIMPL` and let the caller fall back.

**(b) The API has no XP equivalent** → guard the feature. Precedents already in the tree:  
the Vista Common Item Dialog falls back to `LegacyFileDialog`, DirectWrite/Direct2D is  
disabled, dark mode / TaskDialog / Application Restart / DWM are version-gated and simply  
never activate.

### 5. Verify — both configurations

```bash
python tools\checkXpBinary.py PowerEditor\bin\Notepad++.exe
```

Run it for **Release and Debug**. Debug is where `/OPT:REF` problems surface: without it,  
unused YY-Thunks stubs drag `esent.dll`, `pdh.dll`, `winhttp.dll`, ... into the import  
table and XP fails to load the process before `main` runs.

Sanity-check the checker itself:

```bash
python tools\checkXpBinary.py C:\Windows\SysWOW64\notepad.exe   # must exit 1
```

If that passes, the script is silently accepting everything and cannot be trusted.

Also confirm optional dependencies stayed out of the import table:

```bash
dumpbin /imports PowerEditor\bin\Notepad++.exe | findstr /i gdiplus
```

Empty output is correct — GDI+ is loaded with `LoadLibrary`, never linked.

### 6. Test on real hardware

Static checks do not prove the app runs. At minimum:

- Windows XP SP3: starts, icons render, file dialogs work, save/load works
- Windows 10: no functional regression

Icons deserve a specific look: XP's `user32` cannot decode **PNG-compressed** icon  
entries (Vista+ feature). If upstream ships new PNG-only icons, they will load through  
the GDI+ path in `NppGdiPlusIcon.h`. Verify with:

```bash
python tools\check_ico_formats.py       # reports PNG vs BMP per .ico
```

---

## Areas to watch on every upgrade

| Area                    | Why                                                                  |
| ----------------------- | -------------------------------------------------------------------- |
| **Icon resources**      | New/updated `.ico` files may be PNG-only; XP needs the GDI+ path     |
| **New Vista+ APIs**     | Compiler errors list them; each needs pattern (a) or (b)             |
| **Dark mode / theming** | XP's uxtheme lacks `DrawThemeTextEx` and friends                     |
| **File dialogs**        | `IFileDialog` does not exist on XP; must route to `LegacyFileDialog` |
| **DPI handling**        | XP has no Per-Monitor DPI APIs; fall back to system DPI              |
| **Bundled deps**        | Scintilla / Lexilla / Boost upgrades can pull in new Vista+ calls    |

---

## Regression checklist

- [ ] Release **and** Debug build cleanly
- [x] `checkXpBinary.py` returns 0 for both configurations
- [x] Reverse self-test on `SysWOW64\notepad.exe` returns 1
- [x] `dumpbin /imports` shows no `gdiplus.dll` (and no other optional dep)
- [x] XP SP3: app starts, icons render, open/save dialogs work
- [x] `bin/localization/*.xml` present (94 languages copied by `build.py` after build)
- [x] Windows 10: no functional regression

---

## Troubleshooting

| Symptom                                                | Cause                                                                                                                 |
| ------------------------------------------------------ | --------------------------------------------------------------------------------------------------------------------- |
| XP: "is not a valid Win32 application"                 | Subsystem version ≠ 5.01; check `MinimumRequiredVersion`                                                              |
| XP: "The procedure entry point X could not be located" | A post-XP import slipped in; run the checker                                                                          |
| Only Debug fails                                       | Missing `/OPT:REF` — YY-Thunks stubs dragging in extra DLLs                                                           |
| Everything links but icons are missing on XP           | PNG-in-ico; XP's user32 cannot decode it                                                                              |
| Crash with `Fault PC = Bad Address = 0x0000000B`       | Called a bad function pointer — usually a guessed export ordinal                                                      |
| Icon entries after the first parse wrong               | `GRPICONDIRENTRY` is 14 bytes vs `ICONDIRENTRY` 16; do not use `LookupIconIdFromDirectoryEx`                          |
| `MSB4166: environment variable name or value too long` | A huge env var (e.g. `ACC_PRODUCT_CONFIG_V3`, ~488 KB). Clear it, then build                                          |
| `LNK1104: cannot open file ...exe`                     | The exe is still running; close it first                                                                              |
| `GetLastError()` always returns the same odd value     | It is a *stale* code (183 `ERROR_ALREADY_EXISTS` comes from startup `CreateMutex`); the failing API never set its own |
