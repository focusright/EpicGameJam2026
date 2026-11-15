# Godzilla Steven handoff for Codex

Project: `G:/gamejams/EpicGameJam2026/EpicGameJam2026.uproject` (Unreal Engine 5.9).

## Current Windows build

- Launch `Saved/DLSS-Staged/Windows/EpicGameJam2026.exe`. This is the staged package updated and tested in the recent Codex sessions. The older `Saved/StagedBuilds/Windows` output predates those updates.
- The latest Shipping build and staged executable match. See `Saved/CityStreet-Build.log`. Verified Enter from title into combat, Play Reel after combat, and the updated city during the opening impact. The full six-shot reel was not replayed during the latest ground pass.
- The current stage uses the full cook that includes `M_FilmStock` and the DLSS shaders. Later city changes were C++ only, so the Shipping executable was rebuilt and copied into that package. Cook and stage again after asset, shader, or packaging changes.
- At this update, the latest ground changes in `DirectorsCutFilm.cpp` and `DirectorsCutStage.h` are uncommitted. The windows and roof/wall passes are committed as `1fc8acf` and `afff4c4`.

## Recent changes

- **Reel city** — `ADirectorsCutStage::BuildCity()` in `DirectorsCutFilm.cpp` builds individual windows on every side, mixing dark panes with warm amber and cool blue panes. Foreground buildings and collapsible buildings use the shared film material over colored window backings. Walls have corner piers, trim between floors, and recessed double-door entrances. Roofs have parapets, membrane seams, access sheds, HVAC grilles, vents, and antennas.
- **Street and impact dressing** — The city has dark asphalt, curbs, segmented sidewalks, drains, manhole covers, faded road markings, and cracked repair patches. Each collapsing building has a nearby rubble group. `PoseDestruction()` derives rubble visibility from the same collapse time as the building, so skipped shots and replay reset evaluate correctly.
- **Film enemies** — `BuildEnemy()` in `DirectorsCutStage.cpp` creates one thin black square with two rows of open sprocket holes. The center uses `/Game/Props/FilmEnemy/M_FilmStock` and is split across the two pieces so it follows the slice animation. The material is a two-sided Substrate material with **Colored Transmittance Only** (`BLEND_MODULATE`). This was the blend verified to preserve the dark tint and show the floor through it. The previous translucent blend drew a pale panel in the package. `Config/DefaultGame.ini` always cooks `/Game/Props/FilmEnemy`.
- **DLSS 4.5** — The UE 5.8 v8.9.0 DLSS and StreamlineNGXCommon plugins were ported to 5.9 and are under `Plugins/`. The game starts in DLSS Super Resolution Quality with Preset M on supported hardware. **L** toggles it, restores the previous render scale when off, and displays a brief HUD status. It works while paused. `-NoDLSS` starts with it off. The RTX 3080 Laptop test confirmed 854 × 481 → 1280 × 720 at 66.7% scale. See `Saved/DLSS-Smoke-Quality.log`. Timing and a full-reel visual comparison have not been measured.
- **Film grain** — The HUD draws the main grain over the upscaled scene during Screening. The native camera grain is secondary. The HUD grain was visible during the updated reel playback.

## Earlier package fixes

Packaged Windows Shipping builds were broken in playable ways even after cook succeeded. Root causes and fixes:

1. **Cook failure (Landmass)** — `bSkipEditorContent=True` made cook fail because Landmass soft-refs editor materials (`BrushBounds` → `TilingAA*`, `M_BrushSprite_01` → `S_Terrain`). Fix: set `bSkipEditorContent=False` in `Config/DefaultGame.ini`, disable `Landmass` / `Water` / `Buoyancy` / `Volumetrics` in `.uproject`, and keep `Config/DefaultEditor.ini` with `CookContentMissingSeverity=Warning`. Do **not** add `DirectoriesToNeverCook=(Path="/Landmass")` while Landmass still mounts — that double-registers `/Landmass/` and ensures-fail the cook.

