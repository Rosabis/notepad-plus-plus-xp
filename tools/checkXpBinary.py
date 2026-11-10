#!/usr/bin/env python3
"""Check that a PE binary is loadable on Windows XP.

Used by the CI workflow after every build. It verifies three things:

1. the PE subsystem version is 5.01 (Windows XP 32 bit) / 5.02 (XP x64, Server 2003);
2. the import table does not reference a DLL that does not exist on XP;
3. the import table does not reference a Vista+ function directly -- those have to be
   resolved with GetProcAddress (see src/MISC/Common/NppWinXpCompat.h) or provided by
   YY-Thunks, and in both cases they must not end up as a static import.

Exit code is non-zero when a hard error is found.
"""

import struct
import sys

# DLLs that exist on Windows XP / Server 2003 (the classic set plus a few common ones).
XP_DLLS = {
    "advapi32.dll", "comctl32.dll", "comdlg32.dll", "crypt32.dll", "dbghelp.dll",
    "gdi32.dll", "imm32.dll", "kernel32.dll", "msimg32.dll", "msvcrt.dll",
    "netapi32.dll", "ole32.dll", "oleaut32.dll", "olepro32.dll", "psapi.dll",
    "sensapi.dll", "shell32.dll", "shlwapi.dll", "urlmon.dll", "user32.dll",
    "uuid.dll", "uxtheme.dll", "version.dll", "wininet.dll", "winmm.dll",
    "winspool.drv", "wintrust.dll", "ws2_32.dll", "wsock32.dll", "rpcrt4.dll",
    "secur32.dll", "setupapi.dll", "iphlpapi.dll", "mpr.dll", "ntdll.dll",
    "oleacc.dll", "usp10.dll",
}

# NOTE: deliberately *not* whitelisted, even though the DLL itself ships with XP:
#
#   esent.dll  -- XP's copy predates JetInit3 / JetOSSnapshotFreeze / JetSetCurrentIndex4 /
#                 JetGetTruncateLogInfoInstance, which YY-Thunks references. A binary that
#                 imports esent.dll is almost certainly retaining unreferenced YY-Thunks
#                 thunks, so keeping it out of the list acts as an early warning that
#                 /OPT:REF is not doing its job.
#   userenv.dll, credui.dll, dnsapi.dll, pdh.dll, winhttp.dll
#              -- same story: present on XP SP3, but they only appear when unreferenced
#                 YY-Thunks thunks survive the link, which is a build configuration bug
#                 worth failing on rather than papering over.

