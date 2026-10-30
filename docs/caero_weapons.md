# Caero weapon firing and targeting

`fire_caero_weapon` composes native C9C2 with the CB01 projectile allocator for
Pinner Direct, Pinner Mimic, Dual Launch, Brent Ground, Chargeable and Brent Hunter. Surface cost is the
definition's first role byte times 256, plus 255: Direct costs 6,399 units and
Mimic 7,423. Underground mode overrides that cost with **80FFh (33,023)**.
Lifetime is the next definition byte times 256. Boost and engine buffer are
separate from this weapon reserve.

The guards reject protected/crashing players, exhausted projectile pools and
insufficient reserve. Brent Hunter requires a negative object token other than
FFFF. Brent Ground ordinarily requires a map-cell token; Redux also permits
unguided shots without a target in Delphi. A valid weapon remains ready
without a trigger edge. Ordinary trigger-edge weapons do not repeat while held; the Dual Launch follow-up has the release-edge condition described below.
Primary and secondary readiness now drive their original cockpit colours.

## Acquisition and retention

1E77/6D08 constructs a fixed-point forward ray from player heading and pitch,
clips it against terrain/city geometry, then checks the airborne list in native
order. CF94 retains an existing target rather than tracing again every frame.
It resolves the target bounds, checks wrapped world distance, transforms with
the current view basis and applies the original 55-unit rectangular and 2,916
squared-radius limits. Negative projected coordinates use one's-complement
magnitude, preserving the original asymmetry.

Object-only and building-only weapons reject the other target class. Caero
building locks additionally reject energy towers and require state bit 40.
The target marker follows the projected coordinates, switches size at native
radial bucket two and uses secondary readiness colours. Camera rendering and
lock projection share the same fixed-point basis construction. Caps Lock clears
the selection for reacquisition.

Hunter projectiles resolve their retained object token on every update. On
removal, 7A52's reference repair makes dependent missiles self-targeting and
clears the selected lock before a reusable aircraft record can launch again.
The existing CC61 guidance then continues from the missile's own attitude.

## Controls and connected campaign

1 and 2 select the unlocked Pinners; Space or left mouse fires the primary.
Mission 21 introduces Brent Hunter: **0 selects it; Alt or right mouse fires**.
Mission 50 introduces Brent Ground on key 6; mission 80 introduces the Forbes
Diffuser on key 5. Both are connected to projectile flight, building collisions
and campaign objectives.

## Verification

- 1,640 native firing cases cover energy boundaries, trigger edges, target kinds,
  pool exhaustion, protected states and the underground cost override.
- 1,024 native CF4A cases compare retained/rejected locks and partial coordinate
  writes; 256 native ray cases compare fixed-point endpoints.
- 256 complete native 6D08 cases compare acquisition against original Delphi
  building geometry and two aircraft, including occlusion and list order.
- Original-pack mission 21 completes eight counted removals using Hunters,
  its messages and docking, with controlled player aim/position and charging.
  A separate retirement check verifies missile-reference repair before reuse.

These comparisons do not establish an uncontrolled retail-equivalent playthrough
or exact camera/input ordering in every exterior view. The live synchronised
DOSBox comparison tool remains deferred.

## Dual Launch: press for the capsule, release for the follow-up

Mission 57 unlocks selection 3 (number key 3) and its automatic selection 7.
CA4A launches the primary capsule on primary-fire press and changes selection
to 7. CA99 launches its homing follow-up on **primary-fire release**, provided
there is enough reserve energy and the oldest active player projectile still
has capsule definition 1956h. A missing or different oldest projectile resets
selection to 3. A successful follow-up targets the capsule and also resets to 3.
Holding fire does not launch the follow-up. If release occurs before sufficient
energy is available, another press/release is needed; it is not a queued shot.

The previous apparent default-mask defect was an incomplete probe, not a retail
bug. Every frame, `3D7C–3D87` saves the current held-input mask, computes
`previous & ~current`, and writes that released-input mask into the immediate
operand at `CAAC`. CA99's `TEST DX,0080h` in the unpacked image is therefore
self-modifying code: `0080h` is replaced before gameplay firing. DX still carries
the configured primary-action mask, so this tests release of primary fire.
The reconstruction now supplies that release event from keyboard or mouse input.
Retail mission-57 playtesting confirmed a working follow-up, motivating this
correction; it did not by itself establish the precise input edge.

When admitted, CBE7 homes on the capsule, with separation-dependent speed and
shared sound pitch. At a separation metric below 20 it makes three blast passes:
aircraft use a larger horizontal window and squared-distance threshold than
ground and stationary records. CD13 supplies distance-dependent strength to
the ordinary object-impact handler. Both projectiles enter the native 256-tick
retirement path, and recipe 71E8h appears at the capsule. A removed target causes
self-guided straight flight through the existing target-release path.

