// Fill out your copyright notice in the Description page of Project Settings.

#include "TPGameDemo.h"
#include <functional>
#include "TPGameDemoGameState.h"

void RoomState::DisableRoom()
{
    RoomStatus = RoomState::Status::Dead;
}

ATPGameDemoGameState::ATPGameDemoGameState(const FObjectInitializer& ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
}

ATPGameDemoGameState::~ATPGameDemoGameState()
{
    for(int i = 0; i < RoomBuilders.Num(); ++i)
    {
        TArray<ARoomBuilder*>& builderRow = RoomBuilders[i];
        for (int c = 0; c < builderRow.Num(); ++c)
        {
            builderRow[c] = nullptr;
        }
    }
    RoomBuilders.Empty();

    for(int i = 0; i < WallBuilders.Num(); ++i)
    {
        TArray<AWallBuilder*>& builderRow = WallBuilders[i];
        for (int c = 0; c < builderRow.Num(); ++c)
        {
            builderRow[c] = nullptr;
        }
    }
    WallBuilders.Empty();
}

void ATPGameDemoGameState::InitialiseArrays()
{
    ATPGameDemoGameMode* gameMode = (ATPGameDemoGameMode*) GetWorld()->GetAuthGameMode();

    for (int x = 0; x < NumGridsXY; ++x)
    {
        TArray<WallStateCouple> wallsRow;
        TArray<RoomState> roomsRow;
        TArray<ARoomBuilder*> roomBuilderRow;
        TArray<AWallBuilder*> wallBuilderRow;
        for (int y = 0; y < NumGridsXY; ++y)
        {
            wallsRow.Add(WallStateCouple());
            roomsRow.Add(RoomState(FIntPoint(NumGridUnitsX, NumGridUnitsY)));

            roomBuilderRow.Add(nullptr);
            wallBuilderRow.Add(nullptr);
        }
        // Add one final wall couple (where the west wall will be the east wall of the final room, and the south wall will be ignored).
        wallsRow.Add(WallStateCouple());
        wallBuilderRow.Add(nullptr);
        RoomStates.Add(roomsRow);
        WallStates.Add(wallsRow);
        RoomBuilders.Add(roomBuilderRow);
        WallBuilders.Add(wallBuilderRow);
    }
    // Add the last row of wall couples (where the south wall will be the north wall of the final room, and the west wall will be ignored).
    TArray<WallStateCouple> wallsRow;
    TArray<AWallBuilder*> wallBuilderRow;
    for (int y = 0; y < NumGridsXY + 1; ++y)
    {
        wallsRow.Add(WallStateCouple());
        wallBuilderRow.Add(nullptr);
    }
    WallStates.Add(wallsRow);
    WallBuilders.Add(wallBuilderRow);
}

void ATPGameDemoGameState::Tick( float DeltaTime )
{
    for (auto wallPosition : WallsToUpdate)
    {
        auto roomCoords = wallPosition.WallCoupleCoords;
        auto wallType = wallPosition.WallType;
        auto wallState = GetWallState(roomCoords, wallType);
        auto neighbourCoords = GetRoomCoords(GetNeighbouringRoomIndices(roomCoords, wallType));
        bool roomExists = DoesRoomExist(roomCoords);
        bool neighbourExists = DoesRoomExist(neighbourCoords); 

        if (!(roomExists || neighbourExists))
        {
            DisableWallState(roomCoords, wallType);
            continue;
        }

        // Door Spawned State:
        const bool roomTrained = IsRoomTrained(roomCoords);
        const bool neighbourTrained = IsRoomTrained(neighbourCoords);

        EnableWallState(roomCoords, wallType);
        if (roomTrained && neighbourTrained)
            DisableDoorState(roomCoords, wallType);
        else
            EnableDoorState(roomCoords, wallType);
        // Door Locked State:
        UpdateDoorLockedStateForNeighbouringRooms(roomCoords, neighbourCoords, wallType);
        LockDoorIfOnPerimeter(roomCoords);
        if (PerimeterDoorsNeedUnlocked)
        {
            UnlockPerimeterDoors();
            ++CurrentPerimeter;
            NumPerimeterRoomsConnected = 0;
            PerimeterDoorsNeedUnlocked = false;
            ATPGameDemoGameMode* gameMode = (ATPGameDemoGameMode*) GetWorld()->GetAuthGameMode();
            UpdateSignalStrength(gameMode->DefaultSignalStrength);
            OnPerimeterComplete.Broadcast();
        }
    }
    WallsToUpdate.Empty();
}

//============================================================================
// Acessors
//============================================================================

bool ATPGameDemoGameState::DoesRoomExist(FIntPoint roomCoords) const
{
    return GetRoomStateChecked(roomCoords).RoomExists();
}

bool ATPGameDemoGameState::IsRoomConnected(FIntPoint roomCoords) const
{
    return GetRoomStateChecked(roomCoords).RoomStatus == RoomState::Connected;
}

