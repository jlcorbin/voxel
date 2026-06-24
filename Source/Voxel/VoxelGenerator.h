// Pure, copyable, thread-safe voxel world generator. Coordinates are in VOXELS
// (not Unreal units). Solidity is a deterministic function of world position, so
// chunks tile seamlessly and worker threads can sample neighbouring voxels without
// any adjacent chunk being loaded. Placeholder sine heightmap — real FastNoiseLite
// terrain (2D height + 3D density) replaces HeightAt()/IsSolid() in Phase 3b.

#pragma once

#include "CoreMinimal.h"

struct FVoxelGenerator
{
	int32 ChunkSize = 32;       // voxels per chunk axis
	int32 BaseHeight = 8;       // baseline surface height (world Z, voxels)
	int32 HeightAmplitude = 6;  // hill amplitude (voxels)
	float NoiseScale = 0.02f;   // world-space frequency of the placeholder heightmap

	FORCEINLINE int32 HeightAt(int32 WorldX, int32 WorldY) const
	{
		const float H = BaseHeight + HeightAmplitude * 0.5f *
			(FMath::Sin(WorldX * NoiseScale) + FMath::Cos(WorldY * NoiseScale));
		return FMath::RoundToInt(H);
	}

	FORCEINLINE bool IsSolid(int32 WorldX, int32 WorldY, int32 WorldZ) const
	{
		return WorldZ <= HeightAt(WorldX, WorldY);
	}

	enum class EChunkClass : uint8
	{
		AllAir,    // entirely above the surface — no solids
		Interior,  // entirely below the surface — solid with no exposed faces
		Surface,   // straddles the surface — has geometry, must be meshed
	};

	// Cheap classification (no per-voxel meshing) so trivial chunks never spawn an
	// actor or build a mesh. Conservative: never classifies a chunk with exposed
	// faces as AllAir/Interior. Samples the footprint expanded by one voxel so the
	// Interior test also accounts for horizontal neighbours.
	EChunkClass Classify(const FIntVector& C) const
	{
		const int32 X0 = C.X * ChunkSize;
		const int32 Y0 = C.Y * ChunkSize;

		int32 MinH = MAX_int32;
		int32 MaxH = MIN_int32;
		for (int32 X = X0 - 1; X <= X0 + ChunkSize; ++X)
		{
			for (int32 Y = Y0 - 1; Y <= Y0 + ChunkSize; ++Y)
			{
				const int32 H = HeightAt(X, Y);
				MinH = FMath::Min(MinH, H);
				MaxH = FMath::Max(MaxH, H);
			}
		}

		const int32 ZMin = C.Z * ChunkSize;
		const int32 ZMax = ZMin + ChunkSize - 1;

		if (ZMin > MaxH)      return EChunkClass::AllAir;    // lowest voxel is above every column
		if (ZMax + 1 <= MinH) return EChunkClass::Interior;  // chunk + its neighbours all solid
		return EChunkClass::Surface;
	}
};