# Functions that only exist from Windows Vista/7/8/10 on. If one of them shows up in the
# import table the binary will not start on XP.
POST_XP_FUNCTIONS = {
    "GetTickCount64", "GetFinalPathNameByHandleW", "GetFinalPathNameByHandleA",
    "LCMapStringEx", "GetLocaleInfoEx", "GetProductInfo", "CompareStringEx",
    "GetFileInformationByHandleEx", "SetFileInformationByHandle",
    "InitOnceExecuteOnce", "InitOnceInitialize", "InitializeSRWLock",
    "ReleaseSRWLockExclusive", "AcquireSRWLockExclusive", "WaitOnAddress",
    "FlsAlloc", "FlsGetValue", "FlsSetValue", "FlsFree",
    "EncodePointer", "DecodePointer", "InitializeCriticalSectionEx",
    "CreateSymbolicLinkW", "CreateSymbolicLinkA", "GetModuleHandleExW",
    "GetSystemTimePreciseAsFileTime", "QueryUnbiasedInterruptTime",
    "GetCurrentProcessorNumber", "GetCurrentProcessorNumberEx",
    "SetThreadErrorMode", "GetErrorMode", "GetLogicalProcessorInformationEx",
    "RegSetKeyValueW", "RegDeleteKeyValueW", "RegCopyTreeW", "RegDeleteTreeW",
    "LoadIconWithScaleDown", "LoadIconMetric", "SHCreateItemFromParsingName",
    "SHGetKnownFolderPath", "SHSetKnownFolderPath", "SHGetPropertyStoreForWindow",
    "DwmSetWindowAttribute", "DwmGetWindowAttribute", "DwmIsCompositionEnabled",
    "GetDpiForWindow", "GetDpiForSystem", "GetSystemMetricsForDpi",
    "AdjustWindowRectExForDpi", "SystemParametersInfoForDpi",
    "SetProcessDpiAwarenessContext", "SetThreadDpiAwarenessContext",
    "SetProcessDpiAwareness", "GetDpiForMonitor", "GetProcessDpiAwareness",
    "SetWindowCompositionAttribute", "CreateFile2", "GetFileVersionInfoExW",
    # NTFS Alternate Data Stream enumeration: Server 2003 / Vista, absent on 32-bit XP.
    "FindFirstStreamW", "FindNextStreamW", "FindFirstStreamTransactedW",
    # Process/thread and path helpers that are easy to call by accident.
    # NB: GetProcessId and SetDllDirectoryW are XP SP1+, so they are deliberately absent here.
    "QueryFullProcessImageNameW", "QueryFullProcessImageNameA",
    "GetThreadId", "GetThreadUILanguage",
    "AddDllDirectory", "SetDefaultDllDirectories",
    "CancelIoEx", "CancelSynchronousIo", "GetQueuedCompletionStatusEx",
    "CreateThreadpoolWork", "SubmitThreadpoolWork", "CloseThreadpoolWork",
    "CreateWaitableTimerExW", "SetWaitableTimerEx",
    "RegisterApplicationRestart", "UnregisterApplicationRestart",
    "GetUserDefaultLocaleName", "GetSystemDefaultLocaleName",
    "LocaleNameToLCID", "LCIDToLocaleName", "IsValidLocaleName",
    "GetNumberFormatEx", "GetCurrencyFormatEx", "GetDateFormatEx", "GetTimeFormatEx",
    "EnumSystemLocalesEx", "ResolveLocaleName", "FindNLSString", "FindNLSStringEx",
    "IdnToAscii", "IdnToUnicode", "NormalizeString", "IsNormalizedString",
    # Theming / composition / clipboard notifications.
    "BeginBufferedPaint", "EndBufferedPaint", "BufferedPaintInit", "BufferedPaintUnInit",
    "BeginBufferedAnimation", "EndBufferedAnimation", "BufferedPaintRenderAnimation",
    "BufferedPaintStopAllAnimations", "GetBufferedPaintBits",
    "GetThemeTransitionDuration", "DrawThemeTextEx",
    "AddClipboardFormatListener", "RemoveClipboardFormatListener",
    "ChangeWindowMessageFilter", "ChangeWindowMessageFilterEx",
    # Shell / known folders. NB: SHOpenFolderAndSelectItems, SHGetImageList and IsThemeActive
    # all exist on XP and are therefore intentionally not listed.
    "SHCreateItemFromIDList", "SHCreateShellItemArrayFromIDLists",
    "SHGetStockIconInfo",
    "PathCchCombine", "PathCchAppend", "PathCchCanonicalizeEx",
    # Registry helpers introduced with Vista.
    "RegGetValueW", "RegGetValueA", "RegLoadMUIStringW",
    # Misc.
    "GetTickCount64", "InterlockedCompareExchange64",
    "InitializeConditionVariable", "SleepConditionVariableSRW",
    "WakeConditionVariable", "WakeAllConditionVariable",
    "PowerCreateRequest", "PowerSetRequest", "PowerClearRequest",
}