bool ATPGameDemoGameState::IsRoomTrained(FIntPoint roomCoords) const
{
    auto room = GetRoomStateChecked(roomCoords);
    return room.RoomStatus == RoomState::Status::Trained || room.RoomStatus == RoomState::Connected;
}

bool ATPGameDemoGameState::DoesWallExist(FIntPoint roomCoords, EDirectionType wallType)
{
    auto wallState = GetWallState(roomCoords, wallType);
    return wallState.bWallExists;
}

bool ATPGameDemoGameState::DoesDoorExist(FIntPoint roomCoords, EDirectionType wallType)
{
    auto wallState = GetWallState(roomCoords, wallType);
	if (!wallState.bWallExists && wallState.bDoorExists)
	{
		ensure(false); // Invalid state!
	}
    return wallState.bWallExists && wallState.bDoorExists;
}

float ATPGameDemoGameState::GetRoomHealth(FIntPoint roomCoords) const
{
    return GetRoomStateChecked(roomCoords).RoomHealth;
}

int ATPGameDemoGameState::GetDoorPositionOnWall(FIntPoint roomCoords, EDirectionType wallType)
{
    WallState& wallState = GetWallState(roomCoords, wallType);
    return wallState.DoorPosition;
}

bool ATPGameDemoGameState::IsDoorUnlocked(FIntPoint roomCoords, EDirectionType wallDirection)
{
    return GetWallStatesForRoom(roomCoords)[(int)wallDirection]->DoorState != EDoorState::Locked;
}

FIntPoint ATPGameDemoGameState::GetSignalPointPositionInRoom(FIntPoint roomCoords) const
{
    return GetRoomStateChecked(roomCoords).SignalPoint;
}

bool ATPGameDemoGameState::TilePositionIsEmpty(FIntPoint roomCoords, FIntPoint tilePosition) const
{
    return GetRoomStateChecked(roomCoords).TileIsEmpty(tilePosition);
}

bool ATPGameDemoGameState::RoomTilePositionIsEmpty(FRoomPositionPair roomAndPosition) const
{
    return TilePositionIsEmpty(roomAndPosition.RoomCoords, roomAndPosition.PositionInRoom);
}

bool ATPGameDemoGameState::RoomIsWithinPerimeter(FIntPoint roomCoords) const
{
    return FMath::Abs(roomCoords.X) < CurrentPerimeter && FMath::Abs(roomCoords.Y) < CurrentPerimeter;
}

EQuadrantType ATPGameDemoGameState::GetQuadrantTypeForRoomCoords(FIntPoint roomCoords) const
{
    FIntPoint roomIndices = GetRoomXYIndicesChecked(roomCoords);
    int x = roomIndices.X;
    int y = roomIndices.Y;
    if(x >= NumGridsXY / 2)
    {
        if (y >= NumGridsXY / 2)
        {
            return EQuadrantType::NorthEast;
        }
        return EQuadrantType::NorthWest;
    }
    else if (y < NumGridsXY / 2)
    {
        return EQuadrantType::SouthWest;
    }
    return EQuadrantType::SouthEast;
}

TArray<WallState*> ATPGameDemoGameState::GetWallStatesForRoom(FIntPoint roomCoords)
{
    FIntPoint roomIndices = GetRoomXYIndicesChecked(roomCoords);
    TArray<WallState*> wallStates;
    wallStates.Empty();
    auto northNeighbourIndices = GetNeighbouringRoomIndices(roomCoords, EDirectionType::North);
    auto eastNeighbourIndices = GetNeighbouringRoomIndices(roomCoords, EDirectionType::East);
    wallStates.Add(&WallStates[northNeighbourIndices.X][northNeighbourIndices.Y].SouthWall);
    wallStates.Add(&WallStates[eastNeighbourIndices.X][eastNeighbourIndices.Y].WestWall);
    wallStates.Add(&WallStates[roomIndices.X][roomIndices.Y].SouthWall);
    wallStates.Add(&WallStates[roomIndices.X][roomIndices.Y].WestWall);
    return wallStates;
}

FIntPoint ATPGameDemoGameState::GetNeighbouringRoomIndices(FIntPoint roomCoords, EDirectionType neighbourPosition) const
{
    FIntPoint roomIndices = GetRoomXYIndicesChecked(roomCoords);

    switch(neighbourPosition)
    {
        case EDirectionType::North: return FIntPoint(roomIndices.X + 1, roomIndices.Y);
        case EDirectionType::East:  return FIntPoint(roomIndices.X, roomIndices.Y + 1);
        case EDirectionType::South: return FIntPoint(roomIndices.X - 1, roomIndices.Y);
        case EDirectionType::West:  return FIntPoint(roomIndices.X, roomIndices.Y - 1);
        default: ensure(false); return FIntPoint(-1, -1);
    }
}

