// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Engine.h"
#include "TPGameDemo.generated.h"

#define ON_SCREEN_DEBUGGING 0

UENUM(BlueprintType)
enum class EDirectionType : uint8
{
    North UMETA (DisplayName = "North"),
    East  UMETA (DisplayName = "East"),
    South UMETA (DisplayName = "South"),
    West  UMETA (DisplayName = "West"),
    NumDirectionTypes
};

UENUM(BlueprintType)
enum class ECellState : uint8
{
    Open UMETA (DisplayName = "Open"),
    Closed  UMETA (DisplayName = "Closed"),
    Door UMETA (DisplayName = "Door"),
    NumStates
};

USTRUCT(Blueprintable)
struct FRoomPositionPair
{
    GENERATED_USTRUCT_BODY()
    FIntPoint RoomCoords;
    FIntPoint PositionInRoom;
};

namespace LevelBuilderHelpers
{
    const FString LevelsDir();

    FIntPoint GetTargetPointForAction(FIntPoint startingPoint, EDirectionType actionType, int numSpaces = 1);

    bool GridPositionIsValid(FIntPoint position, int sizeX, int sizeY);

    const FString ActionStringDelimiter();

    /*
    Takes in a text file and fills an array with numbers.
    File should be a square grid format with numbers separated by spaces.
    If invertX is true, the lines in the text file will be entered from bottom to top
    into the 2D int array (rather than top to bottom).
    */
    void FillArrayFromTextFile (FString fileName, TArray<TArray<int>>& arrayRef, bool invertX = false);

    /*
    Writes a 2D int array to a text file. File will have a square grid format with numbers 
    separated by spaces. If invertX is true, the lines in the text file will be entered 
    from bottom to top (rather than top to bottom).
    */
    void WriteArrayToTextFile (TArray<TArray<int>>& arrayRef, FString fileName, bool invertX = false);

    void PrintArray(TArray<TArray<int>>& arrayRef);
};