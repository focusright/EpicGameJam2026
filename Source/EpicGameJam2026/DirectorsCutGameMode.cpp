#include "DirectorsCutGameMode.h"

#include "Camera/CameraActor.h"
#include "Components/AudioComponent.h"
#include "DirectorsCutHUD.h"
#include "DirectorsCutStage.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Sound/SoundWaveProcedural.h"

namespace DirectorsCut
{
	const FShot Shots[ReelSize] = {
		{TEXT("LANDFALL"),        3.4f, FColor(102, 188, 184)},
		{TEXT("FIRST STOMP"),     3.3f, FColor(217, 147, 86)},
		{TEXT("ROTOR SQUADRON"),  3.7f, FColor(134, 161, 209)},
		{TEXT("JET INTERCEPT"),   3.6f, FColor(191, 156, 204)},
		{TEXT("COUNTERATTACK"),   3.5f, FColor(212, 98, 89)},
		{TEXT("TOKYO FALLS"),     4.1f, FColor(226, 187, 110)},
	};
}

using namespace DirectorsCut;

namespace
{
	constexpr float SlashDuration = 0.34f;
	constexpr float DashDuration = 0.20f;
	constexpr float DashRecharge = 0.85f;
	constexpr float WindupDuration = 0.70f;
	constexpr float LungeDuration = 0.27f;
	constexpr float CutDuration = 0.55f;
	constexpr int32 SampleRate = 24000;

	FVector2D ClampToArena(FVector2D Point, float Margin = 0.0f)
	{
		return FVector2D(
			FMath::Clamp(Point.X, -ADirectorsCutStage::ArenaDepth - Margin, ADirectorsCutStage::ArenaDepth + Margin),
			FMath::Clamp(Point.Y, -ADirectorsCutStage::ArenaWidth - Margin, ADirectorsCutStage::ArenaWidth + Margin));
	}
}

void ADirectorsCutController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ADirectorsCutController::SlashAtCursor);
	InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ADirectorsCutController::Slash);
	InputComponent->BindKey(EKeys::LeftShift, IE_Pressed, this, &ADirectorsCutController::Dodge);
	InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &ADirectorsCutController::Dodge);
	InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &ADirectorsCutController::Confirm);
	FInputKeyBinding& PauseBind = InputComponent->BindKey(EKeys::P, IE_Pressed, this, &ADirectorsCutController::TogglePause);
	PauseBind.bExecuteWhenPaused = true;
	FInputKeyBinding& QuitBind = InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ADirectorsCutController::QuitGame);
	QuitBind.bExecuteWhenPaused = true;
}

ADirectorsCutGameMode* ADirectorsCutController::GetCutGame() const
{
	return GetWorld()->GetAuthGameMode<ADirectorsCutGameMode>();
}

void ADirectorsCutController::SetCursorVisible(bool bVisible)
{
	bShowMouseCursor = bVisible;
	bEnableClickEvents = bVisible;
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
}

FVector2D ADirectorsCutController::ReadMove() const
{
	return FVector2D(
		static_cast<int32>(IsInputKeyDown(EKeys::W) || IsInputKeyDown(EKeys::Up)) -
			static_cast<int32>(IsInputKeyDown(EKeys::S) || IsInputKeyDown(EKeys::Down)),
		static_cast<int32>(IsInputKeyDown(EKeys::D) || IsInputKeyDown(EKeys::Right)) -
			static_cast<int32>(IsInputKeyDown(EKeys::A) || IsInputKeyDown(EKeys::Left))).GetSafeNormal();
}

bool ADirectorsCutController::ReadAim(const FVector& PlaneOrigin, FVector2D& Aim) const
{
	FVector RayOrigin;
	FVector RayDirection;
	if (!DeprojectMousePositionToWorld(RayOrigin, RayDirection) || FMath::IsNearlyZero(RayDirection.Z))
	{
		return false;
	}
	const double Distance = (PlaneOrigin.Z - RayOrigin.Z) / RayDirection.Z;
	if (Distance <= 0.0)
	{
		return false;
	}
	const FVector Point = RayOrigin + RayDirection * Distance - PlaneOrigin;
	Aim = FVector2D(Point.X, Point.Y);
	return true;
}

