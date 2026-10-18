@echo off
setlocal
if not defined VDD_VSINSTALL exit /b 1
call "%VDD_VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul
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
