#!/usr/bin/env bash
set -euo pipefail

repository_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
source_image="${repository_root}/main-emulator/kernel8-32.img"
destination="/mnt/e/kernel8-32.img"

if [[ ! -f "${source_image}" ]]; then
    echo "Kernel image not found: ${source_image}" >&2
    exit 1
fi

echo "Kernel image details:"
stat --printf='  Size: %s bytes\n  Modified: %y\n' "${source_image}"

if [[ -d /mnt/e ]]; then
    cp -- "${source_image}" "${destination}"
    echo "Copied kernel image to ${destination}"
    exit 0
fi

if ! command -v cmd.exe >/dev/null; then
    echo "Windows drive E: is not mounted at /mnt/e and cmd.exe is unavailable" >&2
    exit 1
fi

windows_source="$(wslpath -w "${source_image}")"
if [[ ! -d /mnt/c ]]; then
    echo "Windows drive C: is not mounted at /mnt/c" >&2
    exit 1
fi

if ! (cd /mnt/c && cmd.exe /C "if exist E:\NUL (exit /b 0) else (exit /b 1)"); then
    echo "Windows drive E: is unavailable" >&2
    exit 1
fi

(cd /mnt/c && cmd.exe /C copy /Y "${windows_source}" "E:\kernel8-32.img")
echo "Copied kernel image to E:\\kernel8-32.img"

if ! (cd /mnt/c && powershell.exe -NoProfile -Command '$shell = New-Object -ComObject Shell.Application; $drive = $shell.NameSpace(17).ParseName("E:"); if ($null -eq $drive) { exit 1 }; $eject = $drive.Verbs() | Where-Object { ($_.Name -replace "[&.]", "") -match "Eject|Ejetar" } | Select-Object -First 1; if ($null -eq $eject) { exit 1 }; $eject.DoIt(); Start-Sleep -Milliseconds 500; if (Test-Path "E:\") { exit 1 }'); then
    echo "Copied the kernel image, but could not eject Windows drive E:" >&2
    exit 1
fi

echo "Ejected Windows drive E:"
echo "Done!!!"
