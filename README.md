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

Original Digger Remastered project and source:

https://www.digger.org

## What is ELKS?

ELKS is a small operating system inspired by Linux, designed for old and resource-constrained 16-bit x86 machines.

## This port

The goal of this port is simple:

> Make Digger feel like Digger, but running on ELKS.

The gameplay, timing, sounds, title screen, high-score flow, and overall feel are intended to stay close to the original Digger Remastered experience.

## Building

From the ELKS build directory:

```sh
cd elks
make
```

The build also places the title image next to the game binary so it can be found at runtime.

Useful build variants may include:

```sh
make SOUND=0
make SOUND=direct
make TITLEBMP=0
```

## Running

Copy the built game and its runtime files to your ELKS environment, then run:

```sh
digger
```

Keep `digtitle.bmp` next to the binary if you want the title screen.

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
n / N            -> preserved for title-screen player-count toggle
```

## Credits

Digger Remastered was created and maintained by Andrew Jenner and contributors.

This repository contains an ELKS port of that work.

Original project:

https://www.digger.org

## License

This project follows the GPLv2 license terms of the original Digger Remastered source.
