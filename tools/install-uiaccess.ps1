<#
.SYNOPSIS
    Installs SnipPinUIA.exe into a secure folder so Windows honours its
    uiAccess manifest (z-order band above other topmost windows).

.DESCRIPTION
    Windows only starts a uiAccess application when it is
      1. digitally signed with a certificate the machine trusts, and
      2. located in a secure folder such as %ProgramFiles%.

    Official release builds may already be signed. For unsigned builds (for
    example ones you compiled yourself) pass -SelfSign: the script creates a
    local code-signing certificate, trusts it on THIS machine only, signs the
    executable and then deletes the certificate's private key so it cannot be
    used to sign anything else.

    Run from an elevated PowerShell prompt in the folder that contains
    SnipPinUIA.exe. Requires Windows 10/11 (PowerShell 5+) for -SelfSign; on
    Windows 7/8 see docs/UIACCESS.md for the manual signtool procedure.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File .\install-uiaccess.ps1 -SelfSign

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File .\install-uiaccess.ps1 -Uninstall
#>
[CmdletBinding()]
param(
    [string]$Source = $PSScriptRoot,
    [string]$Destination = (Join-Path $env:ProgramFiles 'SnipPin'),
    [switch]$SelfSign,
    [switch]$Uninstall
)

$ErrorActionPreference = 'Stop'
$certSubject = 'CN=SnipPin Local UIAccess Signing'
$shortcut = Join-Path ([Environment]::GetFolderPath('CommonPrograms')) 'SnipPin (UIAccess).lnk'

$principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Please run this script from an elevated (Run as administrator) PowerShell prompt.'
}

if ($Uninstall) {
    Get-Process SnipPinUIA -ErrorAction SilentlyContinue | Stop-Process -Force
    if (Test-Path $shortcut) { Remove-Item $shortcut -Force }
    if (Test-Path $Destination) { Remove-Item $Destination -Recurse -Force }
    foreach ($store in 'Cert:\LocalMachine\Root', 'Cert:\LocalMachine\TrustedPublisher') {
        Get-ChildItem $store | Where-Object Subject -eq $certSubject | Remove-Item -Force
    }
    Write-Host 'SnipPin (UIAccess) was removed.'
    return
}

$exe = Join-Path $Source 'SnipPinUIA.exe'
if (-not (Test-Path $exe)) { throw "SnipPinUIA.exe not found in '$Source'." }

New-Item -ItemType Directory -Force -Path $Destination | Out-Null
Get-Process SnipPinUIA -ErrorAction SilentlyContinue | Stop-Process -Force
Copy-Item $exe -Destination $Destination -Force
foreach ($extra in 'SnipPin.exe', 'README.md', 'LICENSE', 'UIACCESS.md') {
    $path = Join-Path $Source $extra
    if (Test-Path $path) { Copy-Item $path -Destination $Destination -Force }
}
$installed = Join-Path $Destination 'SnipPinUIA.exe'

$signature = Get-AuthenticodeSignature $installed
if ($signature.Status -ne 'Valid') {
    if (-not $SelfSign) {
        throw "SnipPinUIA.exe is not signed by a trusted certificate (status: $($signature.Status)). Re-run with -SelfSign or use a signed release."
    }
    if ($PSVersionTable.PSVersion.Major -lt 5) {
        throw '-SelfSign needs PowerShell 5 (Windows 10+). See docs/UIACCESS.md for Windows 7/8.'
    }
    Write-Host 'Creating a local code-signing certificate...'
    $cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject $certSubject `
        -CertStoreLocation 'Cert:\CurrentUser\My' -KeyUsage DigitalSignature -NotAfter (Get-Date).AddYears(10)
    $publicCer = Join-Path $env:TEMP 'snippin-uiaccess.cer'
    Export-Certificate -Cert $cert -FilePath $publicCer | Out-Null
    Import-Certificate -FilePath $publicCer -CertStoreLocation 'Cert:\LocalMachine\Root' | Out-Null
    Import-Certificate -FilePath $publicCer -CertStoreLocation 'Cert:\LocalMachine\TrustedPublisher' | Out-Null
    Remove-Item $publicCer -Force

    $result = Set-AuthenticodeSignature -FilePath $installed -Certificate $cert -HashAlgorithm SHA256
    # The private key is no longer needed: delete it so it cannot sign anything else.
    Remove-Item "Cert:\CurrentUser\My\$($cert.Thumbprint)" -DeleteKey -Force
    if ($result.Status -ne 'Valid') { throw "Signing failed: $($result.StatusMessage)" }
    Write-Host 'Signed SnipPinUIA.exe with a certificate trusted on this machine only.'
}

$shell = New-Object -ComObject WScript.Shell
$link = $shell.CreateShortcut($shortcut)
$link.TargetPath = $installed
$link.WorkingDirectory = $Destination
$link.Description = 'SnipPin with UIAccess (snippets above other topmost windows)'
$link.Save()

Write-Host "Installed to $Destination"
Write-Host "Start menu shortcut: $shortcut"
Write-Host 'Tip: close the standard SnipPin.exe before starting SnipPinUIA.exe (only one instance runs at a time).'
