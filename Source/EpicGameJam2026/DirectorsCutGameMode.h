#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "DirectorsCutGameMode.generated.h"

class ACameraActor;
class ADirectorsCutStage;

namespace DirectorsCut
{
	constexpr int32 ReelSize = 6;
	constexpr float OpeningDuration = 2.6f;
	constexpr float PayoutDuration = 4.2f;
	constexpr float PayoutStart = 0.55f;
	constexpr float BagInterval = 0.46f;
	enum class EPhase : uint8 { Title, Combat, Wrap, Screening, GetPaid, Payout, Reception };
	enum class EEnemyMove : uint8 { Waiting, Chase, Windup, Lunge, Recover, Cut };
	enum class ECue : uint8 { Swing, Splice, Dash, Warning, Impact, Projector, Aircraft, Cash, Count };

	struct FShot
	{
		const TCHAR* Title;
		float Duration;
		FColor Color;
	};

	extern const FShot Shots[ReelSize];
}

class ADirectorsCutGameMode;

UCLASS()
class ADirectorsCutController : public APlayerController
{
	GENERATED_BODY()

public:
	void SetCursorVisible(bool bVisible);
	FVector2D ReadMove() const;
	bool ReadAim(const FVector& PlaneOrigin, FVector2D& Aim) const;

protected:
	virtual void SetupInputComponent() override;

private:
	ADirectorsCutGameMode* GetCutGame() const;
	void Slash();
	void SlashAtCursor();
	void Dodge();
	void Confirm();
};

UCLASS()
class ADirectorsCutGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADirectorsCutGameMode();
	virtual void Tick(float DeltaSeconds) override;

	void Slash(bool bAimWithMouse);
	void Dodge();
	void Confirm();
	DirectorsCut::EPhase GetPhase() const { return Phase; }
	float GetPhaseTime() const { return PhaseTime; }
	int32 GetFilmShot() const { return FilmShot; }
	float GetShotTime() const { return ShotTime; }
	int32 GetReelCount() const { return ReelCount; }
	bool HasShot(int32 Index) const { return EarnedShots[Index]; }
	bool Won() const { return ReelCount == DirectorsCut::ReelSize; }
	int32 GetPayoutPercent() const;
	float GetDashCharge() const;
	float GetSpliceFlash() const { return SpliceFlash; }
	int32 GetLastSplice() const { return LastSplice; }

protected:
	virtual void BeginPlay() override;

private:
	struct FEnemy
	{
		FVector2D Position = FVector2D::ZeroVector;
		FVector2D Direction = FVector2D::UnitX();
		DirectorsCut::EEnemyMove Move = DirectorsCut::EEnemyMove::Waiting;
		float Time = 0.0f;
	};

	UPROPERTY()
	TObjectPtr<ADirectorsCutStage> Stage;

	UPROPERTY()
	TObjectPtr<ACameraActor> Camera;

	TArray<TArray<int16>> Sounds;

	DirectorsCut::EPhase Phase = DirectorsCut::EPhase::Title;
	float PhaseTime = 0.0f;
	FVector2D Position = FVector2D::ZeroVector;
	FVector2D Facing = FVector2D::UnitX();
	FVector2D DashDirection = FVector2D::UnitX();
	FVector2D SlashDirection = FVector2D::UnitX();
	FEnemy Enemies[DirectorsCut::ReelSize];
	bool EarnedShots[DirectorsCut::ReelSize] = {};
	float SlashTime = 0.0f;
	float DashTime = 0.0f;
	float DashCooldown = 0.0f;
	float SpliceFlash = 0.0f;
	int32 LastSplice = INDEX_NONE;
	int32 ReelCount = 0;
	int32 FilmShot = INDEX_NONE;
	float ShotTime = 0.0f;
	bool bFilmActionPlayed = false;
	int32 PaidBags = 0;

	void SetPhase(DirectorsCut::EPhase NewPhase);
	void StartTake();
	void UpdateCombat(float DeltaSeconds);
	void UpdateEnemy(int32 Index, float DeltaSeconds);
	void Splice(int32 Index);
	void UpdateArena();
	void UpdateScreening(float DeltaSeconds);
	void UpdatePayout();
	void PlayCue(DirectorsCut::ECue Cue);
	void BuildSounds();
};
