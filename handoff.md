# Godzilla Steven handoff for Codex

Project: `G:/gamejams/EpicGameJam2026/EpicGameJam2026.uproject` (Unreal Engine 5.9).

## What just happened (Cursor session)

Packaged Windows Shipping builds were broken in playable ways even after cook succeeded. Root causes and fixes:

1. **Cook failure (Landmass)** — `bSkipEditorContent=True` made cook fail because Landmass soft-refs editor materials (`BrushBounds` → `TilingAA*`, `M_BrushSprite_01` → `S_Terrain`). Fix: set `bSkipEditorContent=False` in `Config/DefaultGame.ini`, disable `Landmass` / `Water` / `Buoyancy` / `Volumetrics` in `.uproject`, and keep `Config/DefaultEditor.ini` with `CookContentMissingSeverity=Warning`. Do **not** add `DirectoriesToNeverCook=(Path="/Landmass")` while Landmass still mounts — that double-registers `/Landmass/` and ensures-fail the cook.

2. **Packaged game empty + dead input** — Title UI drew, but Steven/arena were missing, cursor hidden, Start Editing hover worked, click/Enter did nothing. Cause: `ADirectorsCutStage::Build()` returned false because `/Engine/EngineMaterials/EmissiveTexturedMaterial` was **not cooked**. BeginPlay then `SetActorTickEnabled(false)` and never called `SetPhase(Title)`, so cursor never enabled; `Confirm()` early-out on `!IsActorTickEnabled()`. Fix in `DirectorsCutStage.cpp`: load `EmissiveMeshMaterial` (is in the pak) and fall back to `BasicShapeMaterial`. Also cook `/Engine/EngineMaterials`. Cursor/input: show cursor early in BeginPlay; `DefaultInput.ini` now uses legacy `PlayerInput`/`InputComponent`, `bCaptureMouseOnLaunch=False`, `CaptureDuringMouseDown`, `DoNotLock`.

3. **Earlier compile packaging fail** — `MipGenSettings` is not on `UTexture2D` in 5.9; removed from HUD grain texture setup in `DirectorsCutHUD.cpp`.

Shipping `EpicGameJam2026` Win64 compile succeeded after these changes. User needs to **repackage** and verify the staged build shows Steven, cursor, and Start Editing / Enter.

## Current game features (relevant)

- C++ directors-cut flow in `DirectorsCutGameMode` / `DirectorsCutStage` / `DirectorsCutHUD` / `DirectorsCutFilm`.
- Title → combat → reel ready → screening → payout. GlobalDefaultGameMode is `DirectorsCutGameMode`; map `L_Default`.
- **P** pauses (`SetGamePaused` + `bExecuteWhenPaused`); PAUSED HUD overlay.
- Screening: dim DirectionalLight ×0.5; camera PP vignette/fringe + film grain intensity; HUD also draws a **reliable noise overlay** during Screening because native UE film grain is flaky (needs streamed grain texture / scalability).
- Arena dressing: film reel props, dark teal floor `#102223` with large checker + finer center-cross checker; benches without emissive/point lights (user reverted lighting experiments — do not reintroduce heavy lighting without ask).
- Title Steven: `/Game/Player/Mesh/1/Steven1`, pose + slash anims, machete on `hand_rSocket`. Title camera off-center for left UI panel.

## Files touched recently

| File | Why |
|------|-----|
| `Source/EpicGameJam2026/DirectorsCutStage.cpp` | Emissive material fallback so packaged Build() succeeds |
| `Source/EpicGameJam2026/DirectorsCutGameMode.cpp` | Reel PP + sun dim; early cursor; Confirm gate; film grain texture bind attempt |
| `Source/EpicGameJam2026/DirectorsCutHUD.cpp/.h` | Screening film-grain HUD overlay (transient noise texture) |
| `Config/DefaultGame.ini` | Packaging/cook dirs; `bSkipEditorContent=False`; AlwaysCook EngineMaterials |
| `Config/DefaultEditor.ini` | Cook missing-content severity Warning |
| `Config/DefaultInput.ini` | Cursor-friendly capture + legacy input classes |
| `EpicGameJam2026.uproject` | Disable Landmass/Water/Buoyancy/Volumetrics |

Many of these may still be **uncommitted**. Check `git status` before committing.

## Open / verify next

1. **Repackage Windows Shipping** and run `Saved/StagedBuilds/Windows/EpicGameJam2026.exe` (or project Packaged build). Confirm: Steven visible on title, mouse cursor visible, Start Editing click + Enter start the slash → combat.
2. If Build still fails in package, check shipping log for `Director's Cut could not load the required stage assets` — Now logs which of Godzilla/Pigment/Light/Shapes failed.
3. Film grain: HUD overlay should be obvious during Godzilla reel; native camera PP grain is secondary. Do not spend time on CameraActor Details Blend Weight during REEL COMPLETE — effects are cleared when leaving Screening.
4. Title camera framing (older Codex task): Steven centered in the **right** visible panel without clipping the sword mid-slash, no snap on Start Editing. Values and recordings notes from the previous handoff may still apply if framing was left unfinished — re-check current `SetArenaCamera` / title transform in `DirectorsCutStage.cpp` against PIE before changing.

## Packaging notes

- Cook-only verify:  
  `RunUAT.bat BuildCookRun -project=... -platform=Win64 -clientconfig=Shipping -cook -skipstage -skipbuild -nocompileeditor`
- Live Coding blocks command-line `Build.bat` while the editor has Live Coding active — Ctrl+Alt+F11 or close editor.
- IoStore pak list: `UnrealPak.exe ...\EpicGameJam2026-Windows.utoc -List` (`.pak` alone is less useful with IoStore).
- Known-good cooked assets include Godzilla, Steven1, BasicShapes, BasicShapeMaterial, EmissiveMeshMaterial, FilmReel props, Tripo machete. `EmissiveTexturedMaterial` was missing from the broken package.

## Do not

- Reintroduce lighting overrides / Lumen CVar experiments / bench emissive lights unless the user asks.
- Assume CameraActor Post Process Blend Weight alone enables film grain (needs intensity overrides + grain texture; HUD overlay is the reliable path).
- Flip `bSkipEditorContent` back to True without solving Landmass refs — cook will fail again.