FRoomPositionPair ATPGameDemoGameState::GetNeighbouringCell(FRoomPositionPair roomAndPosition, EDirectionType direction)
{
    FRoomPositionPair neighbourRoomAndPosition = roomAndPosition;
    switch(direction)
    {
    case EDirectionType::North:
        neighbourRoomAndPosition.PositionInRoom.X += 1;
        break;
    case EDirectionType::East:
        neighbourRoomAndPosition.PositionInRoom.Y += 1;
        break;
    case EDirectionType::South:
        neighbourRoomAndPosition.PositionInRoom.X -= 1;
        break;
    case EDirectionType::West:
        neighbourRoomAndPosition.PositionInRoom.Y -= 1;
        break;
    default: ensure(!"Urecognized direction!"); break;
    }
    WrapRoomPositionPair(neighbourRoomAndPosition);
    return neighbourRoomAndPosition;
}

TArray<bool> ATPGameDemoGameState::GetNeighbouringRoomStates(FIntPoint roomCoords) const
{
    TArray<bool> neighbouringRoomStates{false, false, false, false};
    
    for (int p = 0; p < (int)EDirectionType::NumDirectionTypes; ++p)
    {
        FIntPoint neighbour = GetNeighbouringRoomIndices(roomCoords, (EDirectionType)p);
        if (RoomXYIndicesValid(neighbour) && RoomStates[neighbour.X][neighbour.Y].RoomExists())
            neighbouringRoomStates[p] = true;
    }
    return neighbouringRoomStates;
}

TArray<int> ATPGameDemoGameState::GetDoorPositionsForExistingNeighbours(FIntPoint roomCoords)
{
    TArray<bool> neighbourStates = GetNeighbouringRoomStates(roomCoords);
    TArray<int> doorPositions = {0,0,0,0};
    for (EDirectionType p = EDirectionType::North; p < EDirectionType::NumDirectionTypes; p = (EDirectionType)((int)p + 1))
    {
        if (neighbourStates[(int)p])
        {
            EDirectionType direction = (EDirectionType)p;
            WallState& wallState = GetWallState(roomCoords, direction);
            doorPositions[(int)p] = wallState.DoorPosition;
        }
    }
    return doorPositions;
}

ARoomBuilder* ATPGameDemoGameState::GetRoomBuilder(FIntPoint roomCoords)
{
    FIntPoint roomIndices = GetRoomXYIndicesChecked(roomCoords);
    return RoomBuilders[roomIndices.X][roomIndices.Y];
}

AWallBuilder* ATPGameDemoGameState::GetWallBuilder(FIntPoint roomCoords, EDirectionType direction)
{
    FIntPoint wallRoomIndices = GetRoomXYIndicesChecked(roomCoords);
    if (direction == EDirectionType::South || direction == EDirectionType::West)
        return WallBuilders[wallRoomIndices.X][wallRoomIndices.Y];
    FIntPoint neighbourRoom = GetNeighbouringRoomIndices(roomCoords, direction);
    return WallBuilders[neighbourRoom.X][neighbourRoom.Y];
}

//============================================================================
// Modifiers
//============================================================================

void ATPGameDemoGameState::SetRoomHealth(FIntPoint roomCoords, float health)
{
    FIntPoint roomIndices = GetRoomXYIndicesChecked(roomCoords);
    RoomStates[roomIndices.X][roomIndices.Y].RoomHealth = health;
    RoomBuilders[roomIndices.X][roomIndices.Y]->HealthChanged(health);
    if (RoomStates[roomIndices.X][roomIndices.Y].RoomHealth <= 0.0f && 
        RoomStates[roomIndices.X][roomIndices.Y].RoomExists())
        DisableRoomState(roomCoords);
}

void ATPGameDemoGameState::UpdateRoomHealth(FIntPoint roomCoords, float healthDelta)
{
    FIntPoint roomIndices = GetRoomXYIndicesChecked(roomCoords);
    RoomStates[roomIndices.X][roomIndices.Y].RoomHealth += healthDelta;
    RoomBuilders[roomIndices.X][roomIndices.Y]->HealthChanged(RoomStates[roomIndices.X][roomIndices.Y].RoomHealth);
    if (RoomStates[roomIndices.X][roomIndices.Y].RoomHealth <= 0.0f && 
        RoomStates[roomIndices.X][roomIndices.Y].RoomExists())
        DisableRoomState(roomCoords);
}

void ATPGameDemoGameState::UpdateSignalStrength(float delta)
{
    SignalStrength = FMath::Min(FMath::Max(0.0f, SignalStrength + delta), MaxSignalStrength);
    if (SignalStrength <= 0.0f)
    {
        OnSignalLost.Broadcast();
    }
}

void ATPGameDemoGameState::SetRoomBuilder(FIntPoint roomCoords, ARoomBuilder* roomBuilderActor)
{
    FIntPoint roomIndices = GetRoomXYIndicesChecked(roomCoords);
    RoomBuilders[roomIndices.X][roomIndices.Y] = roomBuilderActor;
}

