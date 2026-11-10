@echo off
REM Build diagnostic tool with proper Windows SDK environment
setlocal

REM Find latest Windows SDK
for /f "delims=" %%v in ('dir /b "C:\Program Files (x86)\Windows Kits\10\Include\" 2^>nul') do set "SDK_VER=%%v"

set "VC=C:\Users\Arthur\VS2022BT\VC\Tools\MSVC\14.44.35207"
set "SDK=C:\Program Files (x86)\Windows Kits\10"

set "INCLUDE=%VC%\include;%SDK%\Include\%SDK_VER%\um;%SDK%\Include\%SDK_VER%\ucrt;%SDK%\Include\%SDK_VER%\shared"
set "LIB=%VC%\lib\x86;%SDK%\Lib\%SDK_VER%\um\x86;%SDK%\Lib\%SDK_VER%\ucrt\x86"

cd /d "C:\Users\Arthur\Documents\GitHub\notepad-plus-plus-xp\tools"
"%VC%\bin\Hostx64\x86\cl.exe" /nologo /EHsc /O2 diagnose_icons.cpp user32.lib /Fe:diagnose_icons.exe

if exist diagnose_icons.exe (
    echo === Running diagnostic ===
    .\diagnose_icons.exe
)