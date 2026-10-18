@echo off
setlocal
set "VSINSTALL="
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -version "[17.0,18.0)" -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
if not defined VSINSTALL exit /b 1
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /W4 /WX /I"%~1\Common\Include" /I"%~1\ZakoVDD\Edid" "%~dp0..\tests\source-contract.cpp" /Fo"%~2\source-contract.obj" /Fe"%~2\source-contract.exe" >"%~2\source-contract-build.log" 2>&1
if errorlevel 1 (type "%~2\source-contract-build.log" & exit /b 1)
"%~2\source-contract.exe" "%~2" >"%~2\contract-layout.json"
if errorlevel 1 exit /b 1
dumpbin /headers "%~1\x64\Release\ZakoVDD\ZakoVDD.dll" >"%~2\dll-headers.txt"
if errorlevel 1 exit /b 1
dumpbin /imports "%~1\x64\Release\ZakoVDD\ZakoVDD.dll" >"%~2\dll-imports.txt"
if errorlevel 1 exit /b 1
dumpbin /exports "%~1\x64\Release\ZakoVDD\ZakoVDD.dll" >"%~2\dll-exports.txt"
exit /b %ERRORLEVEL%
