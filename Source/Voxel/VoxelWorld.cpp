#include "VoxelWorld.h"

#include "VoxelChunk.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Tasks/Task.h"
#include "Misc/ScopeLock.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AVoxelWorld::AVoxelWorld()
{
	PrimaryActorTick.bCanEverTick = true;

	// Default surface material (overridable per-instance on BP_VoxelWorld).
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(
		TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"));
	if (MatFinder.Succeeded())
	{
		VoxelMaterial = MatFinder.Object;
	}
}

void AVoxelWorld::BeginPlay()
{
	Super::BeginPlay();

	Generator.ChunkSize = ChunkSize;
	Generator.BaseHeight = BaseHeight;
	Generator.HeightAmplitude = HeightAmplitude;
	Generator.NoiseScale = NoiseScale;

	ApplyQueue = MakeShared<FVoxelApplyQueue, ESPMode::ThreadSafe>();
}

void AVoxelWorld::EndPlay(const EEndPlayReason::Type Reason)
{
	for (TPair<FIntVector, FVoxelChunkSlot>& Pair : Chunks)
	{
		if (Pair.Value.Actor)
		{
			Pair.Value.Actor->Destroy();
		}
	}
	Chunks.Empty();

	for (AVoxelChunk* C : Pool)
	{
		if (C)
		{
			C->Destroy();
		}
	}
	Pool.Empty();

	// In-flight worker tasks keep their own shared ref to the queue; dropping ours
	// is safe — their results are simply discarded.
	ApplyQueue.Reset();

	Super::EndPlay(Reason);
}

void AVoxelWorld::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	FVector PlayerLoc;
	if (!GetPlayerLocation(PlayerLoc))
	{
		return;
	}

	// One-time: drop the player onto the surface so it never spawns inside the terrain.
	if (bAutoPlacePlayer && !bPlayerPlaced && TryPlacePlayer(PlayerLoc))
	{
		bPlayerPlaced = true;
		GetPlayerLocation(PlayerLoc); // refresh after the teleport
	}

	const FIntVector PlayerChunk = WorldToChunk(PlayerLoc);
	if (!bHasLastPlayer || PlayerChunk != LastPlayerChunk)
	{
		RecomputeDesired(PlayerChunk);
		LastPlayerChunk = PlayerChunk;
		bHasLastPlayer = true;
	}

	DispatchLoads();
	DrainAndApply();
}

FIntVector AVoxelWorld::WorldToChunk(const FVector& Loc) const
{
	const float ChunkExtent = ChunkSize * VoxelSize;
	return FIntVector(
		FMath::FloorToInt(Loc.X / ChunkExtent),
		FMath::FloorToInt(Loc.Y / ChunkExtent),
		FMath::FloorToInt(Loc.Z / ChunkExtent));
}

FVector AVoxelWorld::ChunkToWorld(const FIntVector& C) const
{
	const float ChunkExtent = ChunkSize * VoxelSize;
	return FVector(C.X * ChunkExtent, C.Y * ChunkExtent, C.Z * ChunkExtent);
}

bool AVoxelWorld::GetPlayerLocation(FVector& Out) const
{
	if (const UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (const APawn* Pawn = PC->GetPawn())
			{
				Out = Pawn->GetActorLocation();
				return true;
			}
			FVector CamLoc;
			FRotator CamRot;
			PC->GetPlayerViewPoint(CamLoc, CamRot);
			Out = CamLoc;
			return true;
		}
	}
	return false;
}

