#!/usr/bin/env bash
set -euo pipefail

DRIVE_INPUT="${1:-${DRIVE:-${SD_DRIVE:-E}}}"
DRIVE_LETTER="$(echo "${DRIVE_INPUT}" | tr -d ':' | tr 'a-z' 'A-Z')"
DRIVE_LETTER_LOWER="$(echo "${DRIVE_LETTER}" | tr 'A-Z' 'a-z')"

mnt_drive="/mnt/${DRIVE_LETTER_LOWER}"
win_drive="${DRIVE_LETTER}:"

repository_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
source_zip="${repository_root}/release/sdcard_release.zip"

if [[ ! -f "${source_zip}" ]]; then
    echo "Release zip file not found: ${source_zip}" >&2
    echo "Run ./build_release.sh first to generate the release package." >&2
    exit 1
fi

echo "Release package details:"
stat --printf='  Size: %s bytes\n  Modified: %y\n' "${source_zip}"
echo "Target drive: ${win_drive} (WSL path: ${mnt_drive})"

if [[ -d "${mnt_drive}" ]]; then
    echo "Extracting release package to ${mnt_drive}..."
    if command -v unzip >/dev/null 2>&1; then
        unzip -q -o "${source_zip}" -d "${mnt_drive}"
    else
        python3 -m zipfile -e "${source_zip}" "${mnt_drive}"
    fi
    echo "Extracted release package to ${mnt_drive}"
    exit 0
fi

if ! command -v cmd.exe >/dev/null 2>&1; then
    echo "Windows drive ${win_drive} is not mounted at ${mnt_drive} and cmd.exe is unavailable" >&2
    exit 1
fi

windows_source="$(wslpath -w "${source_zip}")"
if [[ ! -d /mnt/c ]]; then
    echo "Windows drive C: is not mounted at /mnt/c" >&2
    exit 1
fi

if ! (cd /mnt/c && cmd.exe /C "if exist ${win_drive}\\NUL (exit /b 0) else (exit /b 1)"); then
    echo "Windows drive ${win_drive} is unavailable" >&2
    exit 1
fi

echo "Extracting release package to Windows drive ${win_drive}..."
(cd /mnt/c && powershell.exe -NoProfile -Command "Expand-Archive -LiteralPath '${windows_source}' -DestinationPath '${win_drive}\\' -Force")
echo "Extracted release package to ${win_drive}\\"

if ! (cd /mnt/c && powershell.exe -NoProfile -Command '$shell = New-Object -ComObject Shell.Application; $drive = $shell.NameSpace(17).ParseName("'"${win_drive}"'"); if ($null -eq $drive) { exit 1 }; Start-Sleep -Milliseconds 7000; $eject = $drive.Verbs() | Where-Object { ($_.Name -replace "[&.]", "") -match "Eject|Ejetar" } | Select-Object -First 1; if ($null -eq $eject) { exit 1 }; $eject.DoIt(); Start-Sleep -Milliseconds 1000; if (Test-Path "'"${win_drive}"'\\") { exit 1 }'); then
    echo "Extracted the release package, but could not eject Windows drive ${win_drive}" >&2
    exit 1
fi

echo "Ejected Windows drive ${win_drive}"
echo "Done!!!"
