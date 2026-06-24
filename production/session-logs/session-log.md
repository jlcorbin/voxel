## Session End: 20260618_233740
### Commits
b8b2fde pc setup
37f13b8 initial
4159939 Repo scaffold: LFS attributes + ignore
---

## Session End: 20260618_234724
### Commits
b8b2fde pc setup
37f13b8 initial
4159939 Repo scaffold: LFS attributes + ignore
---

## Session End: 20260618_235238
### Commits
b8b2fde pc setup
37f13b8 initial
4159939 Repo scaffold: LFS attributes + ignore
### Uncommitted Changes
.claude/docs/technical-preferences.md
---

## Session End: 20260619_001846
### Commits
b8b2fde pc setup
37f13b8 initial
4159939 Repo scaffold: LFS attributes + ignore
### Uncommitted Changes
.claude/docs/technical-preferences.md
Voxel.uproject
---

## Archived Session State: 20260619_003536
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
---

## Session End: 20260619_003536
### Commits
b8b2fde pc setup
37f13b8 initial
4159939 Repo scaffold: LFS attributes + ignore
### Uncommitted Changes
.claude/docs/technical-preferences.md
Voxel.uproject
---

## Archived Session State: 20260622_230937
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
---

## Archived Session State: 20260622_232025
# Voxel World — Session Handoff

<!-- The session-start hook previews this file. Read it first, then docs/voxel-build-log.md. -->

**Last update:** 2026-06-22 · **Stop reason:** Phase 3a C++ written, awaiting build + BP.

## TL;DR — where we are
Building a **cubic (Minecraft-style) voxel world** in **Unreal Engine 5.7**, from-scratch
C++, rendering chunks via the **RealtimeMeshComponent** plugin. Phase 2 slice DONE. **Phase 3a
(streaming) C++ is written** — `AVoxelWorld` (streaming manager, pooled, threaded), `FVoxelGenerator`,
`FVoxelMesher`, and a refactored poolable `AVoxelChunk`. **Next: build + restart, make `BP_VoxelWorld`,
delete `VoxelChunk_Slice`, PIE-test streaming.** Chosen: full-3D spherical streaming, radius 6→8,
pooled, user builds the BP by hand.

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
---

## Session End: 20260622_232025
### Uncommitted Changes
Source/Voxel/VoxelChunk.cpp
Source/Voxel/VoxelChunk.h
docs/voxel-build-log.md
production/session-logs/session-log.md
production/session-state/active.md
---

## Archived Session State: 20260622_233903
# Voxel World — Session Handoff

<!-- The session-start hook previews this file. Read it first, then docs/voxel-build-log.md. -->

**Last update:** 2026-06-22 · **Stop reason:** Phase 3a C++ written, awaiting build + BP.

## TL;DR — where we are
Building a **cubic (Minecraft-style) voxel world** in **Unreal Engine 5.7**, from-scratch
C++, rendering chunks via the **RealtimeMeshComponent** plugin. Phase 2 slice DONE. **Phase 3a
(streaming) C++ is written** — `AVoxelWorld` (streaming manager, pooled, threaded), `FVoxelGenerator`,
`FVoxelMesher`, and a refactored poolable `AVoxelChunk`. **Next: build + restart, make `BP_VoxelWorld`,
delete `VoxelChunk_Slice`, PIE-test streaming.** Chosen: full-3D spherical streaming, radius 6→8,
pooled, user builds the BP by hand.

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
---

## Session End: 20260622_233903
### Uncommitted Changes
.vscode/compileCommands_Default.json
.vscode/compileCommands_Default/Voxel.1.rsp
.vscode/compileCommands_Voxel.json
.vscode/compileCommands_Voxel/Voxel.1.rsp
Source/Voxel/VoxelChunk.cpp
Source/Voxel/VoxelChunk.h
docs/voxel-build-log.md
production/session-logs/session-log.md
production/session-state/active.md
---

## Archived Session State: 20260622_234827
# Voxel World — Session Handoff