class PE:
    def __init__(self, data):
        self.data = data
        e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
        if data[e_lfanew:e_lfanew + 4] != b"PE\0\0":
            raise ValueError("not a PE image")

        coff = e_lfanew + 4
        (self.machine, num_sections, _, _, _, opt_size, _) = struct.unpack_from("<HHIIIHH", data, coff)
        self.opt = coff + 20
        self.magic = struct.unpack_from("<H", data, self.opt)[0]
        self.plus = (self.magic == 0x20B)

        # Subsystem and subsystem version
        self.subsystem_version = struct.unpack_from("<HH", data, self.opt + 48)
        self.subsystem = struct.unpack_from("<H", data, self.opt + 68)[0]

        # Data directories
        dir_offset = self.opt + (112 if self.plus else 96)
        num_dirs = struct.unpack_from("<I", data, self.opt + (108 if self.plus else 92))[0]
        self.directories = []
        for i in range(min(num_dirs, 16)):
            rva, size = struct.unpack_from("<II", data, dir_offset + i * 8)
            self.directories.append((rva, size))

        # Sections
        self.sections = []
        sec_off = self.opt + opt_size
        for i in range(num_sections):
            base = sec_off + i * 40
            name = data[base:base + 8].rstrip(b"\0").decode("ascii", "replace")
            vsize, vaddr, rawsize, rawptr = struct.unpack_from("<IIII", data, base + 8)
            self.sections.append((name, vaddr, vsize, rawptr, rawsize))

    def rva_to_offset(self, rva):
        for _, vaddr, vsize, rawptr, rawsize in self.sections:
            if vaddr <= rva < vaddr + max(vsize, rawsize):
                delta = rva - vaddr
                if delta < rawsize:
                    return rawptr + delta
        return None

    def imports(self):
        """Yield (dll, [function, ...]) pairs from the import table."""
        if len(self.directories) < 2:
            return
        rva, _ = self.directories[1]
        if rva == 0:
            return
        off = self.rva_to_offset(rva)
        if off is None:
            return

        entry_size = 8 if self.plus else 4
        ordinal_flag = 1 << (63 if self.plus else 31)

        while True:
            chunk = self.data[off:off + 20]
            if len(chunk) < 20:
                return
            orig_thunk, _, _, name_rva, first_thunk = struct.unpack("<IIIII", chunk)
            if orig_thunk == 0 and name_rva == 0 and first_thunk == 0:
                return

            name_off = self.rva_to_offset(name_rva)
            dll = ""
            if name_off is not None:
                end = self.data.find(b"\0", name_off)
                dll = self.data[name_off:end].decode("ascii", "replace").lower()

            thunk_rva = orig_thunk if orig_thunk else first_thunk
            thunk_off = self.rva_to_offset(thunk_rva)
            functions = []
            if thunk_off is not None:
                while True:
                    raw = self.data[thunk_off:thunk_off + entry_size]
                    if len(raw) < entry_size:
                        break
                    value = int.from_bytes(raw, "little")
                    if value == 0:
                        break
                    if value & ordinal_flag:
                        functions.append("ordinal#%d" % (value & 0xFFFF))
                    else:
                        hint_off = self.rva_to_offset(value)
                        if hint_off is None:
                            break
                        end = self.data.find(b"\0", hint_off + 2)
                        functions.append(self.data[hint_off + 2:end].decode("ascii", "replace"))
                    thunk_off += entry_size

            yield dll, functions
            off += 20


def main(argv):
    if len(argv) != 2:
        print("usage: checkXpBinary.py <path-to-exe>")
        return 2

    path = argv[1]
    with open(path, "rb") as handle:
        data = handle.read()

    pe = PE(data)
    machine_name = {0x014C: "x86", 0x8664: "x64", 0xAA64: "ARM64"}.get(pe.machine, hex(pe.machine))
    subsystem_name = {2: "GUI", 3: "CUI"}.get(pe.subsystem, str(pe.subsystem))

    print("File            : %s" % path)
    print("Machine         : %s" % machine_name)
    print("Subsystem       : %s" % subsystem_name)
    print("Subsystem ver.  : %d.%02d" % pe.subsystem_version)
    print("")

    errors = []

    expected = (5, 2) if pe.machine == 0x8664 else (5, 1)
    if pe.subsystem_version != expected:
        errors.append("subsystem version is %d.%02d, expected %d.%02d for Windows XP"
                      % (pe.subsystem_version + expected))

    unknown_dlls = []
    bad_functions = []
    total_dlls = 0

    for dll, functions in pe.imports():
        if not dll:
            continue
        total_dlls += 1
        if dll not in XP_DLLS:
            unknown_dlls.append(dll)
        for function in functions:
            if function in POST_XP_FUNCTIONS:
                bad_functions.append("%s!%s" % (dll, function))

    print("Imported DLLs (%d):" % total_dlls)
    for dll, _ in pe.imports():
        if dll:
            print("  %-24s %s" % (dll, "" if dll in XP_DLLS else "  <-- NOT AVAILABLE ON WINDOWS XP"))
    print("")

    for dll in unknown_dlls:
        errors.append("imports %s which does not exist on Windows XP" % dll)
    for entry in bad_functions:
        errors.append("statically imports %s which does not exist on Windows XP" % entry)

    if errors:
        print("FAILED - the binary will not start on Windows XP:")
        for error in errors:
            print("  * %s" % error)
        return 1

    print("OK - subsystem version and import table are Windows XP compatible.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
