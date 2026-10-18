# Focused offline regression checks against a real successful build artifact.
param([Parameter(Mandatory)][string]$ArtifactDirectory)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$prepare = Join-Path $PSScriptRoot '../scripts/prepare-vdd-test-materials.ps1'
$scratch = Join-Path ([IO.Path]::GetTempPath()) ('vdd-material-check-' + [guid]::NewGuid().ToString('N'))
$candidate = Join-Path $scratch 'materials'
$build = Join-Path $ArtifactDirectory 'source/x64/Release/ZakoVDD'
$evidence = Join-Path $ArtifactDirectory 'evidence'
$before = (Get-FileHash -LiteralPath (Join-Path $build 'ZakoVDD.inf')).Hash
& $prepare -BuildDirectory $build -EvidenceDirectory $evidence -LicensePath (Join-Path $ArtifactDirectory 'source/LICENSE') -OutputDirectory $candidate
& $prepare -VerifyOnly -OutputDirectory $candidate
if ((Get-FileHash -LiteralPath (Join-Path $build 'ZakoVDD.inf')).Hash -cne $before) { throw 'Input INF was modified' }
$original = Get-Content -LiteralPath (Join-Path $build 'ZakoVDD.inf') -Raw
$prepared = Get-Content -LiteralPath (Join-Path $candidate 'ZakoVDD.inf') -Raw
if ([regex]::Replace($original, '(?m)^DriverVer\s*=[^\r\n]*', 'DriverVer=09/07/2026,100.0.17.5') -cne $prepared) {
    throw 'More than DriverVer changed'
}
function Expect-Rejection {
    param([scriptblock]$Action, [string]$Expected)
    try { & $Action } catch {
        if ($_.Exception.Message -notlike "*$Expected*") { throw }
        Write-Host "Correctly rejected: $Expected"
        return
    }
    throw "Expected rejection: $Expected"
}
Expect-Rejection { & $prepare -BuildDirectory $build -EvidenceDirectory $evidence -LicensePath (Join-Path $ArtifactDirectory 'source/LICENSE') -OutputDirectory $candidate } 'refusing to overwrite'
Add-Content -LiteralPath (Join-Path $candidate 'vdd_settings.xml') -Value '<!-- changed -->'
Expect-Rejection { & $prepare -VerifyOnly -OutputDirectory $candidate } 'hash/size mismatch'
Copy-Item -LiteralPath (Join-Path $build 'vdd_settings.xml') -Destination (Join-Path $candidate 'vdd_settings.xml')
Set-Content -LiteralPath (Join-Path $candidate 'unexpected.cat') -Value 'not a valid catalog'
Expect-Rejection { & $prepare -VerifyOnly -OutputDirectory $candidate } 'Unexpected extra files'
$manifestPath = Join-Path $candidate 'manifest.json'
$claims = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$claims.install_ready = $true
$claims | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath
Expect-Rejection { & $prepare -VerifyOnly -OutputDirectory $candidate } 'readiness claim'
$wrongEvidence = Join-Path $scratch 'wrong-evidence'
New-Item -ItemType Directory -Path $wrongEvidence | Out-Null
$wrongSource = Get-Content -LiteralPath (Join-Path $evidence 'source.json') -Raw | ConvertFrom-Json
$wrongSource.source_commit = '22b15167bec5b9ec9ae76e75cbc697704965ab08'
$wrongSource | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $wrongEvidence 'source.json')
Expect-Rejection {
    & $prepare -BuildDirectory $build -EvidenceDirectory $wrongEvidence -LicensePath (Join-Path $ArtifactDirectory 'source/LICENSE') -OutputDirectory (Join-Path $scratch 'wrong-source')
} 'Only the pinned v0.17.5'
# Leave only these explicitly named test files in the OS temp directory; no cleanup.
Write-Host "Offline preparation regression checks passed. Scratch: $scratch"
