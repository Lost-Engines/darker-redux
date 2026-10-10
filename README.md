# Darker Redux

An enhanced version of [Darker's faithful C++ reconstruction](https://github.com/Lost-Engines/darker-cpp), using the original 1995 game's assets. Redux improves presentation and repairs original-game defects; the faithful project remains the reference for original behaviour.

Explore the reverse engineering, maps and models at [Lost Engines](https://lostengines.com/darker).

## Getting started

The original game data is required. Fetch the packs, manual, map and optional music assets with:

```sh
./scripts/fetch-assets.sh
```

On Windows:

```bat
scripts\fetch-assets.bat
```

Then run `./darker`, or double-click `darker.exe` on Windows. You can also supply a local installation with `--data-dir PATH`. The Bash helper defaults to `~/.local/share/darker` (respecting `XDG_DATA_HOME`); the Windows helper defaults to the directory beside the executable. Both accept a destination directory as their first argument.

An installation in the working directory takes precedence over the per-user default. Saves use the original `DARKER.SAV` alongside the selected packs, allowing interoperability with DOSBox. Run one version at a time when sharing saves.

Read the downloaded manual and keep the city map handy: the briefings expect you to find named locations. Use `--help` for command-line options. Music defaults to `soundblaster_awe32`; `--music=roland-lapc` selects the enhanced LAPC-I arrangement described below.

For prerequisites and build commands, see the [original project's build instructions](https://github.com/Lost-Engines/darker-cpp#building-the-source-code). The same commands apply here. CI runs manually during Redux development, with an optional release step.

## Redux improvements

- **Resolution and field of view:** `--resolution 960x540` sets the software rendering resolution; `--scale` still controls window magnification. The default remains 320×240. `--fov 75` sets the full-canvas vertical field of view in degrees (15–140); `[` and `]` adjust it by five degrees during flight. The default preserves the original projection, approximately 50.2°. Wider aspect ratios show more horizontally. Sky and ground bands follow the horizon throughout wide-angle views, including steep pitch and bank angles, without the original fixed-angle cutoffs. This first pass deliberately leaves cockpit and HUD drawing at their original size and position: expect a small overlay in the top-left and the original cockpit view restriction. Use F4 to inspect the full world view. Menus and briefings retain their native resolution. Higher resolutions increase software-rendering cost.
- **Extended draw distance:** `--draw-distance N` sets the surface radius in map cells (2–182, default 64). `--fog-start N` independently sets where fog begins (default 0); full fog occurs at the draw distance. For example, `./darker --draw-distance 182 --fog-start 100`. All visible wrapped copies of buildings, actors and effects are rendered. The simulation's world size is unchanged. Large distances increase software-rendering cost; use a Release build.
- **Skimma direction indicator:** the rightmost symbol no longer copies a stray blue pixel outside its artwork. This pixel is also present in retail.
- **German mission 76:** the missing capital Ö in “Örtlichkeiten” is composed from the original O and Ü artwork, replacing the invalid font lookup and retaining the letter's normal spacing.
- **Roland opening timing:** the opening LAPC-I arrangement's drifting part starts 384 sequence ticks earlier and gains 384 ticks of silence at its loop start. Its intro now ends with the other parts, and every loop lasts 9168 ticks instead of drifting by 384 ticks per repetition. This is an inferred musical correction based on the source timing and listening comparisons, not a claim about the composer's original setup. Other arrangements and pieces retain their timing.
- **Roland missing percussion:** LAPC-I playback automatically uses `MTGM.MID` beside the game packs or ROMs to supply otherwise-unmapped rhythm notes. A separate emulated Roland device preserves Darker's own melodic instruments. The fetch helpers download Roland's conversion-bank archive and extract the bank (Bash requires `unzip`; Windows uses `tar.exe`). Without the bank, playback continues with a warning and the original missing percussion. Explicit bank and SoundFont comparison options remain available. This is the percussion arrangement selected during listening tests, not proof of the original intended instrumentation.
- **Mission 80 convoy:** staggered vehicles execute their own approach distance, then join the common route instead of accumulating the other vehicles' approach distances. This repairs the translated routes responsible for the observed building traversal. The common authored route, speed, spacing and mission logic are retained. Joining the shared route is an inferred correction to the original route data.

These repairs happen in memory. Original packs and saves are not rewritten to apply them, and the faithful engine remains unchanged.
