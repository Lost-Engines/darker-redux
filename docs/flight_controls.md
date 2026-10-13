# Flight steering controls

`flight_controls_state::update` reconstructs the ordinary keyboard/mouse path at
`7AD6–7BC4` and its response helpers `7C04`, `7C50` and `7C73`. It produces the
bank and pitch drives consumed by the flight callbacks. Platform event capture,
joystick input is now integrated in Redux; VR orientation remains separate.

Each axis retains separate keyboard and mouse targets, the previous mouse
counter and the published midpoint reference. Arrow keys initially select a
force of 3510; Ctrl changes the retained force with the original frame step.
Opposite arrows with Ctrl reduce it towards zero. Opposite arrows without Ctrl
retain the stored target but filter towards zero for that update. Releasing
keys clears their target through the same midpoint filter.

Nonzero keyboard drive takes precedence over mouse processing. Mouse history
therefore remains pending while keyboard steering is active. Mouse input uses
wrapping accumulated counters, an inverted vertical counter and the original
sensitivity (default 12). Its decaying target preserves the asymmetric rounding
for negative values. Both sources publish a signed midpoint multiplied by the
frame step and shifted by eight; no floating-point smoothing is substituted.

`tools/generate_flight_controls_reference.py WORKSPACE` captures 1,024 updates
from the executable, covering held-key combinations, Ctrl, release, keyboard
handover, counter wrapping and sensitivities 1/12/25. Tests compare both output
drives and all eight persistent words. The fixture selects ordinary mouse mode
and suppresses the separate joystick fallback through its native input flags.

The application now supplies GLFW relative mouse counters and held arrow/Ctrl
keys to this filter. The former free-camera controls have been removed. Host
mouse sensitivity still needs comparison with the original DOS input path.

## Underground large mouse sweeps

A reported difference remains under investigation: large captured-mouse sweeps
can pull the reconstruction into a tunnel wall more readily than retail under
DOSBox, particularly when travelling beyond where an uncaptured cursor would
have reached the window edge. No speculative steering-strength reduction has
been applied.

The tunnel fixture now has 3,072 native D510/D5C9 updates. The additional 1,024
feed sustained horizontal or vertical sweeps, holds and reversals through native
7AD6 before the tunnel callback, over frame steps 1/2/8/16. All reconstructed
motion fields match. This exercises D86B's 3072 steering-demand cap and its
asymmetric approach rate, rather than only the earlier ±2048 synthetic inputs.
It verifies controller arithmetic for supplied counters, not equivalence between
physical mouse motion on two host input stacks.

Inspection of the unmodified DOSBox 0.74-3 source archive
(`https://deb.debian.org/debian/pool/main/d/dosbox/dosbox_0.74-3.orig.tar.gz`)
shows another possible source of differences. `src/gui/sdlmain.cpp` passes
captured relative movement through DOSBox sensitivity; `src/ints/mouse.cpp`
applies its own sensitivity and mickey conversion. Its PS/2 path bypasses cursor
bounds, but `DoPS2Callback` reduces accumulated differences modulo 256 and marks
overflow. Darker's 1259 callback accumulates the delivered byte and sign without
recovering overflow distance. The reconstruction instead uses GLFW raw motion
where available. Packet loss/wrapping and host acceleration are therefore
plausible explanations for fast-sweep differences, not evidence of a
window-edge clamp in Darker's tunnel controller. Which input path and delivered
counts explain the observed playtest still needs a matched live trace. A slow
and fast sweep over the same physical distance would help distinguish speed-
dependent packet delivery from a position limit.

## Redux joystick input

Redux polls GLFW joysticks on the main thread. Recognised gamepads use GLFW's
standard mapping; other joysticks expose raw axes and buttons (including hats
reported by GLFW as buttons). Standard gamepad triggers become buttons 16/17
at half travel. The default left stick supplies bank/pitch, with positive Y
pulling the nose up. Device indices and axis/button numbers are displayed
one-based; saved indices are zero-based. Auto chooses the first present device.
Explicit device slots are retained when disconnected, rather than silently
switching to another controller. Slots may change if devices are reordered.

The controls submenu saves device selection, axes, inversion, extra dead zone,
sensitivity and action bindings. Missing axes/buttons produce neutral input.
The device name and live axis readings aid setup. GLFW supplies normalised axes,
so there is no emulated DOS gameport calibration procedure. The optional host
dead zone (default 5%) precedes the original response's own centre dead zone.

The retail code at 7B7D–7BC4 enters its joystick fallback only when preceding
keyboard/mouse steering supplies no drive. 7BCA averages current and previous
axis readings, removes the inner 1/16 of calibrated travel and saturates at
0x0f00; 7BA6/7BBD scale the signed result by the simulation timestep. Redux
normalises host axes to ±32767 and uses that as the calibrated extent. The
native-reference generator captures 256 complete fallback updates, including
signed wrapping and asymmetric rounding. Keyboard/mouse fixtures remain intact.

Physical button edges and held states are distinct. Button actions share the
existing flight command paths; brake, firing, radar and look-around remain
held actions. Connection/focus changes and menu transitions consume held buttons
until release. Button capture suppresses flight/menu commands, and pausing can
be cancelled by a new button press without executing its assigned action.
Joystick input is disabled while the window is unfocused. These behaviours
have automated coverage; physical-device playtesting is still pending.

The retail manual lists joystick steering on page 17 and configurable joystick
A/B buttons on page 22. The 1995 demo additionally documents automatic
calibration, J to recalibrate, and fire buttons for menu acceptance.

This implementation is currently Redux-only. Backport the verified analogue
input path to darker-cpp after interactive testing; the faithful project still
has no host joystick polling.
