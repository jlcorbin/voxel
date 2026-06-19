# Voxel World — Session Handoff

<!-- The session-start hook previews this file. Read it first, then docs/voxel-build-log.md. -->

**Last session:** 2026-06-19 · **Stop reason:** end of day, Phase 2 complete.

## TL;DR — where we are
Building a **cubic (Minecraft-style) voxel world** in **Unreal Engine 5.7**, from-scratch
C++, rendering chunks via the **RealtimeMeshComponent** plugin. **Phase 2 vertical slice is
DONE and walk-tested in PIE.** Next is **Phase 3a: chunk streaming around the player.**

## Read these first (in order)
1. `production/session-state/active.md` — this file.
2. `docs/voxel-build-log.md` — running build log (newest state at top, full history + lessons).
3. `docs/architecture/voxel-architecture-proposal.md` — the 8 approved architecture decisions
   ("all recommended"). This is the contract for everything we build.
4. `Source/Voxel/VoxelChunk.{h,cpp}` — the working slice code.

## What exists / works
- C++ module **`Voxel`** (`Source/Voxel/`). Depends on `RealtimeMeshComponent` (in `Voxel.Build.cs`).
- **`AVoxelChunk`** — flat `uint8[ChunkSize³]` voxel data; sine-wave heightmap generation;
  naive face-cull mesher → `URealtimeMeshSimple` (single poly-group/material slot 0);
  complex-as-simple collision. Tuning knobs are UPROPERTYs (ChunkSize=32, VoxelSize=100,
  BaseHeight, HeightAmplitude, NoiseScale, VoxelMaterial). `RegenerateChunk()` is CallInEditor.
- Verified in PIE: renders correctly (culling + winding good), character can stand on it.
- A test actor **`VoxelChunk_Slice`** is hand-placed at world [3500,0,0] in `Lvl_ThirdPerson`.

## NEXT STEP — Phase 3a: chunk streaming (one approved slice)
Goal: many chunks load/unload in a radius around the player at 60 FPS, generated and meshed
**off the game thread**. Per the architecture (Axis 4 & 7), the plan to PROPOSE to the user
(collaborative — get approval before building):
1. **`AVoxelWorld` (C++)** chunk manager: tracks player chunk-coord, keeps a ring of chunks
   within render radius 8 loaded, unloads beyond it. Owns/spawns `AVoxelChunk`s (or pooled
   mesh components). Throttles mesh "apply" to a few chunks/frame on the game thread.
2. **Move generation + greedy/naive meshing onto worker threads** (`UE::Tasks::Launch`).
   Workers touch ONLY plain C++ data (no UObjects). Game thread applies finished StreamSets.
   The current mesher is already a pure data pass — lift it into a free function/struct.
3. **`BP_VoxelWorld`** Blueprint subclass exposing the tuning knobs; drag into level / spawn.
   This replaces the hand-placed `VoxelChunk_Slice` (delete that test actor).
4. World-space chunk coords: chunk (cx,cy,cz) → actor/section at (cx,cy,cz)*ChunkSize*VoxelSize.
   Neighbor-aware culling across chunk borders (sample the adjacent chunk's edge voxels so seams
   between chunks don't render interior faces).
5. PIE acceptance: fly/walk around, chunks stream in/out, `stat unit` holds ~16.6 ms, no hitches.

Greedy meshing (Axis 3) can be swapped in during or right after 3a as a drop-in optimization.

## Environment + mcp-unreal facts (so you don't re-learn them)
- mcp-unreal bridge runs on port 8090. **`status.editor_online` MISREPORTS `false`** even when
  connected — verify with a real call (`get_level_actors`), not the flag.
- **Remote Control API (port 30010) is OFF** → `call_function` fails. Use `execute_script` (Python).
- **`execute_script` returns no stdout and reports `success:true` even on Python exceptions.**
  Wrap logic in try/except, write results to `Saved/<name>.txt`, then Read that file.
- The **Bash sandbox cannot read `C:\Program Files`** (engine/plugin files). To inspect engine
  or plugin headers, read them via `execute_script` (the editor process can) and dump to `Saved/`.
- Adding a **new UCLASS requires an editor restart** (Live Coding won't register it). Edits hot-patch
  via Live Coding (`Ctrl+Alt+F11`). A `Build.cs` dependency change wants a full rebuild.
- Editor viewport isn't realtime → `capture_viewport` returns a **stale frame**. Before capturing:
  `LevelEditorSubsystem.editor_set_viewport_realtime(True)` + `.editor_invalidate_viewports()`;
  aim via `UnrealEditorSubsystem.set_level_viewport_camera_info(loc, rot)`.
- `unreal.Rotator(a,b,c)` positional order is **(roll, pitch, yaw)** — use keyword args.

## RealtimeMeshComponent API (installed build, verified against headers)
- Builder: `RealtimeMesh::TRealtimeMeshBuilderLocal<void,void,void,1,void> Builder(StreamSet);`
  then `EnableTangents/EnableColors/EnableTexCoords/EnablePolyGroups`. `AddVertex(FVector3f)`
  returns a fluent vertex builder: `.SetNormalAndTangent(N,T).SetTexCoord(uv).SetColor(c)`.
  `AddTriangle(v0,v1,v2, polyGroup)`. Winding used: tris (0,1,3) + (1,2,3) per quad.
- Apply: `RMC->InitializeRealtimeMesh<URealtimeMeshSimple>()`; `SetupMaterialSlot(0,...)`;
  `Mesh->CreateSectionGroup(FRealtimeMeshSectionGroupKey::Create(FRealtimeMeshLODKey(0),0),
  MoveTemp(StreamSet))` (auto-creates a section per poly group).
- Collision: `Mesh->SetCollisionConfig(FRealtimeMeshCollisionConfiguration())` (defaults:
  complex-as-simple + async cook) then `UpdateSectionConfig(FRealtimeMeshSectionKey::
  CreateForPolyGroup(GroupKey,0), FRealtimeMeshSectionConfig(0), /*bShouldCreateCollision=*/true)`.
- Headers live under the engine plugin `Marketplace/Realtimee81af4739f42V19/.../Public/`
  (`RealtimeMeshSimple.h`, `Interface/Core/RealtimeMeshBuilder.h`, `.../RealtimeMeshKeys.h`,
  `.../RealtimeMeshCollision.h`). Reference box gen: `Private/Mesh/RealtimeMeshBasicShapeTools.cpp`.

## Hard rules (unchanged)
- User does ALL git in GitHub Desktop. **Never run git or touch `.git`.**
- Collaborative: inspect + propose, user decides, then build. STOP between phases.
- mcp-unreal can't build C++, set object-ref pins, EnhancedInput refs, or AnimGraph internals —
  give exact manual steps for those.
- Performance is the whole game: every choice judged on holding 60 FPS.

## Approved deps (technical-preferences.md → Allowed Libraries)
- RealtimeMeshComponent (in use). FastNoiseLite (approved, NOT yet added — lands Phase 3b for real terrain).