void ATPGameDemoGameState::SetWallBuilder(FIntPoint roomCoords, AWallBuilder* builder)
{
    FIntPoint roomIndices = GetRoomXYIndicesChecked(roomCoords);
    WallBuilders[roomIndices.X][roomIndices.Y] = builder;
}

void ATPGameDemoGameState::SetRoomConnected(FIntPoint roomCoords)
{
    FIntPoint roomIndices = GetRoomXYIndicesChecked(roomCoords);
    if (DoesRoomExist(roomCoords))
    {
        if (RoomStates[roomIndices.X][roomIndices.Y].RoomStatus != RoomState::Status::Connected)
        {
            RoomStates[roomIndices.X][roomIndices.Y].SetRoomConnected();
            RoomWasConnected(roomCoords);
            GetRoomBuilder(roomCoords)->RoomWasConnected();
        }
    }
}

void ATPGameDemoGameState::SetSignalPointInRoom(FIntPoint roomCoords, FIntPoint signalPointInRoom)
{
    FIntPoint roomIndices = GetRoomXYIndicesChecked(roomCoords);
    RoomStates[roomIndices.X][roomIndices.Y].SignalPoint = signalPointInRoom;
}

void ATPGameDemoGameState::EnableRoomState(FIntPoint roomCoords, float complexity, float density)
{
    // initialize random door positions for walls that haven't yet generated their door positions....
    auto wallStates = GetWallStatesForRoom(roomCoords);

    for(int p = 0; p < (int)EDirectionType::NumDirectionTypes; ++p)
    {
        auto wallState = wallStates[p];
        if(!wallState->HasDoor())
        {
            EDirectionType direction = (EDirectionType)p;
            int maxDoorPosition = (direction == EDirectionType::North || direction == EDirectionType::South) ? NumGridUnitsY - 2
                                                                                                                : NumGridUnitsX - 2;
            wallState->GenerateRandomDoorPosition(maxDoorPosition);
        }
    }

    FIntPoint roomIndices = GetRoomXYIndicesChecked(roomCoords);
    if (!DoesRoomExist(roomCoords))
    {
        RoomStates[roomIndices.X][roomIndices.Y].InitializeRoom(MaxRoomHealth, complexity, density);

        RoomBuilders[roomIndices.X][roomIndices.Y]->BuildRoom({wallStates[(int)EDirectionType::North]->DoorPosition,
                                                               wallStates[(int)EDirectionType::East]->DoorPosition,
                                                               wallStates[(int)EDirectionType::South]->DoorPosition,
                                                               wallStates[(int)EDirectionType::West]->DoorPosition},
                                                               complexity, density);
        FlagWallsForUpdate(roomCoords);
    }
}

void ATPGameDemoGameState::DisableRoomState(FIntPoint roomCoords)
{
    FIntPoint roomIndices = GetRoomXYIndicesChecked(roomCoords);
    if (DoesRoomExist(roomCoords))
    {
        RoomStates[roomIndices.X][roomIndices.Y].DisableRoom();
        RoomBuilders[roomIndices.X][roomIndices.Y]->DestroyRoom();
        FlagWallsForUpdate(roomCoords);
    }
}

void ATPGameDemoGameState::SetRoomTrained(FIntPoint roomCoords)
{
    FIntPoint roomIndices = GetRoomXYIndicesChecked(roomCoords);
    if (DoesRoomExist(roomCoords))
    {
        RoomStates[roomIndices.X][roomIndices.Y].SetRoomTrained();
        FlagWallsForUpdate(roomCoords);
    }
}

void ATPGameDemoGameState::SetRoomTrainingProgress(FIntPoint roomCoords, float progress)
{
    FIntPoint roomIndices = GetRoomXYIndicesChecked(roomCoords);
    if (DoesRoomExist(roomCoords))
    {
        RoomStates[roomIndices.X][roomIndices.Y].TrainingProgress = progress;
        RoomBuilders[roomIndices.X][roomIndices.Y]->TrainingProgressUpdated(progress);
        TArray<bool> neighbourStates = GetNeighbouringRoomStates(roomCoords);
        for (int i = 0; i < (int)EDirectionType::NumDirectionTypes; ++i)
        {
            EDirectionType direction = (EDirectionType) i;
            if(neighbourStates[i])
            {
                AWallBuilder* wallBuilder = GetWallBuilder(roomCoords, direction);
                EDirectionType relativeDirection = (direction == EDirectionType::North) ? EDirectionType::South :
                                                   (direction == EDirectionType::East ) ? EDirectionType::West : direction;
                wallBuilder->TrainingProgressUpdatedForDoor(relativeDirection, progress);
            }
        }
    }
}

