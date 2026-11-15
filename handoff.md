# Godzilla Steven handoff for Cursor

Project: `G:/gamejams/EpicGameJam2026/EpicGameJam2026.uproject` (Unreal Engine 5.9).

## Current request and blocker

The opening screen has the title and button on the left and Steven in the scene on the right. The user wants Steven centered in that right-hand visible area. Preserve the earlier requirements: show his face from a low, close camera; keep his full body and the entire sword animation in frame; do not introduce a camera snap when Start Editing is clicked.

The latest user screenshot is `C:/Users/rocke/AppData/Local/Temp/codex-clipboard-27587611-9561-47a4-9a3d-277494f5f0c2.png`. It shows Steven too far left in the scene area. At 1920 px wide, the scene begins around x=900 and its center is around x=1410.

**The camera framing is unfinished.** The current code centers Steven's opening pose, but the sword tip runs off the right edge near 1.0 second into the slash. Do not report the framing as complete until the full animation has been checked.

Current values in `Source/EpicGameJam2026/DirectorsCutStage.cpp`:

- Title Steven relative transform: yaw `110`, location `(-90, -73, -87)`, scale `2.0`. I reverted an experimental yaw of `135`: it made the face more of a side profile and still clipped the sword.
- Title camera position: `ArenaOrigin + (-380, -410 + 30 * sin(Time * 0.35), 145)`.
- Title camera aim: `ArenaOrigin + (6, -228, 110)` when the Steven mesh exists; FOV `88`.
- This puts Steven's body near the center of the right-hand area at the opening pose. The sword is clipped at the right edge around 1.0 second into the animation. The latest matching video is `Saved/Recordings/Viewport_20261115-061226_DTUZPCBUUELVPM2WV0R9XH308.mp4` (yaw 110, FOV 88). The frame extracted at 1.0 second is `Saved/Recordings/center_88_10.png`.

A prior framing kept the entire swing in view but left Steven too far left: same camera position, aim `(-55, -150, 110)`, FOV `66`. The matching video is `Saved/Recordings/Viewport_20261115-060240_6NVKXGDQ3HMNGWEBH1MINUK9D.mp4`. Another FOV 74 attempt moved him partway toward center but left him short of the panel center; its swing video is `Saved/Recordings/Viewport_20261115-061013_C2UVBDX84WKD8VNHZI2EAP9XM.mp4`.

Possible next approach: find a composition between these two extremes, or make a gentle, continuous camera adjustment during the swing while keeping the opening pose centered. The user already rejected a sudden camera shift on click. Do not change the authored animation just to solve framing. Check the raised sword at about 0.6 s, its far right reach at about 1.0 s, and the boots and head throughout. Also check clicks after the title camera has drifted to both ends of its `sin` movement.

## Existing title implementation

- Opening Steven mesh: `/Game/Player/Mesh/1/Steven1`.
- Opening pose: `/Game/Player/Animation/mixamo/AS_Steven1_Slash_Pose`.
- Start slash: `/Game/Player/Animation/Stable_Sword_Outward_Slash_Anim` (2.0333 s, Steven1 skeleton). It begins and ends in the opening pose.
- Machete: `/Game/TripoModels/long_blade_machete_3d_model/long_blade_machete_3d_model`, attached to Steven's `hand_rSocket`.
- Steven's animation sits roughly 43 cm above its root, so the title mesh has a `-87` Z offset at scale 2.0 to place his feet on the floor.
- `DirectorsCutHUD.cpp` labels the button **Start Editing**. Clicking it or pressing Enter calls `Confirm()` in `DirectorsCutGameMode.cpp`. The `TitleStart` phase plays the slash, waits for its duration plus 0.1 s, then enters combat. The title camera has a separate `TitleCameraTime` that continues across the click; this removed the previous camera snap. `Config/DefaultGame.ini` cooks `/Game/Player/Animation` for packaged builds.

## Files and validation

The source and config changes for the title pose, button, start animation, and camera are currently **uncommitted**. Check `git status --short` before editing. The yaw reversion from 135 to 110 passed Live Coding, so the open editor matches the current source. Earlier iterations passed a Win64 Development build; the camera framing still needs its own final build after the clipping is solved.

Unreal MCP was available in this session through the local editor. We used `LiveCodingToolset.LiveCodingToolset.CompileLiveCoding`, `EditorToolset.EditorAppToolset.StartPIE/StopPIE`, `InputToolset.InputToolset.ApplyInput` with Enter, and `SceneCaptureToolset.SceneCaptureToolset.CaptureGameViewport` or `StartViewportRecording/StopViewportRecording`. If Cursor lacks MCP, use the open Unreal editor and Play In Editor manually. The recorded videos under `Saved/Recordings` are local test artifacts, not project assets.

The older `LS_Steven_MacheteSlash` Control Rig workflow is separate from this opening-screen camera task. The user has supplied the finished slash asset above; preserve it.
