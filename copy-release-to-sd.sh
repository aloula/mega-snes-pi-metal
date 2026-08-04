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
source_zip="${repository_root}/release/sdcard_release.zip"

if [[ ! -f "${source_zip}" ]]; then
    echo "Release zip file not found: ${source_zip}" >&2
    echo "Run ./build_release.sh first to generate the release package." >&2
    exit 1
fi

echo "Release package details:"
stat --printf='  Size: %s bytes\n  Modified: %y\n' "${source_zip}"
echo "Target drive: ${win_drive} (WSL path: ${mnt_drive})"

is_mounted_in_linux() {
    local path="$1"
    if [[ ! -d "${path}" ]]; then
        return 1
    fi
    if command -v mountpoint >/dev/null 2>&1; then
        if mountpoint -q "${path}"; then
            local target
            target="$(findmnt -n -o TARGET --target "${path}" 2>/dev/null || true)"
            if [[ "${target}" == "${path}" || -z "${target}" ]]; then
                return 0
            fi
        fi
    elif command -v findmnt >/dev/null 2>&1; then
        local target
        target="$(findmnt -n -o TARGET --target "${path}" 2>/dev/null || true)"
        if [[ "${target}" == "${path}" ]]; then
            return 0
        fi
    else
        if grep -qE "[[:space:]]${path}[[:space:]]" /proc/mounts 2>/dev/null; then
            return 0
        fi
    fi
    return 1
}

is_available_in_windows() {
    local win_drive="$1"
    if command -v powershell.exe >/dev/null 2>&1; then
        (cd /mnt/c 2>/dev/null && powershell.exe -NoProfile -Command "if (Test-Path '${win_drive}\\') { exit 0 } else { exit 1 }") 2>/dev/null
        return $?
    elif command -v cmd.exe >/dev/null 2>&1; then
        (cd /mnt/c 2>/dev/null && cmd.exe /C "if exist ${win_drive}\\ (exit /b 0) else (exit /b 1)") 2>/dev/null
        return $?
    fi
    return 1
}

eject_sd_drive() {
    sync 2>/dev/null || true
    if command -v powershell.exe >/dev/null 2>&1; then
        echo "Flushing file buffers and ejecting Windows drive ${win_drive}..."
        if (cd /mnt/c 2>/dev/null && powershell.exe -NoProfile -Command '$shell = New-Object -ComObject Shell.Application; $drive = $shell.NameSpace(17).ParseName("'"${win_drive}"'"); if ($null -eq $drive) { exit 1 }; Start-Sleep -Milliseconds 1000; $eject = $drive.Verbs() | Where-Object { ($_.Name -replace "[&.]", "") -match "Eject|Ejetar" } | Select-Object -First 1; if ($null -eq $eject) { exit 1 }; $eject.DoIt(); Start-Sleep -Milliseconds 1000; if (Test-Path "'"${win_drive}"'\\") { exit 1 }') 2>/dev/null; then
            echo "Ejected Windows drive ${win_drive}"
        else
            echo "Drive ${win_drive} was not automatically ejected (you may safely remove it after unmounting)."
        fi
    fi
}

if is_mounted_in_linux "${mnt_drive}"; then
    echo "Extracting release package to ${mnt_drive}..."
    if command -v unzip >/dev/null 2>&1; then
        unzip -q -o "${source_zip}" -d "${mnt_drive}"
    else
        python3 -m zipfile -e "${source_zip}" "${mnt_drive}"
    fi
    echo "Extracted release package to ${mnt_drive}"
    eject_sd_drive
    echo "Done!!!"
    exit 0
fi

if is_available_in_windows "${win_drive}"; then
    windows_source="$(wslpath -w "${source_zip}")"
    echo "Extracting release package to Windows drive ${win_drive}..."
    (cd /mnt/c && powershell.exe -NoProfile -Command "Expand-Archive -LiteralPath '${windows_source}' -DestinationPath '${win_drive}\\' -Force")
    echo "Extracted release package to ${win_drive}\\"
    eject_sd_drive
    echo "Done!!!"
    exit 0
fi

echo "Error: Target SD card drive ${win_drive} (WSL path: ${mnt_drive}) is unavailable or no SD card is attached." >&2
exit 1
