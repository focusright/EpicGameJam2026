#include "DirectorsCutStage.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"

using namespace DirectorsCut;

namespace
{
	const FColor Asphalt(40, 52, 61);
	const FColor Concrete(110, 132, 141);
	const FColor Steel(43, 73, 86);
	const FColor StreetPaint(165, 163, 129);
	const FColor Lamp(246, 192, 101);
	const FColor Flame(255, 127, 38);
	const FColor DustColor(170, 126, 89);
	const FColor Rust(193, 75, 50);
	const FColor Black(24, 29, 33);
	const FColor Cash(172, 131, 75);
	const FColor CashDark(123, 89, 54);

	constexpr float FirstImpact = 7.25f;
	constexpr float LastImpact = 21.8f;

	float Smooth(float Value)
	{
		Value = FMath::Clamp(Value, 0.0f, 1.0f);
		return Value * Value * (3.0f - 2.0f * Value);
	}

	void PositionCamera(ACameraActor& Camera, FVector Position, FVector Target, float FOV)
	{
		Camera.SetActorLocationAndRotation(Position, (Target - Position).Rotation());
		Camera.GetCameraComponent()->SetFieldOfView(FOV);
	}

	float KaijuPath(float Time)
	{
		return -1280.0f + 3090.0f * Smooth(Time / 24.0f);
	}
}

