# Runs only on the disposable hosted Windows runner; never installs a driver.
$ErrorActionPreference = 'Stop'
$evidence = Join-Path $env:GITHUB_WORKSPACE 'evidence'
$sourceProject = Join-Path $env:GITHUB_WORKSPACE 'source/ZakoVDD/ZakoVDD.vcxproj'
$taskPattern = 'C:\Program Files*\Windows Kits\10\build\*\bin\Microsoft.DriverKit.Build.Tasks.17.0.dll'
# The installed WDK imports use Include\wdf\umdf, outside versioned SDK folders.
$headerPattern = 'C:\Program Files*\Windows Kits\10\Include\wdf\umdf\*\wdf.h'

function Test-WdkIntegration {
    param([string]$Label)
    $headers = @(Get-ChildItem $headerPattern -File -ErrorAction SilentlyContinue)
    $tasks = @(Get-ChildItem $taskPattern -ErrorAction SilentlyContinue)
    $headers.FullName | Set-Content "$evidence/headers-$Label.txt"
    $tasks | Select-Object FullName, @{n='FileVersion';e={$_.VersionInfo.FileVersion}} |
        ConvertTo-Json -AsArray | Set-Content "$evidence/tasks-$Label.json"
    # Preprocessing exercises the actual project's toolset/import integration,
    # without running compiler, package or signing targets.
    & msbuild $sourceProject /p:Configuration=Release /p:Platform=x64 /p:SignMode=Off "/pp:$evidence/project-$Label.xml" *> "$evidence/integration-$Label.log"
    $code = $LASTEXITCODE
    Write-Host "WDK integration ($Label): headers=$($headers.Count), tasks=$($tasks.Count), project preprocess exit=$code"
    return ($headers.Count -gt 0 -and $tasks.Count -gt 0 -and $code -eq 0)
}

Start-Transcript -Path "$evidence/wdk-prepare.log"
try {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { throw 'vswhere missing' }
    & $vswhere -products '*' -requires Microsoft.Component.MSBuild -version '[17.0,18.0)' -format json |
        Set-Content "$evidence/visual-studio.json"
    $vs = & $vswhere -products '*' -requires Microsoft.Component.MSBuild -version '[17.0,18.0)' -property installationPath -latest
    if (-not $vs) { throw 'VS2022 MSBuild installation missing' }
    $msbuild = (Get-Command msbuild -CommandType Application -ErrorAction Stop).Source
    $expectedMsbuild = Join-Path $vs 'MSBuild/Current/Bin/amd64/MSBuild.exe'
    if ($msbuild -ne $expectedMsbuild) { throw "Expected x64 MSBuild at $expectedMsbuild; resolved $msbuild" }
    [ordered]@{
        path = $msbuild
        architecture = 'x64'
        file_version = (Get-Item -LiteralPath $msbuild).VersionInfo.FileVersion
        sha256 = (Get-FileHash -LiteralPath $msbuild -Algorithm SHA256).Hash
    } | ConvertTo-Json | Set-Content "$evidence/msbuild-host.json"
    Write-Host "Using x64 MSBuild: $msbuild"
    Get-ChildItem 'C:\Program Files*\Windows Kits\10\build\*\bin\*\InfVerif.dll', 'C:\Program Files*\Windows Kits\10\build\bin\*\InfVerif.dll' -File -ErrorAction SilentlyContinue |
        Select-Object FullName, @{n='FileVersion';e={$_.VersionInfo.FileVersion}} |
        ConvertTo-Json -AsArray | Set-Content "$evidence/infverif-libraries.json"
    & msbuild -version | Set-Content "$evidence/msbuild.txt"

    if (-not (Test-WdkIntegration 'initial')) {
        $headers = @(Get-ChildItem $headerPattern -File -ErrorAction SilentlyContinue)
        $tasks = @(Get-ChildItem $taskPattern -ErrorAction SilentlyContinue)
        if (-not ($headers.Count -gt 0 -and $tasks.Count -gt 0)) {
            $installer = Join-Path $env:RUNNER_TEMP 'zako-wdksetup.exe'
            Invoke-WebRequest 'https://go.microsoft.com/fwlink/?linkid=2272234' -OutFile $installer
            Get-FileHash $installer -Algorithm SHA256 | Format-List | Out-File "$evidence/wdk-installer.txt"
            $process = Start-Process $installer -ArgumentList '/quiet', '/norestart', '/features', '+' -Wait -PassThru -WindowStyle Hidden
            $process.ExitCode | Set-Content "$evidence/wdk-setup-exit.txt"
            # Check actual integration again even when setup reports a nonzero code.
        }

        if (-not (Test-WdkIntegration 'after-setup')) {
            $vsixInstaller = Join-Path $vs 'Common7/IDE/VSIXInstaller.exe'
            if (-not (Test-Path $vsixInstaller)) { throw 'VSIXInstaller missing' }
            $vsix = Get-ChildItem 'C:\Program Files*\Windows Kits\10\Vsix' -Filter WDK.vsix -Recurse -File -ErrorAction SilentlyContinue |
                Where-Object FullName -Match '\\VS2022\\' |
                Sort-Object FullName -Descending | Select-Object -First 1
            if (-not $vsix) { throw 'WDK.vsix missing after recursive VS2022 search' }
            $vsix.FullName | Set-Content "$evidence/wdk-vsix.txt"
            $vsixLog = Join-Path $evidence 'wdk-vsix-install.log'
            $process = Start-Process $vsixInstaller -ArgumentList '/q', ('/logFile:"' + $vsixLog + '"'), ('"' + $vsix.FullName + '"') -Wait -PassThru -WindowStyle Hidden
            $process.ExitCode | Set-Content "$evidence/wdk-vsix-exit.txt"
            Write-Host "VSIX exit code: $($process.ExitCode); checking actual integration"
            if (-not (Test-WdkIntegration 'after-vsix')) { throw 'WDK integration unavailable; inspect setup, VSIX and preprocessing logs' }
        }
    }
    Get-ChildItem 'C:\Program Files*\Windows Kits\10\Include' -Directory -ErrorAction SilentlyContinue |
        Select-Object FullName | ConvertTo-Json -AsArray | Set-Content "$evidence/windows-kit-versions.json"
} finally {
    # Preserve installer diagnostics on both success and failure.
    Get-ChildItem $env:TEMP -File -ErrorAction SilentlyContinue |
        Where-Object Name -Match '^(dd_.*|.*WDK.*)\.(log|txt)$' |
        ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $evidence }
    Stop-Transcript
}
