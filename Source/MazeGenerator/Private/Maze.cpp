// Copyright LowkeyMe. All Rights Reserved. 2022


#include "Maze.h"

#include "Algorithms/Algorithm.h"
#include "Algorithms/Backtracker.h"
#include "Algorithms/Division.h"
#include "Algorithms/Eller.h"
#include "Algorithms/HaK.h"
#include "Algorithms/Kruskal.h"
#include "Algorithms/Prim.h"
#include "Algorithms/Sidewinder.h"

#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

DEFINE_LOG_CATEGORY(LogMaze);

FMazeSize::FMazeSize(): X(5), Y(5)
{
}

FMazeSize::operator FIntVector2() const
{
	return FIntVector2{X, Y};
}

FMazeCoordinates::FMazeCoordinates(): X(0), Y(0)
{
}

void FMazeCoordinates::ClampByMazeSize(const FMazeSize& MazeSize)
{
	if (X >= MazeSize.X)
	{
		X = MazeSize.X - 1;
	}
	if (Y >= MazeSize.Y)
	{
		Y = MazeSize.Y - 1;
	}
}

bool FMazeCoordinates::operator==(const FMazeCoordinates& Other) const
{
	return X == Other.X && Y == Other.Y;
}

bool FMazeCoordinates::operator!=(const FMazeCoordinates& Other) const
{
	return !(*this == Other);
}

FMazeCoordinates::operator TTuple<int, int>() const
{
	return TPair<int32, int32>{X, Y};
}

