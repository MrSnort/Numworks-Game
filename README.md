# Dungeon of NumWorks

A small dungeon crawler for the NumWorks calculator.

You wake up in a dark crypt with nothing but a torch. Find the three keys scattered
through the rooms, avoid the slimes that hunt you in the light, and reach the portal
to descend. Five floors stand between you and the surface.

## Gameplay

- Each floor is a randomly generated room-and-corridor dungeon.
- The torch lights only a few tiles around you. Explored ground stays dimly visible.
- Slimes wander until they spot you, then chase. Touching one costs a heart.
- Collect every key to open the exit portal, then step into it.
- Clearing a floor restores one heart and increases both the score and the number of slimes.
- Run out of hearts and the run ends.

## Controls

| Key | Action |
| --- | --- |
| Arrows | Move |
| OK | Select / retry |
| Home | Quit |

## Building

Install node.js, GCC, the embedded ARM toolchain and the nwlink package, then run:

```
make build
```

With a calculator connected:

```
make run
```

## Emulator

The game runs on the desktop too. The emulator implements the EADK calls used by the
game and opens a window with the calculator screen.

Requirements: a C++ compiler and the X11 development headers.

```
make emu
./target/emulator
```

Arrows move, Enter or Space is OK, Escape is Home (quits), Backspace is Back.
