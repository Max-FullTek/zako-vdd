# Fail closed before executing a downloaded tool. Hash pins content; Authenticode
# independently checks the publisher and the machine's existing certificate trust.
function Assert-TrustedBuildTool {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][ValidatePattern('^[0-9A-Fa-f]{64}$')][string]$ExpectedSha256
    )
    $hash = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
    if ($hash -ine $ExpectedSha256) { throw 'Build tool SHA256 mismatch; execution refused' }
    $signature = Get-AuthenticodeSignature -LiteralPath $Path
    if ($signature.Status -ne 'Valid' -or $null -eq $signature.SignerCertificate -or
        $signature.SignerCertificate.Subject -notmatch '(^|,\s*)CN=Microsoft Corporation(,|$)' -or
        $signature.SignerCertificate.Subject -notmatch '(^|,\s*)O=Microsoft Corporation(,|$)') {
        throw 'Build tool Microsoft signature/trust validation failed; execution refused'
    }
    [ordered]@{
        sha256 = $hash
        signature_status = [string]$signature.Status
        signer_subject = $signature.SignerCertificate.Subject
        signer_thumbprint = $signature.SignerCertificate.Thumbprint
    }
}
