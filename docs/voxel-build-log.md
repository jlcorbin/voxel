# Voxel World — Build Log

<!-- Newest state at top. This is the single source of truth for "where are we". -->

## CURRENT STATE — 2026-06-19 (end of session)

**Phase:** 2 (Vertical Slice) — ✅ **COMPLETE.** Walk test PASSED (collision works, can
stand/walk on the chunk in PIE).
**Next:** Phase 3a — chunk streaming around the player (first real-architecture slice;
generation + meshing move to background threads). **See `production/session-state/active.md`
for the full tomorrow handoff.**

**What's proven this session:** one 32³ cubic chunk generated in C++ (sine heightmap),
naive-cull meshed into RealtimeMesh, single material, complex-as-simple collision,
visible and walkable in PIE. Capture: `Saved/voxel_slice_capture4.png` (gitignored).

**Open housekeeping for tomorrow:** the editor-placed test actor `VoxelChunk_Slice`
(world [3500,0,0]) is still in `Lvl_ThirdPerson` (only persisted if you saved the level).
It gets removed when `BP_VoxelWorld` takes over chunk spawning in 3a.

### mcp-unreal lessons learned (carry forward)
- Adding a NEW UCLASS needs an editor restart (Live Coding won't register it). Edits hot-patch.
- Remote Control API (port 30010) is OFF → `call_function` fails; use `execute_script` (Python) instead.
- `execute_script` returns no stdout and reports success even on Python exceptions →
  wrap in try/except and write results to `Saved/*.txt`, then read the file.
- `unreal.Rotator(a,b,c)` positional order is (roll, pitch, yaw) — use keywords.
- Editor viewport is not realtime by default → `capture_viewport` returns a STALE frame.
  Fix: `LevelEditorSubsystem.editor_set_viewport_realtime(True)` + `editor_invalidate_viewports()`
  before capturing. Drive camera via `UnrealEditorSubsystem.set_level_viewport_camera_info`.

### Phase 2 progress
- Step A ✅ RealtimeMeshComponent installed (Fab → Install to Engine; mounts as engine plugin).
- Step B ✅ C++ module `Voxel` created (`Source/Voxel/`), builds clean.
- Step C ✅ Code written:
  - `Source/Voxel/Voxel.Build.cs` — added `RealtimeMeshComponent` dependency.
  - `Source/Voxel/VoxelChunk.h/.cpp` — `AVoxelChunk`: flat `uint8[32³]` data, sine
    heightmap gen, naive face-cull mesher into `URealtimeMeshSimple`, complex-as-simple
    collision. Tuning knobs exposed (ChunkSize, VoxelSize, BaseHeight, amplitude, noise,
    material). `RegenerateChunk()` is CallInEditor.
  - RMC API verified against the **installed plugin headers** (version build `V19`) and the
    plugin's own box generator for correct winding/normals/tangents — not guessed.
- Step D ⏳ build (you).
- Step E ⏳ spawn + viewport capture (me).
- Step F ⏳ PIE walk test (you).

### Environment (verified Phase 0)
- Engine: Unreal Engine 5.7 ✅
- mcp-unreal bridge: online, port 8090 (NOTE: `status.editor_online` flag
  misreports `false`; editor connectivity confirmed via live `get_level_actors`).
- Project: Third Person template (`/Game/ThirdPerson/Lvl_ThirdPerson`), World
  Partition on, `PlayerStart` at Z≈302. Gives us a playable character for PIE.

### Decisions locked (2026-06-18, "all recommended")
1. World style: **CUBIC** (Minecraft-style blocks).
2. Implementation: **from-scratch C++** (Voxel Plugin 2 ruled out — no cubic).
3. Mesh/render: **RealtimeMeshComponent + greedy meshing** (slice uses naive face-culling first, greedy swapped in after one chunk renders).
4. Chunk: **32³ voxels, 1 m (100 uu) each**, render radius 8, async gen+mesh (threading lands with streaming in 3a).
5. Data: **flat `uint8` array** (32 KB/chunk); palette/RLE only at save layer later.
6. Terrain: **FastNoiseLite**, 2D heightmap → 3D density + biomes later (3b).
7. Split: **C++** = data/gen/mesh/threading; **Blueprint** = `BP_VoxelWorld` tuning + spawn.
8. Budget: 60 FPS / 16.6 ms, ≤3000 draw calls, ≤~2500 chunks, greedy mesh ≤3 ms off-thread.

### mcp-unreal capability notes (carry forward)
- CAN: status, get_level_actors, search_assets, get_asset_info, blueprint_query,
  spawn_actor, set_property (some), capture_viewport, run_console_command.
- CANNOT (→ manual steps): compile/build C++, create C++ classes/modules, set
  object-reference pins, EnhancedInput action refs, AnimGraph internals.

---

## HISTORY

### 2026-06-18 — Phase 0 + Phase 1
- Phase 0: connected, confirmed UE 5.7, project state captured.
- Phase 1: researched UE 5.7 voxel approaches (Voxel Plugin 2 status, from-scratch
  C++, RealtimeMeshComponent, greedy meshing, chunking, noise, collision). Wrote
  `docs/architecture/voxel-architecture-proposal.md` with an 8-axis decision matrix.
