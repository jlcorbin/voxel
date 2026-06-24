// A single rendered voxel chunk: a poolable holder around one RealtimeMeshComponent.
// It no longer generates anything itself — AVoxelWorld feeds it a finished mesh
// (built on a worker thread) via ApplyMesh(). Generation/meshing live in
// VoxelGenerator.h / VoxelMesher.h.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoxelChunk.generated.h"

class URealtimeMeshComponent;
class UMaterialInterface;

// Forward-declared so the heavy RealtimeMesh headers stay out of this header.
namespace RealtimeMesh { struct FRealtimeMeshStreamSet; }

UCLASS()
class VOXEL_API AVoxelChunk : public AActor
{
	GENERATED_BODY()

public:
	AVoxelChunk();

	/** Replace this chunk's geometry with a finished stream set (game thread). */
	void ApplyMesh(RealtimeMesh::FRealtimeMeshStreamSet&& Streams, UMaterialInterface* Material);

	/** Called when the chunk is returned to the pool. */
	void ClearMesh();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel")
	TObjectPtr<URealtimeMeshComponent> RealtimeMeshComponent;
};