<!-- The session-start hook previews this file. Read it first, then docs/voxel-build-log.md. -->

**Last update:** 2026-06-22 · **Stop reason:** Phase 3a C++ written, awaiting build + BP.

## TL;DR — where we are
Building a **cubic (Minecraft-style) voxel world** in **Unreal Engine 5.7**, from-scratch
C++, rendering chunks via the **RealtimeMeshComponent** plugin. Phase 2 slice DONE. **Phase 3a
(streaming) C++ is written** — `AVoxelWorld` (streaming manager, pooled, threaded), `FVoxelGenerator`,
`FVoxelMesher`, and a refactored poolable `AVoxelChunk`. **Next: build + restart, make `BP_VoxelWorld`,
delete `VoxelChunk_Slice`, PIE-test streaming.** Chosen: full-3D spherical streaming, radius 6→8,
pooled, user builds the BP by hand.

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
---

## Session End: 20260622_234827
### Uncommitted Changes
.vscode/compileCommands_Default.json
.vscode/compileCommands_Default/Voxel.1.rsp
.vscode/compileCommands_Voxel.json
.vscode/compileCommands_Voxel/Voxel.1.rsp
Source/Voxel/VoxelChunk.cpp
Source/Voxel/VoxelChunk.h
docs/voxel-build-log.md
production/session-logs/session-log.md
production/session-state/active.md
---

## Archived Session State: 20260622_234938
# Voxel World — Session Handoff

<!-- The session-start hook previews this file. Read it first, then docs/voxel-build-log.md. -->

**Last update:** 2026-06-22 · **Stop reason:** Phase 3a C++ written, awaiting build + BP.

## TL;DR — where we are
Building a **cubic (Minecraft-style) voxel world** in **Unreal Engine 5.7**, from-scratch
C++, rendering chunks via the **RealtimeMeshComponent** plugin. Phase 2 slice DONE. **Phase 3a
(streaming) C++ is written** — `AVoxelWorld` (streaming manager, pooled, threaded), `FVoxelGenerator`,
`FVoxelMesher`, and a refactored poolable `AVoxelChunk`. **Next: build + restart, make `BP_VoxelWorld`,
delete `VoxelChunk_Slice`, PIE-test streaming.** Chosen: full-3D spherical streaming, radius 6→8,
pooled, user builds the BP by hand.

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
---

## Session End: 20260622_234938
### Uncommitted Changes
.vscode/compileCommands_Default.json
.vscode/compileCommands_Default/Voxel.1.rsp
.vscode/compileCommands_Voxel.json
.vscode/compileCommands_Voxel/Voxel.1.rsp
Source/Voxel/VoxelChunk.cpp
Source/Voxel/VoxelChunk.h
docs/voxel-build-log.md
production/session-logs/session-log.md
production/session-state/active.md
---

## Archived Session State: 20260622_235200
# Voxel World — Session Handoff

<!-- The session-start hook previews this file. Read it first, then docs/voxel-build-log.md. -->

**Last update:** 2026-06-22 · **Stop reason:** Phase 3a C++ written, awaiting build + BP.

## TL;DR — where we are
Building a **cubic (Minecraft-style) voxel world** in **Unreal Engine 5.7**, from-scratch
C++, rendering chunks via the **RealtimeMeshComponent** plugin. Phase 2 slice DONE. **Phase 3a
(streaming) C++ is written** — `AVoxelWorld` (streaming manager, pooled, threaded), `FVoxelGenerator`,
`FVoxelMesher`, and a refactored poolable `AVoxelChunk`. **Next: build + restart, make `BP_VoxelWorld`,
delete `VoxelChunk_Slice`, PIE-test streaming.** Chosen: full-3D spherical streaming, radius 6→8,
pooled, user builds the BP by hand.

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
---