void ADirectorsCutStage::BuildCity()
{
	Prop(City, EShape::Box, FVector(0, 0, -110), FVector(14500, 13700, 200), Asphalt);
	FRandomStream Random(8415);
	const FColor Walls[] = {FColor(65, 93, 106), FColor(90, 107, 109), FColor(111, 124, 126), FColor(51, 78, 95)};
	for (int32 X = -5; X <= 5; ++X)
	{
		for (int32 Y = -5; Y <= 5; ++Y)
		{
			if (Y == 0 || X == 0)
			{
				continue;
			}
			const FVector Location(X * 910 + Random.FRandRange(-100, 100), Y * 850 + Random.FRandRange(-90, 90), 0);
			const double Width = Random.FRandRange(355, 590);
			const double Depth = Random.FRandRange(330, 610);
			const double Height = Random.FRandRange(305, FMath::Abs(Y) <= 1 ? 660 : 1275);
			const FColor Wall = Walls[Random.RandRange(0, UE_ARRAY_COUNT(Walls) - 1)];
			Prop(City, EShape::Box, Location + FVector(0, 0, 0), FVector(700, 680, 23), Concrete);
			Prop(City, EShape::Box, Location + FVector(0, 0, Height / 2), FVector(Width, Depth, Height), Wall);
			Prop(City, EShape::Box, Location + FVector(0, 0, Height + 11), FVector(Width + 25, Depth + 25, 22), Steel);
			Prop(City, EShape::Box, Location + FVector(-Width * 0.16, 0, Height + 42), FVector(Width * 0.30, Depth * 0.51, 64), Wall);
			for (double Floor = 95.0; Floor < Height - 30.0; Floor += 76.0)
			{
				const FColor Window = Random.FRand() < 0.34f ? Lamp : Steel;
				for (double Side : {-1.0, 1.0})
				{
					Prop(City, EShape::Box, Location + FVector(Side * (Width / 2 + 1), 0, Floor),
						FVector(4, Depth * 0.86, 17), Window);
					Prop(City, EShape::Box, Location + FVector(0, Side * (Depth / 2 + 1), Floor),
						FVector(Width * 0.86, 4, 17), Window);
				}
			}
		}
	}
	for (int32 Index = -27; Index <= 27; ++Index)
	{
		Prop(City, EShape::Box, FVector(Index * 180, 0, 0.5), FVector(72, 9, 1), StreetPaint);
		Prop(City, EShape::Box, FVector(0, Index * 170, 0.5), FVector(9, 68, 1), StreetPaint);
	}
	for (int32 Index = 0; Index < 10; ++Index)
	{
		for (double Side : {-1.0, 1.0})
		{
			Prop(City, EShape::Box, FVector(Side * 355, Index * 43 - 195, 1), FVector(99, 26, 2), StreetPaint);
			Prop(City, EShape::Box, FVector(Index * 43 - 195, Side * 355, 1), FVector(26, 99, 2), StreetPaint);
		}
	}
	// A lattice tower anchors the Tokyo skyline across camera cuts.
	const FVector Tower(-2300, 2150, 0);
	for (double X : {-1.0, 1.0})
	{
		for (double Y : {-1.0, 1.0})
		{
			const FVector Base(X * 310, Y * 310, 0);
			const FVector Tip(X * 49, Y * 49, 2280);
			Prop(City, EShape::Box, Tower + (Base + Tip) / 2, FVector((Tip - Base).Size(), 54, 54),
				Rust, (Tip - Base).Rotation());
		}
	}
	for (int32 Level = 1; Level < 10; ++Level)
	{
		const double Width = 630 - Level * 53;
		const double Height = Level * 228;
		const FColor Color = Level % 3 == 0 ? Concrete : Rust;
		for (double Side : {-1.0, 1.0})
		{
			Prop(City, EShape::Box, Tower + FVector(Side * Width / 2, 0, Height), FVector(28, Width, 32), Color);
			Prop(City, EShape::Box, Tower + FVector(0, Side * Width / 2, Height), FVector(Width, 28, 32), Color);
		}
	}
	Prop(City, EShape::Box, Tower + FVector(0, 0, 1730), FVector(333, 333, 92), Concrete);
	Prop(City, EShape::Box, Tower + FVector(0, 0, 2270), FVector(140, 140, 87), Rust);
	Prop(City, EShape::Cylinder, Tower + FVector(0, 0, 2455), FVector(37, 37, 380), Concrete);

	for (int32 Index = 0; Index < 12; ++Index)
	{
		const double Side = Index % 2 == 0 ? -1.0 : 1.0;
		const float Time = Index < 2 ? 0.9f + Index * 0.12f :
			(Index < 7 ? FirstImpact + 0.17f * (Index - 2) : LastImpact + 0.14f * (Index - 7));
		const FVector Location(KaijuPath(Time) + (Index % 5 - 2) * 132, Side * (233 + Index % 3 * 130), 0);
		const float Height = Random.FRandRange(235, 510);
		USceneComponent* Building = Group(City, Location);
		Shape(Building, EShape::Box, FVector(0, 0, Height / 2), FVector(136, 180, Height), Walls[Index % 4]);
		Shape(Building, EShape::Box, FVector(0, 0, Height + 9), FVector(149, 193, 18), Concrete);
		for (float Floor = 80; Floor < Height - 24; Floor += 65)
		{
			Shape(Building, EShape::Box, FVector(0, Side * -91, Floor), FVector(108, 4, 15), Lamp);
		}
		Buildings.Add({Building, Location, Time, Height});
	}
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Dust); ++Index)
	{
		Dust[Index] = Shape(City, EShape::Ball, FVector::ZeroVector, FVector(100), Index % 5 == 0 ? Flame : DustColor,
			FRotator::ZeroRotator, true);
		Dust[Index]->SetVisibility(false);
	}
	PointLight(City, FVector(-850, 0, 440), FColor(255, 148, 68), 115000, 2000);
	PointLight(City, FVector(1600, 0, 760), FColor(124, 203, 238), 220000, 3600);
}

