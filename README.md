# SPLIT/SECOND SPEEDOMETER HUD

A custom speedometer overlay designed for the game Split/Second. This mod provides an in-game display of your vehicle's speed.

## Features

- **Smart Layout**: Automatically centers text and aligns units dynamically.
- **Fully Customizable**:
  - Move/scale Speed Value, Unit, and Gauge Bar independently.
  - Supports negative coordinates for precise positioning.
- **Gauge Bar**: Adjustable width, height, and skew.
- **Persistence**: Settings are saved automatically to 'speedometer.ini' and loaded on startup.
- **Integrated Logging**: Diagnostic system for troubleshooting, logging to 'speedometer.log'.

## Installation

This mod requires **Ultimate ASI Loader** to function. You will find pre-built versions of the speedometer mod in the releases section of this repository.

1.  **Ultimate ASI Loader**: `dinput8.dll` (Ultimate ASI Loader) is included in the release package. This DLL is essential for the `.asi` mod to load and function correctly in the game.
    - **Note**: If you are using other ASI mods, you might need to rename `dinput8.dll` to `version.dll` to avoid conflicts.
2.  **Copy ASI Loader**: Place the included `dinput8.dll` (or `version.dll` if renamed) next to your game's executable (`SplitSecond.exe`).
3.  **Download Mod**: Download the `speedometer.asi` file from the [releases page](https://github.com/theUssa1n/SplitSecond-HUD-Mod/releases) (or similar link) for your desired game version.
4.  **Copy Mod Files**: Copy the `splitsecond-speedometer.asi` file and the `fonts` folder to your game's root directory (where `SplitSecond.exe` is located).
    - **MANDATORY**: Speedometer styles will not show without the `fonts` folder.
5.  **Run the Game**: Launch `SplitSecond.exe`. The speedometer HUD should now be active in-game.

## Version Information

This project supports two main versions of Split/Second:

- **Retail v1 [Cracked] / SSDC Builds**: This version is compatible _only_ with the cracked retail game and SSDC builds.
- **Modded Steam Builds** (Latest Elysium mod / Xenon): This version is designed for Steam versions of the game that use the Elysium or Xenon mods.

To switch between versions, you will need to use the corresponding `speedometer.asi` file built for that specific game version. Ensure you download and install the correct `.asi` from the releases.

## Controls

- `[ F1 ]`: Open/Close Configuration Menu.
  - Customize everything: Position, Font, Scale, Layout.
  - Settings are saved automatically to 'speedometer.ini'.
- `[ K ]`: Toggle HUD Visibility (On/Off).
  - HUD automatically activates when the race starts.
  - Recommended for laptop users if F1 conflicts with system keys.
- `[ M ]`: Quick Switch Units (MPH / KM/H).
  - Can also be changed in the Config Menu.

## Troubleshooting

If the HUD is not showing up:

1.  Ensure the "fonts" folder is in your game directory.
2.  Laptop users: Try holding `[ Fn ] + F1`, or just use the `[ K ]` key.
3.  Check `speedometer.log` in your game folder. If the mod fails, share this log with the developer.
4.  Disable other overlays (Steam, Discord, MSI Afterburner) if rendering issues occur.
5.  Check if your Antivirus has quarantined `dinput8.dll/version.dll` or `speedometer.asi`.

## Source Code Prerequisites

This project is developed using Visual Studio. To compile the source code, you will need:

- Visual Studio 2022 (or newer) with C++ development workload.
- Microsoft DirectX SDK (June 2010).

The necessary libraries (MinHook and ImGui) are included within the project's folders.

### Building Specific Versions from Source

The source code includes support for both **Retail v1 [Cracked] / SSDC Builds** and **Modded Steam Builds**. You can switch between these versions by modifying a preprocessor directive in the [drawing.cpp](drawing.cpp) file:

- To build for the **Modded Steam Build** version, **uncomment** the line `#define USE_STEAM_VERSION` (around line 91) in [drawing.cpp](drawing.cpp).
- To build for the **Retail v1 [Cracked] / SSDC Builds** version, ensure the line `#define USE_STEAM_VERSION` (around line 91) in [drawing.cpp](drawing.cpp) is **commented out**.

## Anti-Virus False Positives

It is common for game modifications, especially those that use techniques like DLL injection and API hooking (which this mod utilizes), to be flagged by anti-virus software. This often occurs due to:

- **Code Injection**: The mod's method of loading into the game process is similar to techniques used by malicious software.
- **API Hooking**: Intercepting game functions to display the HUD can be misinterpreted as suspicious behavior by anti-virus heuristics.
- **Lack of Digital Signature**: As a community-developed mod, `speedometer.asi` is not digitally signed, which can make anti-virus programs more suspicious.

**This mod is NOT malicious.** It is designed solely to enhance your Split/Second gameplay experience.

**What to do if your anti-virus flags the mod:**

- **Add to Exclusions**: The most common solution is to add `speedometer.asi` and `dinput8.dll` (or `version.dll`) to your anti-virus software's exclusion or whitelist.
- **Report as False Positive**: Consider reporting the file as a false positive to your anti-virus vendor. This helps them improve their detection algorithms.

## Credits

- Special thanks to asdfghj AKA Lahvuun for making this idea possible.

## License

This project is licensed under the MIT License.
