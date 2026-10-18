# Mocked trust failures only; does not download, execute or trust any binary.
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/../scripts/assert-build-tool.ps1"
$script:hash = 'A' * 64
$script:status = 'Valid'
$script:subject = 'CN=Microsoft Corporation, O=Microsoft Corporation, C=US'
$script:hasCertificate = $true
function Get-FileHash { param($LiteralPath, $Algorithm) [pscustomobject]@{ Hash = $script:hash } }
function Get-AuthenticodeSignature {
    param($LiteralPath)
    $cert = if ($script:hasCertificate) { [pscustomobject]@{ Subject = $script:subject; Thumbprint = 'test' } } else { $null }
    [pscustomobject]@{ Status = $script:status; SignerCertificate = $cert }
}
function Expect-Rejection {
    param([string]$Expected)
    try { Assert-TrustedBuildTool -Path 'mock-only.exe' -ExpectedSha256 ('A' * 64) | Out-Null }
    catch {
        if ($_.Exception.Message -notlike "*$Expected*") { throw }
        return
    }
    throw "Expected rejection: $Expected"
}
Assert-TrustedBuildTool -Path 'mock-only.exe' -ExpectedSha256 ('A' * 64) | Out-Null
$script:hash = 'B' * 64
Expect-Rejection 'SHA256 mismatch'
$script:hash = 'A' * 64
foreach ($script:status in @('NotSigned', 'HashMismatch', 'NotTrusted', 'UnknownError')) {
    Expect-Rejection 'signature/trust validation failed'
}
$script:status = 'Valid'
$script:subject = 'CN=Microsoft Corporation Fake, O=Microsoft Corporation, C=US'
Expect-Rejection 'signature/trust validation failed'
$script:subject = 'CN=Microsoft Corporation, O=Other, C=US'
Expect-Rejection 'signature/trust validation failed'
$script:hasCertificate = $false
Expect-Rejection 'signature/trust validation failed'
Write-Host 'Build tool hash/publisher/trust rejection checks passed.'
