#include "VoxelChunk.h"

#include "RealtimeMeshComponent.h"
#include "RealtimeMeshSimple.h"
#include "Interface/Core/RealtimeMeshBuilder.h" // FRealtimeMeshStreamSet (full type)
#include "Interface/Core/RealtimeMeshKeys.h"
#include "Interface/Core/RealtimeMeshCollision.h"
#include "Materials/MaterialInterface.h"

AVoxelChunk::AVoxelChunk()
{
	PrimaryActorTick.bCanEverTick = false;

	RealtimeMeshComponent = CreateDefaultSubobject<URealtimeMeshComponent>(TEXT("RealtimeMesh"));
	SetRootComponent(RealtimeMeshComponent);
}

void AVoxelChunk::ApplyMesh(RealtimeMesh::FRealtimeMeshStreamSet&& Streams, UMaterialInterface* Material)
{
	using namespace RealtimeMesh;

	// Re-init gives a clean slate on pool reuse (cheap; happens at most a few/frame).
	URealtimeMeshSimple* Mesh = RealtimeMeshComponent->InitializeRealtimeMesh<URealtimeMeshSimple>();
	if (Material)
	{
		Mesh->SetupMaterialSlot(0, TEXT("Primary"), Material);
	}

	const FRealtimeMeshSectionGroupKey GroupKey = FRealtimeMeshSectionGroupKey::Create(FRealtimeMeshLODKey(0), 0);
	Mesh->CreateSectionGroup(GroupKey, MoveTemp(Streams));

	// Complex-as-simple collision so the character can stand/walk on chunks.
	Mesh->SetCollisionConfig(FRealtimeMeshCollisionConfiguration());
	const FRealtimeMeshSectionKey SectionKey = FRealtimeMeshSectionKey::CreateForPolyGroup(GroupKey, 0);
	Mesh->UpdateSectionConfig(SectionKey, FRealtimeMeshSectionConfig(0), /*bShouldCreateCollision=*/true);
}

void AVoxelChunk::ClearMesh()
{
	// Pooled chunks are hidden by the world and keep their (small) mesh until reused;
	// the next ApplyMesh re-initialises the RealtimeMesh, so nothing is needed here yet.
}
