# GTA2 Native — macOS (Apple Silicon) & iOS

A native 64-bit ARM port of **Grand Theft Auto 2** (PC v10.5) for macOS and iPhone/iPad,
built on top of the [CriminalRETeam/gta2_re](https://github.com/CriminalRETeam/gta2_re)
matching decompilation. No emulation, no Wine: the decompiled game code is compiled with
clang for arm64 and runs on a small SDL3 platform layer.

> The source code in this repository contains no game data. GTA2 was released as freeware
> by Rockstar Games in 2004. Building from source needs your own copy of its `data/` folder.
> The IPA files on the [Releases](https://github.com/ErcouldnT/gta2-native/releases) page
> are ready to play and include it.

## Install on iPhone/iPad with Sideloadly

No Mac or Xcode needed: [Sideloadly](https://sideloadly.io) runs on Windows and macOS.

1. Download an IPA from [Releases](https://github.com/ErcouldnT/gta2-native/releases):
   `GTA2-native-en.ipa` (English) or `GTA2-native-tr.ipa` (Turkish, Türkçe).
2. Install [Sideloadly](https://sideloadly.io). On Windows, also install iTunes and iCloud
   from Apple's website (not the Microsoft Store versions), as Sideloadly asks.
3. Connect the device with a cable, unlock it and tap **Trust**.
4. Open Sideloadly, drop the IPA on it, enter your Apple ID and press **Start**.
   A free Apple ID is enough. Sideloadly signs the app with it.
5. On the device, enable **Settings → Privacy & Security → Developer Mode** (iOS 16+) and restart when asked.
6. Trust your certificate in **Settings → General → VPN & Device Management**, under your Apple ID.
7. Start GTA2.

With a free Apple ID the app stops opening after **7 days**. Install it again with Sideloadly
to renew it; your saves are kept as long as you do not delete the app. Requires iOS 16 or later.

## Features

- Runs natively on Apple Silicon Macs and on iPhone/iPad (iOS 16+)
- Metal rendering through SDL3, widescreen view (4:3 HUD kept centred)
- Touch controls with icons: a floating joystick that moves the character or car
  **in the direction you hold** (no tank controls), plus fire, jump, enter/exit car,
  reverse, weapon switch, special and menu buttons
- Hold the camera button and move the stick to pan the view
- Aim assist: holding fire on foot turns you towards the nearest target ahead
- Day/night cycle (10 minutes, starting at noon): the streets darken and lit areas stand out
- Game controller support through SDL (implemented, not yet tested with real hardware)
- Original music and sound effects (Miles Sound System calls reimplemented on SDL audio)
- Intro movie (Bink replaced by AVFoundation playback of an H.264 conversion)
- Saves and settings stored in the app's Documents folder (visible in the Files app)
- **Optional Turkish translation** with real Turkish letters (ç ğ ı İ ö ş ü) added to the
  game's fonts — see [Turkish](#turkish-türkçe)

Not supported: network multiplayer (DirectPlay is stubbed out).

## Requirements

- macOS on Apple Silicon, Xcode (command line tools for the Mac build, full Xcode for iOS)
- CMake ≥ 3.20 and SDL3 (`brew install cmake sdl3`)
- For iOS: the SDL3 source tree next to this repository (`../SDL`, tested with `release-3.4.18`)
  and an Apple developer account (a free one works for your own device)
- Optional: `ffmpeg` to convert the intro movie
- The GTA2 PC game data (`data/` folder of the freeware release)

```bash
git clone --recursive https://github.com/ErcouldnT/gta2-native.git
git clone --branch release-3.4.18 https://github.com/libsdl-org/SDL.git   # iOS only
```

The `3rdParty/gta2_re_compile_tools` submodule is needed: the port reuses its
DirectX/DirectPlay headers.

## macOS

```bash
GTA2_ROOT=/path/to/GTA2 port/run-mac.sh      # GTA2_ROOT contains data/
```

This configures `build-mac/`, builds and starts the game. Keyboard controls are the
original PC ones; a connected game controller also works.

## iPhone / iPad

1. Connect the device, unlock it, trust the Mac and enable Developer Mode.
2. Build, install and copy the game data in one step:

   ```bash
   GTA2_ROOT=/path/to/GTA2 port/install-ios.sh
   ```

   With another Apple developer account, pass your team and a unique bundle id:

   ```bash
   PORT_TEAM_ID=ABCDE12345 PORT_BUNDLE_ID=com.example.gta2 GTA2_ROOT=... port/install-ios.sh
   ```

The data goes to `Documents/GTA2/data` inside the app. You can also copy it there yourself
with the Files app or Finder (`SKIP_DATA=1` skips the copy).

## Turkish (Türkçe)

The tools in `port/tools/` build a Turkish `e.gxt` and add the missing Turkish letters to
the fonts:

| Tool | Purpose |
|------|---------|
| `gxt.py` | Dumps a GXT text file to JSON and builds it back |
| `build_tr.py` | Merges translation batches (`out_*.tsv`) into a Turkish `e.gxt` |
| `check_tr.py` | Checks that highlight marks, `%d`/`%s` codes and speaker markers survived translation |
| `sty_tr_font.py` | Adds ğ Ğ ş Ş ı İ glyphs (made from the fonts' own letters) to a style file (`.sty`) |

Ç Ö Ü ç ö ü already exist in the original fonts, so the game shows them without changes.
The other letters need the patched `.sty` files.

```bash
GTA2_TR_GXT=/path/to/e_tr.gxt GTA2_ROOT=... port/install-ios.sh
```

This installs the translation as `e.gxt` and patched copies of `fstyle.sty`, `wil.sty`,
`bil.sty` and `ste.sty` on the device. Your original game files are not modified.

## How the port works

- `port/compat/`: a force-included prelude and stand-in Windows headers (`windows.h`,
  DirectX, COM macros) so the decompiled sources compile unchanged with clang.
- `port/platform/`: the SDL3 platform layer — window, Metal renderer implementing the
  game's `gbh_*`/`Vid_*` driver API, DirectInput emulation, touch overlay and direct
  control, Miles audio, Bink→AVFoundation, registry emulation in a text file,
  case-insensitive file paths.
- 64-bit fixes in the game code: pointers stored inside 32-bit file/script data use
  `PTR32(T)`, a 4-byte handle type (`port/compat/port_ptr32.hpp`); integer fields that
  hold pointers use `sptr`/`uptr`. Port-specific changes to `Source/` are inside `#ifdef GTA2_PORT`.

Debug switches (environment variables): `PORT_NO_AUDIO`, `PORT_NO_MOVIES`,
`PORT_WIDESCREEN`/`PORT_43`, `PORT_TANK_CONTROLS`, `PORT_NO_AIM_ASSIST`, `PORT_NO_DAYNIGHT`,
`PORT_DAYNIGHT_PHASE`, `PORT_AUTOKEYS`, `PORT_AUTOTOUCH`,
`PORT_SHOTS` + `PORT_SHOT_DIR`; `-DPORT_ASAN=ON` builds with AddressSanitizer.

## Credits

- The decompilation this port is built on: [CriminalRETeam/gta2_re](https://github.com/CriminalRETeam/gta2_re)
  and its contributors.
- [SDL](https://libsdl.org) for windowing, input and audio.
- Touch button icons are rendered from Apple's SF Symbols at build time.

Grand Theft Auto and GTA2 are trademarks of Take-Two Interactive Software. This is an
unofficial fan project, not affiliated with or endorsed by Rockstar Games or Take-Two.
