# Darker Redux

An enhanced version of the Lost Engines [faithful C++ reconstruction of Darker](https://github.com/Lost-Engines/darker-cpp), using the original 1995 game's assets. Redux improves presentation and repairs original-game defects; the faithful project remains the reference for original behaviour.

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

## Display options

Press **F10** from the menus, briefings or flight to open the display options. The pilot-selection and run menus also have an **Options** link. The scene pauses while you adjust resolution, vertical FOV, draw distance, fog, distant fading, minimum light size and smooth shading. Changes appear immediately; hold **Tab** to hide the panel and inspect the scene.

Use **Up/Down** to select a row, **Left/Right** to adjust it, and **Enter** to activate it. The mouse selects rows and the `<` / `>` controls adjust values. **Escape** cancels and restores the opening settings. **Apply and save** keeps your choices for future sessions; **Return without saving** keeps them for this session only. **Restore defaults** previews the recommended Redux settings and can still be cancelled. Changing render resolution leaves the window size unchanged. The scene remains paused if it was paused before opening options.

Preferences live in `$XDG_CONFIG_HOME/darker-redux/options.cfg`, or `~/.config/darker-redux/options.cfg` when unset; Windows uses `%APPDATA%/darker-redux/options.cfg`. These are separate from the original game's saves. Explicit command-line options override saved preferences. Delete the file or apply **Restore defaults** to return to defaults. Menu text for these new Redux options is currently English.

## Redux improvements

- **Resolution and field of view:** `--resolution 960x540` sets the software rendering resolution; `--scale` still controls window magnification. The default remains 320×240. `--fov 75` sets the full-canvas vertical field of view in degrees (15–140); `[` and `]` adjust it by five degrees during flight. The default preserves the original projection, approximately 50.2°. Wider aspect ratios show more horizontally. Sky and ground bands follow the horizon throughout wide-angle views, including steep pitch and bank angles, without the original fixed-angle cutoffs. Cockpits and instruments use uniform nearest-neighbour scaling based on display height, centred horizontally over the lower black band. Wider screens retain black margins beside the panel; narrower screens crop its sides. F4 removes the cockpit and bands for the full world view, retaining a centred HUD. Menus and briefings retain their native resolution. Higher resolutions increase software-rendering cost.
- **Extended draw distance and fog:** `--draw-distance N` sets the surface radius in map cells (2–182, default **90**; original radius 15). `--fog-start N` sets where fog begins (default **2**); **`--fade-distance N` (default 32)** is where Delphi reaches the original palette's full dark fog. Only from there to `--draw-distance` do those dark silhouettes fade linearly into the background. The default fade distance is capped to the draw distance when using shorter views; explicit values must be between fog start and draw distance. Equal fade and draw distances disable the final blend. `--fog-curve N` controls logarithmic build-up (default **10**): 0 is linear, larger values thicken nearby fog. At one tenth of the fog range, curves 0, 10 and 100 give approximately 10%, 29% and 52% fog before palette quantisation. Delphi retains its original colours and darkening until the final fade towards the dark ground/background colour; `--fog-target original` disables that final blend. Halon retains its authored palette fog and tunnels retain their original lighting. Both stages preserve beacon lighting before blending and quantise colours to the existing palette; Gouraud interpolation happens before fog, avoiding interpolation through unrelated palette ramps. This is a uniform background colour, not yet volumetric or matched to individual sky bands. All visible wrapped copies of buildings, actors and effects are rendered; the simulation's world size is unchanged. Large distances increase software-rendering cost; use a Release build.
- **Finer rendering angles:** camera and model rotations, sky bands, following-camera offsets and visual targeting use all 65,536 angle positions per turn instead of the original 1,024 sine phases (approximately 0.0055° rather than 0.352°). A shared 128 KiB table is generated once from mathematical sine, rounded to signed 16-bit values with symmetric ±32767 peaks; rendering uses direct lookups. The original table truncates almost exactly `32767 × sin(angle)`, except its positive peak is 32766 rather than 32767. Redux removes that one-unit asymmetry from rendering and stops clearing the camera’s low six angle bits. Flight dynamics retain the original sine table and fixed-point arithmetic; matrix, position and simulation-timing precision are otherwise unchanged.
- **Gouraud shading:** enabled at every mesh distance unless disabled with F9, removing the original distance-based switch to flat shading. Authored flat polygons remain flat. Moving models remain meshes until their projected bounding diameter is at most one render pixel, replacing the original fixed-distance dot cutoffs. This accounts for resolution and FOV; dots use their own lighting and fog rather than borrowing the previous model’s shading.
- **Distant lights:** `--minimum-light-size N` sets a minimum city light disc diameter in 320×200 reference pixels (default **0.5**, **0** disables). It scales with rendering height, independently of FOV and window magnification, and rounds to whole pixels. Streetlights, beacon lights and fountain lights/reflections share this minimum; projectile effects are unchanged. Existing lighting and fog still apply. The disc rasteriser caps radii at 256 render pixels.
- **Skimma direction indicator:** the rightmost symbol no longer copies a stray blue pixel outside its artwork. This pixel is also present in retail.
- **German mission 76:** the missing capital Ö in “Örtlichkeiten” is composed from the original O and Ü artwork, replacing the invalid font lookup and retaining the letter's normal spacing.
- **Roland opening timing:** the opening LAPC-I arrangement's drifting part starts 384 sequence ticks earlier and gains 384 ticks of silence at its loop start. Its intro now ends with the other parts, and every loop lasts 9168 ticks instead of drifting by 384 ticks per repetition. This is an inferred musical correction based on the source timing and listening comparisons, not a claim about the composer's original setup. Other arrangements and pieces retain their timing.
- **Roland missing percussion:** LAPC-I playback automatically uses `MTGM.MID` beside the game packs or ROMs to supply otherwise-unmapped rhythm notes. A separate emulated Roland device preserves Darker's own melodic instruments. The fetch helpers download Roland's conversion-bank archive and extract the bank (Bash requires `unzip`; Windows uses `tar.exe`). Without the bank, playback continues with a warning and the original missing percussion. Explicit bank and SoundFont comparison options remain available. This is the percussion arrangement selected during listening tests, not proof of the original intended instrumentation.
- **Mission 80 convoy:** staggered vehicles execute their own approach distance, then join the common route instead of accumulating the other vehicles' approach distances. This repairs the translated routes responsible for the observed building traversal. The common authored route, speed, spacing and mission logic are retained. Joining the shared route is an inferred correction to the original route data.

These repairs happen in memory. Original packs and saves are not rewritten to apply them, and the faithful engine remains unchanged.
