# Steven’s machete animation handoff

Open `G:/gamejams/EpicGameJam2026/EpicGameJam2026.uproject` in Unreal Engine 5.9. The user wants to learn the animation workflow and personally pose and time Steven’s two-handed horizontal slash with the long machete.

## How to help

- The user explicitly asked to leave the existing animation alone. Coach them through the editor one step at a time. Let them set the poses, timing, and performance. Ask before making any project edits or generating another take.
- For editor inspection, prefer the existing local Unreal MCP connection if Cursor has access to it. It previously responded at `http://127.0.0.1:8000/mcp`. The user objected to desktop takeover. Give manual Unreal steps if the direct connection is unavailable.
- If usage is running low, keep the next step small and record the current frame, pose, and blocker. The user wants an early warning when the current client exposes remaining usage.

## Project state

| Asset | Unreal content path | Purpose |
| --- | --- | --- |
| Steven’s Control Rig | `/Game/Player/Rig/CR_Steven_Body` | Duplicated from Manny’s `CR_Mannequin_Body` and adapted to Steven. The neutral pose was checked against all 80 Steven reference bones. |
| Steven’s mesh and skeleton | `/Game/Player/Mesh/Steven_Mesh` and `/Game/Player/Mesh/Steven_Skeleton` | Character and skeleton used by the rig and export. |
| Authoring Level Sequence | `/Game/Player/Animation/LS_Steven_MacheteSlash` | Contains Steven’s editable Control Rig track, the machete attached to `hand_r`, and the AnimGenOffline track with Kimodo constraints. |
| Previous animation export | `/Game/Player/Animation/AS_Steven_MacheteSlash` | A 1.6-second draft. The user found both the original Kimodo export and the rebuilt version unacceptable. Preserve it as a reference. |
| Long machete mesh | `/Game/TripoModels/long_blade_machete_3d_model/long_blade_machete_3d_model` | The weapon already bound in the authoring sequence. |

The first Kimodo export had a backward lean, sliding feet, and folded wrists. The second draft used the existing Control Rig poses for the body and a scripted two-hand grip. The user said it looked better and still fell well short. Their latest recording is `C:/Users/rocke/Videos/steven-wacked-02.mp4`. No new user-authored poses have been reported since the switch to coaching.

## Where to resume

1. Ask what the user currently sees in Unreal. If they have yet to start posing, guide them to open `LS_Steven_MacheteSlash` in Sequencer. If they want a clean take, walk them through duplicating it in the editor before editing.
2. Start with the guard at frame 0, or pick up at whatever frame they have already reached. Set the stance and torso first. Use `hand_r_ik_ctrl` to place the blade and `hand_l_ik_ctrl` to bring the support hand onto the handle. These controls need the arms in IK mode. View the pose from the front and side with the prop attached, then help the user key the controls they changed.
3. Once the user is happy with the guard, work through windup, strike, follow-through, and recovery. The old sequence is 30 fps with Kimodo constraints at frames 0, 10, 15, 19, 24, 32, and 46. The user can choose different timing for their take.
4. When the user is ready for AnimGenOffline, help them turn their chosen poses into Kimodo constraints and test a take. Review the result with Steven and the machete together before baking or exporting anything.
