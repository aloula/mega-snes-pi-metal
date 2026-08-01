# 5-in-1 Game Console Emulator

![Baremetal Emulator](res/5-in-1_Baremetal_Emulator.png)

*Read this README in other languages: [English](README.md) | [Português do Brasil](README.pt-BR.md)*

A unified, low-latency, bare-metal multi-console emulator for the Raspberry Pi 3B+. This project merges the **SNES-PI** and **MEGA-PI** emulators into a single bare-metal kernel. It includes support for the **Super Nintendo (SNES)**, **Nintendo Entertainment System (NES)** (via Nestopia), **Sega Mega Drive / Sega CD (Genesis)** (via PicoDrive), **Sega Master System (SMS)** (via PicoDrive), and **PC Engine / PC Engine CD (TurboGrafx-16)** (via Beetle PCE Fast), allowing real-time switching between systems directly from the On-Screen Display (OSD) menu.

Built on the **Circle C++ bare-metal environment**, **Snes9x**, **PicoDrive**, **Nestopia**, and **Beetle PCE Fast**, it runs directly on the ARM CPU without an underlying operating system, ensuring maximum speed, minimal input latency, and exact hardware timing.

🎥 **Video Demonstration**: [Watch MEGA-SNES Pi Metal running on a Raspberry Pi 3B+](https://youtu.be/jyMUjcQem-0)

---

### 🚀 Key Features

* **Multi-Console Emulation**: Run SNES, NES, Sega Master System, Sega Mega Drive/Mega CD, and PC Engine/PC Engine CD games from a single boot image.
* **Low Latency**: Direct hardware access bypassing OS overhead, providing sub-millisecond input and audio response.
* **Unified OSD Menu**: Dynamic graphical user interface featuring:
  * Dynamic header banners rendered in high-legibility 12x22 font changing based on the selected system.
  * Real-time console switching via **L** and **R** shoulder buttons.
  * Selection state persistence: returning to OSD menu remembers the exact active tab and last game played.
  * 8-tab browsing per system:
    * **SNES/NES/SMS**: `ALL`, `FAV`, and 6 auto-balanced alphabetical tabs.
    * **Mega Drive**: `ALL`, `FAV`, 5 auto-balanced alphabetical tabs, and `MCD`.
    * **PC Engine**: `ALL`, `FAV`, 5 auto-balanced alphabetical tabs, and `PCD` (PC Engine CD).
  * 9 built-in color themes (`default`, `green`, `grayscale`, `cyberpunk`, `sapphire`, `synthwave`, `arctic`, `amber`, `ruby`) with optional auto-rotation on every power cycle.
  * Favorite lists (`favorites.txt`) managed directly from the UI.
* **Save States Support**: Game states can be saved/loaded in Slot 0 (stored as `.s0` files alongside the ROMs) using **SELECT + D-pad Left** (or **L Shoulder/Trigger**) to save, and **SELECT + D-pad Right** (or **R Shoulder/Trigger**) to load.
* **Rewind Feature**: Rewind up to 5 seconds of gameplay using **SELECT + D-pad Up** (or keyboard **F6**). Automatic rewind is disabled for SA-1 SNES cartridges to prevent performance and audio interruptions, and for the Mega Drive games *The Cursed Knight* and *Steel Empire* to prevent PicoDrive state-stability freezes; manual save/load states remain available.
* **High-Fidelity Audio**: Hardware-authentic audio resampling and interpolation (Gaussian audio for standard SNES games, a low-overhead profile for SA-1 games, and YM2413 FM audio for SMS).
* **Display Scaling**: Nearest-neighbor scaling for Sega games and linear/Gaussian aspect scaling for SNES games.
* **Screensaver & Audio Mute**: Automatically dims the screen by 50% and mutes audio output after controller inactivity. Pressing any controller button immediately restores full brightness and audio. Configurable via `cmdline.txt`.

---

### 📁 SD Card Configuration

To load games and BIOS files, organize your SD card root directories as follows:

```
SD:/
 ├── cmdline.txt             (Boot parameters including screensaver timeout)
 ├── system_order.txt        (Optional text file to customize console order and default boot system)
 ├── osd_theme.txt           (Optional text file to select OSD theme: default, green, grayscale, cyberpunk, sapphire, synthwave, arctic, amber, ruby, custom, all)
 ├── osd_colors.txt          (Optional text file to override OSD element colors: background, border, text, etc.)
 ├── bios/
 │    ├── bios_CD_U.bin      (Sega CD - US Region BIOS)
 │    ├── bios_CD_E.bin      (Mega CD - EU Region BIOS)
 │    ├── bios_CD_J.bin      (Mega CD - JP Region BIOS)
 │    └── syscard3.pce       (PC Engine CD - System Card 3.0 BIOS)
 └── roms/
      ├── snes/              (SNES ROM files: .sfc, .smc)
      ├── nes/               (NES ROM files: .nes)
      ├── megadrive/         (Mega Drive ROM files: .bin, .md, .gen)
      ├── megacd/            (Sega CD ROM files: .iso, .cue, .chd)
      ├── mastersystem/      (Master System ROM files: .sms, .gg, .bin)
      ├── pce/               (PC Engine / PCE CD files: .pce, .cue, .chd)
      └── favorites.txt      (Auto-generated file tracking favorite games)
```

> [!TIP]
> **Configuring Screensaver Timeout**: Add or edit `screensaver=<seconds>` in `cmdline.txt` on the SD card root (e.g. `screensaver=60` for 60 seconds, `screensaver=120` for 2 minutes, or `screensaver=0` to disable the screensaver completely).

> [!IMPORTANT]
> **Pi 3 Performance Profile**: The bundled `config.txt` configures the Pi 3 at 1.4 GHz with a 500 MHz core clock and `over_voltage=4` for demanding games. Active cooling is required; remove these settings if the system is unstable.

> [!TIP]
> **Configuring Splash Screen Duration**: Add or edit `splash=<seconds>` in `cmdline.txt` on the SD card root (e.g. `splash=4` for 4 seconds, `splash=2` for 2 seconds, or `splash=0` to completely skip the splash screen on boot).

> [!TIP]
> **Multiple Splash Screens & Automatic Rotation**: You can now store multiple splash screens on your SD card! Simply create a `splash/` folder on the SD card root (e.g., `SD:/splash/retro1.raw16`, `SD:/splash/retro2.raw16`) or use numbered files on the root (e.g., `SD:/Splash_Screen1.raw16`, `SD:/Splash_Screen2.raw16`). The emulator will automatically discover all splash images and cycle to the next splash screen on every boot!

> [!TIP]
> **Customizing System Order**: Create or edit `system_order.txt` on the SD card root to set your preferred system cycling order for **L** / **R** shoulder buttons. The first system in the list will automatically become the default boot system on startup (e.g., `megadrive`, `snes`, `nes`, `mastersystem`, `pce`).

> [!TIP]
> **Customizing OSD Theme**: Create or edit `osd_theme.txt` on the SD card root and set one of these values: `default` (slate blue), `green` (CRT green), `grayscale` (stealth slate), `cyberpunk` (neon cyan), `sapphire` (royal blue), `synthwave` (electric violet), `arctic` (polar mint), `amber` (solar gold), `ruby` (crimson red), `custom`, or `all` (cycles through all 9 themes on every boot).

> [!TIP]
> **Customizing OSD Colors**: Set `osd_theme.txt` to `custom`, then create or edit `osd_colors.txt` on the SD card root using `key=value` lines (example: `background=#000000`, `border=8,12,16`, `text=26,28,30`). The repository includes multiple ready-to-copy palettes in `osd_colors.txt` (High Contrast Dark, Warm Amber Terminal, Ice Blue).

> [!NOTE]
> Save state files (e.g., `Game.s0` / `Game.srm`) are saved directly into the folder containing the ROM being played.

---

### 🎮 Controller Layout (Gamesir Nova Lite & Standard Xbox 360)

The emulator supports standard XInput gamepads out-of-the-box (like the **Gamesir Nova Lite** detected under USB Vendor/Product ID `ven3537-1040`).

### 🖥️ OSD Menu Navigation
* **D-pad**: Navigate ROM list (Up / Down) or switch tabs (Left / Right).
* **A / B Buttons**: Start / select highlighted game.
* **Y Button**: Add to Favorites (`*` prefix).
* **X Button**: Remove from Favorites (Unfavorite).
* **SELECT + X**: Force rescan SD card ROM directories & rebuild `library.cache`.
* **START + SELECT**: Resets or exits the current game to return to the OSD menu.

> [!TIP]
> **Fast Boot ROM Library Cache**: On startup, the system loads the cached ROM list directly from `SD:/roms/library.cache` in under 2ms. If you add or remove ROM files on your SD card, press **SELECT + X** in the OSD menu (or delete `SD:/roms/library.cache` on your PC) to perform a full directory rescan and rebuild the cache.

---

### 🕹️ Gameplay Mappings

#### 1. Super Nintendo (SNES) Layout
Button mappings preserve physical positions matching the original SNES controller layout:

| Gamesir Button (Xbox Layout) | Physical Position | Mapped SNES Button |
| :--- | :--- | :--- |
| **A** | Bottom | **B** |
| **B** | Right | **A** |
| **X** | Left | **Y** |
| **Y** | Top | **X** |
| **LB** / **LT** | Left Shoulder / Trigger | **L** |
| **RB** / **RT** | Right Shoulder / Trigger | **R** |
| **Start** | Center-Right | **Start** |
| **Select** | Center-Left | **Select** |

#### 2. Sega Mega Drive / Genesis Layout
The controller layout dynamically adjusts depending on whether the game is a 3-button or 6-button title (detected automatically by ROM name or override tags like `(3b)`/`(6b)`):

##### 3-Button Controller Mode (Default for standard games)
Optimized face button mappings for comfortable 3-button play:

| Gamesir Button (Xbox Layout) | Mapped Sega Button |
| :--- | :--- |
| **A** | **A** |
| **B** | **B** |
| **X** | **C** |
| **RT** (Right Trigger) | **C** (Fallback) |
| **Start** | **Start** |
| **Select** | **Mode** |

##### 6-Button Controller Mode (Active for fighting/arcade games utilizing all buttons)
Maps the standard six-button Sega controller layout:

| Gamesir Button (Xbox Layout) | Mapped Sega Button |
| :--- | :--- |
| **A** | **A** |
| **B** | **B** |
| **RT** (Right Trigger) | **C** |
| **X** | **X** |
| **Y** | **Y** |
| **LT** (Left Trigger) / **RB** | **Z** |
| **LB** | **X** (Fallback) |
| **Start** | **Start** |
| **Select** | **Mode** |

##### 3. PC Engine (PCE) / TurboGrafx-16 Layout
Button mappings preserve physical positions matching the original 2-button PC Engine controller:

| Gamesir Button (Xbox Layout) | Physical Position | Mapped PCE Button |
| :--- | :--- | :--- |
| **A** | Bottom | **Button I** |
| **B** | Right | **Button II** |
| **Start** | Center-Right | **Run** |
| **Select** | Center-Left | **Select** |

---

### 💾 Save, Load, and Rewind State Combos
* **SELECT + D-pad Left** OR **SELECT + L Shoulder/Trigger**: Save state to Slot 0.
* **SELECT + D-pad Right** OR **SELECT + R Shoulder/Trigger**: Load state from Slot 0.
* **SELECT + D-pad Up** (or keyboard **F6**): Rewind state (5-second buffer).

---

### 🔌 Retroflag Safe Shutdown & Reset (NESPi, SuperPi, MegaPi cases)

This bare-metal kernel provides native, hardware-level support for the physical buttons and status indicators on Retroflag cases without needing an underlying operating system or Python scripts.

#### Hardware Wiring & Pin Mapping
* **Power Button** (BCM GPIO 3): Monitored by the kernel. Toggling the power switch to OFF triggers a safe shutdown routine.
* **Reset Button** (BCM GPIO 2): Monitored by the kernel. Pressing the physical reset button triggers a system reboot.
* **Status LED** (BCM GPIO 14): Controlled by the kernel. Set to solid HIGH on boot and turns off upon shutdown.
* **Power Enable / Keep-Alive** (BCM GPIO 4): Kept HIGH on boot to maintain the case power supply circuit. Pulls LOW during shutdown to instruct the case hardware to safely cut the 5V line.

> [!IMPORTANT]
> Make sure the physical **SAFE SHUTDOWN** switch located on the internal PCB of your Retroflag case is set to **ON** to enable this hardware-level signaling.

#### OSD Safe Shutdown & Reset Messages
When a button press is detected, the emulator instantly halts gameplay or OSD menu loops and overlays an on-screen dialog:
* **Shutdown:** Clears the screen and displays `"SHUTTING DOWN..."` on a dark-themed container for 2 seconds. The FAT filesystem is cleanly unmounted, the status LED is turned off, and the power enable pin is pulled low to safely cut the power.
* **Reset:** Clears the screen and displays `"REBOOTING SYSTEM..."` for 2 seconds. The FAT filesystem is cleanly unmounted, and the system reboots back into the bootloader/OSD menu.

---


### 🛠️ Compilation & Deployment

To compile the projects, you must have the `arm-none-eabi` cross-compilation toolchain and standard build utilities installed on your host system.

#### 1. Installing the Toolchain & Build Tools

##### Linux (Ubuntu / Debian)
```bash
sudo apt update
sudo apt install gcc-arm-none-eabi g++-arm-none-eabi build-essential zip
```

##### Linux (Arch Linux)
```bash
sudo pacman -S arm-none-eabi-gcc arm-none-eabi-newlib base-devel zip
```

##### Linux (Fedora)
```bash
sudo dnf install gcc-arm-none-eabi newlib-arm-none-eabi make zip
```

##### macOS
Install the toolchain via [Homebrew](https://brew.sh/):
```bash
brew tap osx-cross/arm
brew install arm-none-eabi-gcc
```
Or alternatively:
```bash
brew install --cask gcc-arm-embedded
```
You will also need `make` and `zip` if you don't already have them installed:
```bash
brew install make zip
```

#### 2. Building the 5-in-1 Bare-Metal Emulator
To build the main 5-in-1 multi-console emulator kernel:
```bash
cd main-emulator
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)
```
This produces `main-emulator/kernel8-32.img`.

#### 3. Building Standalone Emulator Targets
- **Super Nintendo**: `cd snes-emulator && make -j$(nproc)` $\rightarrow$ produces `snes-emulator/kernel8-32.img`
- **Sega Mega Drive**: `cd mega-emulator && make -j$(nproc)` $\rightarrow$ produces `mega-emulator/kernel8-32.img`
- **Sega Master System**: `cd master-emulator && make -j$(nproc)` $\rightarrow$ produces `master-emulator/kernel8-32.img`

#### 5. Generating the SD Card Release Package
To compile and package all boot files along with the required SD card folder tree (`roms/snes`, `roms/megadrive`, `roms/megacd`, `roms/mastersystem`, and `bios`) automatically:
```bash
./build_release.sh
```
This script clean builds the unified project and saves the final package to `release/sdcard_release.zip`. Extract the contents of this zip directly onto the root of a FAT32-formatted SD Card.

---

### 📚 Third-Party Resources & References

This project is built upon the incredible work of the following open-source projects:

* **Circle**: A C++ bare-metal environment for the Raspberry Pi.
  * Repository: [rsta2/circle](https://github.com/rsta2/circle)
* **PicoDrive**: A fast, highly-optimized Sega Mega Drive/Genesis/Master System and Sega CD emulator.
  * Repository: [notaz/picodrive](https://github.com/notaz/picodrive)
* **Snes9x**: A portable, high-compatibility Super Nintendo Entertainment System (SNES) emulator.
  * Repository: [snes9xgit/snes9x](https://github.com/snes9xgit/snes9x)
* **Nestopia**: A highly accurate Nintendo Entertainment System (NES/Famicom) emulator used as the base for this project's NES implementation.
  * Project page: [nestopia.sourceforge.net](http://nestopia.sourceforge.net/)
* **Beetle PCE Fast**: A high-performance PC Engine (TG16) / PC Engine CD emulator core (based on Mednafen) used for the project's PC Engine emulation.
  * Repository: [libretro/beetle-pce-fast-libretro](https://github.com/libretro/beetle-pce-fast-libretro)