void ATPGameDemoGameState::EnableWallState(FIntPoint roomCoords, EDirectionType wallType)
{
    if (!DoesWallExist(roomCoords, wallType))
    {
        GetWallState(roomCoords, wallType).InitializeWall();
        auto wallBuilder = GetWallBuilder(roomCoords, wallType);
        if (wallBuilder != nullptr)
        {
            if (wallType == EDirectionType::North || wallType == EDirectionType::South)
                wallBuilder->BuildSouthWall();
            else
                wallBuilder->BuildWestWall();
        }
    }
}

void ATPGameDemoGameState::DisableWallState(FIntPoint roomCoords, EDirectionType wallType)
{
    if (DoesWallExist(roomCoords, wallType))
    {
        GetWallState(roomCoords, wallType).DisableWall();
        auto wallBuilder = GetWallBuilder(roomCoords, wallType);
        if (wallBuilder != nullptr)
        {
            if (wallType == EDirectionType::North || wallType == EDirectionType::South)
                wallBuilder->DestroySouthWall();
            else
                wallBuilder->DestroyWestWall();
        }
    }
}

void ATPGameDemoGameState::EnableDoorState(FIntPoint roomCoords, EDirectionType wallType)
{
    if (!DoesDoorExist(roomCoords, wallType))
    {
        GetWallState(roomCoords, wallType).InitializeDoor();
        auto wallBuilder = GetWallBuilder(roomCoords, wallType);
        if (wallBuilder != nullptr)
        {
            if (wallType == EDirectionType::North || wallType == EDirectionType::South)
                wallBuilder->SpawnSouthDoor();
            else
                wallBuilder->SpawnWestDoor();
        }
    }
}

void ATPGameDemoGameState::DisableDoorState(FIntPoint roomCoords, EDirectionType wallType)
{
    if (DoesDoorExist(roomCoords, wallType))
    {
        GetWallState(roomCoords, wallType).DisableDoor();
        auto wallBuilder = GetWallBuilder(roomCoords, wallType);
        if (wallBuilder != nullptr)
        {
            if (wallType == EDirectionType::North || wallType == EDirectionType::South)
                wallBuilder->DestroySouthDoor();
            else
                wallBuilder->DestroyWestDoor();
        }
    }
}

void ATPGameDemoGameState::GeneratePerimeterRooms()
{
	UnlockPerimeterDoors();
	int numRoomsOnEdge = CurrentPerimeter * 2 + 1;
	int cornerRoom = numRoomsOnEdge / 2;
	for (int i = -cornerRoom; i < cornerRoom + 1; ++i)
	{
		DoorOpened(FIntPoint(i, -cornerRoom), EDirectionType::East, 0.2f, 0.2f);
		DoorOpened(FIntPoint(i, cornerRoom), EDirectionType::West, 0.2f, 0.2f);
		DoorOpened(FIntPoint(-cornerRoom, i), EDirectionType::North, 0.2f, 0.2f);
		DoorOpened(FIntPoint(cornerRoom, i), EDirectionType::South, 0.2f, 0.2f);
	}
}

void ATPGameDemoGameState::ConnectPerimeterRooms()
{
	int numRoomsOnEdge = CurrentPerimeter * 2 + 1;
	int cornerRoom = numRoomsOnEdge / 2;
	for (int i = -cornerRoom; i < cornerRoom + 1; ++i)
	{
		SetRoomConnected(FIntPoint(i, -cornerRoom));
		SetRoomConnected(FIntPoint(i, cornerRoom));
		SetRoomConnected(FIntPoint(-cornerRoom, i));
		SetRoomConnected(FIntPoint(cornerRoom, i));
	}
	
}

void ATPGameDemoGameState::DestroyDoorInRoom(FIntPoint roomCoords, EDirectionType doorWallDirection)
{
    if (auto wallBuilder = GetWallBuilder(roomCoords, doorWallDirection))
    {
        if (doorWallDirection == EDirectionType::North || doorWallDirection == EDirectionType::South)
            wallBuilder->DestroySouthDoor();
        else
            wallBuilder->DestroyWestDoor();
    }
}

void ATPGameDemoGameState::ActorEnteredTilePosition(FIntPoint roomCoords, FIntPoint tilePosition)
{
    FIntPoint roomIndices = GetRoomXYIndicesChecked(roomCoords);
    RoomStates[roomIndices.X][roomIndices.Y].ActorEnteredTilePosition(tilePosition);
}

void ATPGameDemoGameState::ActorExitedTilePosition(FIntPoint roomCoords, FIntPoint tilePosition)
{
    FIntPoint roomIndices = GetRoomXYIndicesChecked(roomCoords);
    RoomStates[roomIndices.X][roomIndices.Y].ActorExitedTilePosition(tilePosition);
}

void ATPGameDemoGameState::DestroyNeighbouringDoors(FIntPoint roomCoords, TArray<bool> positionsToDestroy)
{
    for (int p = 0; p < (int)EDirectionType::NumDirectionTypes; ++p)
    {
        if (p < positionsToDestroy.Num() && positionsToDestroy[p])
        {
            EDirectionType direction = (EDirectionType)p;
            DestroyDoorInRoom(roomCoords, direction);
        }
    }
}