void ADirectorsCutController::Slash()
{
	if (UGameplayStatics::IsGamePaused(this))
	{
		return;
	}
	if (ADirectorsCutGameMode* Game = GetCutGame())
	{
		Game->Slash(false);
	}
}

void ADirectorsCutController::SlashAtCursor()
{
	if (UGameplayStatics::IsGamePaused(this))
	{
		return;
	}
	if (ADirectorsCutGameMode* Game = GetCutGame())
	{
		Game->Slash(true);
	}
}

void ADirectorsCutController::Dodge()
{
	if (UGameplayStatics::IsGamePaused(this))
	{
		return;
	}
	if (ADirectorsCutGameMode* Game = GetCutGame())
	{
		Game->Dodge();
	}
}

void ADirectorsCutController::Confirm()
{
	if (UGameplayStatics::IsGamePaused(this))
	{
		return;
	}
	if (ADirectorsCutGameMode* Game = GetCutGame())
	{
		Game->Confirm();
	}
}

void ADirectorsCutController::TogglePause()
{
	const bool bPause = !UGameplayStatics::IsGamePaused(this);
	UGameplayStatics::SetGamePaused(this, bPause);
	if (bPause)
	{
		SetCursorVisible(true);
	}
	else if (const ADirectorsCutGameMode* Game = GetCutGame())
	{
		const DirectorsCut::EPhase Phase = Game->GetPhase();
		SetCursorVisible(Phase == DirectorsCut::EPhase::Title || Phase == DirectorsCut::EPhase::Combat ||
			Phase == DirectorsCut::EPhase::ReelReady || Phase == DirectorsCut::EPhase::GetPaid ||
			Phase == DirectorsCut::EPhase::Reception);
	}
}

void ADirectorsCutController::QuitGame()
{
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

ADirectorsCutGameMode::ADirectorsCutGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = nullptr;
	PlayerControllerClass = ADirectorsCutController::StaticClass();
	HUDClass = ADirectorsCutHUD::StaticClass();
}

void ADirectorsCutGameMode::BeginPlay()
{
	Super::BeginPlay();
	Stage = GetWorld()->SpawnActor<ADirectorsCutStage>();
	Camera = GetWorld()->SpawnActor<ACameraActor>();
	if (!Stage || !Camera || !Stage->Build())
	{
		SetActorTickEnabled(false);
		return;
	}
	BuildSounds();
	if (APlayerController* Controller = GetWorld()->GetFirstPlayerController())
	{
		Controller->SetViewTarget(Camera);
	}
	SetPhase(EPhase::Title);
	Stage->SetArenaCamera(*Camera, 0.0f, true);
}

void ADirectorsCutGameMode::SetPhase(EPhase NewPhase)
{
	Phase = NewPhase;
	PhaseTime = 0.0f;
	if (ADirectorsCutController* Controller = Cast<ADirectorsCutController>(GetWorld()->GetFirstPlayerController()))
	{
		Controller->SetCursorVisible(NewPhase == EPhase::Title || NewPhase == EPhase::Combat ||
			NewPhase == EPhase::ReelReady || NewPhase == EPhase::GetPaid || NewPhase == EPhase::Reception);
	}
}

