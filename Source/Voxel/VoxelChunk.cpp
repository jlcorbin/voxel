#include "VoxelChunk.h"

#include "RealtimeMeshComponent.h"
#include "RealtimeMeshSimple.h"
#include "Interface/Core/RealtimeMeshBuilder.h"
#include "Interface/Core/RealtimeMeshKeys.h"
#include "Interface/Core/RealtimeMeshCollision.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// 8 corners of a unit voxel, as {dx, dy, dz} offsets in {0,1}.
	// Indices match RealtimeMeshBasicShapeTools' box layout so winding stays correct.
	constexpr int32 CornerOffsets[8][3] = {
		{0, 1, 1}, // 0  (-X,+Y,+Z)
		{1, 1, 1}, // 1  (+X,+Y,+Z)
		{1, 0, 1}, // 2  (+X,-Y,+Z)
		{0, 0, 1}, // 3  (-X,-Y,+Z)
		{0, 1, 0}, // 4  (-X,+Y,-Z)
		{1, 1, 0}, // 5  (+X,+Y,-Z)
		{1, 0, 0}, // 6  (+X,-Y,-Z)
		{0, 0, 0}, // 7  (-X,-Y,-Z)
	};

	struct FFaceDef
	{
		int32 Corner[4];        // the four corners in add-order
		FVector3f Normal;
		FVector3f Tangent;
		int32 Neighbor[3];      // direction to test for occlusion
	};

	// 6 faces, lifted directly from the plugin's verified box generator.
	const FFaceDef Faces[6] = {
		// +Z (top)
		{ {0, 1, 2, 3}, { 0,  0,  1}, { 0, -1,  0}, { 0,  0,  1} },
		// -X
		{ {4, 0, 3, 7}, {-1,  0,  0}, { 0, -1,  0}, {-1,  0,  0} },
		// +Y
		{ {5, 1, 0, 4}, { 0,  1,  0}, {-1,  0,  0}, { 0,  1,  0} },
		// +X
		{ {6, 2, 1, 5}, { 1,  0,  0}, { 0,  1,  0}, { 1,  0,  0} },
		// -Y
		{ {7, 3, 2, 6}, { 0, -1,  0}, { 1,  0,  0}, { 0, -1,  0} },
		// -Z (bottom)
		{ {7, 6, 5, 4}, { 0,  0, -1}, { 0,  1,  0}, { 0,  0, -1} },
	};

	// Per-corner UVs, matching the box tool's (0,0)(0,1)(1,1)(1,0) order.
	const FVector2f FaceUVs[4] = {
		{0.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}
	};
}

AVoxelChunk::AVoxelChunk()
{
	PrimaryActorTick.bCanEverTick = false;

	RealtimeMeshComponent = CreateDefaultSubobject<URealtimeMeshComponent>(TEXT("RealtimeMesh"));
	SetRootComponent(RealtimeMeshComponent);

	// Default surface material so the blocks read clearly in-editor and in PIE.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(
		TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"));
	if (MatFinder.Succeeded())
	{
		VoxelMaterial = MatFinder.Object;
	}
}

void AVoxelChunk::BeginPlay()
{
	Super::BeginPlay();
	RegenerateChunk();
}

void AVoxelChunk::RegenerateChunk()
{
	GenerateVoxels();
	GenerateMesh();
}

bool AVoxelChunk::IsSolid(int32 X, int32 Y, int32 Z) const
{
	if (X < 0 || Y < 0 || Z < 0 || X >= ChunkSize || Y >= ChunkSize || Z >= ChunkSize)
	{
		return false; // outside the chunk is air
	}
	return Voxels[VoxelIndex(X, Y, Z)] != 0;
}

void AVoxelChunk::GenerateVoxels()
{
	Voxels.Init(0, ChunkSize * ChunkSize * ChunkSize);

	// Placeholder heightmap: a couple of sine waves. Deterministic, dependency-free.
	// Real FastNoiseLite terrain (2D height + 3D density) replaces this in Phase 3b.
	for (int32 X = 0; X < ChunkSize; ++X)
	{
		for (int32 Y = 0; Y < ChunkSize; ++Y)
		{
			const float FX = X * NoiseScale;
			const float FY = Y * NoiseScale;
			const float H = BaseHeight + HeightAmplitude * 0.5f * (FMath::Sin(FX) + FMath::Cos(FY));
			const int32 Top = FMath::Clamp(FMath::RoundToInt(H), 0, ChunkSize - 1);

			for (int32 Z = 0; Z <= Top; ++Z)
			{
				Voxels[VoxelIndex(X, Y, Z)] = 1;
			}
		}
	}
}

void AVoxelChunk::GenerateMesh()
{
	using namespace RealtimeMesh;

	FRealtimeMeshStreamSet StreamSet;
	TRealtimeMeshBuilderLocal<void, void, void, 1, void> Builder(StreamSet);
	Builder.EnableTangents();
	Builder.EnableColors();
	Builder.EnableTexCoords();
	Builder.EnablePolyGroups();

	const FColor White = FColor::White;

	for (int32 Z = 0; Z < ChunkSize; ++Z)
	{
		for (int32 Y = 0; Y < ChunkSize; ++Y)
		{
			for (int32 X = 0; X < ChunkSize; ++X)
			{
				if (!IsSolid(X, Y, Z))
				{
					continue;
				}

				for (const FFaceDef& Face : Faces)
				{
					// Skip faces hidden behind a solid neighbor.
					if (IsSolid(X + Face.Neighbor[0], Y + Face.Neighbor[1], Z + Face.Neighbor[2]))
					{
						continue;
					}

					const int32 Base = Builder.NumVertices();
					for (int32 i = 0; i < 4; ++i)
					{
						const int32* Off = CornerOffsets[Face.Corner[i]];
						const FVector3f Pos(
							(X + Off[0]) * VoxelSize,
							(Y + Off[1]) * VoxelSize,
							(Z + Off[2]) * VoxelSize);

						Builder.AddVertex(Pos)
							.SetNormalAndTangent(Face.Normal, Face.Tangent)
							.SetTexCoord(FaceUVs[i])
							.SetColor(White);
					}

					// Same winding the plugin's box uses: (0,1,3) and (1,2,3), poly group 0.
					Builder.AddTriangle(Base + 0, Base + 1, Base + 3, 0);
					Builder.AddTriangle(Base + 1, Base + 2, Base + 3, 0);
				}
			}
		}
	}

	if (Builder.NumVertices() == 0)
	{
		return; // empty chunk, nothing to draw
	}

	URealtimeMeshSimple* Mesh = RealtimeMeshComponent->InitializeRealtimeMesh<URealtimeMeshSimple>();
	if (VoxelMaterial)
	{
		Mesh->SetupMaterialSlot(0, TEXT("Primary"), VoxelMaterial);
	}

	// One section group, auto-creating a section per poly group (we only use group 0).
	const FRealtimeMeshSectionGroupKey GroupKey = FRealtimeMeshSectionGroupKey::Create(FRealtimeMeshLODKey(0), 0);
	Mesh->CreateSectionGroup(GroupKey, MoveTemp(StreamSet));

	// Enable complex-as-simple collision so the character can stand on the chunk.
	Mesh->SetCollisionConfig(FRealtimeMeshCollisionConfiguration());
	const FRealtimeMeshSectionKey SectionKey = FRealtimeMeshSectionKey::CreateForPolyGroup(GroupKey, 0);
	Mesh->UpdateSectionConfig(SectionKey, FRealtimeMeshSectionConfig(0), /*bShouldCreateCollision=*/true);
}
