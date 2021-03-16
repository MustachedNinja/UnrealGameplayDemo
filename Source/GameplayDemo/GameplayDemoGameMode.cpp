// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayDemoGameMode.h"
#include "GameplayDemoHUD.h"
#include "GameplayDemoCharacter.h"
#include "UObject/ConstructorHelpers.h"

AGameplayDemoGameMode::AGameplayDemoGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPersonCPP/Blueprints/FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

	// use our custom HUD class
	HUDClass = AGameplayDemoHUD::StaticClass();
}
