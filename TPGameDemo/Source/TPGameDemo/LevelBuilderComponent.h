// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/ActorComponent.h"
#include "TPGameDemo.h"
//#include "MazeActor.h"
#include "LevelBuilderComponent.generated.h"

USTRUCT(Blueprintable)
struct FWallSegmentDescriptor
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category = "Room Inner Walls")
    FIntPoint Start;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category = "Room Inner Walls")
    FIntPoint End;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category = "Room Inner Walls")
    EDirectionType Direction;
};

/*
The levels in this project are based on grid mazes with equally-spaced cells, as found in many examples of Q-Learning problems
https://webdocs.cs.ualberta.ca/~sutton/book/ebook/node1.html

This level builder component maintains a 2D binary array -- LevelStructure -- that defines where walls exist within the maze (1 = wall, 0 = open space).
In the ue4 editor, an actor blueprint is implemented which takes an instance of this class as a component. That actor is then used to programatically fill 
the maze geometry from the level blueprint.
*/

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class TPGAMEDEMO_API ULevelBuilderComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	ULevelBuilderComponent();

	// Called when the game starts
	virtual void BeginPlay() override;
	
	// Called every frame
	virtual void TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction ) override;

    UFUNCTION (BlueprintCallable, Category = "Level Building")
        virtual TArray<FWallSegmentDescriptor> GenerateLevelOfSize(int sideLength, float normedDensity, float normedComplexity, FIntPoint roomCoords);

    UFUNCTION (BlueprintCallable, Category = "Level Building")
        virtual TArray<FWallSegmentDescriptor> GenerateLevel(float normedDensity, float normedComplexity, FIntPoint roomCoords);

    UFUNCTION (BlueprintCallable, Category = "Level Building")
        virtual TArray<FWallSegmentDescriptor> GenerateInnerStructure(int sideLength, float normedDensity, float normedComplexity);

    UFUNCTION (BlueprintCallable, Category = "Level Building")
        virtual TArray<FWallSegmentDescriptor> RegenerateInnerStructure(float normedDensity, float normedComplexity, FIntPoint roomCoords);

    UFUNCTION (BlueprintCallable, Category = "Level Building")
        virtual void LoadLevel (FIntPoint roomCoords);

    UFUNCTION (BlueprintCallable, Category = "Level Building")
        virtual int GetNumGridUnitsX();

    UFUNCTION (BlueprintCallable, Category = "Level Building")
        virtual int GetNumGridUnitsY();

    UFUNCTION (BlueprintCallable, Category = "Level Building")
        virtual bool IsGridPositionBlocked (int x, int y);

    UFUNCTION (BlueprintCallable, Category = "Level Building")
        virtual ECellState GetCellState (int x, int y);

    UFUNCTION (BlueprintCallable, Category = "Level Building")
        virtual EDirectionType GetWallTypeForDoorPosition(int x, int y);
    
    UFUNCTION (BlueprintCallable, Category = "Level Building")
        virtual EDirectionType GetWallTypeForBlockPosition(int x, int y);
    
    UFUNCTION (BlueprintCallable, Category = "Level Building")
        FVector2D GetClosestEmptyCell (int x, int y);

    /** Returns the world position of cell x|y mapped from positive indexing (0,0 -> n,n) to world positions centered at 0,0 (-n/2 * gridUnitX, -n/2 * gridUnitY -> n/2 * gridUnitX, n/2 * gridUnityY). 
        The x position is offset by RoomOffsetX * grid (room) width. Similar for the y position.
        If getCentre is true, the returned position will be centred within the grid cell. 
        If getCentre is false, the returned position will be the back left corner of the cell.
    */
    UFUNCTION (BlueprintCallable, Category = "Level Building")
        FVector2D GetCellWorldPosition (int x, int y, int RoomOffsetX, int RoomOffsetY, bool getCentre = true);

    /** Starts in centre of grid and winds out clockise looking for empty grid cells. */
    UFUNCTION (BlueprintCallable, Category = "Level Building")
        FVector2D FindMostCentralEmptyCell();

    /** Starts in centre of grid and winds out clockise looking for empty grid cells. */
    UFUNCTION (BlueprintCallable, Category = "Level Building")
        FVector2D FindMostCentralSpawnPosition(int RoomOffsetX, int RoomOffsetY);

    UFUNCTION (BlueprintCallable, Category = "Level Building")
        void UpdateInnerWallCellActorCounts(FIntPoint roomCoords, bool Increment);

private:
    FIntPoint           GetRandomEvenCell();
    bool                IsCellTouchingDoorCell(FIntPoint cellPosition);
    FString             CurrentLevelPath;
    bool                LevelsDirFound = false;
    TArray<TArray<int>> LevelStructure;
	
};
