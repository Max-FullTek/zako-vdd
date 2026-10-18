<#
.SYNOPSIS
Prepares or verifies minimal offline baseline/fork signing materials. Never installs or signs.
.DESCRIPTION
The output deliberately has no catalog or certificate. Its INF uses a fixed test
date/version; exact build identity lives in manifest.json. This is not an automatic
release or upgrade policy. Resolve OS-specific packaging,
signing, trust, backup and device switching before using it on a test machine.
#>
[CmdletBinding(DefaultParameterSetName = 'Prepare')]
param(
    [Parameter(Mandatory, ParameterSetName = 'Prepare')][string]$BuildDirectory,
    [Parameter(Mandatory, ParameterSetName = 'Prepare')][string]$LicensePath,
    [Parameter(Mandatory, ParameterSetName = 'Prepare')][string]$EvidenceDirectory,
    [Parameter(Mandatory)][string]$OutputDirectory,
    [Parameter(Mandatory, ParameterSetName = 'Verify')][switch]$VerifyOnly
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)

function Get-MaterialIdentity {
    param($Source)
    if ($Source.tag -eq 'v0.17.5' -and
        $Source.source_commit -eq '88d117431549bcba22fea27ded3d4804c79b51fb' -and
        $Source.repository -eq 'https://github.com/qiin2333/zako-vdd') {
        return @{ date = '09/07/2026'; version = '100.0.17.5' }
    }
    if ($Source.tag -eq 'v0.18.0-hardened' -and
        $Source.repository -eq 'https://github.com/Max-FullTek/zako-vdd' -and
        $Source.source_commit -match '^[0-9a-f]{40}$' -and
        $Source.source_commit -eq $Source.workflow_commit -and
        $Source.upstream_base_commit -eq '22b15167bec5b9ec9ae76e75cbc697704965ab08') {
        return @{ date = '10/19/2026'; version = '100.0.18.0' }
    }
    throw 'Only the pinned baseline or reviewed v0.18.0 fork build is accepted'
}