void ADirectorsCutStage::BuildGodzilla()
{
	KaijuRoot = Group(City);
	if (!GodzillaMesh)
	{
		return;
	}
	const FBoxSphereBounds Bounds = GodzillaMesh->GetBounds();
	GodzillaScale = 2050.0f / FMath::Max(1.0, Bounds.BoxExtent.Z * 2);
	GodzillaFootOffset = (Bounds.BoxExtent.Z - Bounds.Origin.Z) * GodzillaScale;
	const FReferenceSkeleton& Skeleton = GodzillaMesh->GetRefSkeleton();
	GodzillaRestPose = Skeleton.GetRefBonePose();
	GodzillaComponentPose.SetNum(GodzillaRestPose.Num());
	for (int32 Index = 0; Index < GodzillaRestPose.Num(); ++Index)
	{
		const int32 Parent = Skeleton.GetParentIndex(Index);
		GodzillaComponentPose[Index] = Parent == INDEX_NONE ? GodzillaRestPose[Index] :
			GodzillaRestPose[Index] * GodzillaComponentPose[Parent];
		GodzillaBones.Add(Skeleton.GetBoneName(Index), Index);
	}
	const int32* Heel = GodzillaBones.Find(TEXT("Ankle_L"));
	const int32* Toe = GodzillaBones.Find(TEXT("ToesEnd_L"));
	if (Heel && Toe)
	{
		const FVector Foot = GodzillaComponentPose[*Toe].GetLocation() - GodzillaComponentPose[*Heel].GetLocation();
		GodzillaYaw = -Foot.Rotation().Yaw;
	}
	Godzilla = NewObject<UPoseableMeshComponent>(this);
	Godzilla->SetupAttachment(KaijuRoot);
	Godzilla->SetSkinnedAssetAndUpdate(GodzillaMesh);
	Godzilla->SetRelativeScale3D(FVector(GodzillaScale));
	Godzilla->SetRelativeRotation(FRotator(0, GodzillaYaw, 0));
	Godzilla->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Godzilla->RegisterComponent();
}

void ADirectorsCutStage::BuildAircraft(int32 Index, bool bHelicopter)
{
	USceneComponent* Body = Aircraft[Index] = Group(City);
	const FColor Hull = bHelicopter ? FColor(81, 92, 69) : FColor(122, 145, 147);
	Shape(Body, EShape::Ball, FVector(0, 0, 0), bHelicopter ? FVector(242, 91, 85) : FVector(289, 62, 49), Hull);
	Shape(Body, EShape::Ball, FVector(51, 0, 24), FVector(104, 61, 52), Steel);
	Shape(Body, EShape::Box, FVector(-79, 0, 26), FVector(27, 13, 87), Hull, FRotator(-27, 0, 0));
	if (bHelicopter)
	{
		Shape(Body, EShape::Box, FVector(-128, 0, 9), FVector(163, 17, 20), Hull);
		for (double Side : {-1.0, 1.0})
		{
			Shape(Body, EShape::Box, FVector(0, Side * 47, -51), FVector(173, 8, 7), Hull);
		}
		USceneComponent* Rotor = Rotors[Index] = Group(Body, FVector(-17, 0, 59));
		Shape(Rotor, EShape::Box, FVector::ZeroVector, FVector(386, 12, 4), Black);
		Shape(Rotor, EShape::Box, FVector::ZeroVector, FVector(12, 386, 4), Black);
	}
	else
	{
		for (double Side : {-1.0, 1.0})
		{
			Shape(Body, EShape::Box, FVector(-38, Side * 66, -6), FVector(71, 146, 10), Hull, FRotator(0, Side * 29, 0));
			Shape(Body, EShape::Box, FVector(-93, Side * 47, 4), FVector(42, 64, 9), Steel, FRotator(0, Side * 23, 0));
		}
	}
	for (int32 Lane = 0; Lane < 2; ++Lane)
	{
		Tracers[Index * 2 + Lane] = Shape(City, EShape::Box, FVector::ZeroVector, FVector(100, 5, 5),
			Lamp, FRotator::ZeroRotator, true);
		Tracers[Index * 2 + Lane]->SetVisibility(false);
	}
}

