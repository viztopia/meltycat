@echo off
setlocal
pushd "%~dp0"

if /i "%VSCMD_ARG_TGT_ARCH%"=="x64" goto compile

set "MELTYCAT_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%MELTYCAT_VSWHERE%" goto missing
for /f "usebackq delims=" %%I in (`"%MELTYCAT_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "MELTYCAT_VS=%%I"
if not defined MELTYCAT_VS goto missing
call "%MELTYCAT_VS%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 goto failed

:compile
rem MSVC 14.51 and later reject the legacy coroutine header selected by C++/WinRT in C++17.
set "MELTYCAT_COROUTINES="
for /f "tokens=1,2 delims=." %%A in ("%VCToolsVersion%") do (
    if %%A GTR 14 set "MELTYCAT_COROUTINES=/await:strict"
    if %%A EQU 14 if %%B GEQ 51 set "MELTYCAT_COROUTINES=/await:strict"
)
if not exist build mkdir build
rc /nologo /fo build\meltycat.res src\meltycat.rc
if errorlevel 1 goto failed
set "MELTYCAT_CXXFLAGS=/nologo /std:c++17 %MELTYCAT_COROUTINES% /O2 /MT /EHsc /W4 /WX /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /D_WIN32_WINNT=0x0A00"
cl %MELTYCAT_CXXFLAGS% ^
   /Fo:build\ /Fe:build\meltycat.exe src\main.cpp src\cat.cpp src\animation.cpp src\tray_menu.cpp ^
   src\skin_spec.cpp src\skin_library.cpp src\skin_window.cpp src\poof.cpp build\meltycat.res ^
   /link /MACHINE:X64 /SUBSYSTEM:WINDOWS /MANIFEST:EMBED /MANIFESTINPUT:src\meltycat.manifest ^
   user32.lib gdi32.lib gdiplus.lib dwmapi.lib shell32.lib shcore.lib ole32.lib comdlg32.lib advapi32.lib windowsapp.lib
if errorlevel 1 goto failed
echo Built "%CD%\build\meltycat.exe"
if /i "%~1"=="--test" (
    cl %MELTYCAT_CXXFLAGS% /Fo:build\ /Fe:build\windows-tripod-paws.exe tests\windows-tripod-paws.cpp ^
       build\cat.obj build\skin_spec.obj build\skin_library.obj build\meltycat.res ^
       /link /MACHINE:X64 /SUBSYSTEM:CONSOLE gdiplus.lib shell32.lib ole32.lib advapi32.lib windowsapp.lib
    if errorlevel 1 goto failed
    build\windows-tripod-paws.exe
    if errorlevel 1 goto failed
)
popd
exit /b 0

:missing
echo MSVC C++ Build Tools and a Windows SDK are required to rebuild meltycat. 1>&2
:failed
popd
exit /b 1
