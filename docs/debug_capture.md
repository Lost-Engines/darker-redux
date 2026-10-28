# Reproducible gameplay screenshots

Debug and RelWithDebInfo builds accept `--capture-plan plan.json` with `--level`.
The normal mission setup, scripts, AI, weapons and software renderer run. The
capture loop advances eight native timer ticks at a time without waiting for
wall time or rendering intermediate frames. It automatically advances briefings,
opens a hidden window, writes PPM images and state JSON, and exits after the last
shot. A working GLFW display is still required (Xvfb with Mesa works on Linux).
Release builds contain neither the option nor its implementation.

```sh
./build-relwithdebinfo/darker --level=1 --skip-intro --mute --music=none \
  --no-mouse --joystick=off --scale=1 --resolution=3840x2160 \
  --capture-plan shots.json
```

No saves are written, as with ordinary `--level` entry. No enemy is moved or
created, no effects are fabricated, and no objective is completed by this tool.
Explicit player positioning, freezing and refilling are deliberate debug setup;
retain the plan alongside published captures and describe these as staged native
gameplay screenshots, not an uninterrupted playthrough.

A small plan (output paths are relative to the plan file):

```json
{
  "version": 1,
  "fov": 50,
  "events": [{"tick": 0, "freeze": true}],
  "shots": [
    {"tick": 0, "file": "initial.ppm", "state_only": true},
    {
      "tick": 3000,
      "file": "enemy.ppm",
      "focus": {"actor": 1},
      "offset": [0.7, 1.0, 600],
      "roll": -10
    }
  ]
}
```

Ticks count from flight entry, at approximately 500 per second. Times must be
sorted multiples of eight, at most one hour; multiple shots can share a tick.
Events at that tick are applied before shots. A trigger pressed at tick 1000
first launches on the next simulation step, so capture it at tick 1008 or later.
There is no simulation between multiple cameras at one tick.

Positions and offsets use `[map column in cells, map row in cells, native
altitude]`. One horizontal cell is 256 position units; vertical geometry is
scaled by eight (2048 height units per equivalent cell). Angles are degrees.
Heading zero faces decreasing rows, 90 faces decreasing columns; positive pitch
looks upwards. Positions retain the engine's native wrapping domain; aim uses
the shortest wrapped displacement. Keep tunnel cameras inside the actual passage.

Events support:

- `position`: relocate the player and release its landing protection.
- `angles`: player heading, pitch and roll; or `aim` using the focus syntax below.
- `roll`: bank used with `aim`.
- `freeze`: hold/release the player's movement while the world continues.
- `refill`: fill native energy capacities or Skimma ammunition without unlocking weapons.
- `weapon`: one-based original weapon selection. A weapon unavailable in this
  mission is rejected; paired automatic selections still follow native firing rules.
- `primary`, `secondary`: held fire state, retained until the next event changes it.
  Chargeable uses secondary hold/release. Ordinary shots require a new press.

Each shot requires `tick` and `file`. `state_only: true` writes just the JSON
snapshot (including active actors and live projectiles), useful for planning.
Otherwise, `cockpit: true` uses the player's current cockpit view. External shots
use `focus` and `offset`, with optional absolute camera `position` and `roll`.
Focus is exactly one of `{"player": true}`, `{"actor": 1}`,
`{"point": [column, row, height]}`, or `{"projectile": "player"}` / `"hostile"`
(the newest live projectile). Missing actors/projectiles fail rather than
silently showing another subject. External cameras omit HUD overlays.

A `name.ppm.json` sidecar records the actual tick, camera/player poses, actor
identities, projectile records, marked ground cells and weapon selections.
The caller should additionally record the executable hash, source revision,
command line and plan hash. The fixed-step result is reproducible with those
inputs; it is not a promise to match an interactive play session's timing.

The camera does not redirect weapon targeting. Weapon aim follows the player's
attitude even while the camera observes a different actor or point. Render-only
settings (resolution, fog, draw distance) still use the normal command line;
use explicit values and a separate `XDG_CONFIG_HOME` when capturing independently
of saved preferences.
