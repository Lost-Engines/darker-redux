# Darker Redux

An enhanced version of the Lost Engines [faithful C++ reconstruction of Darker](https://github.com/Lost-Engines/darker-cpp), using the original 1995 game's assets. Redux improves presentation and repairs original-game defects; the faithful project remains the reference for original behaviour.

Explore the reverse engineering, maps and models at [Lost Engines](https://lostengines.com/darker).

## Getting started

The original game's data files are required to run this.  Darker is considered abandonware at this point, and images of the original media are widely available: https://archive.org/details/darker-cdrom/ or https://www.myabandonware.com/game/darker-2dn#download.  If you wish to provide your own data files, just pass the `--data-dir` commandline argument.

To make things easier, there's automatic fetch script to get all necessary data (including the manual and map and sound emulation ROMs) into a default directory in your profile:
```sh
./scripts/fetch-assets.sh
```
or on windows:
```sh
scripts\fetch-assets.bat
```


Then run `./darker`, or double-click `darker.exe` on Windows. You can also supply a local installation with `--data-dir PATH`. The Bash helper defaults to `~/.local/share/darker` (respecting `XDG_DATA_HOME`); the Windows helper defaults to the directory beside the executable. Both accept a destination directory as their first argument.

An installation in the working directory takes precedence over the per-user default. Saves use the original `DARKER.SAV` alongside the selected packs, allowing interoperability with DOSBox. Run one version at a time when sharing saves.

Read the downloaded manual and keep the city map handy: the briefings expect you to find named locations. Use `--help` for command-line options. Music defaults to `soundblaster_awe32`; `--music=roland-lapc` selects the enhanced LAPC-I arrangement described below.

For prerequisites and build commands, see the [original project's build instructions](https://github.com/Lost-Engines/darker-cpp#building-the-source-code). The same commands apply here.

For optimised developer playtesting with debug symbols, assertions and developer cheats:

```sh
cmake -S . -B build-relwithdebinfo -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-relwithdebinfo --parallel
./build-relwithdebinfo/darker --cheats-dev
```

Redux's `RelWithDebInfo` deliberately keeps `NDEBUG` undefined in project code.
Release builds still omit developer cheats; Debug remains unoptimised for stepping
through code. With a multi-configuration generator, add `--config RelWithDebInfo`
to the build command and run the executable from that configuration's directory.


## Redux improvements

### In-game menu

Press **F10** from the menus, briefings or flight to open the options. The pilot-selection and run menus also have an **Options** link. The scene pauses while you adjust resolution, vertical FOV, draw distance, fog, distant fading, minimum light size and Gouraud shading. Changes appear immediately; hold **Tab** to hide the panel and inspect the scene.

Open **Controls** to rebind flight, weapon and camera keys, adjust mouse sensitivity, or invert vertical mouse movement. Select a binding and press **Enter**, then the new key; **Delete** clears it and **Escape** cancels. Conflicting keys are flagged. Use the mouse wheel or **Page Up/Page Down** to scroll. Choose **Back to options**, then **Apply and save** to keep your changes. The mouse fire-button settings select left, right, middle or extra buttons, or disable mouse firing; swapping an occupied button swaps the two actions. Menus always use the left button.

**Controls → Joystick / gamepad** selects a device (Auto uses the first connected device), bank/pitch axes, inversion, sensitivity and dead zone. Axis numbers start at 1; select an axis row to see its live reading. Select a flight action and press Enter, then the desired joystick button; Delete clears it. Recognised gamepads use a standard layout, including triggers as buttons 16/17. Other joysticks use their reported axis/button numbers. Defaults use the first two axes and buttons 1/2 for firing, 3 for boost, 4 for engine/shield, 5 for brake, 6 for look-around and 7 for radar. In menus, the pitch axis moves the selection, the bank axis adjusts options, and primary/secondary fire accept/go back. Use F10 to open settings, and **Apply and save** to retain them.

Joystick setup is also available entirely from the command line. Explicit arguments override saved settings; omitted options retain them. For example:

```sh
./darker --joystick=1 --joystick-pitch-axis=2 --joystick-invert-pitch --joystick-deadzone=6.25
./darker --joystick=off
./darker --joystick-button=primary=2 --joystick-button=secondary=1
```

Use `--help` for axis, sensitivity and button options. With no saved settings, the first available joystick is selected automatically and the original 6.25% centre dead zone applies. `--joystick-deadzone` changes that threshold directly (0 disables it); no calibration is required.

**Sound** provides separate music and effects volumes, applied immediately across all music engines. Zero mutes that channel; **Apply and save** retains the settings. Command-line `--mute` and `--music=none` still disable their respective audio paths.

Preferences live in `$XDG_CONFIG_HOME/darker-redux/options.cfg`, or `~/.config/darker-redux/options.cfg` when unset; Windows uses `%APPDATA%/darker-redux/options.cfg`. These are separate from the original game's saves. Explicit command-line options override saved preferences. Delete the file or apply **Restore defaults** to return to defaults. Menu text for these new Redux options is currently English.

### Visual enhancements

- **Transparent enlarged radar:** hold Insert or keypad 0 to overlay the radar with a half-opacity backdrop. Contacts, heading and coordinates remain opaque, including in F4 and external views.
- **Falling wrecks:** fatally damaged aircraft remain visible until they hit terrain or a building, instead of fading away when the original damage timer expires. Their original falling motion, smoke and impact effects are retained. Cleanup and mission kill credit now wait for the impact and its short cleanup timer; scripted departures retain their original expiry behaviour.
- **Distant smoke and fire:** particle sprites shrink with projected size, becoming a single actual render pixel only when sufficiently distant for the current resolution and field of view.
- **Finer rendering angles:** camera and model rotations, sky bands, following-camera offsets and visual targeting use all 65,536 angle positions per turn instead of the original 1,024 sine phases (approximately 0.0055° rather than 0.352°). A shared 128 KiB table is generated once from mathematical sine, rounded to signed 16-bit values with symmetric ±32767 peaks; rendering uses direct lookups. The original table truncates almost exactly `32767 × sin(angle)`, except its positive peak is 32766 rather than 32767. Redux removes that one-unit asymmetry from rendering and stops clearing the camera’s low six angle bits. Flight dynamics retain the original sine table and fixed-point arithmetic; matrix, position and simulation-timing precision are otherwise unchanged.
- **Gouraud shading:** enabled at every mesh distance unless disabled with F9, removing the original distance-based switch to flat shading. Authored flat polygons remain flat. Moving models remain meshes until their projected bounding diameter is at most one render pixel, replacing the original fixed-distance dot cutoffs. This accounts for resolution and FOV; dots use their own lighting and fog rather than borrowing the previous model’s shading.
- **Fountain lights:** white fountain lamps use the beacons’ luminous shading so they remain bright at a distance.
- **Resolution and field of view:** `--resolution 1600x1200` sets the software rendering resolution; `--scale` controls window magnification, pixel size multiples. The default remains 320×240. `--fov 75` sets the full-canvas vertical field of view in degrees (15–140); `[` and `]` adjust it by five degrees during flight. The default preserves the original projection, approximately 50.2°. Wider aspect ratios show more horizontally.
- Sky and ground bands follow the horizon throughout wide-angle views, including steep pitch and bank angles, without the original fixed-angle cutoffs.
- **Extended draw distance and fog:** `--draw-distance N` sets the surface radius in map cells (2–182, default **90**; original radius 15). `--fog-start N` sets where fog begins (default **2**); **`--fade-distance N` (default 32)** is where Delphi reaches the original palette's full dark fog. Only from there to `--draw-distance` do those dark silhouettes fade linearly into the background. The default fade distance is capped to the draw distance when using shorter views; explicit values must be between fog start and draw distance. Equal fade and draw distances disable the final blend. `--fog-curve N` controls logarithmic build-up (default **10**): 0 is linear, larger values thicken nearby fog. At one tenth of the fog range, curves 0, 10 and 100 give approximately 10%, 29% and 52% fog before palette quantisation. Delphi retains its original colours and darkening until the final fade towards the dark ground/background colour; `--fog-target original` disables that final blend. Halon retains its authored palette fog and tunnels retain their original lighting. Both stages preserve beacon lighting before blending and quantise colours to the existing palette; Gouraud interpolation happens before fog, avoiding interpolation through unrelated palette ramps.
- **Distant lights:** `--minimum-light-size N` sets a minimum city light disc diameter in 320×200 reference pixels (default **0.5**, **0** disables). It scales with rendering height, independently of FOV and window magnification, and rounds to whole pixels. Streetlights, beacon lights and fountain lights/reflections share this minimum; projectile effects are unchanged. Powered streetlight discs dim with their regional beacon and disappear at zero power, rather than retaining the original special-colour ramp’s residual glow. Fixed-colour fountain lights and reflections keep their authored lighting. Existing fog still applies. The disc rasteriser caps radii at 256 render pixels.

### Gameplay enhancements
- **Nightmare reinforcements:** restored the original authored spawn positions and fade-in, avoiding overlapping arrivals that immediately collide. This correction is also backported to the faithful engine.
- **Nightmare steering:** removed the scripted height-512 altitude hold during the late truck hunt. Pitch remains under player control; the A-key altitude hold is still available. The beacon patterns, enemies and objective progression are unchanged.
- 
### Bug fixes

- **Skimma direction indicator:** the rightmost symbol no longer copies a stray blue pixel outside its artwork. This pixel is also present in retail.
- **Roland opening timing:** the opening LAPC-I arrangement's drifting part starts 384 sequence ticks earlier and gains 384 ticks of silence at its loop start. Its intro now ends with the other parts, and every loop lasts 9168 ticks instead of drifting by 384 ticks per repetition. This is an inferred musical correction based on the source timing and listening comparisons, not a claim about the composer's original setup. Other arrangements and pieces retain their timing.
- **Roland missing percussion:** LAPC-I playback automatically uses `MTGM.MID` beside the game packs or ROMs to supply otherwise-unmapped rhythm notes. A separate emulated Roland device preserves Darker's own melodic instruments. The fetch helpers download Roland's conversion-bank archive and extract the bank (Bash requires `unzip`; Windows uses `tar.exe`). Without the bank, playback continues with a warning and the original missing percussion. Explicit bank and SoundFont comparison options remain available. This is the percussion arrangement selected during listening tests, not proof of the original intended instrumentation.
- **Mission 80 convoy:** staggered vehicles execute their own approach distance, then join the common route instead of accumulating the other vehicles' approach distances. This repairs the translated routes responsible for the observed building traversal. The common authored route, speed, spacing and mission logic are retained. Joining the shared route is an inferred correction to the original route data.
- **German mission 76:** the missing capital Ö in “Örtlichkeiten” is composed from the original O and Ü artwork, replacing the invalid font lookup and retaining the letter's normal spacing.

All of these repairs happen in memory. Original packs and saves are not rewritten to apply them, and the alternative [faithful engine](https://github.com/Lost-Engines/darker-cpp) remains unchanged.

### Developer additions

- **Developer cheats (Debug / RelWithDebInfo):** launch with `--cheats-dev`, then press **K** during flight to eliminate active aircraft and ground/static mission targets. Normal objective credit and cleanup run on the next simulation update; reserve waves and uncounted ground escorts are preserved. This switch and key handler are absent from Release builds.
