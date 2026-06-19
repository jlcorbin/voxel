# Technical Preferences

<!-- Populated by /setup-engine. Updated as the user makes decisions throughout development. -->
<!-- All agents reference this file for project-specific standards and conventions. -->

## Engine & Language

- **Engine**: Unreal Engine 5.7
- **Language**: C++ (primary), Blueprint (gameplay prototyping)
- **Rendering**: Lumen (GI/reflections) + Nanite (virtualized geometry) — UE 5.7 defaults; revisit per target hardware
- **Physics**: Chaos (UE 5 default)

## Input & Platform

<!-- Written by /setup-engine. Read by /ux-design, /ux-review, /test-setup, /team-ui, and /dev-story -->
<!-- to scope interaction specs, test helpers, and implementation to the correct input methods. -->

- **Target Platforms**: [TO BE CONFIGURED — platform not yet decided]
- **Input Methods**: Keyboard/Mouse
- **Primary Input**: Keyboard/Mouse
- **Gamepad Support**: [TO BE CONFIGURED — set when platform target is decided]
- **Touch Support**: [TO BE CONFIGURED — set when platform target is decided]
- **Platform Notes**: [TO BE CONFIGURED — set when platform target is decided]

## Naming Conventions

<!-- Unreal Engine C++ conventions -->
- **Classes**: Prefixed PascalCase (`A`=Actor, `U`=UObject, `F`=struct, `E`=enum — e.g., `APlayerController`)
- **Variables**: PascalCase (e.g., `MoveSpeed`); Booleans `b`-prefixed (e.g., `bIsAlive`)
- **Signals/Events**: Delegates, `On`-prefixed PascalCase (e.g., `OnHealthChanged`)
- **Files**: Match class without prefix (e.g., `PlayerController.h` / `PlayerController.cpp`)
- **Scenes/Prefabs**: Levels `.umap`; content assets type-prefixed PascalCase (`BP_`, `M_`, `T_`, `SM_`)
- **Constants**: UPPER_SNAKE_CASE or `constexpr` PascalCase

## Performance Budgets

- **Target Framerate**: 60 FPS
- **Frame Budget**: 16.6 ms
- **Draw Calls**: ≤ ~3000 (PC target; revisit per hardware/platform)
- **Memory Ceiling**: [TO BE CONFIGURED — set when target hardware known]

## Testing

- **Framework**: Unreal Automation (Automation Spec + Functional Tests)
- **Minimum Coverage**: [TO BE CONFIGURED]
- **Required Tests**: Balance formulas, gameplay systems, networking (if applicable)

## Forbidden Patterns

<!-- Add patterns that should never appear in this project's codebase -->
- [None configured yet — add as architectural decisions are made]

## Allowed Libraries / Addons

<!-- Add approved third-party dependencies here -->
- **RealtimeMeshComponent** (TriAxis Games, free/open-source) — runtime chunk mesh rendering for the voxel engine. Approved 2026-06-18; integration begins Phase 2. Replaces ProceduralMeshComponent (lower memory, async updates, built-in LODs).
- **FastNoiseLite** (single-header, MIT) — terrain noise. Approved 2026-06-18; integration deferred to Phase 3b (real terrain generation). Not yet added to the build.

## Architecture Decisions Log

<!-- Quick reference linking to full ADRs in docs/architecture/ -->
- [No ADRs yet — use /architecture-decision to create one]

## Engine Specialists

<!-- Written by /setup-engine when engine is configured. -->
<!-- Read by /code-review, /architecture-decision, /architecture-review, and team skills -->
<!-- to know which specialist to spawn for engine-specific validation. -->

- **Primary**: unreal-specialist
- **Language/Code Specialist**: ue-blueprint-specialist (Blueprint graphs) or unreal-specialist (C++)
- **Shader Specialist**: unreal-specialist (no dedicated shader specialist — primary covers materials)
- **UI Specialist**: ue-umg-specialist (UMG widgets, CommonUI, input routing, widget styling)
- **Additional Specialists**: ue-gas-specialist (Gameplay Ability System, attributes, gameplay effects), ue-replication-specialist (property replication, RPCs, client prediction, netcode)
- **Routing Notes**: Invoke primary for C++ architecture and broad engine decisions. Invoke Blueprint specialist for Blueprint graph architecture and BP/C++ boundary design. Invoke GAS specialist for all ability and attribute code. Invoke replication specialist for any multiplayer or networked systems. Invoke UMG specialist for all UI implementation.

### File Extension Routing

<!-- Skills use this table to select the right specialist per file type. -->
<!-- If a row says [TO BE CONFIGURED], fall back to Primary for that file type. -->

| File Extension / Type | Specialist to Spawn |
|-----------------------|---------------------|
| Game code (.cpp, .h files) | unreal-specialist |
| Shader / material files (.usf, .ush, Material assets) | unreal-specialist |
| UI / screen files (.umg, UMG Widget Blueprints) | ue-umg-specialist |
| Scene / prefab / level files (.umap, .uasset) | unreal-specialist |
| Native extension / plugin files (Plugin .uplugin, modules) | unreal-specialist |
| Blueprint graphs (.uasset BP classes) | ue-blueprint-specialist |
| General architecture review | unreal-specialist |