void ADirectorsCutStage::PoseGodzilla(float Time)
{
	const float StepPhase = Time * 3.1f;
	const float Stomp = FMath::Max(Smooth((Time - (FirstImpact - 1.25f)) / 0.83f) *
		(1.0f - Smooth((Time - (FirstImpact - 0.3f)) / 0.32f)),
		Smooth((Time - (LastImpact - 1.25f)) / 0.83f) * (1.0f - Smooth((Time - (LastImpact - 0.3f)) / 0.32f)));
	const float OpeningStomp = Smooth(Time / 0.63f) * (1.0f - Smooth((Time - 0.67f) / 0.27f));
	const float Roar = Smooth((Time - 16.65f) / 0.7f) * (1.0f - Smooth((Time - 19.3f) / 0.65f));
	KaijuRoot->SetRelativeLocationAndRotation(FVector(KaijuPath(Time), 33 * FMath::Sin(StepPhase), GodzillaFootOffset + 12 * FMath::Sin(StepPhase * 2)),
		FRotator(0, 9 * FMath::Sin(Time * 0.55f), 2.3 * FMath::Sin(StepPhase)));
	if (!Godzilla)
	{
		return;
	}
	Godzilla->BoneSpaceTransforms = GodzillaRestPose;
	const FVector BodySide = FRotator(0, -GodzillaYaw, 0).RotateVector(FVector::YAxisVector);
	const auto Bend = [&](const TCHAR* BoneName, FVector Axis, float Angle)
	{
		if (const int32* Index = GodzillaBones.Find(BoneName))
		{
			const FVector BoneAxis = GodzillaComponentPose[*Index].InverseTransformVectorNoScale(Axis).GetSafeNormal();
			FTransform& Bone = Godzilla->BoneSpaceTransforms[*Index];
			Bone.SetRotation(Bone.GetRotation() * FQuat(BoneAxis, FMath::DegreesToRadians(Angle)));
		}
	};
	const float Walk = 12.0f * FMath::Sin(StepPhase);
	const float Lift = 29.0f * FMath::Max(Stomp, OpeningStomp);
	Bend(TEXT("Hip_L"), BodySide, Walk - Lift);
	Bend(TEXT("Hip_R"), BodySide, -Walk);
	Bend(TEXT("Knee_L"), BodySide, 6.0f + Lift * 0.65f);
	Bend(TEXT("Knee_R"), BodySide, 6.0f - Walk * 0.27f);
	Bend(TEXT("Spine1_M"), BodySide, 5.0f * FMath::Sin(StepPhase * 0.5f) - Roar * 7.0f);
	Bend(TEXT("Chest_M"), FVector::UpVector, 8.0f * FMath::Sin(Time * 1.9f));
	Bend(TEXT("Head_M"), BodySide, -Roar * 15.0f);
	Bend(TEXT("Jaw_M"), BodySide, 7.0f + 27.0f * Roar);
	Bend(TEXT("Shoulder_L"), BodySide, Walk * 0.7f - Roar * 15.0f);
	Bend(TEXT("Shoulder_R"), BodySide, -Walk * 0.7f - Roar * 15.0f);
	Bend(TEXT("joint1_M"), FVector::UpVector, 14.0f * FMath::Sin(Time * 2.1f) + Roar * 16.0f);
	Bend(TEXT("joint3_M"), FVector::UpVector, 17.0f * FMath::Sin(Time * 2.1f - 0.5f));
	Godzilla->MarkRefreshTransformDirty();
}

void ADirectorsCutStage::PoseAircraft(float Time)
{
	const FVector Target(KaijuPath(Time), 0, 1180);
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Aircraft); ++Index)
	{
		const bool bHelicopter = Index < 2;
		const double Side = Index % 2 == 0 ? -1.0 : 1.0;
		const double FlyPast = bHelicopter ? 17.35 : 18.15;
		const double Defeated = Smooth((Time - FlyPast) / (bHelicopter ? 2.65f : 2.0f));
		const double Angle = Side * (Time * (bHelicopter ? 0.47 : 1.14) + 0.78 + Index * 1.3);
		const double Radius = bHelicopter ? 1380 : 2450;
		const FVector Position = Target + FVector(FMath::Cos(Angle) * Radius + Defeated * 1300,
			FMath::Sin(Angle) * Radius + Side * Defeated * 910, 275 + Index * 178 - Defeated * 2240);
		const bool bVisible = Defeated < 0.97;
		FRotator Rotation = (Target - Position).Rotation();
		Rotation.Roll = Side * (13 + Defeated * 96);
		Aircraft[Index]->SetVisibility(bVisible, true);
		Aircraft[Index]->SetRelativeLocationAndRotation(Position, Rotation);
		if (bHelicopter)
		{
			Rotors[Index]->SetRelativeRotation(FRotator(0, FMath::Fmod(Time * 1370, 360), 0));
		}
		for (int32 Lane = 0; Lane < 2; ++Lane)
		{
			const FVector From = Position + FVector(0, Side * (Lane * 37 + 22), -16);
			const FVector Hit = Target + FVector(25, Lane * 95 - 47, 70 + FMath::Sin(Time * 13 + Index) * 140);
			const float Flight = FMath::Frac(Time * (bHelicopter ? 4.3f : 5.9f) + 0.43f * Lane + 0.21f * Index);
			UStaticMeshComponent* Tracer = Tracers[Index * 2 + Lane];
			Tracer->SetVisibility(bVisible && Defeated < 0.03 && Flight < 0.84f);
			Tracer->SetRelativeLocationAndRotation(FMath::Lerp(From, Hit, Flight), (Hit - From).Rotation());
		}
	}
}

