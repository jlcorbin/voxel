#include "VoxelMesher.h"

namespace
{
	// 8 corners of a unit voxel as {dx,dy,dz} in {0,1}. Indices/order match the
	// RealtimeMesh box generator so winding and normals are guaranteed correct.
	constexpr int32 CornerOffsets[8][3] = {
		{0, 1, 1}, {1, 1, 1}, {1, 0, 1}, {0, 0, 1},
		{0, 1, 0}, {1, 1, 0}, {1, 0, 0}, {0, 0, 0},
	};

	struct FFaceDef
	{
		int32 Corner[4];
		FVector3f Normal;
		FVector3f Tangent;
		int32 Neighbor[3];
	};

	const FFaceDef Faces[6] = {
		{ {0, 1, 2, 3}, { 0,  0,  1}, { 0, -1,  0}, { 0,  0,  1} }, // +Z
		{ {4, 0, 3, 7}, {-1,  0,  0}, { 0, -1,  0}, {-1,  0,  0} }, // -X
		{ {5, 1, 0, 4}, { 0,  1,  0}, {-1,  0,  0}, { 0,  1,  0} }, // +Y
		{ {6, 2, 1, 5}, { 1,  0,  0}, { 0,  1,  0}, { 1,  0,  0} }, // +X
		{ {7, 3, 2, 6}, { 0, -1,  0}, { 1,  0,  0}, { 0, -1,  0} }, // -Y
		{ {7, 6, 5, 4}, { 0,  0, -1}, { 0,  1,  0}, { 0,  0, -1} }, // -Z
	};

	const FVector2f FaceUVs[4] = { {0, 0}, {0, 1}, {1, 1}, {1, 0} };
}

bool FVoxelMesher::Build(const FVoxelGenerator& Gen, const FIntVector& Coord, float VoxelSize,
	RealtimeMesh::FRealtimeMeshStreamSet& OutStreams)
{
	using namespace RealtimeMesh;

	TRealtimeMeshBuilderLocal<void, void, void, 1, void> Builder(OutStreams);
	Builder.EnableTangents();
	Builder.EnableColors();
	Builder.EnableTexCoords();
	Builder.EnablePolyGroups();

	const int32 CS = Gen.ChunkSize;
	const int32 BaseX = Coord.X * CS;
	const int32 BaseY = Coord.Y * CS;
	const int32 BaseZ = Coord.Z * CS;
	const FColor White = FColor::White;

	for (int32 Z = 0; Z < CS; ++Z)
	{
		for (int32 Y = 0; Y < CS; ++Y)
		{
			for (int32 X = 0; X < CS; ++X)
			{
				const int32 WX = BaseX + X;
				const int32 WY = BaseY + Y;
				const int32 WZ = BaseZ + Z;
				if (!Gen.IsSolid(WX, WY, WZ))
				{
					continue;
				}

				for (const FFaceDef& Face : Faces)
				{
					// Border culling samples the generator directly — no neighbour chunk needed.
					if (Gen.IsSolid(WX + Face.Neighbor[0], WY + Face.Neighbor[1], WZ + Face.Neighbor[2]))
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

					Builder.AddTriangle(Base + 0, Base + 1, Base + 3, 0);
					Builder.AddTriangle(Base + 1, Base + 2, Base + 3, 0);
				}
			}
		}
	}

	return Builder.NumVertices() > 0;
}