void ADirectorsCutGameMode::StartTake()
{
	static const FVector2D Entries[ReelSize] = {
		{550.0, 105.0}, {-470.0, -950.0}, {60.0, 950.0},
		{530.0, -670.0}, {-470.0, 720.0}, {520.0, 750.0}
	};
	for (int32 Index = 0; Index < ReelSize; ++Index)
	{
		Enemies[Index] = FEnemy();
		Enemies[Index].Position = Entries[Index];
		EarnedShots[Index] = false;
	}
	Position = FVector2D::ZeroVector;
	Facing = FVector2D::UnitX();
	SlashTime = DashTime = DashCooldown = SpliceFlash = 0.0f;
	LastSplice = FilmShot = INDEX_NONE;
	ReelCount = PaidBags = 0;
	ShotTime = 0.0f;
	bFilmActionPlayed = false;
	Stage->ResetArena();
	SetPhase(EPhase::Combat);
	UpdateArena();
}

void ADirectorsCutGameMode::Confirm()
{
	if (!IsActorTickEnabled())
	{
		return;
	}
	switch (Phase)
	{
	case EPhase::Title:
		TitleStartDuration = Stage->PlayTitleStartAnimation();
		if (TitleStartDuration > 0.0f)
		{
			SetPhase(EPhase::TitleStart);
			break;
		}
		StartTake();
		break;
	case EPhase::ReelReady:
		SetPhase(EPhase::Screening);
		Stage->ShowFilm(*Camera, INDEX_NONE, 0.0f);
		PlayCue(ECue::Projector);
		break;
	case EPhase::Reception:
		StartTake();
		break;
	case EPhase::GetPaid:
		SetPhase(EPhase::Payout);
		UpdatePayout();
		break;
	default:
		break;
	}
}

void ADirectorsCutGameMode::Slash(bool bAimWithMouse)
{
	if (Phase != EPhase::Combat || PhaseTime < 0.1f || SlashTime > 0.0f || DashTime > 0.0f)
	{
		return;
	}
	SlashDirection = Facing;
	if (const ADirectorsCutController* Controller = Cast<ADirectorsCutController>(GetWorld()->GetFirstPlayerController());
		bAimWithMouse && Controller)
	{
		FVector2D Aim;
		if (Controller->ReadAim(ADirectorsCutStage::ArenaOrigin + FVector(0, 0, 65), Aim) &&
			FVector2D::DistSquared(Aim, Position) > FMath::Square(24.0f))
		{
			SlashDirection = (Aim - Position).GetSafeNormal();
		}
	}
	Facing = SlashDirection;
	SlashTime = SlashDuration;
	PlayCue(ECue::Swing);
}

void ADirectorsCutGameMode::Dodge()
{
	if (Phase != EPhase::Combat || DashCooldown > 0.0f)
	{
		return;
	}
	DashDirection = Facing;
	if (const ADirectorsCutController* Controller = Cast<ADirectorsCutController>(GetWorld()->GetFirstPlayerController()))
	{
		const FVector2D Move = Controller->ReadMove();
		if (!Move.IsNearlyZero())
		{
			DashDirection = Move;
		}
	}
	SlashTime = 0.0f;
	DashTime = DashDuration;
	DashCooldown = DashRecharge;
	Facing = DashDirection;
	PlayCue(ECue::Dash);
}

float ADirectorsCutGameMode::GetDashCharge() const
{
	return 1.0f - DashCooldown / DashRecharge;
}

int32 ADirectorsCutGameMode::GetPayoutPercent() const
{
	return FMath::RoundToInt(100.0f * ReelCount / ReelSize);
}

void ADirectorsCutGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Stage || !Camera)
	{
		return;
	}
	PhaseTime += DeltaSeconds;
	switch (Phase)
	{
	case EPhase::Title:
		TitleCameraTime += DeltaSeconds;
		Stage->SetArenaCamera(*Camera, TitleCameraTime, true);
		break;
	case EPhase::TitleStart:
		TitleCameraTime += DeltaSeconds;
		Stage->SetArenaCamera(*Camera, TitleCameraTime, true);
		if (PhaseTime >= TitleStartDuration + 0.1f)
		{
			StartTake();
		}
		break;
	case EPhase::Combat:
		UpdateCombat(DeltaSeconds);
		break;
	case EPhase::Wrap:
		SpliceFlash = FMath::Max(0.0f, SpliceFlash - DeltaSeconds);
		UpdateArena();
		if (PhaseTime >= (Won() ? 1.15f : 1.55f))
		{
			SetPhase(EPhase::ReelReady);
		}
		break;
	case EPhase::Screening:
		UpdateScreening(DeltaSeconds);
		break;
	case EPhase::Payout:
		UpdatePayout();
		break;
	default:
		break;
	}
}

