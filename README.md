# KillMovePrompt

An SKSE plugin in the style of Dynamic Feed Overhaul: when an enemy is weakened
(low health, staggered or bleeding out), a key prompt appears on them. Press it
and Kaputt plays a killmove on that enemy.

## Requirements (in game)

- SKSE64 + Address Library for SKSE Plugins
- [SkyPrompt](https://www.nexusmods.com/skyrimspecialedition/mods/148703) and its requirements (draws the prompt)
- Kaputt (plays the killmove; uses your Kaputt json tags)

Without Kaputt the prompt still works, but only a ModEvent fires (see below).

## Install

Put `KillMovePrompt.dll` and `KillMovePrompt.ini` in `Data\SKSE\Plugins\`
(or zip the `SKSE` folder and install it with MO2/Vortex).

Log file: `Documents\My Games\Skyrim Special Edition\SKSE\KillMovePrompt.log`.

## Building the DLL — no Visual Studio needed

1. Create a free GitHub account and a new repository.
2. Upload everything in this folder (keep the `.github` folder — it's hidden on some systems).
3. Open the **Actions** tab. The *Build* workflow runs on every push (~15–25 min the first time).
4. When it's green, open the run and download the **KillMovePrompt** artifact. It contains
   `SKSE\Plugins\KillMovePrompt.dll` + `.ini`, ready to install.

### Building locally instead

Needs Visual Studio 2022 (Desktop C++ workload), CMake 3.21+, and vcpkg with `VCPKG_ROOT` set.
From the *x64 Native Tools Command Prompt*:

```
cmake --preset release
cmake --build build/release
```

Set `SKYRIM_MODS_FOLDER` to your MO2 mods folder to have the DLL + ini copied there automatically.

## How it works

- Every `ScanInterval` seconds it picks a target: the actor under the crosshair if it
  qualifies, otherwise the closest qualifying enemy in front of you.
- A target qualifies when it's alive, in range/angle/height, hostile, not essential or
  a follower, and meets one of the `[Conditions]`. If `UseKaputtPrecondition` is on, Kaputt
  also has to agree it has an animation for this pairing.
- The prompt is a SkyPrompt prompt bound to that actor's form ID. Where it's drawn
  (above the head vs. screen position) follows SkyPrompt's own settings.
- On key press it calls `KaputtAPI::submit(player, target, {RequiredTags, BannedTags})`.

## Papyrus hook

Every press sends `ModEvent "KillMovePrompt_Execute"` with the target as the sender:

```papyrus
Event OnInit()
    RegisterForModEvent("KillMovePrompt_Execute", "OnKMPExecute")
EndEvent

Event OnKMPExecute(string eventName, string strArg, float numArg, Form sender)
    Actor victim = sender as Actor
    ; numArg == 1.0 -> Kaputt played a killmove; 0.0 -> nothing played
EndEvent
```

This is how you can wire it to your VioLens edits instead of (or in addition to) Kaputt.

## Credits

- `src/kaputtAPI.h` — Kaputt by Pentalimbed (MIT)
- SkyPrompt API — QTR-Modding (MIT)
- vcpkg ports for CommonLibVR (MinLL) and SkyPrompt API adapted from Dynamic Feed Overhaul's build setup
