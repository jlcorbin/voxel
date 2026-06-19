// Voxel vertical-slice chunk: generates one 32^3 cubic chunk and renders it via
// RealtimeMeshComponent using a naive face-culling mesher. Phase 2 slice — no
// streaming, no editing, single-material. Generation/meshing are pure-data passes
// so they drop onto a worker thread unchanged when streaming lands in Phase 3a.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoxelChunk.generated.h"

class URealtimeMeshComponent;
class UMaterialInterface;

UCLASS()
class VOXEL_API AVoxelChunk : public AActor
{
	GENERATED_BODY()

public:
	AVoxelChunk();

	// --- Tuning knobs (the future BP_VoxelWorld wrapper edits these) ---

	/** Voxels per axis. 32 -> a 32^3 chunk (32 KB of voxel data). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel", meta = (ClampMin = "1", ClampMax = "64"))
	int32 ChunkSize = 32;

	/** Size of one voxel in Unreal units. 100 = 1 m, Minecraft-style. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel", meta = (ClampMin = "1.0"))
	float VoxelSize = 100.0f;

	/** Baseline solid height in voxels before the heightmap is applied. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel")
	int32 BaseHeight = 8;

	/** Hill amplitude in voxels for the slice heightmap. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel")
	int32 HeightAmplitude = 6;

	/** Frequency of the placeholder sine heightmap (real noise arrives in Phase 3b). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel")
	float NoiseScale = 0.15f;

	/** Material for the chunk surface. Defaults to the engine WorldGrid material. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel")
	TObjectPtr<UMaterialInterface> VoxelMaterial = nullptr;

	/** The runtime mesh this chunk renders into (root component). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel")
	TObjectPtr<URealtimeMeshComponent> RealtimeMeshComponent;

	/** Generate voxel data + rebuild the mesh now. Usable from the editor details panel. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Voxel")
	void RegenerateChunk();

protected:
	virtual void BeginPlay() override;

private:
	// Flat voxel storage: 1 = solid, 0 = air. Index = X + Y*Size + Z*Size*Size.
	TArray<uint8> Voxels;

	FORCEINLINE int32 VoxelIndex(int32 X, int32 Y, int32 Z) const
	{
		return X + Y * ChunkSize + Z * ChunkSize * ChunkSize;
	}

	/** Out-of-bounds reads count as air, so the chunk's outer shell stays closed. */
	bool IsSolid(int32 X, int32 Y, int32 Z) const;

	void GenerateVoxels(); // fill Voxels from the slice heightmap
	void GenerateMesh();   // naive face-cull mesher -> RealtimeMesh
};
