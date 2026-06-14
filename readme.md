# STM32H747I-DISCO Music Keyboard

A touchscreen piano keyboard built with TouchGFX on the STM32H747I-Discovery board.
Each on-screen key plays a sine-wave tone through the headphone jack, with
hold-to-sustain behaviour, ADSR envelope shaping, octave shifting, and a touch /
joystick volume control.

---

## Features

- 24 piano keys (2 octaves: C4 through B5, white and black keys)
- **Touch trigger**: notes start the instant the key is pressed
- **Hold-to-sustain**: tone continues for as long as the key is held
- **Minimum note duration**: short taps still produce an audible note (~300 ms)
- **ADSR envelope** via codec hardware volume — smooth attack and release, no clicks
- **Octave shifting** from -2 to +2 (audible range C2 through B7)
- **Volume control** via on-screen slider and 5-way joystick (up/down)
- **Live displays**: note name (e.g., "C#5"), exact frequency in Hz, current octave, current volume
- **Dual-core build** (CM7 runs everything; CM4 boots but is idle)

---

## Hardware Requirements

- **STM32H747I-DISC0** Discovery board
- Headphones or powered speakers (3.5 mm jack into **CN11**, the green jack)
- USB cable (Micro-B) for power, flashing, and debugging
- ST-LINK V3 (built into the board)

---

## Software Requirements

| Tool | Version Used | Notes |
|---|---|---|
| STM32CubeIDE | 2.1.1 | Newer versions should work too |
| TouchGFX Designer | 4.26.1 | Required for editing the UI |
| STM32CubeProgrammer | latest | Used for flashing |
| STM32 Cube Firmware H7 | V1.13.0 | Install via CubeMX |
| GCC ARM toolchain | bundled with CubeIDE | — |

---

## First-Time Setup

### 1. Clone the repo

```bash
git clone <your-repo-url> Keyboard_v1
cd Keyboard_v1
```

