// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GameplayDemoHUD.generated.h"

UCLASS()
class AGameplayDemoHUD : public AHUD
{
	GENERATED_BODY()

public:
	AGameplayDemoHUD();

	/** Primary draw call for the HUD */
	virtual void DrawHUD() override;

private:
	/** Crosshair asset pointer */
	class UTexture2D* CrosshairTex;

};

