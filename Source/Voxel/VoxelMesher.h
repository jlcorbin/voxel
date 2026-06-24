// Pure, thread-safe meshing for one chunk. Builds an FRealtimeMeshStreamSet (a
// CPU-side data structure — safe to construct off the game thread) using naive face
// culling, sampling neighbour solidity directly from the generator so cross-chunk
// borders never render hidden faces and never depend on adjacent chunks being loaded.

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "VoxelGenerator.h"
#include "Interface/Core/RealtimeMeshBuilder.h" // RealtimeMesh::FRealtimeMeshStreamSet (by value below)

// Result of one chunk meshing task. Move-only (FRealtimeMeshStreamSet is non-copyable).
struct FVoxelChunkResult
{
	FIntVector Coord = FIntVector::ZeroValue;
	uint32 Token = 0;
	bool bHasGeometry = false;
	RealtimeMesh::FRealtimeMeshStreamSet Streams;

	FVoxelChunkResult() = default;
	FVoxelChunkResult(FVoxelChunkResult&&) = default;
	FVoxelChunkResult& operator=(FVoxelChunkResult&&) = default;
	FVoxelChunkResult(const FVoxelChunkResult&) = delete;
	FVoxelChunkResult& operator=(const FVoxelChunkResult&) = delete;
};

// Worker threads push finished results here; the game thread drains it each tick.
// Held via TSharedPtr so it outlives AVoxelWorld even if a task is still in flight.
class FVoxelApplyQueue
{
public:
	FCriticalSection Mutex;
	TArray<FVoxelChunkResult> Results;
};

namespace FVoxelMesher
{
	// Builds OutStreams for the chunk at Coord. Positions are chunk-local (the owning
	// actor is placed at the chunk's world origin). Returns true if any geometry was
	// emitted. Safe to call from any thread.
	bool Build(const FVoxelGenerator& Gen, const FIntVector& Coord, float VoxelSize,
		RealtimeMesh::FRealtimeMeshStreamSet& OutStreams);
}