## Session End: 20260622_235200
### Uncommitted Changes
.vscode/compileCommands_Default.json
.vscode/compileCommands_Default/Voxel.1.rsp
.vscode/compileCommands_Voxel.json
.vscode/compileCommands_Voxel/Voxel.1.rsp
Source/Voxel/VoxelChunk.cpp
Source/Voxel/VoxelChunk.h
docs/voxel-build-log.md
production/session-logs/session-log.md
production/session-state/active.md
---

## Archived Session State: 20260623_000005
# Voxel World — Session Handoff

<!-- The session-start hook previews this file. Read it first, then docs/voxel-build-log.md. -->

**Last update:** 2026-06-22 · **Stop reason:** Phase 3a C++ written, awaiting build + BP.

## TL;DR — where we are
Building a **cubic (Minecraft-style) voxel world** in **Unreal Engine 5.7**, from-scratch
C++, rendering chunks via the **RealtimeMeshComponent** plugin. Phase 2 slice DONE. **Phase 3a
(streaming) C++ is written** — `AVoxelWorld` (streaming manager, pooled, threaded), `FVoxelGenerator`,
`FVoxelMesher`, and a refactored poolable `AVoxelChunk`. **Next: build + restart, make `BP_VoxelWorld`,
delete `VoxelChunk_Slice`, PIE-test streaming.** Chosen: full-3D spherical streaming, radius 6→8,
pooled, user builds the BP by hand.

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
---

## Session End: 20260623_000005
### Uncommitted Changes
.vscode/compileCommands_Default.json
.vscode/compileCommands_Default/Voxel.1.rsp
.vscode/compileCommands_Voxel.json
.vscode/compileCommands_Voxel/Voxel.1.rsp
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/B/3S/0MAGR2WERX1YLZV6MMV5E9.uasset
Source/Voxel/VoxelChunk.cpp
Source/Voxel/VoxelChunk.h
docs/voxel-build-log.md
production/session-logs/session-log.md
production/session-state/active.md
---

## Archived Session State: 20260623_000536
# Voxel World — Session Handoff

<!-- The session-start hook previews this file. Read it first, then docs/voxel-build-log.md. -->

**Last update:** 2026-06-22 · **Stop reason:** Phase 3a C++ written, awaiting build + BP.

## TL;DR — where we are
Building a **cubic (Minecraft-style) voxel world** in **Unreal Engine 5.7**, from-scratch
C++, rendering chunks via the **RealtimeMeshComponent** plugin. Phase 2 slice DONE. **Phase 3a
(streaming) C++ is written** — `AVoxelWorld` (streaming manager, pooled, threaded), `FVoxelGenerator`,
`FVoxelMesher`, and a refactored poolable `AVoxelChunk`. **Next: build + restart, make `BP_VoxelWorld`,
delete `VoxelChunk_Slice`, PIE-test streaming.** Chosen: full-3D spherical streaming, radius 6→8,
pooled, user builds the BP by hand.

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
---

## Session End: 20260623_000536
### Uncommitted Changes
.vscode/compileCommands_Default.json
.vscode/compileCommands_Default/Voxel.1.rsp
.vscode/compileCommands_Voxel.json
.vscode/compileCommands_Voxel/Voxel.1.rsp
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/B/3S/0MAGR2WERX1YLZV6MMV5E9.uasset
Source/Voxel/VoxelChunk.cpp
Source/Voxel/VoxelChunk.h
docs/voxel-build-log.md
production/session-logs/session-log.md
production/session-state/active.md
---

## Archived Session State: 20260623_000845
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
---