> Place it in a path **without spaces**, e.g., `C:\TouchGFXProjects\Keyboard_v1\`. Spaces in the path can break some toolchain steps.

### 2. Install the STM32 H7 firmware package

1. Open CubeMX (or CubeIDE → Help → Manage embedded software packages).
2. Search for **STM32CubeH7** and install version **1.13.0**.
3. The package goes to `C:\Users\<yourname>\STM32Cube\Repository\STM32Cube_FW_H7_V1.13.0\`.

This package contains BSP drivers (`stm32h747i_discovery_audio.c`, `wm8994.c`, etc.) that the project references.

### 3. Regenerate TouchGFX code

1. Open `Keyboard_v0.touchgfx` (in the project root) with TouchGFX Designer.
2. **File → Generate Code** (or press F4).

This populates the `CM7/TouchGFX/generated/` folders. They're committed to git, but regenerating ensures they match your local Designer install.

### 4. Import into STM32CubeIDE

1. Open CubeIDE with a workspace of your choice.
2. **File → Import → General → Existing Projects into Workspace**.
3. Browse to the project's `STM32CubeIDE/` folder.
4. Two projects appear: `KEYBOARD_V1_CM7` and `KEYBOARD_V1_CM4`. Tick both.
5. **Leave "Copy projects into workspace" UNCHECKED.** The project uses relative paths that only resolve when it stays in its original folder.
6. Click Finish.

### 5. Configure the external QSPI loader

The board has an external 64 MB QSPI flash chip (Micron MT25TL01G) that holds TouchGFX assets (fonts, images). Programming it requires a loader binary.

1. **Run → Debug Configurations…**
2. Select the `KEYBOARD_V1_CM7` debug entry (or create one for the CM7 project).
3. Open the **Debugger** tab.
4. Scroll to **External Loaders** → check **Use external loader**.
5. Click **Add…**, select `MT25TL01G_STM32H747I-DISCO.stldr` (ships with CubeProgrammer).
6. **Apply**, close.

Without this, flashing will fail with "Load failed" because the ST-LINK can't program the external chip.

### 6. Build and flash

1. Build the CM4 project first: right-click `KEYBOARD_V1_CM4` → Build Project. It should compile in seconds.
2. Build the CM7 project: right-click `KEYBOARD_V1_CM7` → Build Project. Takes longer (~30 s for first build).
3. **Run → Debug** on `KEYBOARD_V1_CM7`. The IDE flashes both cores and starts a debug session.
4. **Power-cycle the board** (unplug and replug USB) after the first flash to ensure the audio codec resets cleanly.

You should now see the keyboard on the touchscreen. Plug in headphones, tap a key, hear a tone.

---

## Project Structure

```
Keyboard_v1/
├── Keyboard_v0.touchgfx          ← TouchGFX project file (open in Designer)
├── stm32h747i-disco.ioc          ← CubeMX configuration
├── README.md                     ← this file
├── .gitignore
│
├── CM7/                          ← Cortex-M7 source (the brain)
│   ├── Core/                     ← CubeMX-generated startup, peripheral init
│   │   └── Src/main.c            ← entry point, peripheral configs
│   ├── TouchGFX/
│   │   ├── gui/                  ← YOUR application code
│   │   │   ├── include/screen1_screen/
│   │   │   │   └── Screen1View.hpp
│   │   │   └── src/screen1_screen/
│   │   │       └── Screen1View.cpp   ← all audio + UI logic
│   │   ├── generated/            ← Designer auto-generated (do not edit)
│   │   └── target/               ← board integration (display, touch)
│   └── LIBJPEG/                  ← JPEG decoder (unused in current project)
│
├── CM4/                          ← Cortex-M4 source (idle in this project)
├── Common/                       ← shared boot code
│
├── STM32CubeIDE/                 ← CubeIDE project files
│   ├── CM7/                      ← .project, .cproject, linker scripts
│   │   └── stm32h747xx_flash_CM7.ld
│   └── CM4/
│       └── stm32h747xx_flash_CM4.ld
│
├── Drivers/                      ← STM32 HAL + BSP drivers
├── Middlewares/                  ← FreeRTOS, TouchGFX framework, LibJPEG
├── Utilities/                    ← JPEG utility wrappers
├── assets/                       ← TouchGFX image sources (PNGs)
├── config/                       ← TouchGFX build configurations
└── generated/                    ← TouchGFX framework code (committed)
```

---

## Key Files to Edit

| What you want to change | File |
|---|---|
| Audio behaviour, key handlers, envelope | `CM7/TouchGFX/gui/src/screen1_screen/Screen1View.cpp` |
| Class declarations | `CM7/TouchGFX/gui/include/screen1_screen/Screen1View.hpp` |
| UI layout (keys, sliders, text widgets) | open `Keyboard_v0.touchgfx` in Designer |
| Peripheral config (SAI, DMA, clocks) | open `stm32h747i-disco.ioc` in CubeMX |
| Memory layout / flash address | `STM32CubeIDE/CM7/stm32h747xx_flash_CM7.ld` |

**Do NOT edit:**
- Any file in `generated/` or `gui_generated/` — overwritten on F4.
- `Screen1ViewBase.hpp/.cpp` — generated; edit `Screen1View.hpp/.cpp` instead.

---

## Hardware Architecture (Quick Reference)

| Region | Address | Used For |
|---|---|---|
| Internal Flash Bank 1 | `0x08000000`, 1 MB | CM7 code |
| Internal Flash Bank 2 | `0x08100000`, 1 MB | CM4 code |
| RAM_D1 | `0x24000000`, 512 KB | CM7 RAM, `audioBuffer[]` |
| RAM_D2 | `0x30000000`, 288 KB | CM4 RAM |
| External QSPI | `0x90000000`, 64 MB | TouchGFX fonts and images |
| External SDRAM | `0xD0000000`, 32 MB | TouchGFX framebuffers |

See `Memory_Architecture_Report.md` for the full details.

---

## How a Key Press Works (Quick Reference)

1. Finger touches screen → FT6x06 touch controller detects it.
2. TouchGFX polls the touch driver on its next 60 Hz tick.
3. The Flex Button for the pressed key fires its **Touch** interaction.
4. `onKeyPressed_X()` runs → calls `playNote(freq, name)`.
5. `playNote` writes a sine wave into `audioBuffer[]`, flushes the CPU cache.
6. DMA (already running circularly) reads the new buffer contents.
7. SAI peripheral clocks the samples to the WM8994 codec.
8. Codec produces analog output through the headphone jack.
9. Envelope state machine ramps codec volume up (attack) and later down (release).

Total latency from touch to audible tone: ~20–35 ms.

See `Touch_to_Audio_Process_Report.md` for the full step-by-step.

---

## Troubleshooting

### "Load failed" when flashing
The external QSPI loader isn't configured for this project's debug config. See setup step 5.

### Build fails with "pdm2pcm_glo.h: No such file or directory"
The PDM2PCM include path is missing from the build configuration. Right-click project → Properties → C/C++ Build → Settings → MCU GCC Compiler → Include paths → add:
`<firmware repo>/Middlewares/ST/STM32_Audio/Addons/PDM/Inc`

### No audio after flashing
- Make sure headphones are in **CN11** (green jack), not CN10.
- Power-cycle the board after flashing (codec state can get stuck).
- Check that BSP audio volume is non-zero in `setupScreen` (currently set to 80).

### TouchGFX text shows question marks (????)
The font doesn't have glyphs for those characters. In Designer, edit the wildcard text widget's **initial value** to include every character you want to display, then F4 → Clean → build.

### Slider doesn't update volume
Make sure the slider has an **Interaction** in Designer with trigger = value-changed, action = call virtual function `onVolumeSliderChanged`.

### Joystick changes volume in both directions (only up, or only down)
The BSP `BSP_JOY_GetState` API can differ between versions. Use `BSP_JOY_GetState(JOY1, JOY_ALL)` and mask the result against `JOY_UP` / `JOY_DOWN` individually.

### Notes click or buzz at the start/end
Increase `ATTACK_TICKS` and `RELEASE_TICKS` in `Screen1View.cpp` for a slower, smoother envelope. Default values: 6 and 10.

---

## Workflow After Setup

For day-to-day development:

1. Edit code in `Screen1View.cpp` / `Screen1View.hpp`.
2. If changing the UI, open the `.touchgfx` file in Designer and edit there → F4 to regenerate.
3. In CubeIDE: build (`Ctrl+B`), flash via Debug, power-cycle, test.
4. Commit your changes:
   ```bash
   git add -u
   git commit -m "what you changed"
   git push
   ```

**After UI changes** (font characters, wildcard values, new widgets), always do **Project → Clean** before building. CubeIDE's incremental build sometimes misses Designer-regenerated files.

---

## License

(Add a license of your choice — MIT, Apache 2.0, or proprietary.)

---

## Acknowledgements

Built with:
- [TouchGFX](https://touchgfx.com/) framework by STMicroelectronics
- STM32CubeH7 firmware package
- FreeRTOS
- WM8994 audio codec driver (BSP)