void ATPGameDemoGameState::DoorOpened(FIntPoint roomCoords, EDirectionType wallDirection, float complexity, float density)
{
    if (IsDoorUnlocked(roomCoords, wallDirection))
    {
        if (!DoesRoomExist(roomCoords))
        {
            EnableRoomState(roomCoords, complexity, density);
        }
        FIntPoint neighbourRoomCoords = GetRoomCoords(GetNeighbouringRoomIndices(roomCoords, wallDirection));
        if (!DoesRoomExist(neighbourRoomCoords))
        {
            EnableRoomState(neighbourRoomCoords, complexity, density);
        }
    }
}

void ATPGameDemoGameState::LockDoor(FIntPoint roomCoords, EDirectionType wallDirection)
{
    auto wallState = GetWallStatesForRoom(roomCoords)[(int)wallDirection];
    wallState->LockDoor();
    auto wallBuilder = GetWallBuilder(roomCoords, wallDirection);
    if (wallBuilder != nullptr)
    {
        if (wallDirection == EDirectionType::North || wallDirection == EDirectionType::South)
            wallBuilder->LockSouthDoor();
        else
            wallBuilder->LockWestDoor();
    }
}

void ATPGameDemoGameState::UnlockDoor(FIntPoint roomCoords, EDirectionType wallDirection)
{
    auto wallState = GetWallStatesForRoom(roomCoords)[(int)wallDirection];
    wallState->UnlockDoor();
    auto wallBuilder = GetWallBuilder(roomCoords, wallDirection);
    if (wallBuilder != nullptr)
    {
        if (wallDirection == EDirectionType::North || wallDirection == EDirectionType::South)
            wallBuilder->UnlockSouthDoor();
        else
            wallBuilder->UnlockWestDoor();
    }
}

void ATPGameDemoGameState::RegisterSignalLostCallback(const FOnSignalLost& Callback)
{
    OnSignalLost.AddLambda([Callback]()
    {
        Callback.ExecuteIfBound();
    });
}

void ATPGameDemoGameState::RegisterPerimeterCompleteCallback(const FOnPerimeterComplete& Callback)
{
    OnPerimeterComplete.AddLambda([Callback]()
    {
        Callback.ExecuteIfBound();
    });
}

//============================================================================
//============================================================================

FIntPoint ATPGameDemoGameState::GetRoomXYIndicesChecked(FIntPoint roomCoords) const
{
    const int x = roomCoords.X + NumGridsXY / 2;
    const int y = roomCoords.Y + NumGridsXY / 2;
    FIntPoint roomIndices = FIntPoint(x,y);
    ensure(RoomXYIndicesValid(roomIndices));
    return roomIndices;
}

const RoomState& ATPGameDemoGameState::GetRoomStateChecked(FIntPoint roomCoords) const
{
    FIntPoint roomIndices = GetRoomXYIndicesChecked(roomCoords);
    return RoomStates[roomIndices.X][roomIndices.Y];
}

FIntPoint ATPGameDemoGameState::GetRoomCoords(FIntPoint roomIndices) const
{
    const int x = roomIndices.X - NumGridsXY / 2;
    const int y = roomIndices.Y - NumGridsXY / 2;
    FIntPoint roomCoords = FIntPoint(x,y);
    return roomCoords;
}

bool ATPGameDemoGameState::RoomXYIndicesValid(FIntPoint roomIndices) const
{
    const int x = roomIndices.X;
    const int y = roomIndices.Y;
    return x >= 0 && x < RoomStates.Num() && y >= 0 && y < RoomStates[0].Num();
}

bool ATPGameDemoGameState::WallXYIndicesValid(FIntPoint wallRoomCoords) const
{
    const int x = wallRoomCoords.X;
    const int y = wallRoomCoords.Y;
    return x >= 0 && x < WallStates.Num() && y >= 0 && y < WallStates[0].Num();
}

bool ATPGameDemoGameState::InnerRoomPositionValid(FIntPoint positionInRoom) const
{
    const int x = positionInRoom.X;
    const int y = positionInRoom.Y;
    return x >= 0 && x < NumGridUnitsX && y >= 0 && y < NumGridUnitsY;
}

void ATPGameDemoGameState::WrapRoomPositionPair (FRoomPositionPair& roomPositionPair)
{
    const FIntPoint pos = roomPositionPair.PositionInRoom;
    FIntPoint roomCoords = roomPositionPair.RoomCoords;
    const int roomOffsetX = pos.X / (NumGridUnitsX - 1) - (pos.X < 0 ? 1 : 0);  
    const int roomOffsetY = pos.Y / (NumGridUnitsY - 1) - (pos.Y < 0 ? 1 : 0);
    roomCoords = {roomCoords.X + roomOffsetX, roomCoords.Y + roomOffsetY};
    const int x = FMath::Fmod(pos.X, NumGridUnitsX - 1) + (pos.X < 0 ? NumGridUnitsX - 1 : 0);
    const int y = FMath::Fmod(pos.Y, NumGridUnitsY - 1) + (pos.Y < 0 ? NumGridUnitsY - 1 : 0);
    roomPositionPair = {roomCoords, {x,y}};
}