AMaze::AMaze()
{
	PrimaryActorTick.bCanEverTick = false;

	GenerationAlgorithms.Add(EGenerationAlgorithm::Backtracker, TSharedPtr<Algorithm>(new Backtracker));
	GenerationAlgorithms.Add(EGenerationAlgorithm::Division, TSharedPtr<Algorithm>(new Division));
	GenerationAlgorithms.Add(EGenerationAlgorithm::HaK, TSharedPtr<Algorithm>(new HaK));
	GenerationAlgorithms.Add(EGenerationAlgorithm::Sidewinder, TSharedPtr<Algorithm>(new Sidewinder));
	GenerationAlgorithms.Add(EGenerationAlgorithm::Kruskal, TSharedPtr<Algorithm>(new Kruskal));
	GenerationAlgorithms.Add(EGenerationAlgorithm::Eller, TSharedPtr<Algorithm>(new Eller));
	GenerationAlgorithms.Add(EGenerationAlgorithm::Prim, TSharedPtr<Algorithm>(new Prim));

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AMaze::UpdateMaze()
{
	ClearMaze();

	if (!(FloorStaticMesh && WallStaticMesh))
	{
		UE_LOG(LogMaze, Warning, TEXT("To create maze specify FloorStaticMesh and WallStaticMesh."));
		return;
	}

	MazeCellSize = GetMaxCellSize();

	if (OutlineStaticMesh)
	{
		CreateMazeOutline();
	}
	MazeGrid = GenerationAlgorithms[GenerationAlgorithm]->GetGrid(MazeSize, Seed);

	if (bGeneratePath)
	{
		PathStart.ClampByMazeSize(MazeSize);
		PathEnd.ClampByMazeSize(MazeSize);
		MazePathGrid = GetMazePath(PathStart, PathEnd, PathLength);
	}

	for (int32 Y = 0; Y < MazeSize.Y; ++Y)
	{
		for (int32 X = 0; X < MazeSize.X; ++X)
		{
			const FVector Location(GetActorLocation() + FVector(MazeCellSize.X * X, MazeCellSize.Y * Y, 0.f));
			AStaticMeshActor* SpawnedActor = GetWorld()->SpawnActor<AStaticMeshActor>(Location, FRotator::ZeroRotator);
			if(SpawnedActor)
			{
				UStaticMeshComponent* MeshComponent = SpawnedActor->GetStaticMeshComponent();
				if(MeshComponent)
				{
					if (bGeneratePath && PathStaticMesh && MazePathGrid.Num() > 0 && MazePathGrid[Y][X])
					{
						MeshComponent->SetStaticMesh(PathStaticMesh);
						PathFloorCells.Add(SpawnedActor);
					}
					else if (MazeGrid[Y][X])
					{
						MeshComponent->SetStaticMesh(FloorStaticMesh);
						FloorCells.Add(SpawnedActor);
					}
					else
					{
						SpawnedActor->SetActorLocation(Location + WallOffset);
						MeshComponent->SetStaticMesh(WallStaticMesh);
						WallCells.Add(SpawnedActor);
					}
				}
			}
		}
	}

	EnableCollision(bUseCollision);
}

void AMaze::CreateMazeOutline()
{
	FVector Location1{0.f};
	FVector Location2{0.f};

	Location1.Y = -MazeCellSize.Y;
	Location2.Y = MazeCellSize.Y * MazeSize.Y;
	for (int32 X = -1; X < MazeSize.X + 1; ++X)
	{
		Location1.X = Location2.X = X * MazeCellSize.X;
		AStaticMeshActor* SpawnedActor1 = GetWorld()->SpawnActor<AStaticMeshActor>(GetActorLocation() + Location1 + OutlineWallOffset, FRotator::ZeroRotator);
		if(SpawnedActor1)
		{
			SpawnedActor1->GetStaticMeshComponent()->SetStaticMesh(OutlineStaticMesh);
			OutlineWallCells.Add(SpawnedActor1);
		}
		AStaticMeshActor* SpawnedActor2 = GetWorld()->SpawnActor<AStaticMeshActor>(GetActorLocation() + Location2 + OutlineWallOffset, FRotator::ZeroRotator);
		if(SpawnedActor2)
		{
			SpawnedActor2->GetStaticMeshComponent()->SetStaticMesh(OutlineStaticMesh);
			OutlineWallCells.Add(SpawnedActor2);
		}
	}

	Location1.X = -MazeCellSize.X;
	Location2.X = MazeCellSize.X * MazeSize.X;
	for (int32 Y = 0; Y < MazeSize.Y; ++Y)
	{
		Location1.Y = Location2.Y = Y * MazeCellSize.Y;
		AStaticMeshActor* SpawnedActor1 = GetWorld()->SpawnActor<AStaticMeshActor>(GetActorLocation() + Location1 + OutlineWallOffset, FRotator::ZeroRotator);
		if(SpawnedActor1)
		{
			SpawnedActor1->GetStaticMeshComponent()->SetStaticMesh(OutlineStaticMesh);
			OutlineWallCells.Add(SpawnedActor1);
		}
		AStaticMeshActor* SpawnedActor2 = GetWorld()->SpawnActor<AStaticMeshActor>(GetActorLocation() + Location2 + OutlineWallOffset, FRotator::ZeroRotator);
		if(SpawnedActor2)
		{
			SpawnedActor2->GetStaticMeshComponent()->SetStaticMesh(OutlineStaticMesh);
			OutlineWallCells.Add(SpawnedActor2);
		}
	}
}

TArray<TArray<uint8>> AMaze::GetMazePath(const FMazeCoordinates& Start, const FMazeCoordinates& End, int32& OutLength)
{
	TArray<TArray<int32>> Graph;
	Graph.Reserve(MazeGrid.Num() * MazeGrid[0].Num());

	// Graph creation.
	for (int32 GraphVertex,
	           Y = 0; Y < MazeGrid.Num(); ++Y)
	{
		for (int32 X = 0; X < MazeGrid[Y].Num(); ++X)
		{
			GraphVertex = Y * MazeGrid[Y].Num() + X;

			Graph.Emplace(TArray<int32>());
			if (!MazeGrid[Y][X])
			{
				continue;
			}

			Graph[GraphVertex].Reserve(4); // There are only 4 directions possible.

			if (X > 0 && MazeGrid[Y][X - 1]) // West direction.
			{
				Graph[GraphVertex].Emplace(GraphVertex - 1);
			}
			if (X + 1 < MazeGrid[Y].Num() && MazeGrid[Y][X + 1]) // East direction.
			{
				Graph[GraphVertex].Emplace(GraphVertex + 1);
			}
			if (Y > 0 && MazeGrid[Y - 1][X]) // North direction.
			{
				Graph[GraphVertex].Emplace(GraphVertex - MazeGrid[Y].Num());
			}
			if (Y + 1 < MazeGrid.Num() && MazeGrid[Y + 1][X]) // South direction.
			{
				Graph[GraphVertex].Emplace(GraphVertex + MazeGrid[Y].Num());
			}

			Graph[GraphVertex].Shrink();
		}
	}

	const int32 StartVertex = Start.Y * MazeGrid[0].Num() + Start.X;
	const int32 EndVertex = End.Y * MazeGrid[0].Num() + End.X;


	TQueue<int32> Vertices;

	const int32 VerticesAmount = MazeGrid.Num() * MazeGrid[0].Num();

	TArray<bool> Visited;
	Visited.Init(false, VerticesAmount);

	TArray<int32> Parents;
	Parents.Init(-1, VerticesAmount);

	TArray<int32> Distances;
	Distances.Init(0, VerticesAmount);

	int32 Vertex;
	Vertices.Enqueue(StartVertex);
	Visited[StartVertex] = true;
	while (Vertices.Dequeue(Vertex))
	{
		for (int32 i = 0; i < Graph[Vertex].Num(); ++i)
		{
			if (const int32 Adjacent = Graph[Vertex][i]; !Visited[Adjacent])
			{
				Visited[Adjacent] = true;
				Vertices.Enqueue(Adjacent);
				Distances[Adjacent] = Distances[Vertex] + 1;
				Parents[Adjacent] = Vertex;
			}
		}
	}

	TArray<int32> GraphPath;
	if (!Visited[EndVertex])
	{
		UE_LOG(LogMaze, Warning, TEXT("Path is not reachable."));
		return TArray<TArray<uint8>>();
	}

	for (int VertexNumber = EndVertex; VertexNumber != -1; VertexNumber = Parents[VertexNumber])
	{
		GraphPath.Emplace(VertexNumber);
	}

	Algo::Reverse(GraphPath);

	TArray<TArray<uint8>> Path;
	Path.Init(TArray<uint8>(), MazeGrid.Num());
	for (int Y = 0; Y < MazeGrid.Num(); ++Y)
	{
		Path[Y].SetNumZeroed(MazeGrid[Y].Num());
	}

	for (int32 VertexNumber, i = 0; i < GraphPath.Num(); ++i)
	{
		VertexNumber = GraphPath[i];

		Path[VertexNumber / MazeGrid[0].Num()][VertexNumber % MazeGrid[0].Num()] = 1;
	}


	OutLength = Distances[EndVertex] + 1;
	return Path;
}

void AMaze::EnableCollision(const bool bShouldEnable)
{
	ECollisionEnabled::Type CollisionType = bShouldEnable ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision;

	for (AStaticMeshActor* Actor : FloorCells)
	{
		if(Actor) Actor->GetStaticMeshComponent()->SetCollisionEnabled(CollisionType);
	}
	for (AStaticMeshActor* Actor : WallCells)
	{
		if(Actor) Actor->GetStaticMeshComponent()->SetCollisionEnabled(CollisionType);
	}
	for (AStaticMeshActor* Actor : OutlineWallCells)
	{
		if(Actor) Actor->GetStaticMeshComponent()->SetCollisionEnabled(CollisionType);
	}
	for (AStaticMeshActor* Actor : PathFloorCells)
	{
		if(Actor) Actor->GetStaticMeshComponent()->SetCollisionEnabled(CollisionType);
	}
}


void AMaze::ClearMaze()
{
	for (AStaticMeshActor* Actor : FloorCells)
	{
		if(Actor) Actor->Destroy();
	}
	FloorCells.Empty();

	for (AStaticMeshActor* Actor : WallCells)
	{
		if(Actor) Actor->Destroy();
	}
	WallCells.Empty();

	for (AStaticMeshActor* Actor : OutlineWallCells)
	{
		if(Actor) Actor->Destroy();
	}
	OutlineWallCells.Empty();

	for (AStaticMeshActor* Actor : PathFloorCells)
	{
		if(Actor) Actor->Destroy();
	}
	PathFloorCells.Empty();
}

FVector2D AMaze::GetMaxCellSize() const
{
	const FVector FloorSize3D = FloorStaticMesh->GetBoundingBox().GetSize();
	const FVector WallSize3D = WallStaticMesh->GetBoundingBox().GetSize();

	const FVector2D FloorSize2D{FloorSize3D.X, FloorSize3D.Y};
	const FVector2D WallSize2D{WallSize3D.X, WallSize3D.Y};

	const FVector2D MaxCellSize = FVector2D::Max(FloorSize2D, WallSize2D);
	if (OutlineStaticMesh)
	{
		const FVector OutlineSize3D = OutlineStaticMesh->GetBoundingBox().GetSize();
		if (const FVector2D OutlineSize2D{OutlineSize3D.X, OutlineSize3D.Y};
			OutlineSize2D.ComponentwiseAllGreaterThan(MaxCellSize))
		{
			return OutlineSize2D;
		}
	}
	return MaxCellSize;
}

void AMaze::GenerateMaze()
{
	UpdateMaze();
}

void AMaze::Randomize()
{
	MazeSize.X = FMath::RandRange(3, 101) | 1; // | 1 to make odd.
	MazeSize.Y = FMath::RandRange(3, 101) | 1;

	TArray<EGenerationAlgorithm> Algorithms;
	const int32 Num = GenerationAlgorithms.GetKeys(Algorithms);
	GenerationAlgorithm = Algorithms[FMath::RandRange(0, Num - 1)];

	Seed = FMath::RandRange(MIN_int32, MAX_int32);

	PathStart.X = 0;
	PathStart.X = 0;
	PathEnd.X = MazeSize.X - 1;
	PathEnd.Y = MazeSize.Y - 1;

	UpdateMaze();
}


void AMaze::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
}