Native comparison covers 1,024 firing cases including the native per-frame
release-mask patch and full dispatcher (default and alternative action masks), 1,024 separation cases, 2,048 category/strength cases
and 2,048 complete movement updates. Separation preserves the unsigned
vertical ordering and horizontal one's-complement quirk. A controlled scene
also exercises the three blast categories, retained distant object, counted
removals, effects and both projectile deadlines. A further combat check presses, holds for 512 ticks and releases primary fire,
verifying the selection changes, single follow-up launch and capsule target.

## Forbes Diffuser

Key 5 selects the gas capsule (selection 5, definition slot 4); selection 4,
slot 3 is the detonator. There is no separate key 4 binding. Redux changes the
original alternating-shot controls:

- Firing gas, missing, or failing to fire leaves gas selected. A valid building
  impact selects the detonator, provided the Diffuser is still selected.
- Firing the detonator leaves it selected after a miss, an early impact or a
  blocked firing attempt. Successful detonation or gas expiry returns to gas.
- Pressing 5 selects gas again, even while a building is smoking. Another gas hit on
  that same active building keeps its deadline and existing smoke unchanged, then
  reselects the detonator. It does not emit another smoke sequence or impact effect.
- Selecting another secondary weapon is respected: a delayed impact or gas
  timeout does not switch back to the Diffuser.

Redux extends CE84's warehouse-only admission to destructible Delphi buildings,
including unmarked ones under the [collateral damage rules](collateral_damage.md).
Both stages require a nonzero damage link in the currently selected model.
Collision categories 0 (ordinary ground target), 2 (fragile) and 3 (Diffuser
warehouse) accept the sequence; category 1 remains protected, so office walls
and protected hangar surfaces still reject it. Beacons, scenery without a damage
model and terminal ruins cannot be primed or detonated. Warehouse roofs retain
their special resistance to ordinary ground weapons. Outside Delphi the Diffuser
retains its permission check. Without a permitted lock, a Delphi shot uses the
direct projectile update instead of resolving a nonexistent homing target.

Gas records one shared target cell and a deadline 2800h ticks ahead, emitting
recipe 75D4h at the building origin. A trigger must hit
that cell in the final 1000h ticks: 6,144–10,239 ticks after gas impact, about
12.29–20.48 seconds. An early trigger does not consume the gas. Success uses
ordinary building destruction; a rejected building hit emits 75A3h at the
projectile. Gas on a different valid building replaces the shared target and timer;
object and terrain impacts retain their ordinary paths.

An explicit active flag is retired at expiry or detonation, so an old deadline
cannot re-arm itself on the next 16-bit clock wrap. Gas can be applied afresh
after expiry. The original switched stages on firing and restarted the timer
on every gas hit; those behaviours remain in the faithful darker-cpp engine.

The successful trigger also expires the fixed sound record at 37F6h by writing
the current clock. The state retains that deadline; the general fixed ambient
sound-record scheduler is not yet connected to playback.

Native firing fixtures still check energy costs, readiness and launch guards;
impact fixtures retain the protected-surface and warehouse detonation-window
comparisons outside the deliberate repeat-gas change. Redux checks cover the
extended surface categories, missing destruction links, misses, repeated gas,
early triggers, expiry, clock wrapping, manual reselection and real projectile
collisions with storage tanks and warehouse blocks. The controlled mission-80
test uses real projectile flight and collision geometry to destroy three skylights with timed pairs,
then the storage tank with Brent Ground, and verifies the return message and
docking.

## Caero Weapon

Key 8 selects the final Caero secondary weapon, introduced in mission 88.
CA55 uses the same negative-object-target gate as Brent Hunter. At object
impact CF21 samples beacon light through 8450, shifts the returned word left
two and adds 45 to its high byte. The resulting strength is a wrapped byte;
it retains a baseline of 45 with no usable beacon power. It is an input to
the ordinary resistance/damage calculation, not a direct hit-point deduction.

CF21 leaves the victim's native object token in AX before calling 8450. That
routine rotates AX and uses two nibbles as coordinate fractions. The
reconstruction deliberately retains these token-derived fractions rather
than substituting the projectile's or victim's fractional position.

Native verification covers 2,048 impact samples with varying position,
altitude, tower type/output and victim token. The shared firing fixture now
also covers selection 8 at energy and target boundaries. Mission 88's
controlled combat uses this weapon, with ordinary Pinner Direct for ground
vehicles that cannot be selected by the air-target acquisition pass.
