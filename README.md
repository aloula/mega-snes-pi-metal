# MEGA-SNES Pi Metal

![MEGA-SNES_Pi_Metal](res/MEGA-SNES_Pi_Metal.png)

A unified, low-latency, bare-metal dual console emulator for the Raspberry Pi 3B+. This project merges the **SNES-PI** and **MEGA-PI** emulators into a single bare-metal kernel, allowing real-time switching between **Super Nintendo Entertainment System (SNES)** and **Sega Mega Drive / Sega CD (Genesis)** consoles directly from the On-Screen Display (OSD) menu.

Built on the **Circle C++ bare-metal environment**, **Snes9x**, and **Picodrive**, it runs directly on the ARM CPU without an underlying operating system, ensuring maximum speed, minimal input latency, and exact hardware timing.

---

### 🚀 Key Features

* **Dual-Console Emulation**: Run both SNES and Sega Mega Drive/Mega CD games from a single boot image.
* **Low Latency**: Direct hardware access bypassing OS overhead, providing sub-millisecond input and audio response.
* **Unified OSD Menu**: Dynamic graphical user interface featuring:
  * Dynamic header banners changing based on the selected system.
  * Real-time console switching via **L** and **R** shoulder buttons.
  * Auto-balanced alphabetical ROM folders (splits games across custom tabs based on active systems).
  * Favorite lists (`favorites.txt`) managed directly from the UI.
* **Automatic Save States**: Game states are loaded/saved in Slot 0 (stored as `.s0` files) located alongside the ROMs.
* **High-Fidelity Audio**: Hardware-authentic audio resampling and interpolation (Gaussian audio for SNES).
* **Display Scaling**: Nearest-neighbor scaling for Sega games and linear/Gaussian aspect scaling for SNES games.

---

### 📁 SD Card Configuration

To load games and BIOS files, organize your SD card root directories as follows:

```
SD:/
 ├── bios/
 │    ├── bios_CD_U.bin      (Sega CD - US Region BIOS)
 │    ├── bios_CD_E.bin      (Mega CD - EU Region BIOS)
 │    └── bios_CD_J.bin      (Mega CD - JP Region BIOS)
 └── roms/
      ├── snes/              (SNES ROM files: .sfc, .smc)
      ├── megadrive/         (Mega Drive ROM files: .bin, .md, .gen)
      ├── megacd/            (Sega CD ROM files: .iso, .cue, .chd)
      └── favorites.txt      (Auto-generated file tracking favorite games)
```

> [!NOTE]
> Save state files (e.g., `Game.s0`) are saved directly into the folder containing the ROM being played.

---

### 🎮 Controller Layout (Gamesir Nova Lite & Standard Xbox 360)

The emulator supports standard XInput gamepads out-of-the-box (like the **Gamesir Nova Lite** detected under USB Vendor/Product ID `ven3537-1040`).

### 🖥️ OSD Menu Navigation
* **D-pad**: Navigate ROM list (Up / Down) or switch tabs (Left / Right).
* **A / B Buttons**: Start / select highlighted game.
* **Y Button**: Add to Favorites (`*` prefix).
* **X Button**: Remove from Favorites (Unfavorite).
* **L / R Shoulder Buttons**: Switch emulator console mode (**SNES** $\leftrightarrow$ **Mega Drive**).
* **START + SELECT**: Resets or exits the current game to return to the OSD menu.

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

---

### 💾 Save and Load State Combos
* **SELECT + D-pad Left**: Save state to Slot 0.
* **SELECT + D-pad Right**: Load state from Slot 0.

---

### 🛠️ Compilation & Deployment

To compile the projects, you must have the `arm-none-eabi` cross-compilation toolchain installed on your host system.

### Build Unified Dual Emulator (Default)
To build the unified kernel:
```bash
cd snes-emulator
make -j$(nproc)
```
This produces `snes-emulator/boot/kernel8-32.img`. Copy the files inside the `snes-emulator/boot/` directory to the fat32 boot partition of your SD card.

### Build Standalone Mega Drive Emulator
To build the standalone Sega emulator:
```bash
cd mega-emulator
make -j$(nproc)
```
This produces `mega-emulator/boot/kernel8-32.img`. Copy the files inside `mega-emulator/boot/` to your SD card.

### Generate SD Card Release Package
To compile and package all boot files along with the required SD card folder tree (`roms/snes`, `roms/megadrive`, `roms/megacd`, and `bios`) automatically:
```bash
./build_release.sh
```
This script clean builds the unified project and saves the final package to `release/sdcard_release.zip`. Extract the contents of this zip directly onto the root of a FAT32-formatted SD Card.

---

### 📚 Third-Party Resources & References

This project is built upon the incredible work of the following open-source projects:

* **Circle**: A C++ bare-metal environment for the Raspberry Pi.
  * Repository: [rsta2/circle](https://github.com/rsta2/circle)
* **PicoDrive**: A fast, highly-optimized Sega Mega Drive/Genesis and Sega CD emulator.
  * Repository: [notaz/picodrive](https://github.com/notaz/picodrive)
* **Snes9x**: A portable, high-compatibility Super Nintendo Entertainment System (SNES) emulator.
  * Repository: [snes9xgit/snes9x](https://github.com/snes9xgit/snes9x)

