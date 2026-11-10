# Building Notepad++ (Windows XP edition)

> **This fork targets Windows XP.** Every build is a 32-bit (Win32) Windows XP build:
> the PE subsystem version is 5.01 and the CRT/STL calls that Windows XP does not provide
> are supplied by [YY-Thunks](https://github.com/Chuyu-Team/YY-Thunks).
> The CI workflow builds `Release` and `Debug` for `Win32` only.

## Microsoft Visual Studio

**Pre-requisites:**

- Microsoft Visual Studio 2022 version 17.5 (C/C++ Compiler, v143 toolset for win32, x64, arm64)
- [YY-Thunks-Objs](https://github.com/Chuyu-Team/YY-Thunks/releases) (only needed to produce a
  binary that actually runs on Windows XP)

There are three components which are built from one visual studio solution:

- `notepad++.exe`: (contains `libSciLexer.lib`)
- `libScintilla.lib` : static library based on [Scintilla](https://www.scintilla.org/)
- `libLexilla.lib` : static library based on [Lexilla](https://www.scintilla.org/Lexilla.html)

Notepad++ is always built **with** Boost regex PCRE support instead of default c++11 regex ECMAScript used by plain Scintilla.

### Build `notepad++.exe`

 1. Open [`PowerEditor\visual.net\notepadPlus.sln`](https://github.com/notepad-plus-plus/notepad-plus-plus/blob/master/PowerEditor/visual.net/notepadPlus.sln)
 2. Select a solution configuration (Debug or Release) and the **Win32** solution platform
 3. Build Notepad++ solution like a normal Visual Studio project. This will also build the dependent Scintilla and Lexilla projects.

### Building a Windows XP compatible binary

The project is compiled with the current `v143` toolset but defines
`_WIN32_WINNT=0x0501` / `NTDDI_VERSION=NTDDI_WINXPSP3` and links with subsystem version
`5.01`. That is not enough on its own: since Visual Studio 2019 16.7 the MSVC static CRT and
the STL use Windows Vista+ entry points (`FlsAlloc`, `EncodePointer`, SRW locks,
`WaitOnAddress`, `GetFileInformationByHandleEx` used by `std::filesystem`, ...).

YY-Thunks provides fallbacks for all of them, so no source change is needed for that part:

 1. Download and unzip [YY-Thunks-Objs](https://github.com/Chuyu-Team/YY-Thunks/releases)
    (e.g. to `C:\YYThunks`).
 2. Build with the `NppYYThunksRoot` property pointing at that folder:

    ```
    msbuild notepadPlus.sln /m /p:configuration=Release /p:platform=Win32 /p:NppYYThunksRoot="C:\YYThunks\"
    ```

    Without the property the project still builds; only the CRT/STL thunks are then missing
    and the resulting `notepad++.exe` will not start on Windows XP.

 3. Optionally verify the produced binary:

    ```
    python tools\checkXpBinary.py PowerEditor\bin\Notepad++.exe
    ```

    It reports the PE subsystem version, lists the imported DLLs and fails when
    non-Windows-XP imports slipped in.

### Windows XP source compatibility rules

The Windows SDK hides every Vista+ declaration when `_WIN32_WINNT == 0x0501`, so any such
API has to be reached through the helpers in
[`PowerEditor/src/MISC/Common/NppWinXpCompat.h`](PowerEditor/src/MISC/Common/NppWinXpCompat.h)
(dynamic `GetProcAddress` plus a down-level fallback) instead of being called directly.

Higher level features that do not exist on XP are switched off at run time:

* the Vista **Common Item Dialog** (`IFileDialog`) is replaced by the classic
  `GetOpenFileName`/`GetSaveFileName` dialogs — see `LegacyFileDialog`;
* **DirectWrite/Direct2D** rendering is disabled (they need Vista+);
* **dark mode**, **TaskDialog**, **Application Restart** and **DWM** code paths are already
  version gated and simply do not activate;
* HMAC-SHA256 is computed with the bundled SHA-256 (`MISC/sha2`) instead of BCrypt.

One more XP-specific limitation is worth calling out because it is easy to miss: Windows
XP's `user32` cannot decode **PNG-compressed** icon entries — PNG-in-ico is a Vista+
feature, and upstream ships PNG-only icons. On XP those go through the GDI+ decoder in
[`NppGdiPlusIcon.h`](PowerEditor/src/MISC/Common/NppGdiPlusIcon.h) instead.

### Bundling the languages (multilingual build)

Notepad++ loads the UI translations from `bin/localization/*.xml` at runtime (the "localization"
folder next to `notepad++.exe`). The MSBuild step does **not** copy them — `build.py` copies
`PowerEditor/installer/nativeLang/*.xml` into `bin/localization/` automatically after a
successful build (94 languages, including `chineseSimplified.xml`). If you ever build with
MSBuild directly (bypassing `build.py`), run that copy step manually or the UI will only
offer the built-in English fallback. These files are plain XML, so they do not affect XP
compatibility of the executable itself.

### Moving to a newer upstream release

See [`XP_UPGRADE.md`](XP_UPGRADE.md) for the step-by-step procedure (merge, resolve the
project-file conflicts, let the compiler list the new Vista+ APIs, verify with
`checkXpBinary.py` in both configurations, then test on real hardware).

### Build `libScintilla.lib` and `libLexilla.lib`

As mentioned above, you'll need `libScintilla.lib` and `libLexilla.lib` for the Notepad++ build. This is done automatically on building the whole solution. So normally you don't need to care about this.

#### Build `libScintilla.lib` with boost and `libLexilla.lib` via nmake

This is not necessary any more and just here for completeness as this option is still available.
Boost is taken from [boost 1.90.0](https://www.boost.org/users/history/version_1_90_0.html) and stripped down to the project needs available at [boost](https://github.com/notepad-plus-plus/notepad-plus-plus/tree/master/boostregex/boost) in this repo.

1. Open the Developer Command Prompt for Visual Studio
2. Go into the [`scintilla\win32\`](https://github.com/notepad-plus-plus/notepad-plus-plus/blob/master/scintilla/win32/)
3. Build the same configuration as notepad++:
   - Release: `nmake -f scintilla.mak`
   - Debug: `nmake DEBUG=1 -f scintilla.mak`
   - Example:
   `nmake -f scintilla.mak`
4. Go into the [`lexilla\src\`](https://github.com/notepad-plus-plus/notepad-plus-plus/tree/master/lexilla/src/)
5. Build the same configuration as notepad++:
   - Release: `nmake -f lexilla.mak`
   - Debug: `nmake DEBUG=1 -f lexilla.mak`

### History

More about the previous build process: <https://community.notepad-plus-plus.org/topic/13959/building-notepad-with-visual-studio-2015-2017>

Since `Notepad++` version 6.0 - 7.9.5, the build of dynamic linked `SciLexer.dll` that is distributed
uses features from Boost's `Boost.Regex` library.

## GCC

If you have [MinGW-w64](https://www.mingw-w64.org/) installed, you can compile Notepad++ with GCC. Otherwise MinGW-w64 can be downloaded [here](https://sourceforge.net/projects/mingw-w64/files/). You can also download some collection of tools which supports MinGW-w64, like [MSYS2](https://www.msys2.org/) or [WinLibs](https://winlibs.com/).

Building Notepad++ is regularly tested on a Windows system by using [MSYS2](https://www.msys2.org/) project. Current versions of tools used to building (such as GCC, Clang, Make or Bash) can be checked by looking at some logs from the finished building (for example in the [current-build page](https://github.com/notepad-plus-plus/notepad-plus-plus/actions/workflows/CI_build.yml)). Other versions may also work but are untested.

**Note:** Before building make sure that the system `PATH` environment variable contains `$MinGW-root$\bin` directory. Otherwise you have to set this directory yourself in Windows settings. You can also use a command like `set PATH=$MinGW-root$\bin;%PATH%` each time `cmd` is launched. But beware that if `PATH` contains several versions of MinGW-w64 GCC, only the first one will be usable.

### Compiling Notepad++ binary

1. Launch `cmd` and add `$MinGW-root$\bin` to `PATH` if necessary.
2. `cd` into `notepad-plus-plus\PowerEditor\gcc`.
3. Run `mingw32-make`.
4. The 32-bit or 64-bit `notepad++.exe` will be generated either in `bin.i686` or in `bin.x86_64` directory respectively, depending on the target CPU of the compiler — look for the full path to the resulting binary at the end of the build process.

#### Some additional information

- The directory containing `notepad++.exe` will also contain everything needed for Notepad++ to start.
- To have a debug build just add `DEBUG=1` to the `mingw32-make` invocation above. The output directory then will be suffixed with `-debug`.
- To see commands being executed add `VERBOSE=1` to the same command.
- When a project is built outside of the `PowerEditor/gcc` directory, for example when using `-f` option, then the entire project path must not contain any spaces. Additionally, the path to `makefile` of this project should be listed as first.
- When a project is built through MinGW-w64 with multilib support, a specific target can be forced by passing `TARGET_CPU` variable with `x86_64` or `i686` as value.
- To use Clang instead of GCC for compilation provide `CXX` variable with `clang++` as value.
- To use [Clang analyzer](https://clang-analyzer.llvm.org/) together with Clang provide `CLANGANALYZE=1` to the `mingw32-make` invocation.
