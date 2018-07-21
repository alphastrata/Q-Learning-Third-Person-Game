// Fill out your copyright notice in the Description page of Project Settings.
#include "TPGameDemo.h"
#include "TPGameDemoGameState.h"
#include "MazeActor.h"


// Sets default values
AMazeActor::AMazeActor (const FObjectInitializer& ObjectInitializer) : Super (ObjectInitializer) 
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AMazeActor::BeginPlay()
{
	Super::BeginPlay();
    
    MaxHealth = 100.0f;

    ATPGameDemoGameState* gameState = (ATPGameDemoGameState*) GetWorld()->GetGameState();
    ATPGameDemoGameMode* gameMode = (ATPGameDemoGameMode*) GetWorld()->GetAuthGameMode();
    if (gameMode != nullptr && gameState != nullptr)
    {
        gameState->OnMazeDimensionsChanged.AddLambda([this]()
        {
            UpdateMazeDimensions();
        });
        CurrentLevelNumGridUnitsX   = gameState->NumGridUnitsX;
        CurrentLevelNumGridUnitsY   = gameState->NumGridUnitsY;
        CurrentLevelGridUnitLengthXCM = gameState->GridUnitLengthXCM;
        CurrentLevelGridUnitLengthYCM = gameState->GridUnitLengthYCM;
        MaxHealth = gameMode->DefaultMaxHealth;
    }
    
    Health = MaxHealth;

    UpdatePosition (false);
}

void AMazeActor::SetOccupyCells(bool bShouldOccupy)
{
    bOccupyCells = bShouldOccupy;
}

float AMazeActor::GetHealthPercentage()
{
    return Health / MaxHealth;
}

void AMazeActor::TakeDamage (float damageAmount)
{
    Health -= damageAmount;
    CheckDeath();
}

void AMazeActor::CheckDeath()
{
    if (Health <= 0.0f)
    {
        Health = 0.0f;
        IsAlive = false;
        ATPGameDemoGameState* gameState = (ATPGameDemoGameState*) GetWorld()->GetGameState();
        if (gameState != nullptr && bOccupyCells)
        {
            gameState->ActorExitedTilePosition(CurrentRoomCoords, FIntPoint(GridXPosition, GridYPosition));
        }
        OnActorDied.Broadcast();
    }
}

void AMazeActor::UpdateMazeDimensions()
{
    ATPGameDemoGameState* gameState = (ATPGameDemoGameState*) GetWorld()->GetGameState();
    if (gameState != nullptr)
    {
        CurrentLevelNumGridUnitsX   = gameState->NumGridUnitsX;
        CurrentLevelNumGridUnitsY   = gameState->NumGridUnitsY;
        CurrentLevelGridUnitLengthXCM = gameState->GridUnitLengthXCM;
        CurrentLevelGridUnitLengthYCM = gameState->GridUnitLengthYCM;
    }
}

void AMazeActor::UpdatePosition (bool broadcastChange)
{
    FVector worldPosition = GetActorLocation();
    const int totalGridLengthCMX = CurrentLevelGridUnitLengthXCM * CurrentLevelNumGridUnitsX;
    const int totalGridLengthCMY = CurrentLevelGridUnitLengthYCM * CurrentLevelNumGridUnitsY;
    const int overlappingGridLengthCMX = totalGridLengthCMX - CurrentLevelGridUnitLengthXCM;
    const int overlappingGridLengthCMY = totalGridLengthCMY - CurrentLevelGridUnitLengthYCM;
    // map -gridLength/2 > gridLength/2 to 0 > gridLength.
    const int mappedX = worldPosition.X + overlappingGridLengthCMX / 2;
    const int mappedY = worldPosition.Y + overlappingGridLengthCMY / 2;
    // get current room coords. negative values should start indexed from -1, not 0 (hence the ternary addition).
    const int roomX = FMath::Abs(mappedX / overlappingGridLengthCMX) + (mappedX < 0 ? 1 : 0) * FMath::Sign(mappedX);
    const int roomY = FMath::Abs(mappedY / overlappingGridLengthCMY) + (mappedY < 0 ? 1 : 0) * FMath::Sign(mappedY);
    CurrentRoomCoords = FIntPoint(roomX, roomY);
    // divide mappedX and mappedY to get individual cell coordinates within room. If negative, should index backwards.
    const int numUnitsX = (int) (mappedX /*- CurrentLevelGridUnitLengthXCM * 0.5f*/) / (CurrentLevelGridUnitLengthXCM);
    const int numUnitsY = (int) (mappedY /*- CurrentLevelGridUnitLengthYCM * 0.5f*/) / (CurrentLevelGridUnitLengthYCM);
    GridXPosition = numUnitsX % (CurrentLevelNumGridUnitsX - 1);
    GridYPosition = numUnitsY % (CurrentLevelNumGridUnitsY - 1);
    if (mappedX < 0)
        GridXPosition = (CurrentLevelNumGridUnitsX - 2) - FMath::Abs(GridXPosition);
    if (mappedY < 0)
        GridYPosition = (CurrentLevelNumGridUnitsY - 2) - FMath::Abs(GridYPosition);

    if (broadcastChange && (GridYPosition != PreviousGridYPosition || GridXPosition != PreviousGridXPosition))
    {
        ATPGameDemoGameState* gameState = (ATPGameDemoGameState*) GetWorld()->GetGameState();
        if (gameState != nullptr && bOccupyCells)
        {
            gameState->ActorExitedTilePosition(PreviousRoomCoords, FIntPoint(PreviousGridXPosition, PreviousGridYPosition));
            gameState->ActorEnteredTilePosition(CurrentRoomCoords, FIntPoint(GridXPosition, GridYPosition));
        }

        PositionChanged();
        GridPositionChangedEvent.Broadcast();
        PreviousGridXPosition = GridXPosition;
        PreviousGridYPosition = GridYPosition;
        if (PreviousRoomCoords != CurrentRoomCoords)
        {
            RoomCoordsChanged();
            RoomCoordsChangedEvent.Broadcast();
            PreviousRoomCoords = CurrentRoomCoords;
        }
    }
}

FRoomPositionPair AMazeActor::GetRoomAndPosition()
{
    return {CurrentRoomCoords, {GridXPosition, GridYPosition}};
}

// Called every frame
void AMazeActor::Tick( float DeltaTime )
{
	Super::Tick( DeltaTime );
    UpdatePosition();
}

bool AMazeActor::IsOnGridEdge() const
{
    if (ATPGameDemoGameState* gameState = (ATPGameDemoGameState*) GetWorld()->GetGameState())
    {
        const int numUnitsX = gameState->NumGridUnitsX;
        const int numUnitsY = gameState->NumGridUnitsY;
        return GridXPosition == 0 || GridXPosition == numUnitsX - 1 || GridYPosition == 0 || GridYPosition == numUnitsY - 1;
    }
    return false;
}

void AMazeActor::PositionChanged(){}
void AMazeActor::RoomCoordsChanged(){}