FVector2D ATPGameDemoGameState::GetWorldXYForRoomAndPosition(FRoomPositionPair roomPositionPair)
{
    const FIntPoint roomCoords = roomPositionPair.RoomCoords;
    const FIntPoint pos = roomPositionPair.PositionInRoom;
    return GetGridCellWorldPosition(pos.X, pos.Y, roomCoords.X, roomCoords.Y);
}

WallState& ATPGameDemoGameState::GetWallState(FIntPoint roomCoords, EDirectionType direction)
{
    FIntPoint roomIndices = GetRoomXYIndicesChecked(roomCoords);
    switch(direction)
    {
        case EDirectionType::North: 
        {
            FIntPoint neighbourRoom = GetNeighbouringRoomIndices(roomCoords, direction);
            return WallStates[neighbourRoom.X][neighbourRoom.Y].SouthWall;
        }
        case EDirectionType::East:
        {
            FIntPoint neighbourRoom = GetNeighbouringRoomIndices(roomCoords, direction);
            return WallStates[neighbourRoom.X][neighbourRoom.Y].WestWall;
        }
        case EDirectionType::South:
        {
            return WallStates[roomIndices.X][roomIndices.Y].SouthWall;
        }
        case EDirectionType::West: 
            return WallStates[roomIndices.X][roomIndices.Y].WestWall;
        default: ensure(false); return WallStates[0][0].SouthWall;
    }
}

bool ATPGameDemoGameState::IsWallInUpdateList(FIntPoint coords, EDirectionType wallType)
{
    for(auto wallDescriptor : WallsToUpdate)
        if(wallDescriptor.WallCoupleCoords == coords && wallDescriptor.WallType == wallType)
            return true;
    return false;
}

void ATPGameDemoGameState::FlagWallsForUpdate(FIntPoint roomCoords)
{
    FIntPoint roomIndices = GetRoomXYIndicesChecked(roomCoords);
    TArray<WallState*> wallStates;
    wallStates.Empty();
    auto northNeighbourCoords = GetRoomCoords(GetNeighbouringRoomIndices(roomCoords, EDirectionType::North));
    auto eastNeighbourCoords = GetRoomCoords(GetNeighbouringRoomIndices(roomCoords, EDirectionType::East));

    AsyncTask(ENamedThreads::GameThread, [this, roomCoords, northNeighbourCoords, eastNeighbourCoords]()
    {
        if(!IsWallInUpdateList(roomCoords, EDirectionType::South))
            WallsToUpdate.Add({roomCoords, EDirectionType::South});
        if(!IsWallInUpdateList(roomCoords, EDirectionType::West))
            WallsToUpdate.Add({roomCoords, EDirectionType::West});
        if(!IsWallInUpdateList(northNeighbourCoords, EDirectionType::South))
            WallsToUpdate.Add({northNeighbourCoords, EDirectionType::South});
        if(!IsWallInUpdateList(eastNeighbourCoords, EDirectionType::West))
            WallsToUpdate.Add({eastNeighbourCoords, EDirectionType::West});
    });
    
}

// ----------------------- Inner Grid Properties -------------------------------------

void ATPGameDemoGameState::SetGridUnitLengthXCM (int x)
{
    GridUnitLengthXCM = x;
    OnMazeDimensionsChanged.Broadcast();
}

void ATPGameDemoGameState::SetGridUnitLengthYCM (int y)
{
    GridUnitLengthYCM = y;
    OnMazeDimensionsChanged.Broadcast();
}

void ATPGameDemoGameState::SetNumGridUnitsX (int numUnitsX)
{
    NumGridUnitsX = numUnitsX;
    OnMazeDimensionsChanged.Broadcast();
}

void ATPGameDemoGameState::SetNumGridUnitsY (int numUnitsY)
{
    NumGridUnitsY = numUnitsY;
    OnMazeDimensionsChanged.Broadcast();
}

FVector2D ATPGameDemoGameState::GetCellWorldPosition(UObject* worldContextObject, int x, int y, int RoomOffsetX, int RoomOffsetY, bool getCentre /* = true */)
{
    ATPGameDemoGameState* gameState = (ATPGameDemoGameState*) worldContextObject->GetWorld()->GetGameState();

    if (gameState == nullptr)
        return FVector2D (0.0f, 0.0f);

    return gameState->GetGridCellWorldPosition(x, y, RoomOffsetX, RoomOffsetY, getCentre);
}