## Session End: 20260623_000845
### Uncommitted Changes
.vscode/compileCommands_Default.json
.vscode/compileCommands_Default/Voxel.1.rsp
.vscode/compileCommands_Voxel.json
.vscode/compileCommands_Voxel/Voxel.1.rsp
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/0/9R/UKIMD8EYGPZFS8OC6GX24A.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/1/EU/X5MT1GOX9C5UUGZAKRVC3R.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/1/K3/79QZQ7KG2RXNWMD6RSPKUM.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/1/KL/UYSRPQ5SOYIFVLEQINPH31.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/1/MV/2U2IE6JXO1320Q32RJQKSE.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/1/X5/JDOWWCJ7DZVSHT4Y29GQ4D.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/2/AX/QEPSHOMS1K2ANUR3LXWOJM.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/2/TV/DYUIEBPX2N8UFXT326WX2O.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/2/V0/9XTUAMN5S6UUCFB5K0ZGZM.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/3/C5/3LNK9WKCBL4YVMWIBXQZ1I.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/3/D3/XWEMBL7KZTNQP83W1KTMXE.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/3/E2/L159AMTR83MORHTF2GEOKE.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/3/L5/790AP8RYBMX9YBLGTW6WE6.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/4/CQ/I205QOSSXONYWBP69RVMVA.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/5/25/3IO0LF8AOO4YLSEAEN2JO1.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/5/9M/BJR5SDQDQW7QDFFPNUUMNX.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/5/KG/ETKAO35ANVW90XFH3J8GJ2.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/5/W9/ZAAGYIUQZ2W6WEO3HPXAX7.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/6/3C/KAT90QYQLFOHNMOF9BEDZK.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/6/8I/HFL05AWIVBP8M8UXTV4H5K.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/6/G9/77SUGCMHUBU5E7FP1DLCVD.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/6/UK/OT5UOZ3WBJ9P30TU3DBDEK.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/6/XM/SN8KJL01L24VVIYC52MNFR.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/6/YB/C5UDZYKFL5M0YW62JXMPIZ.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/7/GV/W5DNM2FP1ON7VGU0C2BGMV.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/7/JD/OCW0UZT4ANZZD9YHEAOMAP.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/7/O7/IHUOIGG1MW1YZY097JJ8O7.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/7/XU/RW3P9HUUCOCG5TL3TPG6XF.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/8/0N/1OONKTGJMLVBN21M6JE59I.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/8/7A/C0UVMH95QVQO1KMTVK7MSB.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/8/HN/XXZLD30WH49IXWEHJXSVAR.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/8/WI/FHUVUALB8C8XRJ0PY9XXTI.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/8/XH/SKCKQTQDTE8ZNW6C0FCBRO.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/9/6V/AV0JKCFBVPU58UEBGB0GC0.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/9/EE/OCR348BHEJWLHO72TR249H.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/9/UM/1F7R5WQFM2D9KH7A94JFBU.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/A/3J/Q8CQM512VVS7UZZW0KQRSV.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/A/8N/4L4W0MLV9WFUGRTMUBLBOP.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/A/BN/LKN4IB4C3UPW62GHBEKTIW.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/A/IP/MGVQPE8M923OBBJLZBI1PC.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/B/0L/IL99N1JAI1J4A2DFQBQL53.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/B/2F/XTDQUHCYJJV6JCH4OJSVLG.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/B/3S/0MAGR2WERX1YLZV6MMV5E9.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/B/HX/PES0ONLF01PSQTJCC5USD2.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/B/P1/LRQT09TOYHOXK9DY22EIKE.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/B/RX/297QBXCKTP06G7NIHIULK4.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/C/27/6ZCCZ1F7ZO6DM3C8OLVGTF.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/C/HY/Q7CPP7VHOXBAMAJI90W626.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/D/13/EPTYZCFZWXT7Z7XYKZ88XK.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/D/50/395Y4QE4IIEWI3KSE3SB6L.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/D/8U/VKFLMM0CV8ZCC0P3TQGTTM.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/D/K5/NQBPXM7AM8N39CBW4ZT57P.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/D/M3/6T1XG4EDG4ZEE1U9HMYL4C.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/D/MG/EDRUPW3EOYDDSI6HNSIVT1.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/E/JR/W7S9610Y7RWELGF93FZYAE.uasset
Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/E/SS/QXUZ4IXFNKHCYFWERA1HQA.uasset
Source/Voxel/VoxelChunk.cpp
Source/Voxel/VoxelChunk.h
docs/voxel-build-log.md
production/session-logs/session-log.md
production/session-state/active.md
---