void ADirectorsCutStage::PoseDestruction(float Time)
{
	for (const FBuilding& Building : Buildings)
	{
		const float Collapse = Smooth((Time - Building.ImpactTime) / 0.85f);
		const double Side = FMath::Sign(Building.Position.Y);
		Building.Root->SetRelativeLocationAndRotation(Building.Position + FVector(86 * Collapse, Side * 145 * Collapse,
			-Building.Height * 0.12 * Collapse), FRotator(-31 * Collapse, Side * 23 * Collapse, Side * 78 * Collapse));
	}
	const float Impact = Time >= LastImpact ? LastImpact : (Time >= FirstImpact ? FirstImpact : 0.90f);
	const float Age = Time - Impact;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Dust); ++Index)
	{
		const float Progress = Age - 0.026f * Index;
		const bool bVisible = Progress > 0 && Progress < 1.65f;
		Dust[Index]->SetVisibility(bVisible);
		if (bVisible)
		{
			const double Angle = Index * 2.399963;
			const float Burst = FMath::Sin(PI * Progress / 1.65f);
			Dust[Index]->SetRelativeLocation(FVector(KaijuPath(Impact) + FMath::Cos(Angle) * Progress * (230 + 11 * Index),
				FMath::Sin(Angle) * Progress * (280 + 15 * Index), 43 + Progress * (92 + 8 * Index)));
			Dust[Index]->SetRelativeScale3D(FVector(0.30 + Burst * (0.57 + 0.055 * Index)));
		}
	}
}

void ADirectorsCutStage::ShowFilm(ACameraActor& Camera, int32 Shot, float ShotTime)
{
	ShowSet(City);
	struct FCameraShot
	{
		FVector Offset;
		FVector Travel;
		FVector Look;
		float FOV;
	};
	static const FCameraShot Cuts[] = {
		{FVector(3380, -1830, 2150), FVector(-460, -170, -185), FVector(-80, 15, 1050), 58.0f},
		{FVector(3740, 2510, 1490),  FVector(-410, 350, -180),  FVector(-90, 0, 950),   58.0f},
		{FVector(1870, -1350, 390),  FVector(-200, -450, 130),  FVector(-110, 70, 550), 69.0f},
		{FVector(2490, 2780, 1900),  FVector(-460, 180, -370),  FVector(10, 0, 1300),   61.0f},
		{FVector(3460, -2160, 2370), FVector(-600, 200, -270),  FVector(20, -50, 1230), 57.0f},
		{FVector(2510, -1410, 1310), FVector(-370, -80, 210),   FVector(-120, 0, 1420), 57.0f},
		{FVector(4240, 3170, 2010),  FVector(540, 230, 310),    FVector(-350, 0, 770),  63.0f}
	};
	static_assert(UE_ARRAY_COUNT(Cuts) == ReelSize + 1);
	const FCameraShot& Cut = Cuts[Shot + 1];
	// Each shot evaluates the same timeline, so a skipped shot leaves the next shot in its correct state.
	float StoryTime = Shot == INDEX_NONE ? ShotTime : OpeningDuration + ShotTime;
	for (int32 Index = 0; Index < Shot; ++Index)
	{
		StoryTime += Shots[Index].Duration;
	}
	PoseGodzilla(StoryTime);
	PoseAircraft(StoryTime);
	PoseDestruction(StoryTime);
	const float Length = Shot == INDEX_NONE ? OpeningDuration : Shots[Shot].Duration;
	const FVector Focus(KaijuPath(StoryTime), 0, 0);
	const FVector Motion = Cut.Travel * Smooth(ShotTime / Length);
	const float Shake = FMath::Max(FMath::Max(0.0f, 1.0f - FMath::Abs(StoryTime - FirstImpact) * 2.3f),
		FMath::Max(0.0f, 1.0f - FMath::Abs(StoryTime - LastImpact) * 2.3f));
	const FVector CameraShake(0, 29 * Shake * FMath::Sin(ShotTime * 71), 21 * Shake * FMath::Cos(ShotTime * 79));
	PositionCamera(Camera, City->GetComponentLocation() + Focus + Cut.Offset + Motion + CameraShake,
		City->GetComponentLocation() + Focus + Cut.Look, Cut.FOV);
}