function Assert-Materials {
    param([string]$Directory)
    $manifest = Get-Content -LiteralPath (Join-Path $Directory 'manifest.json') -Raw | ConvertFrom-Json
    if ($manifest.schema -ne 1 -or $manifest.state -ne 'unsigned-materials-only' -or
        $manifest.install_ready -ne $false -or $manifest.signing_complete -ne $false) {
        throw 'Unexpected material identity or readiness claim'
    }
    $source = Get-Content -LiteralPath (Join-Path $Directory 'provenance/source.json') -Raw | ConvertFrom-Json
    $identity = Get-MaterialIdentity $source
    if ($manifest.source_tag -cne $source.tag -or $manifest.source_commit -cne $source.source_commit -or
        $manifest.build_workflow_commit -cne $source.workflow_commit -or
        $manifest.driver_version -cne $identity.version -or $manifest.driver_date -cne $identity.date -or
        $source.configuration -ne 'Release' -or $source.platform -ne 'x64') {
        throw 'Unexpected material identity or readiness claim'
    }
    $expected = @('LICENSE', 'ZakoVDD.dll', 'ZakoVDD.inf', 'vdd_settings.xml',
        'provenance/source.json', 'provenance/output-sha256.json', 'provenance/source-contract-sha256.json')
    if (@($manifest.files).Count -ne $expected.Count) { throw 'Unexpected manifest file count' }
    foreach ($relative in $expected) {
        $entry = @($manifest.files | Where-Object path -CEQ $relative)
        if ($entry.Count -ne 1) { throw "Missing or duplicate manifest entry: $relative" }
        $file = Get-Item -LiteralPath (Join-Path $Directory $relative)
        if ($file.PSIsContainer -or $file.Length -ne $entry[0].bytes -or
            (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash -cne $entry[0].sha256) {
            throw "Material hash/size mismatch: $relative"
        }
    }
    $actual = @(Get-ChildItem -LiteralPath $Directory -Recurse -File)
    if ($actual.Count -ne ($expected.Count + 1)) { throw 'Unexpected extra files in unsigned materials' }
    $inf = Get-Content -LiteralPath (Join-Path $Directory 'ZakoVDD.inf') -Raw
    $directive = [regex]::Escape("DriverVer=$($identity.date),$($identity.version)")
    if (@([regex]::Matches($inf, "(?m)^$directive`r?$" )).Count -ne 1) {
        throw 'Fixed baseline INF version missing'
    }
    Write-Host 'Offline hashes and baseline identity verified; materials remain unsigned and not install-ready.'
}

if ($VerifyOnly) {
    Assert-Materials $OutputDirectory
    return
}
if (Test-Path -LiteralPath $OutputDirectory) { throw 'Output directory must not exist; refusing to overwrite' }
$source = Get-Content -LiteralPath (Join-Path $EvidenceDirectory 'source.json') -Raw | ConvertFrom-Json
$identity = Get-MaterialIdentity $source
if ($source.configuration -ne 'Release' -or $source.platform -ne 'x64' -or
    $source.workflow_commit -notmatch '^[0-9a-f]{40}$') {
    throw 'Only Release x64 builds with a recorded workflow commit are accepted'
}
$hashes = @(Get-Content -LiteralPath (Join-Path $EvidenceDirectory 'output-sha256.json') -Raw | ConvertFrom-Json)
foreach ($name in @('ZakoVDD.dll', 'ZakoVDD.inf', 'vdd_settings.xml')) {
    $record = @($hashes | Where-Object path -IEQ $name)
    $inputFile = Get-Item -LiteralPath (Join-Path $BuildDirectory $name)
    if ($record.Count -ne 1 -or $inputFile.Length -ne $record[0].bytes -or
        (Get-FileHash -LiteralPath $inputFile.FullName -Algorithm SHA256).Hash -cne $record[0].sha256) {
        throw "Build evidence mismatch: $name"
    }
}
$infText = Get-Content -LiteralPath (Join-Path $BuildDirectory 'ZakoVDD.inf') -Raw
if ([regex]::Matches($infText, '(?m)^DriverVer\s*=.*$').Count -ne 1) { throw 'Expected exactly one DriverVer' }
if ($infText -match '\$ARCH\$|\$UMDFVERSION\$' -or $infText -notmatch '\[Standard\.NTamd64\]') {
    throw 'Expected an already-built x64 INF, not the source template'
}
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
New-Item -ItemType Directory -Path (Join-Path $OutputDirectory 'provenance') | Out-Null
foreach ($name in @('ZakoVDD.dll', 'vdd_settings.xml')) {
    Copy-Item -LiteralPath (Join-Path $BuildDirectory $name) -Destination (Join-Path $OutputDirectory $name)
}
Copy-Item -LiteralPath $LicensePath -Destination (Join-Path $OutputDirectory 'LICENSE')
foreach ($name in @('source.json', 'output-sha256.json', 'source-contract-sha256.json')) {
    Copy-Item -LiteralPath (Join-Path $EvidenceDirectory $name) -Destination (Join-Path $OutputDirectory "provenance/$name")
}
# Only the copied INF is stamped. No catalog is copied: INF changes invalidate it,
# and any later embedded DLL signature must precede catalog generation/signing.
$infText = [regex]::Replace($infText, '(?m)^DriverVer\s*=[^\r\n]*', "DriverVer=$($identity.date),$($identity.version)")
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'ZakoVDD.inf'), $infText, [Text.Encoding]::ASCII)
$files = @(Get-ChildItem -LiteralPath $OutputDirectory -Recurse -File | Sort-Object FullName | ForEach-Object {
    [ordered]@{ path = [IO.Path]::GetRelativePath([IO.Path]::GetFullPath($OutputDirectory), $_.FullName).Replace('\', '/')
        bytes = $_.Length; sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
})
[ordered]@{
    schema = 1
    state = 'unsigned-materials-only'
    install_ready = $false
    signing_complete = $false
    source_tag = $source.tag
    source_commit = $source.source_commit
    build_workflow_commit = $source.workflow_commit
    preparation_workflow_commit = $env:GITHUB_SHA
    driver_date = $identity.date
    driver_version = $identity.version
    version_policy = 'Fixed test identity; exact source/workflow commits identify the build. No automatic upgrade or release.'
    package_policy = 'Minimal Windows VDD only: no Vulkan bridge, probe, layer registration or certificate bundled.'
    inf_change = 'DriverVer only; original input hash retained in provenance/output-sha256.json.'
    device_policy = 'Retains Root\ZakoVDD and shared interfaces; controlled replacement only, no independent coexistence.'
    pending = @(
        'Confirm test-machine Windows edition/build and choose the corresponding INF packaging; upstream warning 2084 remains unresolved.',
        'Choose signing identity and permitted OS trust policy. Keep private keys outside repository/artifacts. No trust or boot-policy changes are authorized.',
        'Finalize INF and any embedded DLL signature, generate a fresh catalog, sign it, and verify signer plus catalog membership for every driver file.',
        'Read and preserve installed device IDs, active OEM INF, complete exported original package, settings and existing certificate state before any switch.',
        'Approve test-machine installation separately; retain the previous package and use explicit binding/rollback with reboot recovery. Do not use automatic package cleanup.',
        'Verify actual device/streaming/HDR/cursor/reconnect/sleep behavior before release. Existing Sunshine certificate pinning requires a separately reviewed integration change.'
    )
    files = $files
} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'manifest.json') -Encoding utf8
Assert-Materials $OutputDirectory
