#!/usr/bin/env bash
set -euo pipefail

repository_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
source_zip="${repository_root}/release/sdcard_release.zip"

if [[ ! -f "${source_zip}" ]]; then
    echo "Release zip file not found: ${source_zip}" >&2
    echo "Run ./build_release.sh first to generate the release package." >&2
    exit 1
fi

echo "Release package details:"
stat --printf='  Size: %s bytes\n  Modified: %y\n' "${source_zip}"

if [[ -d /mnt/e ]]; then
    echo "Extracting release package to /mnt/e..."
    if command -v unzip >/dev/null 2>&1; then
        unzip -q -o "${source_zip}" -d /mnt/e
    else
        python3 -m zipfile -e "${source_zip}" /mnt/e
    fi
    echo "Extracted release package to /mnt/e"
    exit 0
fi

if ! command -v cmd.exe >/dev/null 2>&1; then
    echo "Windows drive E: is not mounted at /mnt/e and cmd.exe is unavailable" >&2
    exit 1
fi

windows_source="$(wslpath -w "${source_zip}")"
if [[ ! -d /mnt/c ]]; then
    echo "Windows drive C: is not mounted at /mnt/c" >&2
    exit 1
fi

if ! (cd /mnt/c && cmd.exe /C "if exist E:\NUL (exit /b 0) else (exit /b 1)"); then
    echo "Windows drive E: is unavailable" >&2
    exit 1
fi

echo "Extracting release package to Windows drive E:..."
(cd /mnt/c && powershell.exe -NoProfile -Command "Expand-Archive -LiteralPath '${windows_source}' -DestinationPath 'E:\' -Force")
echo "Extracted release package to E:\\"

if ! (cd /mnt/c && powershell.exe -NoProfile -Command '$shell = New-Object -ComObject Shell.Application; $drive = $shell.NameSpace(17).ParseName("E:"); if ($null -eq $drive) { exit 1 }; $eject = $drive.Verbs() | Where-Object { ($_.Name -replace "[&.]", "") -match "Eject|Ejetar" } | Select-Object -First 1; if ($null -eq $eject) { exit 1 }; $eject.DoIt(); Start-Sleep -Milliseconds 2000; if (Test-Path "E:\") { exit 1 }'); then
    echo "Extracted the release package, but could not eject Windows drive E:" >&2
    exit 1
fi

echo "Ejected Windows drive E:"
echo "Done!!!"
