@echo off
setlocal
if not defined VDD_VSINSTALL exit /b 1
call "%VDD_VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if exist "%~1\ZakoVDD\Config\SettingsXml.h" (
  cl /nologo /std:c++17 /EHsc /W4 /WX /I"%~1\ZakoVDD\Config" "%~dp0..\tests\settings-safety.cpp" "%~1\ZakoVDD\Util\RefreshRate.cpp" "%~1\ZakoVDD\Control\CommandDispatcher.cpp" /Fo"%~2\\" /Fe"%~2\settings-safety.exe" /link xmllite.lib shlwapi.lib ole32.lib >"%~2\settings-safety-build.log" 2>&1
  if errorlevel 1 (type "%~2\settings-safety-build.log" & exit /b 1)
  "%~2\settings-safety.exe" >"%~2\settings-safety-test.log" 2>&1
  if errorlevel 1 (type "%~2\settings-safety-test.log" & exit /b 1)
)
if exist "%~1\ZakoVDD\Config\RegistryValue.h" (
  cl /nologo /std:c++17 /EHsc /W4 /WX /I"%~1\ZakoVDD\Config" "%~dp0..\tests\registry-value.cpp" /Fo"%~2\registry-value.obj" /Fe"%~2\registry-value.exe" >"%~2\registry-value-build.log" 2>&1
  if errorlevel 1 (type "%~2\registry-value-build.log" & exit /b 1)
  "%~2\registry-value.exe" >"%~2\registry-value-test.log" 2>&1
  if errorlevel 1 (type "%~2\registry-value-test.log" & exit /b 1)
)
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
