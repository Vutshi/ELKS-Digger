# Digger for ELKS

A tiny underground classic, now at home on [**ELKS**](https://github.com/ghaerr/elks).

This project brings **Digger Remastered** to the ELKS operating system, preserving the familiar arcade rhythm of digging tunnels, dodging Nobbins, dropping bags, collecting emeralds, and chasing that next high score.

The original Digger already came from the 16-bit DOS world. The new part here is the operating system: this port adapts the game to run under **ELKS**, the Linux-like OS for very small 8086-class machines.

## What is Digger?

Digger is a fast, charming maze-action game with dirt, gems, monsters, bags of gold, bonus cherries, and unforgettable PC speaker music.

You dig.  
They chase.  
Bags fall.  
Everything gets chaotic very quickly.

## What is ELKS?

ELKS is a small operating system inspired by Linux, designed for old and resource-constrained 16-bit x86 machines.

## This port

The goal of this port is simple:

> Make Digger feel like Digger, but running on ELKS.

The gameplay, timing, sounds, title screen, high-score flow, and overall feel are intended to stay close to the original Digger Remastered experience.

## Building

ELKS  `TOPDIR=/path/to/elks` should be specified. 
Build from the `elks/` directory:

```sh
make
````

Options:

* `KEYBOARD=kraw` — default; raw scancode keyboard input with key press/release events.
* `KEYBOARD=term` — compatibility terminal input backend.
* `SOUND=seq` — default; ELKS kernel sequencer PC speaker sound.
* `SOUND=direct` — older small direct PC speaker backend.
* `SOUND=dos` — original DOS Digger sound logic using the direct PC speaker backend.
* `SOUND=none` — build without sound.
* `TITLEBMP=1/0` — include/omit title bitmap.

Run `make clean` when switching options.


## Running

Copy the built game and its runtime files to your ELKS environment, then run:

```sh
digger
```

Keep `digtitle.bmp` in `/lib` folder.

## Input mapping

```text
Arrow keys       -> movement
WASD             -> movement
Space            -> fire
Ctrl-Space/NUL   -> fire fallback
Ctrl-A           -> fire fallback
p / P            -> pause
q / Q            -> quit
Esc              -> quit
Ctrl-C           -> quit in raw mode
```

## Credits

Digger Remastered was created and maintained by Andrew Jenner and contributors.

This repository contains an ELKS port of that work.

Original project:

https://www.digger.org

## License

This project follows the GPLv2 license terms of the original Digger Remastered source.
