# Voxel World — Architecture Proposal (Phase 1)

> **STATUS: READ-ONLY INVESTIGATION. NOTHING HAS BEEN BUILT.**
> This is a decision document. Each of the 8 axes below lists options, tradeoffs,
> and a recommendation. **You pick; then Phase 2 begins.** No code, assets, or
> Blueprints exist yet. Engine confirmed: **Unreal Engine 5.7**.

Date: 2026-06-18 · Author: Claude (collaborative protocol) · Reviewed against:
unreal-specialist domain, `technical-preferences.md` (60 FPS / 16.6 ms / ≤~3000 draw calls).

---

## TL;DR Recommendation

A **from-scratch C++ CUBIC voxel engine**, greedy-meshed into
**RealtimeMeshComponent** chunks, generated and meshed on **background threads**,
streamed around the player. C++ owns data/generation/meshing; Blueprint owns
spawning and tuning knobs. This is the path that (a) supports add/remove block
editing in Phase 3, and (b) is defensible at 60 FPS. **Voxel Plugin 2 is ruled
out for cubic** (see Axis 2).

If you actually want **smooth/volumetric** terrain instead, the recommendation
flips toward evaluating Voxel Plugin 2 — but with eyes open about its cost and
stability. **Axis 1 is therefore the pivot that decides everything else.**

---

## The 8 Decisions

### 1. World style — CUBIC vs SMOOTH  ⟵ *the pivotal choice*

| | CUBIC (Minecraft blocks) | SMOOTH (volumetric / marching cubes) |
|---|---|---|
| Look | Blocky, readable, stylized | Organic hills, caves, overhangs |
| Editing | Trivial & exact (set voxel = solid/air) | Harder (density fields, re-polygonize) |
| Meshing | Greedy meshing of quads (cheap, well-trodden) | Marching Cubes / Transvoxel + LOD seams |
| Plugin support | **No** mature plugin (VP2 dropped it) | Voxel Plugin 2 (paid, buggy) or custom |
| Phase 3 fit | Editing is a natural extension | Editing is a research project |

**Recommendation: CUBIC.** Your Phase 3 explicitly wants add/remove voxel editing
and biomes — cubic makes editing a one-line data write and re-mesh, where smooth
makes it a density-field surgery problem. Cubic is also the safer 60-FPS target
because greedy meshing is simple and predictable.
**→ DECISION: ______ (CUBIC recommended / SMOOTH)**

---

### 2. Plugin vs from-scratch C++

- **Voxel Plugin 2 (Phyronnaz):** **dropped cubic support entirely** — V1.2's cubic
  & MagicaVoxel workflows are gone; V2 is smooth-only (Nanite/Lumen/marching).
  Docs themselves say it is "actively being developed and can be buggy at times."
  Pro source access is **paid (~$299+)**; a free open-source `VoxelCore` exists but
  is smooth-only. UE 5.6/5.7 compatible.
- **Legacy Voxel Plugin 1.2:** free, supports cubic, but **archived / unmaintained**
  and not built for 5.7 — a dead end for a long-lived project.
- **From-scratch C++:** full control, no license cost, exactly fits cubic + editing.
  More upfront code (chunking, meshing, threading), but all well-documented patterns.

**Recommendation (given CUBIC): from-scratch C++.** VP2 can't do cubic; legacy 1.2
is a maintenance trap. If Axis 1 becomes SMOOTH, re-open VP2 as a candidate despite
the buggy/paid caveats.
**→ DECISION: ______ (FROM-SCRATCH C++ recommended / VP2 if smooth)**

---

### 3. Meshing & rendering — the 60-FPS core

Candidates for putting chunk geometry on screen:

| Approach | Verdict |
|---|---|
| **ISM/HISM (one instance per cube)** | ❌ Simple to start, but millions of instances + massive overdraw and hidden internal faces. Dies at scale. |
| **Nanite per-cube / per-chunk** | ❌ Nanite shines on dense smooth meshes, not millions of axis-aligned quads; overkill and wasteful for cubic. |
| **ProceduralMeshComponent (PMC)** | ⚠️ Works, but memory-heavy and slow update path. Fine for a prototype, not the target. |
| **RealtimeMeshComponent (RMC, TriAxis)** | ✅ ~50% less memory than PMC, async updates, built-in 1–8 LODs, better collision path, UE5.7-supported. Community default for serious UE voxel work. |

**Algorithm:** **greedy meshing** — merge coplanar same-type faces into big quads.
Collapses an 8³ solid cube's 384 faces → as few as 6; routinely 10–30× fewer tris.
Pitfalls to design for up front: **texture atlas** (per-block-type UVs in one
material so greedy merges don't break), and **ambient occlusion baked into vertex
colors** per quad corner.

**Recommendation:** **RealtimeMeshComponent + greedy meshing.** Pragmatic ramp:
the Phase 2 slice uses **naive face-culling** (skip faces between two solid voxels)
to prove the pipeline, then we swap in greedy meshing as a drop-in optimization
once one chunk renders. RMC is the component from day one (no PMC→RMC rewrite later).
RMC is a **plugin dependency** we'll add (free, open-source core).
**→ DECISION: ______ (RMC + greedy, naive-first recommended / other)**

---

### 4. Chunk design

- **Chunk dimensions:** **32×32×32 voxels.** Sweet spot — fewer draw calls than 16³,
  better frustum culling than 64³, 32 KB data at 1 byte/voxel.
- **Voxel size:** **100 uu (1 m)** like Minecraft → a chunk is a 32 m cube.
- **Draw distance:** horizontal radius **8 chunks** (configurable) ≈ 17×17 columns;
  vertical height **4–8 chunks** (128–256 m world height).
- **Load/unload:** main thread tracks the player's chunk coord; a ring of chunks
  within render radius is kept loaded, beyond it unloaded. Collision cooked only
  for a **smaller radius** around the player than the render radius.
- **Threading (non-negotiable for 60 FPS):** voxel generation **and** greedy meshing
  run on **background threads** (`UE::Tasks::Launch`, UE 5.0+). The game thread only
  (a) reads player position, (b) applies finished mesh buffers to RMC, throttled to
  a few chunks/frame to avoid hitches. **UObjects are game-thread-only** — workers
  operate on plain C++ data, never UObjects.

**Recommendation:** 32³ chunks, 1 m voxels, render radius 8, async gen+mesh, throttled
apply. All values exposed as tuning knobs (Axis 7).
**→ DECISION: ______ (recommended as-is / adjust dimensions or radius)**

---

### 5. Voxel data structure

| Structure | Memory / 32³ chunk | Notes |
|---|---|---|
| **Flat 3D array (uint8)** | 32 KB | O(1) access, dead simple, ideal for runtime gen & editing |
| **Palette + indices** | ~8–16 KB typical | Compresses repeated block types; small lookup cost |
| **RLE** | tiny for flat/empty regions | Bad random access — poor for editing |
| **Octree / SVO** | smallest dense-scene cost | Pointer chasing; complexity not worth it for cubic runtime |

**Recommendation:** **flat `uint8` array at runtime** (32 KB/chunk; ~32 MB per 1000
loaded chunks — comfortable). Add **palette/RLE compression only at the save layer**
later if disk size matters. Don't pay octree complexity for a cubic world.
**→ DECISION: ______ (flat array recommended / palette / octree)**

---

### 6. Terrain generation

- **Library:** **FastNoiseLite** — clean C++/Blueprint UE integration, all noise
  types (Perlin, OpenSimplex2). (FastNoise2 is faster via SIMD but heavier to
  integrate; revisit only if noise becomes a profiled bottleneck.)
- **Phase 2 slice:** flat or a **single 2D Perlin heightmap** — surface elevation
  per (x,y), fill solid below, air above. Enough to prove the pipeline.
- **Phase 3b:** add **3D density noise** layered on the heightmap for caves &
  overhangs (negative density = air), then **biomes** via low-frequency noise
  selecting block palettes / height params.
- Perlin vs Simplex: OpenSimplex2 has fewer directional artifacts; either is fine —
  pick per look during tuning.

**Recommendation:** FastNoiseLite; 2D heightmap first, 3D density + biomes later.
FastNoiseLite is a small **third-party dependency** (single header / plugin).
**→ DECISION: ______ (FastNoiseLite recommended / FastNoise2 / hand-rolled)**

---

### 7. C++ / Blueprint split

- **C++ (performance & correctness):** voxel data types, chunk storage, the chunk
  manager (load/unload ring, threading), noise generation, greedy mesher, RMC
  section updates, collision cooking, save/load. Everything in the hot path.
- **Blueprint (orchestration & tuning):** a thin `BP_VoxelWorld` actor that subclasses
  the C++ world manager, exposes tuning knobs (seed, chunk size, render radius,
  voxel size, material) as `EditAnywhere` UPROPERTYs, and is what you drag into the
  level / spawn for PIE. Possibly a debug HUD later.

**Recommendation:** as above — no gameplay logic in Blueprint that runs per-voxel or
per-frame across chunks. Blueprint is the control panel, C++ is the engine.
**→ DECISION: ______ (recommended split / adjust)**

---

### 8. Performance budget (the acceptance bar)

Target: **60 FPS / 16.6 ms**, consistent with `technical-preferences.md`.

| Metric | Budget |
|---|---|
| Voxel size / chunk | 1 m / 32³ (32 m cube) |
| Render radius | 8 chunks horizontal, 4–8 vertical |
| Loaded chunks | ≤ ~2,500 (≈ ≤ 80 MB voxel data) |
| Visible chunk sections (post-cull) | ≤ ~800 |
| **Draw calls** | **≤ 3,000** (project PC budget) |
| Per-chunk greedy mesh (worker thread) | ≤ ~3 ms, off game thread |
| Mesh applies on game thread | ≤ 2–4 chunks/frame (≤ ~2 ms) |
| Collision | simple/complex cooked **async**, player-radius only |
| Frame time | ≤ 16.6 ms with headroom for gameplay |

**These numbers are the pass/fail gate for every Phase 2/3 slice** — we measure with
`stat unit`, `stat scenerendering`, and viewport captures, not vibes.
**→ DECISION: ______ (adopt budget / adjust targets)**

---

## Dependencies this implies (for your awareness — installed only after approval)

1. **RealtimeMeshComponent** (free, open-source; Fab/GitHub) — chunk rendering.
2. **FastNoiseLite** (single-header / small plugin) — terrain noise.
3. A **C++ game module** must exist (this project is currently Blueprint-only at the
   code level — confirmed no `Source/`). Phase 2 step 1 is creating that module,
   which is a **manual editor/IDE action** (mcp-unreal cannot build C++).

Both libs go in `technical-preferences.md → Allowed Libraries` once you approve them.

---

## What Phase 2 will be (preview — not started)

ONE chunk (32³), single 2D-noise or flat layer, generated in C++, greedy/culled mesh
into one RMC actor, visible in PIE, standing on it with the ThirdPerson character.
No streaming, no editing. I'll split it into "what you do by hand in the editor"
(create C++ module + classes, enable plugins, build) vs "what I can do via mcp-unreal"
(spawn the actor, set tuning props, capture the viewport to verify).

---

## Decisions needed from you

Reply with picks for **Axes 1–8** (just "all recommended" works, or override any).
Axis 1 (CUBIC vs SMOOTH) is the one that can re-shape the rest — if you choose SMOOTH,
I'll revise Axes 2–8 before we proceed.