void ADirectorsCutGameMode::UpdateCombat(float DeltaSeconds)
{
	// Bound the simulation step so a hitch cannot skip a blade's active window or a lunge.
	const float Step = FMath::Min(DeltaSeconds, 1.0f / 30.0f);
	const ADirectorsCutController* Controller = Cast<ADirectorsCutController>(GetWorld()->GetFirstPlayerController());
	const FVector2D Move = Controller ? Controller->ReadMove() : FVector2D::ZeroVector;
	const bool bDashing = DashTime > 0.0f;
	if (!bDashing && SlashTime <= 0.0f && !Move.IsNearlyZero())
	{
		Facing = Move;
	}
	const float MoveSpeed = SlashTime > 0.0f ? 165.0f : 410.0f;
	Position = ClampToArena(Position + (bDashing ? DashDirection * 1120.0f : Move * MoveSpeed) * Step);
	DashCooldown = FMath::Max(0.0f, DashCooldown - Step);
	SpliceFlash = FMath::Max(0.0f, SpliceFlash - Step);
	SlashTime = FMath::Max(0.0f, SlashTime - Step);

	const float SwingAge = SlashDuration - SlashTime;
	if (SlashTime > 0.0f && SwingAge >= 0.085f && SwingAge <= 0.225f)
	{
		for (int32 Index = 0; Index < ReelSize; ++Index)
		{
			const FEnemy& Enemy = Enemies[Index];
			const FVector2D ToEnemy = Enemy.Position - Position;
			const double Distance = ToEnemy.Size();
			if (Enemy.Move != EEnemyMove::Waiting && Enemy.Move != EEnemyMove::Cut && Distance < 228.0 &&
				(Distance < 62.0 || FVector2D::DotProduct(SlashDirection, ToEnemy / Distance) > 0.33))
			{
				Splice(Index);
			}
		}
	}
	if (Won())
	{
		SetPhase(EPhase::Wrap);
	}
	else
	{
		for (int32 Index = 0; Index < ReelSize && Phase == EPhase::Combat; ++Index)
		{
			UpdateEnemy(Index, Step);
		}
	}
	DashTime = FMath::Max(0.0f, DashTime - Step);
	UpdateArena();
	if (Phase == EPhase::Combat)
	{
		Stage->PoseDirector(Position, Facing, bDashing || Move.IsNearlyZero() ? 0.0f : PhaseTime * 16.0f,
			SlashTime / SlashDuration, 0.0f);
	}
}

