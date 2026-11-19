# PINTBALL

3D Pinball for Windows - Space Cadet for [PINT 4.0](https://github.com/tenox7/pint),
Windows NT 4.0 on ARM (Raspberry Pi 2, 3 and 4).

PINTBALL is a port of [SpaceCadetPinball](https://github.com/k4zmu2a/SpaceCadetPinball),
the reverse engineered source code of the game, to plain Win32:

- GDI drawing
- waveOut sound effects
- MIDI music through the MCI sequencer
- Win32 menus and dialogs

It does not use SDL2 or Dear ImGui.

## How to play

1. Get the ZIP file from [Releases](https://github.com/tenox7/PINTBALL/releases).
2. Unzip it into a folder on the PINT machine or on a network share.
3. Start `PINTBALL.EXE`.

The game needs the data files of 3D Pinball for Windows: `PINBALL.DAT`, `PINBALL.MID`,
the `SOUND*.WAV` files and the others. Windows NT 4.0, 2000 and XP keep them in
`Program Files\Windows NT\Pinball`. PINTBALL looks for them in these folders:

- the current folder
- the folder of `PINTBALL.EXE`
- `Program Files\Windows NT\Pinball`

The data of Full Tilt! Pinball (`CADET.DAT`) also works.

## Keys

| Action                          | Key              | Mouse         |
| ------------------------------- | ---------------- | ------------- |
| Left flipper                    | Z                | Left button   |
| Right flipper                   | /                | Right button  |
| Plunger                         | Space            | Middle button |
| Table bump: left, right, bottom | X, `.`, Up arrow |               |
| New game                        | F2               |               |
| Pause                           | F3               |               |
| Full screen                     | F4               |               |
| Sound on or off                 | F5               |               |
| Music on or off                 | F6               |               |
| Controls dialog                 | F8               |               |
| Menu bar on or off              | F9               |               |
| Exit                            | Esc              |               |

Push F8 to change the keys. When the menu bar is hidden, the system menu (Alt+Space)
has "Show Menu".

## Options

PINTBALL keeps its options and high scores in `PINTBALL.INI`, in the folder of
`PINTBALL.EXE`. It writes this file when it closes.

| Command line option | Effect                             |
| ------------------- | ---------------------------------- |
| `-fullscreen`       | Start in full screen               |
| `-demo`             | Start the demo                     |
| `-noaudio`          | No sound effects and no music      |
| `-reset`            | Set all options to their defaults  |

## Sound and music

- Sound effects use waveOut, 16-bit stereo at 22050 Hz.
- Music uses the MCI sequencer, so it needs a MIDI device, for example the FM synth of PINT.
- Music is off at the first start. Push F6 to turn it on.
- When PINTBALL finds no sound device, it turns the sound off and keeps this in
  `PINTBALL.INI`. Push F5 to turn it on again.

## Build

You need the PINT 4.0 SDK. In the `SpaceCadetPinball` folder, run:

    nmake -f pint.mak

The `tenox7/pint40sdk` Docker image can also build it:

    docker run --rm -v <MSVC for ARM folder>:/opt/msvc -v "$PWD":/src tenox7/pint40sdk nmake -f pint.mak

`pint.mak` builds only the Win32 sources. The SDL2, Dear ImGui and CMake files of
SpaceCadetPinball are still in the tree, but PINTBALL does not use them.

## Not in PINTBALL

- Game controllers
- The debug overlay, frame time graph, sprite viewer and font selection (these used Dear ImGui)
- Linear filtering
- Mouse buttons 4 and 5 (NT 4.0 has no messages for them)
- A change of the display mode: full screen is a window without a border, at the size
  of the desktop

## Credits

- Original game: Cinematronics and Microsoft
- SpaceCadetPinball: Andrey Muzychenko (k4zmu2a) and contributors,
  <https://github.com/k4zmu2a/SpaceCadetPinball>
- PINT 4.0: <https://github.com/tenox7/pint>

## License

The source code is under the MIT license, see [LICENSE](LICENSE).
The game data files are not part of this license.