2. **Packaged game empty + dead input** — Title UI drew, but Steven/arena were missing, cursor hidden, Start Editing hover worked, click/Enter did nothing. Cause: `ADirectorsCutStage::Build()` returned false because `/Engine/EngineMaterials/EmissiveTexturedMaterial` was **not cooked**. BeginPlay then `SetActorTickEnabled(false)` and never called `SetPhase(Title)`, so cursor never enabled; `Confirm()` early-out on `!IsActorTickEnabled()`. Fix in `DirectorsCutStage.cpp`: load `EmissiveMeshMaterial` (is in the pak) and fall back to `BasicShapeMaterial`. Also cook `/Engine/EngineMaterials`. Cursor/input: show cursor early in BeginPlay; `DefaultInput.ini` now uses legacy `PlayerInput`/`InputComponent`, `bCaptureMouseOnLaunch=False`, `CaptureDuringMouseDown`, `DoNotLock`.

3. **Earlier compile packaging fail** — `MipGenSettings` is not on `UTexture2D` in 5.9; removed from HUD grain texture setup in `DirectorsCutHUD.cpp`.

## Current game features (relevant)

- C++ directors-cut flow in `DirectorsCutGameMode` / `DirectorsCutStage` / `DirectorsCutHUD` / `DirectorsCutFilm`.
- Title → combat → reel ready → screening → payout. GlobalDefaultGameMode is `DirectorsCutGameMode`; map `L_Default`.
- **P** pauses (`SetGamePaused` + `bExecuteWhenPaused`); PAUSED HUD overlay.
- Screening: dim DirectionalLight ×0.5; camera PP vignette/fringe + film grain intensity; HUD also draws a **reliable noise overlay** during Screening because native UE film grain is flaky (needs streamed grain texture / scalability).
- Arena dressing: film reel props, dark teal floor `#102223` with large checker + finer center-cross checker; benches without emissive/point lights (user reverted lighting experiments — do not reintroduce heavy lighting without ask).
- Title Steven: `/Game/Player/Mesh/1/Steven1`, pose + slash anims, machete on `hand_rSocket`. Title camera off-center for left UI panel.

## Earlier packaging and gameplay files

| File | Why |
|------|-----|
| `Source/EpicGameJam2026/DirectorsCutStage.cpp` | Emissive material fallback so packaged Build() succeeds |
| `Source/EpicGameJam2026/DirectorsCutGameMode.cpp` | Reel PP + sun dim; early cursor; Confirm gate; film grain texture bind attempt |
| `Source/EpicGameJam2026/DirectorsCutHUD.cpp/.h` | Screening film-grain HUD overlay (transient noise texture) |
| `Config/DefaultGame.ini` | Packaging/cook dirs; `bSkipEditorContent=False`; AlwaysCook EngineMaterials |
| `Config/DefaultEditor.ini` | Cook missing-content severity Warning |
| `Config/DefaultInput.ini` | Cursor-friendly capture + legacy input classes |
| `EpicGameJam2026.uproject` | Disable Landmass/Water/Buoyancy/Volumetrics |

## Open / verify next

1. Review all six reel shots if making another city art pass. The most recent build was visually checked from the opening camera, including the first impact.
2. Title framing was not rechecked through the full sword swing. If revisiting it, use `SetArenaCamera` and `BuildTitleDirector` in `DirectorsCutStage.cpp`. Aim to keep Steven centered in the right panel with the whole sword swing in frame and a smooth transition into combat.
3. If the packaged title draws but input or the stage fails, check the log for `Director's Cut could not load the required stage assets`. It reports the Godzilla, Pigment, Light, and Shapes load results.

## Packaging notes

- Cook-only verify:  
  `RunUAT.bat BuildCookRun -project=... -platform=Win64 -clientconfig=Shipping -cook -skipstage -skipbuild -nocompileeditor`
- Live Coding blocks command-line `Build.bat` while the editor has Live Coding active — Ctrl+Alt+F11 or close editor.
- Unreal MCP is available for editor-side checks. An already-open editor may be using the previous game DLL or plugin configuration. Verify runtime-only changes in the updated Shipping build or rebuild and reopen the editor before using PIE.
- IoStore pak list: `UnrealPak.exe ...\EpicGameJam2026-Windows.utoc -List` (`.pak` alone is less useful with IoStore).
- Known-good cooked assets include Godzilla, Steven1, BasicShapes, BasicShapeMaterial, EmissiveMeshMaterial, FilmReel props, Tripo machete. `EmissiveTexturedMaterial` was missing from the broken package.

## Do not

- Reintroduce lighting overrides / Lumen CVar experiments / bench emissive lights unless the user asks.
- Assume CameraActor Post Process Blend Weight alone enables film grain (needs intensity overrides + grain texture; HUD overlay is the reliable path).
- Flip `bSkipEditorContent` back to True without solving Landmass refs — cook will fail again.
