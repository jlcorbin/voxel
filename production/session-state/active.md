# Voxel World — Session Handoff

<!-- The session-start hook previews this file. Read it first, then docs/voxel-build-log.md. -->

**Last update:** 2026-06-23 · **Stop reason:** Phase 3a complete & PIE-confirmed.

## TL;DR — where we are
Building a **cubic (Minecraft-style) voxel world** in **Unreal Engine 5.7**, from-scratch
C++, rendering chunks via the **RealtimeMeshComponent** plugin. Phase 2 slice DONE. **Phase 3a
(chunk streaming) is DONE and PIE-confirmed** — chunks stream in/out around the player (full-3D
spherical, radius 6), generated + meshed on `UE::Tasks` workers, pooled, collision works, and the
player auto-spawns onto the surface. **Next: pick greedy meshing (optimization) or Phase 3b (real
terrain) — see "NEXT STEP" below.**

## First thing next session
Measure performance under load: PIE, `stat unit` + `stat scenerendering`, fly around fast, confirm
~16.6 ms and draw calls under ~3000. We built the streaming but haven't profiled it yet.

## Read these first (in order)
1. `production/session-state/active.md` — this file.
2. `docs/voxel-build-log.md` — running build log (newest state at top, full history + lessons).
3. `docs/architecture/voxel-architecture-proposal.md` — the 8 approved architecture decisions
   ("all recommended"). This is the contract for everything we build.
4. `Source/Voxel/` — the working code (Generator, Mesher, Chunk, World).

## What exists / works (Phase 3a, PIE-confirmed)
- C++ module **`Voxel`** (`Source/Voxel/`). Depends on `RealtimeMeshComponent` (in `Voxel.Build.cs`).
- **`FVoxelGenerator.h`** — pure, copyable, thread-safe. World-space sine heightmap `IsSolid()`;
  `Classify()` → AllAir / Interior / Surface so trivial chunks never spawn an actor or mesh.
- **`FVoxelMesher.h/.cpp`** — pure, thread-safe naive-cull mesher → `FRealtimeMeshStreamSet`.
  Border culling samples the generator directly (no neighbour-chunk dependency, no seams).
  Defines `FVoxelChunkResult` (move-only) + `FVoxelApplyQueue` (FCriticalSection + TSharedPtr).
- **`AVoxelChunk`** — poolable holder: `ApplyMesh(streams, material)` (re-inits RMC, single poly
  group/material slot 0, complex-as-simple collision) + `ClearMesh()`. No self-generation.
- **`AVoxelWorld`** — streaming manager (placed via `BP_VoxelWorld` in `Lvl_ThirdPerson`):
  spherical desired set around player; dispatches gen+mesh on `UE::Tasks::Launch` (≤MaxLoadsPerFrame);
  applies finished meshes on game thread (≤MaxAppliesPerFrame); pools chunk actors (≤MaxPooledChunks);
  per-request tokens drop results for chunks unloaded mid-flight; **auto-places the player on the
  surface at start** (`TryPlacePlayer` via `Generator.HeightAt`, knob `bAutoPlacePlayer`).
  Knobs: RenderRadius=6 (target 8), ChunkSize=32, VoxelSize=100, height params, throttles,
  MaxPooledChunks=128, VoxelMaterial (defaults WorldGridMaterial), PlayerSpawnClearance=150.
- PIE-confirmed: chunks stream in/out as you move, collision blocks, player spawns on top.
- `BP_VoxelWorld` is placed in `Lvl_ThirdPerson`. The old `VoxelChunk_Slice` was deleted.

## NEXT STEP — pick one (all per the approved architecture)
**0. Profile first (quick):** PIE + `stat unit` / `stat scenerendering`, fly fast — confirm
   ~16.6 ms and draw calls < ~3000. Streaming is built but not yet measured.

**A. Greedy meshing (Axis 3 optimization)** — replace the naive face-cull loop in `FVoxelMesher`
   with greedy meshing (merge coplanar same-type faces). Big triangle/vertex/draw reduction.
   Pure mesher change — no streaming/threading changes. Watch out: texture atlas / per-face UVs,
   AO in vertex colours (deferred). Good "tighten what we have" step.

**B. Phase 3b — real terrain** — integrate **FastNoiseLite** (approved, not yet added): 2D height
   + 3D density noise for caves/overhangs, then biomes. This is where full-3D streaming pays off
   (right now the heightmap world makes most 3D chunks trivial). Replaces `FVoxelGenerator`'s
   placeholder `HeightAt`/`IsSolid`.

**Then:** 3c add/remove voxel editing (cubic makes this a data write + re-mesh of the touched
   chunk + its border neighbours), 3d materials/biomes (multi-material via poly groups + atlas).

Also pending from architecture: push RenderRadius 6 → 8 once perf is validated; greedy meshing
is the lever if draw calls/tris get tight.

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