void ADirectorsCutStage::BuildCashSet()
{
	Prop(CashSet, EShape::Box, FVector(0, 0, -19), FVector(1900, 2200, 38), FColor(52, 68, 67));
	Prop(CashSet, EShape::Box, FVector(350, 0, 210), FVector(28, 1860, 420), FColor(37, 64, 63));
	for (int32 Index = 0; Index < 10; ++Index)
	{
		Prop(CashSet, EShape::Box, FVector(330, 190 * Index - 855, 214), FVector(16, 9, 420), Steel);
	}
	Prop(CashSet, EShape::Box, FVector(0, 102, 62), FVector(277, 610, 15), CashDark);
	Prop(CashSet, EShape::Box, FVector(0, 102, 29), FVector(267, 597, 58), Black);
	Prop(CashSet, EShape::Box, FVector(-142, 102, 62), FVector(8, 608, 8), Lamp);
	PointLight(CashSet, FVector(-210, 250, 303), FColor(255, 190, 110), 5300, 690);
	PointLight(CashSet, FVector(80, -260, 260), FColor(110, 222, 224), 4100, 790);
	for (int32 Index = 0; Index < ReelSize; ++Index)
	{
		USceneComponent* Bag = MoneyBags[Index] = Group(CashSet);
		Shape(Bag, EShape::Ball, FVector(0, 0, 32), FVector(84, 75, 65), Cash);
		Shape(Bag, EShape::Cone, FVector(0, 0, 66), FVector(45, 45, 52), CashDark);
		Shape(Bag, EShape::Cylinder, FVector(0, 0, 70), FVector(29, 29, 7), Lamp);
		Shape(Bag, EShape::Ball, FVector(0, 0, 82), FVector(38, 41, 18), Cash);
		Shape(Bag, EShape::Box, FVector(-40, 0, 31), FVector(4, 25, 23), Lamp);
	}
}

void ADirectorsCutStage::ShowPayout(ACameraActor& Camera, int32 EarnedBags, float Time)
{
	if (Director->GetAttachParent() != CashSet)
	{
		Director->AttachToComponent(CashSet, FAttachmentTransformRules::KeepRelativeTransform);
	}
	ShowSet(CashSet);
	PoseDirector(FVector2D(185, 250), FVector2D(-0.98, -0.22), 0, 0, 0);
	for (int32 Index = 0; Index < ReelSize; ++Index)
	{
		const float Award = Index < EarnedBags ? Smooth((Time - PayoutStart - Index * BagInterval) / 0.40f) : 0.0f;
		const float FromX = Index < 3 ? -62.0f : 26.0f;
		const float FromY = -122.0f + (Index % 3) * 91.0f;
		const float ToX = Index < 3 ? -45.0f : 48.0f;
		const float ToY = 160.0f + (Index % 3) * 87.0f;
		MoneyBags[Index]->SetRelativeLocationAndRotation(FVector(FMath::Lerp(FromX, ToX, Award),
			FMath::Lerp(FromY, ToY, Award), 70 + 88 * FMath::Sin(PI * Award)),
			FRotator(0, Index * 37 + Award * 185, 29 * FMath::Sin(PI * Award)));
	}
	const FVector Origin = CashSet->GetComponentLocation();
	const float Push = Smooth(Time / PayoutDuration);
	PositionCamera(Camera, Origin + FVector(-630 + 85 * Push, -104 - 56 * Push, 318 - 35 * Push),
		Origin + FVector(0, -61, 93), 54.0f);
}