void ADirectorsCutGameMode::UpdateEnemy(int32 Index, float DeltaSeconds)
{
	FEnemy& Enemy = Enemies[Index];
	Enemy.Time += DeltaSeconds;
	const FVector2D ToDirector = Position - Enemy.Position;
	const double Distance = ToDirector.Size();
	switch (Enemy.Move)
	{
	case EEnemyMove::Waiting:
		if (PhaseTime >= 0.55f + 2.15f * Index || (Index > 0 && ReelCount >= Index))
		{
			Enemy.Move = EEnemyMove::Chase;
			Enemy.Time = 0.0f;
		}
		break;
	case EEnemyMove::Chase:
		if (Distance > UE_SMALL_NUMBER)
		{
			Enemy.Direction = ToDirector / Distance;
		}
		if (Distance <= 192.0)
		{
			Enemy.Move = EEnemyMove::Windup;
			Enemy.Time = 0.0f;
			PlayCue(ECue::Warning);
		}
		else
		{
			Enemy.Position = ClampToArena(Enemy.Position + Enemy.Direction * (218.0f + 15.0f * Index) * DeltaSeconds, 85.0f);
		}
		break;
	case EEnemyMove::Windup:
		if (Enemy.Time < 0.39f && Distance > UE_SMALL_NUMBER)
		{
			Enemy.Direction = ToDirector / Distance;
		}
		if (Enemy.Time >= WindupDuration)
		{
			Enemy.Move = EEnemyMove::Lunge;
			Enemy.Time = 0.0f;
		}
		break;
	case EEnemyMove::Lunge:
	{
		const FVector2D Previous = Enemy.Position;
		Enemy.Position = ClampToArena(Previous + Enemy.Direction * 1110.0f * DeltaSeconds, 65.0f);
		const FVector2D Travel = Enemy.Position - Previous;
		const double Along = Travel.IsNearlyZero() ? 0.0 :
			FMath::Clamp(FVector2D::DotProduct(Position - Previous, Travel) / Travel.SizeSquared(), 0.0, 1.0);
		if (DashTime <= 0.0f && FVector2D::DistSquared(Previous + Travel * Along, Position) < FMath::Square(64.0f))
		{
			SlashTime = 0.0f;
			SetPhase(EPhase::Wrap);
			PlayCue(ECue::Impact);
		}
		else if (Enemy.Time >= LungeDuration)
		{
			Enemy.Move = EEnemyMove::Recover;
			Enemy.Time = 0.0f;
		}
		break;
	}
	case EEnemyMove::Recover:
		if (Enemy.Time >= 0.54f)
		{
			Enemy.Move = EEnemyMove::Chase;
			Enemy.Time = 0.0f;
		}
		break;
	case EEnemyMove::Cut:
		break;
	}
}

void ADirectorsCutGameMode::Splice(int32 Index)
{
	Enemies[Index].Move = EEnemyMove::Cut;
	Enemies[Index].Time = 0.0f;
	EarnedShots[Index] = true;
	++ReelCount;
	LastSplice = Index;
	SpliceFlash = CutDuration;
	PlayCue(ECue::Splice);
}

void ADirectorsCutGameMode::UpdateArena()
{
	Stage->SetArenaCamera(*Camera, PhaseTime, false);
	for (int32 Index = 0; Index < ReelSize; ++Index)
	{
		const FEnemy& Enemy = Enemies[Index];
		const float Time = Enemy.Time + (Phase == EPhase::Wrap && Enemy.Move == EEnemyMove::Cut ? PhaseTime : 0.0f);
		Stage->PoseEnemy(Index, Enemy.Position, Enemy.Direction, Enemy.Move, Time);
	}
	if (Phase == EPhase::Wrap)
	{
		Stage->PoseDirector(Position, Facing, 0.0f, 0.0f, Won() ? 0.0f : PhaseTime);
	}
	Stage->PoseBladeTrail(Position, SlashDirection, Phase == EPhase::Combat ? SlashTime / SlashDuration : 0.0f);
}

void ADirectorsCutGameMode::UpdateScreening(float DeltaSeconds)
{
	ShotTime += DeltaSeconds;
	float Duration = FilmShot == INDEX_NONE ? OpeningDuration : Shots[FilmShot].Duration;
	while (ShotTime >= Duration)
	{
		ShotTime -= Duration;
		do
		{
			++FilmShot;
		}
		while (FilmShot < ReelSize && !EarnedShots[FilmShot]);
		if (FilmShot == ReelSize)
		{
			SetPhase(EPhase::GetPaid);
			return;
		}
		bFilmActionPlayed = false;
		Duration = Shots[FilmShot].Duration;
		PlayCue(ECue::Projector);
	}
	if (!bFilmActionPlayed && ShotTime >= (FilmShot == 1 ? 1.22f : 0.85f))
	{
		const ECue ActionCues[] = {ECue::Impact, ECue::Warning, ECue::Impact, ECue::Aircraft, ECue::Aircraft, ECue::Impact, ECue::Impact};
		PlayCue(ActionCues[FilmShot + 1]);
		bFilmActionPlayed = true;
	}
	Stage->ShowFilm(*Camera, FilmShot, ShotTime);
}

