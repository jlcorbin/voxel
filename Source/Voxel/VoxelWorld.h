// Streams cubic voxel chunks in a spherical radius around the player. Generation
// and meshing run on worker threads (UE::Tasks); only the final RealtimeMesh apply
// runs on the game thread, throttled per frame. Chunk actors are pooled. Trivial
// (all-air / fully-interior) chunks never spawn an actor or build a mesh.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Containers/Queue.h"
#include "VoxelGenerator.h"
#include "VoxelMesher.h"
#include "VoxelWorld.generated.h"

class AVoxelChunk;
class UMaterialInterface;

enum class EVoxelChunkState : uint8
{
	Queued,   // desired, waiting to dispatch
	Pending,  // worker task in flight
	Empty,    // trivial chunk — no actor, no geometry
	Ready,    // actor placed with geometry
};

struct FVoxelChunkSlot
{
	EVoxelChunkState State = EVoxelChunkState::Queued;
	uint32 Token = 0;
	AVoxelChunk* Actor = nullptr; // rooted by the level while spawned
};

UCLASS()
class VOXEL_API AVoxelWorld : public AActor
{
	GENERATED_BODY()

public:
	AVoxelWorld();
	virtual void Tick(float DeltaSeconds) override;

	// --- Tuning knobs ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel", meta = (ClampMin = "1", ClampMax = "16"))
	int32 RenderRadius = 6;                 // in chunks; architecture target is 8

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel", meta = (ClampMin = "1", ClampMax = "64"))
	int32 ChunkSize = 32;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel", meta = (ClampMin = "1.0"))
	float VoxelSize = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel")
	int32 BaseHeight = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel")
	int32 HeightAmplitude = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel")
	float NoiseScale = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel", meta = (ClampMin = "1"))
	int32 MaxLoadsPerFrame = 32;            // worker dispatches per frame (cheap)

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel", meta = (ClampMin = "1"))
	int32 MaxAppliesPerFrame = 8;           // game-thread mesh applies per frame (the hitch budget)

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel", meta = (ClampMin = "0"))
	int32 MaxPooledChunks = 128;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel")
	TObjectPtr<UMaterialInterface> VoxelMaterial = nullptr;

	/** Snap the player onto the terrain surface once at start, so it never spawns inside the hill. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel")
	bool bAutoPlacePlayer = true;

	/** Height above the surface to drop the player from when auto-placing (Unreal units). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel")
	float PlayerSpawnClearance = 150.0f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	FVoxelGenerator Generator;
	TMap<FIntVector, FVoxelChunkSlot> Chunks;
	TQueue<FIntVector> PendingLoads;
	TSet<FIntVector> Desired;
	TArray<FVoxelChunkResult> ReadyResults;
	TSharedPtr<FVoxelApplyQueue, ESPMode::ThreadSafe> ApplyQueue;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AVoxelChunk>> Pool;

	FIntVector LastPlayerChunk = FIntVector::ZeroValue;
	bool bHasLastPlayer = false;
	bool bPlayerPlaced = false;
	uint32 NextToken = 1;

	FIntVector WorldToChunk(const FVector& Loc) const;
	FVector ChunkToWorld(const FIntVector& C) const;
	bool GetPlayerLocation(FVector& Out) const;

	// Teleports the player onto the surface at its current XY. Returns true if a pawn was moved.
	bool TryPlacePlayer(const FVector& CurrentLoc);

	void RecomputeDesired(const FIntVector& Center);
	void DispatchLoads();
	void DrainAndApply();

	AVoxelChunk* AcquireChunk();
	void ReleaseChunk(FVoxelChunkSlot& Slot);
};
