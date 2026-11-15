#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DirectorsCutHUD.generated.h"

class ADirectorsCutGameMode;
class UFont;
class UTexture2D;

UCLASS()
class ADirectorsCutHUD : public AHUD
{
	GENERATED_BODY()

public:
	ADirectorsCutHUD();
	virtual void DrawHUD() override;
	virtual void NotifyHitBoxClick(FName BoxName) override;
	void ShowDLSSStatus(const TCHAR* Status);

private:
	UPROPERTY()
	TObjectPtr<UFont> RegularFont;

	UPROPERTY()
	TObjectPtr<UFont> MonoFont;

	UPROPERTY()
	TObjectPtr<UTexture2D> GrainTexture;

	float Width = 0.0f;
	float Height = 0.0f;
	float UI = 1.0f;
	FString DLSSStatus;
	double DLSSStatusUntil = 0.0;

	void Box(float X, float Y, float W, float H, FColor Color);
	void Text(const FString& Value, float X, float Y, float Size, FColor Color, bool bHeavy = false,
		float Align = 0.0f, bool bMono = false);
	void Button(const TCHAR* Label, float X, float Y, float W);
	void EnsureGrainTexture();
	void DrawFilmGrain(float X, float Y, float W, float H, float Opacity);
	void DrawTitle(bool bStarting);
	void DrawArena(const ADirectorsCutGameMode& Game);
	void DrawReel(const ADirectorsCutGameMode& Game);
	void DrawScreening(const ADirectorsCutGameMode& Game);
	void DrawPayout(const ADirectorsCutGameMode& Game);
	void DrawPaused();
};