void ADirectorsCutGameMode::UpdatePayout()
{
	const int32 BagsInHand = FMath::Clamp(1 + FMath::FloorToInt((PhaseTime - PayoutStart) / BagInterval), 0, ReelCount);
	while (PaidBags < BagsInHand)
	{
		++PaidBags;
		PlayCue(ECue::Cash);
	}
	Stage->ShowPayout(*Camera, ReelCount, PhaseTime);
	if (PhaseTime >= PayoutDuration)
	{
		SetPhase(EPhase::Reception);
	}
}

void ADirectorsCutGameMode::PlayCue(ECue Cue)
{
	if (Sounds.IsValidIndex(static_cast<int32>(Cue)))
	{
		const TArray<int16>& Samples = Sounds[static_cast<int32>(Cue)];
		USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(this);
		Wave->SetSampleRate(SampleRate);
		Wave->NumChannels = 1;
		Wave->Duration = static_cast<float>(Samples.Num()) / SampleRate;
		Wave->QueueAudio(reinterpret_cast<const uint8*>(Samples.GetData()), Samples.Num() * sizeof(int16));
		if (UAudioComponent* Voice = UGameplayStatics::SpawnSound2D(this, Wave, 0.72f))
		{
			Voice->StopDelayed(Wave->Duration + 0.07f);
		}
	}
}

void ADirectorsCutGameMode::BuildSounds()
{
	// Short synthesized cues keep combat and the projector audible without external media.
	struct FTone { float Duration; float StartHz; float EndHz; float Noise; float Volume; };
	const FTone Tones[] = {
		{0.23f, 300,  72, 0.70f, 0.36f}, {0.28f, 570, 970, 0.21f, 0.29f},
		{0.18f, 190,  43, 0.40f, 0.28f}, {0.29f, 152, 205, 0.13f, 0.33f},
		{0.75f,  77,  24, 0.49f, 0.58f}, {0.18f,  57,  72, 0.55f, 0.28f},
		{0.70f, 170,  77, 0.46f, 0.44f}, {0.30f, 680, 920, 0.09f, 0.31f}
	};
	static_assert(UE_ARRAY_COUNT(Tones) == static_cast<int32>(ECue::Count));
	FRandomStream Random(2041);
	for (const FTone& Tone : Tones)
	{
		const int32 SampleCount = FMath::RoundToInt(SampleRate * Tone.Duration);
		TArray<int16>& Samples = Sounds.AddDefaulted_GetRef();
		Samples.SetNumUninitialized(SampleCount);
		float Angle = 0.0f;
		float FilteredNoise = 0.0f;
		for (int32 Index = 0; Index < SampleCount; ++Index)
		{
			const float T = static_cast<float>(Index) / SampleCount;
			Angle += UE_TWO_PI * FMath::Lerp(Tone.StartHz, Tone.EndHz, T) / SampleRate;
			FilteredNoise = 0.60f * FilteredNoise + 0.40f * Random.FRandRange(-1.0f, 1.0f);
			const float Envelope = FMath::Min(T * 45.0f, 1.0f) * FMath::Square(1.0f - T);
			const float Signal = FMath::Lerp(0.74f * FMath::Sin(Angle) + 0.26f * FMath::Sin(Angle * 2.02f),
				FilteredNoise, Tone.Noise);
			Samples[Index] = static_cast<int16>(32767.0f * Tone.Volume * Envelope * Signal);
		}
	}
}