FVector2D ATPGameDemoGameState::GetGridCellWorldPosition (int x, int y, int RoomOffsetX, int RoomOffsetY, bool getCentre /* = true */)
{
    float centreOffset = getCentre ? 0.5f : 0.0f;
    float positionX = ((x - NumGridUnitsX / 2) + centreOffset) * GridUnitLengthXCM;
    float positionY = ((y - NumGridUnitsY / 2) + centreOffset) * GridUnitLengthYCM;
    positionX += RoomOffsetX * NumGridUnitsX * GridUnitLengthXCM - RoomOffsetX * GridUnitLengthXCM;
    positionY += RoomOffsetY * NumGridUnitsY * GridUnitLengthYCM - RoomOffsetY * GridUnitLengthYCM;
    return FVector2D (positionX, positionY);
}

// -------------------------- Perimeter Mechanic ----------------------------------

int ATPGameDemoGameState::GetNumRoomsOnPerimeter()
{
    return CurrentPerimeter * 8;
}

//Should call this in tick room update, if the room in question is connected. That would probably require managing an array of connected perimeter rooms, 
// so we could then check if the room is contained first before adding it. 
void ATPGameDemoGameState::RoomWasConnected(FIntPoint roomCoords)
{
    const int x = FMath::Abs(roomCoords.X);
    const int y = FMath::Abs(roomCoords.Y);
    if (x > CurrentPerimeter || y > CurrentPerimeter)
    {
        CurrentPerimeter = FMath::Max(x,y);
        NumPerimeterRoomsConnected = 1;
    }
    else if (x == CurrentPerimeter || y == CurrentPerimeter)
    {
        ++NumPerimeterRoomsConnected;
        if (NumPerimeterRoomsConnected >= GetNumRoomsOnPerimeter())
        {
            PerimeterDoorsNeedUnlocked = true;
        }
    }
    FlagWallsForUpdate(roomCoords);
}

void ATPGameDemoGameState::UpdateDoorLockedStateForNeighbouringRooms(FIntPoint roomCoords, FIntPoint neighbourCoords, EDirectionType wallType)
{
    const bool roomExists = DoesRoomExist(roomCoords);
    const bool neighbourExists = DoesRoomExist(neighbourCoords); 
    const bool roomConnected = IsRoomConnected(roomCoords);
    const bool neighbourConnected = IsRoomConnected(neighbourCoords);
    const EDoorState doorState = GetWallState(roomCoords, wallType).DoorState;
    const bool doorOnPerimeter = DoorIsOnPerimeter(roomCoords, wallType);
    if (roomConnected || neighbourConnected)
    {
        if (doorState == EDoorState::Locked && !doorOnPerimeter)
        {
            UnlockDoor(roomCoords, wallType);
        }
    }
    else
    {
        const bool oneNotConnectedAndOtherDoesntExist = (roomExists && !roomConnected && !neighbourExists) 
                                                        ||(!roomExists && neighbourExists && !neighbourConnected); 
        if (oneNotConnectedAndOtherDoesntExist && doorState != EDoorState::Locked)
        {
            LockDoor(roomCoords, wallType);
        }
    }
}

void ATPGameDemoGameState::UnlockPerimeterDoors()
{
    for (int p = -CurrentPerimeter; p <= CurrentPerimeter; ++p)
    {
        UnlockDoor(FIntPoint(CurrentPerimeter, p), EDirectionType::North);
        UnlockDoor(FIntPoint(p, CurrentPerimeter), EDirectionType::East);
        UnlockDoor(FIntPoint(-CurrentPerimeter, p), EDirectionType::South);
        UnlockDoor(FIntPoint(p, -CurrentPerimeter), EDirectionType::West);
    }
}

void ATPGameDemoGameState::LockDoorIfOnPerimeter(FIntPoint roomCoords)
{
    const int x = roomCoords.X;
    const int y = roomCoords.Y;
    if (x == CurrentPerimeter)
        LockDoor(roomCoords, EDirectionType::North);
    if (y == CurrentPerimeter)
        LockDoor(roomCoords, EDirectionType::East);
    if (x == -CurrentPerimeter)
        LockDoor(roomCoords, EDirectionType::South);
    if (y == -CurrentPerimeter)
        LockDoor(roomCoords, EDirectionType::West);
}

bool ATPGameDemoGameState::DoorIsOnPerimeter(FIntPoint roomCoords, EDirectionType doorDirection)
{
    const int x = roomCoords.X;
    const int y = roomCoords.Y;
    const int outerPerimeter = CurrentPerimeter + 1;
    if (doorDirection == EDirectionType::North && (x == CurrentPerimeter || x == -outerPerimeter))
        return true;
    if (doorDirection == EDirectionType::South && (x == outerPerimeter || x == -CurrentPerimeter))
        return true;
    if (doorDirection == EDirectionType::East && (y == CurrentPerimeter || y == -outerPerimeter))
        return true;
    if (doorDirection == EDirectionType::West && (y == outerPerimeter || y == -CurrentPerimeter))
        return true;

    return false;
}
