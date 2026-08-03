#!/usr/bin/env bash
set -euo pipefail

DRIVE_INPUT="${1:-${DRIVE:-${SD_DRIVE:-E}}}"
DRIVE_LETTER="$(echo "${DRIVE_INPUT}" | tr -d ':' | tr 'a-z' 'A-Z')"
DRIVE_LETTER_LOWER="$(echo "${DRIVE_LETTER}" | tr 'A-Z' 'a-z')"

if [[ "${DRIVE_LETTER}" == "C" ]]; then
    echo "Error: Drive C: is the system OS drive and cannot be selected as the target SD card drive." >&2
    exit 1
fi

mnt_drive="/mnt/${DRIVE_LETTER_LOWER}"
win_drive="${DRIVE_LETTER}:"

repository_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
source_image="${repository_root}/main-emulator/kernel8-32.img"
destination="${mnt_drive}/kernel8-32.img"

if [[ ! -f "${source_image}" ]]; then
    echo "Kernel image not found: ${source_image}" >&2
    exit 1
fi

echo "Kernel image details:"
stat --printf='  Size: %s bytes\n  Modified: %y\n' "${source_image}"
echo "Target drive: ${win_drive} (WSL path: ${mnt_drive})"

if [[ -d "${mnt_drive}" ]]; then
    cp -- "${source_image}" "${destination}"
    echo "Copied kernel image to ${destination}"
    exit 0
fi

if ! command -v cmd.exe >/dev/null 2>&1; then
    echo "Windows drive ${win_drive} is not mounted at ${mnt_drive} and cmd.exe is unavailable" >&2
    exit 1
fi

windows_source="$(wslpath -w "${source_image}")"
if [[ ! -d /mnt/c ]]; then
    echo "Windows drive C: is not mounted at /mnt/c" >&2
    exit 1
fi

if ! (cd /mnt/c && cmd.exe /C "if exist ${win_drive}\\NUL (exit /b 0) else (exit /b 1)"); then
    echo "Windows drive ${win_drive} is unavailable" >&2
    exit 1
fi

(cd /mnt/c && cmd.exe /C copy /Y "${windows_source}" "${win_drive}\\kernel8-32.img")
echo "Copied kernel image to ${win_drive}\\kernel8-32.img"

if ! (cd /mnt/c && powershell.exe -NoProfile -Command '$shell = New-Object -ComObject Shell.Application; $drive = $shell.NameSpace(17).ParseName("'"${win_drive}"'"); if ($null -eq $drive) { exit 1 }; $eject = $drive.Verbs() | Where-Object { ($_.Name -replace "[&.]", "") -match "Eject|Ejetar" } | Select-Object -First 1; if ($null -eq $eject) { exit 1 }; $eject.DoIt(); Start-Sleep -Milliseconds 500; if (Test-Path "'"${win_drive}"'\\") { exit 1 }'); then
    echo "Copied the kernel image, but could not eject Windows drive ${win_drive}" >&2
    exit 1
fi

echo "Ejected Windows drive ${win_drive}"
echo "Done!!!"
