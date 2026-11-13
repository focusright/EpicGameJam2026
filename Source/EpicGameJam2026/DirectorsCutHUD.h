#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DirectorsCutHUD.generated.h"

class ADirectorsCutGameMode;
class UFont;

UCLASS()
class ADirectorsCutHUD : public AHUD
{
	GENERATED_BODY()

public:
	ADirectorsCutHUD();
	virtual void DrawHUD() override;
	virtual void NotifyHitBoxClick(FName BoxName) override;

private:
	UPROPERTY()
	TObjectPtr<UFont> RegularFont;

	UPROPERTY()
	TObjectPtr<UFont> MonoFont;

	float Width = 0.0f;
	float Height = 0.0f;
	float UI = 1.0f;

	void Box(float X, float Y, float W, float H, FColor Color);
	void Text(const FString& Value, float X, float Y, float Size, FColor Color, bool bHeavy = false,
		float Align = 0.0f, bool bMono = false);
	void Button(const TCHAR* Label, float X, float Y, float W);
	void DrawTitle();
	void DrawArena(const ADirectorsCutGameMode& Game);
	void DrawReel(const ADirectorsCutGameMode& Game);
	void DrawScreening(const ADirectorsCutGameMode& Game);
	void DrawPayout(const ADirectorsCutGameMode& Game);
};