bool AVoxelWorld::TryPlacePlayer(const FVector& CurrentLoc)
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	APlayerController* PC = World->GetFirstPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		return false; // pawn not possessed yet — retry next tick
	}

	const int32 VX = FMath::FloorToInt(CurrentLoc.X / VoxelSize);
	const int32 VY = FMath::FloorToInt(CurrentLoc.Y / VoxelSize);
	const int32 SurfaceVoxelZ = Generator.HeightAt(VX, VY);
	const float TargetZ = (SurfaceVoxelZ + 1) * VoxelSize + PlayerSpawnClearance;

	Pawn->SetActorLocation(
		FVector(CurrentLoc.X, CurrentLoc.Y, TargetZ),
		/*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
	return true;
}

void AVoxelWorld::RecomputeDesired(const FIntVector& Center)
{
	Desired.Reset();
	const int32 R = RenderRadius;
	const int32 R2 = R * R;
	for (int32 dz = -R; dz <= R; ++dz)
	{
		for (int32 dy = -R; dy <= R; ++dy)
		{
			for (int32 dx = -R; dx <= R; ++dx)
			{
				if (dx * dx + dy * dy + dz * dz <= R2)
				{
					Desired.Add(Center + FIntVector(dx, dy, dz));
				}
			}
		}
	}

	// Unload chunks that left the radius.
	TArray<FIntVector> ToRemove;
	for (const TPair<FIntVector, FVoxelChunkSlot>& Pair : Chunks)
	{
		if (!Desired.Contains(Pair.Key))
		{
			ToRemove.Add(Pair.Key);
		}
	}
	for (const FIntVector& Coord : ToRemove)
	{
		ReleaseChunk(Chunks[Coord]);
		Chunks.Remove(Coord);
	}

	// Queue newly-desired chunks. Stale queue entries (re-unloaded before dispatch)
	// are skipped in DispatchLoads by their missing/!Queued slot.
	for (const FIntVector& Coord : Desired)
	{
		if (!Chunks.Contains(Coord))
		{
			Chunks.Add(Coord, FVoxelChunkSlot{ EVoxelChunkState::Queued, 0, nullptr });
			PendingLoads.Enqueue(Coord);
		}
	}
}

void AVoxelWorld::DispatchLoads()
{
	int32 Dispatched = 0;
	FIntVector Coord;
	while (Dispatched < MaxLoadsPerFrame && PendingLoads.Dequeue(Coord))
	{
		FVoxelChunkSlot* Slot = Chunks.Find(Coord);
		if (!Slot || Slot->State != EVoxelChunkState::Queued)
		{
			continue; // unloaded (or already dispatched) since being queued
		}

		const uint32 Token = NextToken++;
		Slot->State = EVoxelChunkState::Pending;
		Slot->Token = Token;

		const FVoxelGenerator Gen = Generator;
		const float VS = VoxelSize;
		TSharedPtr<FVoxelApplyQueue, ESPMode::ThreadSafe> Queue = ApplyQueue;

		UE::Tasks::Launch(UE_SOURCE_LOCATION, [Gen, Coord, Token, VS, Queue]()
		{
			FVoxelChunkResult Result;
			Result.Coord = Coord;
			Result.Token = Token;

			if (Gen.Classify(Coord) == FVoxelGenerator::EChunkClass::Surface)
			{
				Result.bHasGeometry = FVoxelMesher::Build(Gen, Coord, VS, Result.Streams);
			}

			FScopeLock Lock(&Queue->Mutex);
			Queue->Results.Add(MoveTemp(Result));
		});

		++Dispatched;
	}
}

void AVoxelWorld::DrainAndApply()
{
	{
		FScopeLock Lock(&ApplyQueue->Mutex);
		for (FVoxelChunkResult& R : ApplyQueue->Results)
		{
			ReadyResults.Add(MoveTemp(R));
		}
		ApplyQueue->Results.Reset();
	}

	int32 Applied = 0;
	while (Applied < MaxAppliesPerFrame && ReadyResults.Num() > 0)
	{
		FVoxelChunkResult R = ReadyResults.Pop();

		FVoxelChunkSlot* Slot = Chunks.Find(R.Coord);
		if (!Slot || Slot->Token != R.Token || Slot->State != EVoxelChunkState::Pending)
		{
			continue; // chunk was unloaded/superseded while its task ran — drop result
		}

		if (!R.bHasGeometry)
		{
			Slot->State = EVoxelChunkState::Empty; // trivial chunk, no actor needed
			continue;
		}

		AVoxelChunk* Chunk = AcquireChunk();
		Chunk->SetActorLocation(ChunkToWorld(R.Coord));
		Chunk->ApplyMesh(MoveTemp(R.Streams), VoxelMaterial);
		Slot->Actor = Chunk;
		Slot->State = EVoxelChunkState::Ready;
		++Applied;
	}
}

AVoxelChunk* AVoxelWorld::AcquireChunk()
{
	AVoxelChunk* Chunk = nullptr;
	if (Pool.Num() > 0)
	{
		Chunk = Pool.Pop();
		Chunk->SetActorHiddenInGame(false);
		Chunk->SetActorEnableCollision(true);
	}
	else
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		Chunk = GetWorld()->SpawnActor<AVoxelChunk>(AVoxelChunk::StaticClass(), FTransform::Identity, Params);
	}
	return Chunk;
}

void AVoxelWorld::ReleaseChunk(FVoxelChunkSlot& Slot)
{
	if (!Slot.Actor)
	{
		return;
	}

	AVoxelChunk* Chunk = Slot.Actor;
	Slot.Actor = nullptr;

	if (Pool.Num() < MaxPooledChunks)
	{
		Chunk->ClearMesh();
		Chunk->SetActorHiddenInGame(true);
		Chunk->SetActorEnableCollision(false);
		Pool.Add(Chunk);
	}
	else
	{
		Chunk->Destroy();
	}
}
