# Delphi collateral damage and repairs

Redux allows player ground weapons to destroy unmarked Delphi buildings that
already have a destroyed model. Authorisation still controls the target reticle
and homing lock; it no longer grants immunity to everything else. Brent Ground
and both Forbes Diffuser stages can be fired straight ahead without a lock.
A permitted target still receives the usual homing behaviour.

The collision rules remain in force: protected walls reject damage, office
buildings require a doorway hit, and the special warehouse blocks still need
the Diffuser's gas/detonator sequence. Ordinary scenery without a destruction
link, terminal ruins and energy beacons remain indestructible. Destroying a
power station's two transformers affects beacon supply as described below;
it does not damage the beacons themselves.

Halon and underground damage admission is unchanged. Halon already admits its
ground projectiles without Delphi's authorisation gate, but still requires an
appropriate collision surface and a linked destroyed model.

## What persists

| Damage | Result |
|---|---|
| Ordinary or optional building destroyed by the player | Saved on successful mission completion, even if a later mission merely authorises that building as a target. |
| Radar mast destroyed by the player | Loses coverage for the current mission; repaired before saving a successful mission. |
| Other hangar entrance destroyed by the player | Repaired before saving a successful mission. A current landing destination is also repaired when its approach captures the ship. |
| Hemmersan's entrance at (49, 113) destroyed by the player | The attempt ends with a failure message and retry/menu controls. The committed campaign save is unchanged. |
| Building needed by a later scripted destruction objective | Restored at the start of that mission, before the mission applies its own explicit damaged/authorised states. |
| Power-station structures 83–85 | Repaired between missions unless they have been authorised for destruction. This applies regardless of who damaged them; saved authorisation preserves earlier demolition orders. |
| Other damage already supplied by a script or an enemy | Not put on the player service-repair list. It persists unless a subsequent mission needs the building intact for a destruction objective. |

Hemmersan's authored ruins in mission 115 do not trigger the player-caused
failure. Surrounding office buildings are ordinary buildings, not additional
failure triggers. Nightmare has no campaign return hangar and does not use the
Hemmersan failure condition.

Repairs remove only Delphi's damage bit, preserving authorisation, model variants
and door states. Beacon brightness is a different use of the cell state byte and
is never altered by these repairs. The existing DOS-compatible save layout is
unchanged; there is no additional persistent repair log. Failed attempts retain
the usual retry behaviour and do not commit their damage.

## Power stations and beacon supply

This is a Redux gameplay addition. Each of Delphi's three power stations remains
online with one intact type-85 transformer. Destroying both disables it. Types
83 and 84 can also be destroyed, but do not determine whether a station supplies
power.

| Station | Type-85 transformer cells |
|---|---|
| Spervent (also spelled Sperwent in briefings) | (24, 10), (21, 17) |
| Bridfirth | (116, 58), (108, 59) |
| Gesic | (115, 111), (115, 113) |

A failed station reduces beacon output to zero within 18 map cells of the pair's
midpoint. Supply recovers linearly with distance, reaching normal output at 36
cells. Overlapping outages use the lowest supply level. When all three stations
are disabled, every beacon loses power, including those outside these local
radii. Each new outage fades in over 256 game ticks (about half a second).
Charging, scene lighting and streetlights follow the resulting beacon output;
the normal flight/energy rules handle the consequences. Radar coverage is
independent and remains unchanged.

Mission scripts retain their own beacon fades and restorations. The grid scales
those scripted levels by the available supply, so it cannot relight a scripted
blackout, and a script cannot restore power from a destroyed station. The
original scripted attacks on power sites and their failure sequences still run.

Structures 83, 84 and 85 are repaired before saving a completed Delphi mission
unless their target-authorisation bit is set. The same repair runs on entry,
before applying the new mission's explicit setup, to support existing saves.
Mission 75 authorises all 14 components of Spervent: its demolition therefore
persists, and its transformers keep that station offline in subsequent missions.
The general repair of buildings required for a specific scripted objective still
applies when needed.

`delphi_power_grid` keeps supply loss separate from the scripted cell states.
Before save packing, it removes its temporary brightness adjustment, then the
existing scripted beacon queue is committed. Saves retain the ordinary building
damage and authorisation bits; a new run derives station availability from those
transformers again. There is no extra save field and no cumulative dimming from
reapplying the effect each frame. Station positions, falloff distances and fade
time are collected in [`power_grid.h`](../src/game/power_grid.h).

## Finding scripted dependencies

`scripted_destruction_targets` reads the current scenario's active building
objective chain, then walks the player and moving-actor programs. It collects:

- Active objective cells, following FE list continuations.
- Both lists supplied by opcode 34, respecting whether its marked list is an
  objective or merely grants permission.
- Opcode 1D cell-state waits that test the damage bit, including clear-bit tests.

Authorisation-only lists, permission-only waits, type checks, aircraft spawning
sites and compact vehicle routes are not destruction dependencies. Relative
branches, stopped programs, saved continuations and the variable-width altitude
command are handled without executing gameplay or consuming messages. This
includes missions 65 and 77, which replace their building objectives during
flight, and missions whose actor scripts wait for a particular building to fall.

Only the mission being entered is repaired. Future optional targets can therefore
be destroyed early; future required targets stay ruined until their own mission
starts. Setup runs after repairs, so deliberate mission-start damage wins.

`delphi_collateral_damage` separately records successful player hits on intact
service buildings. Hostile bombing and scripted world damage do not populate it.
Its lifetime is one `mission_combat`, and its service repairs run before
`pack_city_state` on successful progression.

## Verification

Regression coverage includes unguided firing, retained homing locks, protected
surfaces, real retail building collisions, Diffuser retries on unmarked buildings,
player/enemy damage separation, landing repair, save packing, optional-target
persistence, the mission-115 setup and the failure screen's retry/menu controls.
All retail Delphi script dependency counts are checked against the independently
decoded mission catalogue, including the embedded objective lists. Power-grid
tests cover real missile impacts on all six retail transformers, partial and
city-wide outages, independent script fades, energy availability, selective
repairs and a mission-75 save/load round trip.

See [Caero weapons](caero_weapons.md) for firing and Diffuser timing, and
[campaign persistence](campaign_status.md) for the existing mission
save boundary.
