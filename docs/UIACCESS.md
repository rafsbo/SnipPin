# UIAccess edition

`SnipPinUIA.exe` is the same program as `SnipPin.exe`, built with a different manifest:

```xml
<requestedExecutionLevel level="asInvoker" uiAccess="true"/>
```

## What it changes

- **Higher z-order band.** Windows created by a uiAccess process (SnipPin's snippets, whole-window mirrors, menus, overlays and stash panel) are placed in a band above ordinary topmost windows. This is the mechanism accessibility tools such as the On-Screen Keyboard and Magnifier use to stay visible above the taskbar, the Start menu and other "always on top" programs.
- **Elevated applications.** A uiAccess process may interact with windows of higher-integrity (administrator) processes in most cases, which usually removes the need for *Restart as administrator*.

What it does **not** change: a window that belongs to another application keeps that application's z-order band. Pinning it with SnipPin uses the normal topmost band, as in the standard edition. For the strongest "on top" behaviour, pin a **snippet** or a **whole-window mirror** of it instead.

## Why Windows needs extra steps

Windows refuses to start a uiAccess program unless **both** of these are true:

1. The executable is **Authenticode-signed** with a certificate that chains to a root trusted by the machine.
2. It runs from a **secure location**: `%ProgramFiles%`, `%ProgramFiles(x86)%` or `%WinDir%\System32` (folders standard users cannot write to).

Otherwise starting it fails with *"A referral was returned from the server"* (error `0x8007202B`).

## Installing on Windows 10 / 11

1. Extract the release zip.
2. Open **PowerShell as administrator** in the extracted folder.
3. If the release is signed:

```bash
powershell -ExecutionPolicy Bypass -File .\install-uiaccess.ps1
```

   If it is not signed (e.g. you built it yourself), let the script create a local certificate:

```bash
powershell -ExecutionPolicy Bypass -File .\install-uiaccess.ps1 -SelfSign
```

The script copies the files to `C:\Program Files\SnipPin`, signs `SnipPinUIA.exe` if needed, and adds a *SnipPin (UIAccess)* Start menu shortcut.

With `-SelfSign`, the certificate is trusted **on this computer only**, and its private key is deleted immediately after signing, so it cannot be used to sign anything else.

To uninstall (removes the files, the shortcut and the local certificate):

```bash
powershell -ExecutionPolicy Bypass -File .\install-uiaccess.ps1 -Uninstall
```

## Installing on Windows 7 / 8

PowerShell on these systems cannot create code-signing certificates. Either use a **signed release** (then copy `SnipPinUIA.exe` to `C:\Program Files\SnipPin` yourself), or sign it with the Windows SDK tools from an elevated command prompt:

```bash
makecert -r -pe -n "CN=SnipPin Local UIAccess Signing" -ss My -sr CurrentUser -a sha256 -cy end -sky signature -eku 1.3.6.1.5.5.7.3.3 SnipPinLocal.cer
```

```bash
certutil -addstore Root SnipPinLocal.cer
```

```bash
signtool sign /fd SHA256 /n "SnipPin Local UIAccess Signing" "C:\Program Files\SnipPin\SnipPinUIA.exe"
```

Afterwards, delete the certificate from *Personal* in `certmgr.msc` so its private key cannot be reused.

## Signing official releases (maintainers)

The GitHub Actions workflow signs both executables when these repository secrets are set:

| Secret | Value |
|---|---|
| `SIGNING_CERT_BASE64` | Base64-encoded `.pfx` code-signing certificate |
| `SIGNING_CERT_PASSWORD` | Password of the `.pfx` |

Free signing for open-source projects is offered by programs such as the [SignPath Foundation](https://signpath.org/). With a publicly trusted certificate, users only need to copy `SnipPinUIA.exe` into `Program Files`.

## Running both editions

Only one SnipPin instance runs per user session. Exit `SnipPin.exe` before starting `SnipPinUIA.exe`. The two editions share the same settings and history.
