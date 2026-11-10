; This file is part of Notepad++ project
; Copyright (C)2026 Arthur <Rosabis>
;
; Redirects the statically-imported kernel32!GetModuleHandleExW to our own
; XP-safe emulation (NppXpGetModuleHandleExW in NppXpGetModuleHandleEx.cpp).
;
; The static MSVC CRT references the import slot __imp__GetModuleHandleExW@12.
; Because this object file is linked *before* any import library, the linker
; resolves that slot to the definition below instead of kernel32.lib's, so the
; final binary no longer imports GetModuleHandleExW from kernel32.dll (which
; does not exist on Windows XP and would otherwise break the loader).

.386
.MODEL FLAT

EXTERN _NppXpGetModuleHandleExW@12 : PROC

.DATA
PUBLIC __imp__GetModuleHandleExW@12
__imp__GetModuleHandleExW@12 DWORD OFFSET _NppXpGetModuleHandleExW@12

.CODE
PUBLIC _GetModuleHandleExW@12
_GetModuleHandleExW@12 PROC
    jmp _NppXpGetModuleHandleExW@12
_GetModuleHandleExW@12 ENDP

END
