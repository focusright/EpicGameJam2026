#include "DirectorsCutStage.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Animation/AnimSequence.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"

using namespace DirectorsCut;

const FVector ADirectorsCutStage::ArenaOrigin(-33000, 0, 0);

namespace
{
	const FVector CityOrigin(28000, 0, 0);
	const FVector CashOrigin(-56000, 0, 0);
	const FColor Ink(25, 33, 40);
	const FColor Teal(56, 124, 127);
	const FColor Gold(237, 172, 77);
	const FColor Steel(165, 191, 193);
	const FColor Sprocket(168, 156, 128);
	const FColor Red(255, 65, 37);
	const FColor Floor(66, 73, 77);

	float Ease(float T)
	{
		T = FMath::Clamp(T, 0.0f, 1.0f);
		return T * T * (3.0f - 2.0f * T);
	}

	void FrameCamera(ACameraActor& Camera, FVector Location, FVector Target, float Fov)
	{
		Camera.SetActorLocationAndRotation(Location, (Target - Location).Rotation());
		UCameraComponent* Lens = Camera.GetCameraComponent();
		Lens->SetFieldOfView(Fov);
		Lens->bConstrainAspectRatio = false;
		Lens->bOverrideAspectRatioAxisConstraint = true;
		Lens->SetAspectRatioAxisConstraint(AspectRatio_MaintainYFOV);
	}

	// Yaws the camera so Target lands at ScreenX (-1 left edge, 1 right edge) for the current viewport shape.
	void FrameCameraOffCenter(ACameraActor& Camera, FVector Location, FVector Target, float Fov, float ScreenX)
	{
		FrameCamera(Camera, Location, Target, Fov);
		FVector2D Viewport(16.0, 9.0);
		if (UGameViewportClient* Client = Camera.GetWorld()->GetGameViewport())
		{
			Client->GetViewportSize(Viewport);
		}
		const UCameraComponent* Lens = Camera.GetCameraComponent();
		const double HalfHeight = FMath::Tan(FMath::DegreesToRadians(Fov * 0.5)) / Lens->AspectRatio;
		const double HalfWidth = HalfHeight * Viewport.X / FMath::Max(Viewport.Y, 1.0);
		FRotator Rotation = Camera.GetActorRotation();
		Rotation.Yaw -= FMath::RadiansToDegrees(FMath::Atan(ScreenX * HalfWidth));
		Camera.SetActorRotation(Rotation);
	}
}

ADirectorsCutStage::ADirectorsCutStage()
{
	StageRoot = CreateDefaultSubobject<USceneComponent>(TEXT("StageRoot"));
	SetRootComponent(StageRoot);
}

USceneComponent* ADirectorsCutStage::Group(USceneComponent* Parent, FVector Location)
{
	USceneComponent* Part = NewObject<USceneComponent>(this);
	Part->SetupAttachment(Parent);
	Part->SetRelativeLocation(Location);
	Part->RegisterComponent();
	return Part;
}

UMaterialInstanceDynamic* ADirectorsCutStage::Paint(FColor Color, bool bGlow)
{
	const uint32 Key = Color.ToPackedARGB() ^ (bGlow ? 0x01000000 : 0);
	if (const TObjectPtr<UMaterialInstanceDynamic>* Existing = Paints.Find(Key))
	{
		return *Existing;
	}
	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(bGlow ? LightMaterial : PigmentMaterial, this);
	if (bGlow)
	{
		const TConstArrayView64<uint8> Pixel(reinterpret_cast<const uint8*>(&Color), sizeof(FColor));
		UTexture2D* Texture = UTexture2D::CreateTransient(1, 1, PF_B8G8R8A8, NAME_None, Pixel);
		Texture->UpdateResource();
		Material->SetTextureParameterValue(TEXT("Texture"), Texture);
	}
	else
	{
		Material->SetVectorParameterValue(TEXT("Color"), FLinearColor::FromSRGBColor(Color));
		Material->SetScalarParameterValue(TEXT("Roughness"), 0.74f);
	}
	Paints.Add(Key, Material);
	return Material;
}

UStaticMeshComponent* ADirectorsCutStage::Shape(USceneComponent* Parent, EShape Kind, FVector Location,
	FVector Size, FColor Color, FRotator Rotation, bool bGlow)
{
	UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(this);
	Mesh->SetupAttachment(Parent);
	Mesh->SetStaticMesh(Shapes[static_cast<int32>(Kind)]);
	Mesh->SetMaterial(0, Paint(Color, bGlow));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeTransform(FTransform(Rotation, Location, Size / 100.0));
	Mesh->SetCastShadow(!bGlow);
	Mesh->RegisterComponent();
	return Mesh;
}

void ADirectorsCutStage::Prop(USceneComponent* Parent, EShape Kind, FVector Location, FVector Size,
	FColor Color, FRotator Rotation)
{
	UStaticMesh* Mesh = Shapes[static_cast<int32>(Kind)];
	FPropBatch* Batch = PropBatches.FindByPredicate([&](const FPropBatch& Candidate)
	{
		return Candidate.Parent == Parent && Candidate.Mesh == Mesh && Candidate.Color == Color;
	});
	if (!Batch)
	{
		UInstancedStaticMeshComponent* Instances = NewObject<UInstancedStaticMeshComponent>(this);
		Instances->SetupAttachment(Parent);
		Instances->SetStaticMesh(Mesh);
		Instances->SetMaterial(0, Paint(Color, false));
		Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Instances->RegisterComponent();
		Batch = &PropBatches.Add_GetRef({Parent, Mesh, Color, Instances});
	}
	Batch->Instances->AddInstance(FTransform(Rotation, Location, Size / 100.0));
}

void ADirectorsCutStage::Dress(USceneComponent* Parent, UStaticMesh* Mesh, FVector Location, float Scale,
	FRotator Rotation)
{
	if (!Mesh)
	{
		return;
	}
	UStaticMeshComponent* PropMesh = NewObject<UStaticMeshComponent>(this);
	PropMesh->SetupAttachment(Parent);
	PropMesh->SetStaticMesh(Mesh);
	PropMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PropMesh->SetRelativeTransform(FTransform(Rotation, Location, FVector(Scale)));
	PropMesh->SetCastShadow(true);
	PropMesh->RegisterComponent();
}

void ADirectorsCutStage::PointLight(USceneComponent* Parent, FVector Location, FColor Color, float Intensity, float Radius)
{
	UPointLightComponent* Lamp = NewObject<UPointLightComponent>(this);
	Lamp->SetupAttachment(Parent);
	Lamp->SetRelativeLocation(Location);
	Lamp->SetLightColor(Color);
	Lamp->SetIntensity(Intensity);
	Lamp->SetAttenuationRadius(Radius);
	Lamp->SetCastShadows(false);
	Lamp->RegisterComponent();
}

bool ADirectorsCutStage::Build()
{
	// Loading materials in the native class constructor can run before cooked startup initializes default materials.
	Shapes = {
		LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")),
		LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")),
		LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")),
		LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"))
	};
	GodzillaMesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Godzilla/Meshes/Godzilla.Godzilla"));
	PigmentMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	LightMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/EmissiveTexturedMaterial.EmissiveTexturedMaterial"));
	if (!GodzillaMesh || !PigmentMaterial || !LightMaterial ||
		Shapes.ContainsByPredicate([](const TObjectPtr<UStaticMesh>& Mesh) { return !Mesh; }))
	{
		UE_LOG(LogTemp, Error, TEXT("Director's Cut could not load the required stage assets."));
		return false;
	}
	for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
	{
		const UStaticMesh* Mesh = It->GetStaticMeshComponent()->GetStaticMesh();
		if (Mesh && Mesh->GetPathName() == TEXT("/Engine/MapTemplates/SM_Template_Map_Floor.SM_Template_Map_Floor"))
		{
			It->SetActorHiddenInGame(true);
		}
	}
	Arena = Group(StageRoot, ArenaOrigin);
	City = Group(StageRoot, CityOrigin);
	CashSet = Group(StageRoot, CashOrigin);
	BuildArena();
	BuildDirector();
	BuildTitleDirector();
	for (int32 Index = 0; Index < ReelSize; ++Index)
	{
		BuildEnemy(Index);
	}
	BuildCity();
	BuildGodzilla();
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Aircraft); ++Index)
	{
		BuildAircraft(Index, Index < 2);
	}
	BuildCashSet();
	ShowSet(Arena);
	return true;
}

void ADirectorsCutStage::ShowSet(USceneComponent* ActiveSet)
{
	if (VisibleSet == ActiveSet)
	{
		return;
	}
	Arena->SetHiddenInGame(ActiveSet != Arena, true);
	City->SetHiddenInGame(ActiveSet != City, true);
	CashSet->SetHiddenInGame(ActiveSet != CashSet, true);
	VisibleSet = ActiveSet;
}

void ADirectorsCutStage::ResetArena()
{
	if (TitleDirector)
	{
		TitleDirector->SetVisibility(false, true);
	}
	Director->SetVisibility(true, true);
	Director->AttachToComponent(Arena, FAttachmentTransformRules::KeepRelativeTransform);
	ShowSet(Arena);
	PoseDirector(FVector2D::ZeroVector, FVector2D::UnitX(), 0.0f, 0.0f, 0.0f);
}

void ADirectorsCutStage::BuildArena()
{
	constexpr double DeckX = 1250.0;
	constexpr double DeckY = 1970.0;
	const FColor LabFloor(72, 74, 76);
	const FColor LabTileA(84, 86, 88);
	const FColor LabTileB(64, 66, 68);
	const FColor BenchMetal(32, 36, 40);
	const FColor BenchTop(24, 28, 32);
	const FColor Aisle(58, 60, 62);

	Prop(Arena, EShape::Box, FVector(0, 0, -29), FVector(7000, 7400, 48), Ink);
	Prop(Arena, EShape::Box, FVector(0, 0, -9), FVector(DeckX, DeckY, 18), Floor);

	// Industrial editing-room floor: dark linoleum with a lighter center aisle.
	Prop(Arena, EShape::Box, FVector(0, 0, 0.4), FVector(1160, 1840, 1.0), LabFloor);
	for (int32 X = -5; X <= 5; ++X)
	{
		for (int32 Y = -9; Y <= 9; ++Y)
		{
			const FColor Tile = ((X + Y) & 1) ? LabTileA : LabTileB;
			Prop(Arena, EShape::Box, FVector(X * 100.0, Y * 100.0, 0.7), FVector(96, 96, 0.8), Tile);
		}
	}
	Prop(Arena, EShape::Box, FVector(0, 0, 1.0), FVector(420, 1400, 0.9), Aisle);
	Prop(Arena, EShape::Box, FVector(0, 0, 1.0), FVector(900, 360, 0.9), Aisle);

	// Editing benches along both long walls (inside the teal).
	auto EditingBench = [&](FVector Origin, float Yaw)
	{
		USceneComponent* Bench = Group(Arena, Origin);
		Bench->SetRelativeRotation(FRotator(0, Yaw, 0));
		Prop(Bench, EShape::Box, FVector(0, 0, 38), FVector(210, 78, 76), BenchMetal);
		Prop(Bench, EShape::Box, FVector(0, 0, 78), FVector(220, 88, 8), BenchTop);
		Prop(Bench, EShape::Box, FVector(-70, 0, 18), FVector(18, 18, 36), Ink);
		Prop(Bench, EShape::Box, FVector(70, 0, 18), FVector(18, 18, 36), Ink);
		// Built-in lightbox.
		Shape(Bench, EShape::Box, FVector(-20, 0, 84), FVector(70, 48, 4), FColor(255, 246, 220), FRotator::ZeroRotator, true);
		PointLight(Bench, FVector(-20, 0, 110), FColor(255, 244, 210), 250, 180);
		// Rewind spindles and roller bank.
		Prop(Bench, EShape::Cylinder, FVector(55, -18, 108), FVector(10, 10, 52), Steel);
		Prop(Bench, EShape::Cylinder, FVector(55, 18, 108), FVector(10, 10, 52), Steel);
		Prop(Bench, EShape::Cylinder, FVector(55, -18, 136), FVector(34, 34, 8), Steel);
		Prop(Bench, EShape::Cylinder, FVector(55, 18, 136), FVector(34, 34, 8), Steel);
		for (int32 Roller = 0; Roller < 4; ++Roller)
		{
			Prop(Bench, EShape::Cylinder, FVector(-55.0 + Roller * 18.0, 28, 96), FVector(8, 8, 22), Steel,
				FRotator(0, 0, 90));
		}
		Prop(Bench, EShape::Box, FVector(20, 28, 92), FVector(90, 10, 6), Sprocket);
	};

	for (double Side : {-1.0, 1.0})
	{
		for (int32 Index = 0; Index < 5; ++Index)
		{
			const double X = -360.0 + Index * 180.0;
			EditingBench(FVector(X, Side * 780, 0), Side > 0 ? 180.0f : 0.0f);
		}
		// Short-end benches near camera and backdrop.
		EditingBench(FVector(Side * 430, -520, 0), 90.0f);
		EditingBench(FVector(Side * 430, 520, 0), -90.0f);
	}

	// Rolling scrap bins in the aisles between benches.
	for (double Side : {-1.0, 1.0})
	{
		for (int32 Index = 0; Index < 3; ++Index)
		{
			const double X = -250.0 + Index * 250.0;
			Prop(Arena, EShape::Cylinder, FVector(X, Side * 620, 36), FVector(70, 70, 72), Ink);
			Prop(Arena, EShape::Cylinder, FVector(X, Side * 620, 74), FVector(74, 74, 6), Steel);
			for (double Wheel : {-22.0, 22.0})
			{
				Prop(Arena, EShape::Cylinder, FVector(X + Wheel, Side * 620 - 28, 10), FVector(14, 14, 8), Steel,
					FRotator(0, 0, 90));
				Prop(Arena, EShape::Cylinder, FVector(X + Wheel, Side * 620 + 28, 10), FVector(14, 14, 8), Steel,
					FRotator(0, 0, 90));
			}
		}
	}

	for (double Side : {-1.0, 1.0})
	{
		Prop(Arena, EShape::Box, FVector(0, Side * 936, 2), FVector(1185, 8, 4), Teal);
		Prop(Arena, EShape::Box, FVector(Side * 590, 0, 2), FVector(8, 1870, 4), Teal);
		for (int32 Index = 0; Index < 18; ++Index)
		{
			Prop(Arena, EShape::Box, FVector(Side * 608, Index * 102 - 867, 3), FVector(21, 50, 6), Sprocket);
		}
		for (double Depth : {-365.0, 365.0})
		{
			USceneComponent* Fixture = Group(Arena, FVector(Depth, Side * 1020, 0));
			Prop(Fixture, EShape::Box, FVector(0, 0, 14), FVector(69, 55, 28), Ink);
			Prop(Fixture, EShape::Cylinder, FVector(0, 0, 110), FVector(15, 15, 200), Steel);
			Prop(Fixture, EShape::Box, FVector(0, 0, 205), FVector(49, 65, 46), Ink, FRotator(0, 0, Side * 24));
			Prop(Fixture, EShape::Box, FVector(0, -Side * 25, 199), FVector(36, 13, 30), Gold);
			PointLight(Fixture, FVector(0, -Side * 90, 210), FColor(255, 174, 101), 1100, 640);
		}
		for (int32 Index = 0; Index < 4; ++Index)
		{
			const double Y = Side * (1005.0 + 80.0 * (Index % 2));
			const double X = 475.0 + 74.0 * (Index / 2);
			Prop(Arena, EShape::Box, FVector(X, Y, 30), FVector(69, 73, 60), Ink);
			Prop(Arena, EShape::Box, FVector(X, Y, 53), FVector(72, 76, 8), Steel);
		}
	}
	for (int32 Index = 0; Index < 12; ++Index)
	{
		const double Y = Index * 190 - 1045;
		Prop(Arena, EShape::Box, FVector(735, Y, 205), FVector(32, 183, 470), Ink);
		Prop(Arena, EShape::Box, FVector(715, Y + 82, 205), FVector(16, 6, 470), Teal);
	}
	BuildArenaDressing();
}

void ADirectorsCutStage::BuildArenaDressing()
{
	UStaticMesh* CanBottom = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Game/Props/FilmReelCanned/film_reel/StaticMeshes/SM_FilmReelCanBottom.SM_FilmReelCanBottom"));
	UStaticMesh* CanTop = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Game/Props/FilmReelCanned/film_reel/StaticMeshes/SM_FilmReelCanTop.SM_FilmReelCanTop"));
	UStaticMesh* Reel = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Game/Props/FilmReelCanned/film_reel/StaticMeshes/SM_FilmReelRoll.SM_FilmReelRoll"));
	UStaticMesh* Tape = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Game/Props/FilmReelCanned/film_reel/StaticMeshes/SM_FilmReelTape.SM_FilmReelTape"));
	if (!CanBottom || !CanTop || !Reel || !Tape)
	{
		UE_LOG(LogTemp, Warning, TEXT("Director's Cut could not load film reel props."));
		return;
	}

	auto CanStack = [&](FVector Base, float Scale, int32 Count, float Yaw)
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const float Z = 16.0f * Scale + Index * 34.0f * Scale;
			Dress(Arena, CanBottom, Base + FVector(0, 0, Z), Scale, FRotator(0, Yaw + Index * 17.0f, 0));
			if (Index == Count - 1)
			{
				Dress(Arena, CanTop, Base + FVector(8.0f * Scale, -6.0f * Scale, Z + 22.0f * Scale),
					Scale, FRotator(8.0f, Yaw + 40.0f, 12.0f));
			}
		}
	};
	auto StandingReel = [&](FVector Location, float Scale, float Yaw)
	{
		Dress(Arena, Reel, Location, Scale, FRotator(90.0f, Yaw, 0));
	};
	auto FlatReel = [&](FVector Location, float Scale, float Yaw)
	{
		Dress(Arena, Reel, Location, Scale, FRotator(0, Yaw, 0));
		Dress(Arena, Tape, Location + FVector(0, 0, 12.0f * Scale), Scale * 0.92f, FRotator(0, Yaw + 25.0f, 0));
	};
	auto ArchiveCluster = [&](FVector Origin, float Scale, int32 Columns, int32 MaxHeight)
	{
		for (int32 Column = 0; Column < Columns; ++Column)
		{
			const float Angle = Column * 47.0f;
			const float Radius = 28.0f * Scale * (0.35f + (Column % 3) * 0.35f);
			const FVector Offset(FMath::Cos(FMath::DegreesToRadians(Angle)) * Radius,
				FMath::Sin(FMath::DegreesToRadians(Angle)) * Radius, 0);
			CanStack(Origin + Offset, Scale * (0.92f + (Column % 2) * 0.08f),
				2 + (Column * 3 + MaxHeight) % MaxHeight, Angle);
		}
	};

	// Dense can stacks on every editing bench (matches the archive photos).
	for (double Side : {-1.0, 1.0})
	{
		for (int32 Index = 0; Index < 5; ++Index)
		{
			const double X = -360.0 + Index * 180.0;
			const float BenchZ = 86.0f;
			CanStack(FVector(X + 55, Side * 780, BenchZ), 0.28f, 4 + (Index % 3), Side * 15.0f + Index * 8.0f);
			CanStack(FVector(X + 20, Side * 780 + Side * 18, BenchZ), 0.24f, 3, -Index * 12.0f);
			FlatReel(FVector(X - 45, Side * 780, BenchZ + 8), 0.22f, Index * 20.0f);
			if (Index % 2 == 0)
			{
				StandingReel(FVector(X - 70, Side * 800, BenchZ + 42), 0.26f, Side > 0 ? 10.0f : -15.0f);
			}
		}
		CanStack(FVector(Side * 430, -520, 86), 0.26f, 5, 20.0f);
		CanStack(FVector(Side * 430, 520, 86), 0.26f, 4, -30.0f);
		FlatReel(FVector(Side * 400, -500, 94), 0.20f, 40.0f);
		FlatReel(FVector(Side * 400, 500, 94), 0.20f, -25.0f);
	}

	// Archive islands inside the play field, leaving a clear center aisle for combat.
	ArchiveCluster(FVector(-360, -520, 0), 0.34f, 5, 6);
	ArchiveCluster(FVector(360, -480, 0), 0.32f, 4, 5);
	ArchiveCluster(FVector(-340, 500, 0), 0.33f, 5, 6);
	ArchiveCluster(FVector(350, 460, 0), 0.31f, 4, 5);
	ArchiveCluster(FVector(-420, 40, 0), 0.30f, 4, 5);
	ArchiveCluster(FVector(420, -60, 0), 0.30f, 4, 5);
	CanStack(FVector(-200, -700, 0), 0.36f, 7, 8.0f);
	CanStack(FVector(180, -720, 0), 0.34f, 6, -12.0f);
	CanStack(FVector(-160, 700, 0), 0.35f, 7, 22.0f);
	CanStack(FVector(200, 680, 0), 0.33f, 6, -18.0f);
	CanStack(FVector(0, -780, 0), 0.38f, 5, 0.0f);
	CanStack(FVector(0, 780, 0), 0.38f, 5, 180.0f);
	StandingReel(FVector(-280, -600, 70), 0.42f, 25.0f);
	StandingReel(FVector(300, -580, 65), 0.40f, -30.0f);
	StandingReel(FVector(-300, 580, 72), 0.44f, 40.0f);
	StandingReel(FVector(280, 560, 68), 0.41f, -55.0f);

	// Loose lids and tape rolls on the aisle edges.
	Dress(Arena, CanTop, FVector(-120, -400, 10), 0.30f, FRotator(12, 20, 8));
	Dress(Arena, CanTop, FVector(140, 380, 10), 0.28f, FRotator(-8, -40, 6));
	Dress(Arena, Tape, FVector(-80, 240, 9), 0.26f, FRotator(0, 70, 0));
	Dress(Arena, Tape, FVector(90, -220, 9), 0.24f, FRotator(0, -55, 0));
	FlatReel(FVector(-40, 0, 10), 0.28f, 15.0f);
	FlatReel(FVector(60, 120, 10), 0.22f, -40.0f);

	// Outside-border storage walls: taller archival stacks like the photos.
	for (double Side : {-1.0, 1.0})
	{
		for (int32 Index = 0; Index < 6; ++Index)
		{
			const double X = -450.0 + Index * 170.0;
			ArchiveCluster(FVector(X, Side * 1100, 0), 0.40f, 4, 7);
		}
		StandingReel(FVector(-300, Side * 1060, 95 * 0.58f), 0.58f, Side > 0 ? 12.0f : -18.0f);
		StandingReel(FVector(340, Side * 1055, 95 * 0.62f), 0.62f, Side > 0 ? 70.0f : -65.0f);
		Prop(Arena, EShape::Box, FVector(-250, Side * 1180, 28), FVector(110, 64, 54), Ink, FRotator(0, Side * 12, 0));
		Prop(Arena, EShape::Box, FVector(-250, Side * 1180, 58), FVector(104, 58, 8), Steel);
		Prop(Arena, EShape::Cylinder, FVector(160, Side * 1205, 42), FVector(70, 70, 84), Sprocket);
		Prop(Arena, EShape::Cylinder, FVector(160, Side * 1205, 88), FVector(28, 28, 18), Steel);
	}
	for (int32 Index = 0; Index < 5; ++Index)
	{
		const double Y = Index * 280 - 560;
		ArchiveCluster(FVector(660, Y, 0), 0.42f, 4, 8);
		ArchiveCluster(FVector(-700, Y, 0), 0.36f, 3, 5);
	}
}

void ADirectorsCutStage::BuildDirector()
{
	Director = Group(Arena);
	Lower = Group(Director, FVector(0, 0, 80));
	Upper = Group(Director, FVector(0, 0, 80));
	const FColor Skin(216, 160, 120);
	const FColor Shirt(119, 132, 92);
	const FColor Pants(181, 147, 102);
	const FColor Boots(83, 56, 40);
	const FColor Hat(34, 34, 34);
	const FColor Hair(115, 109, 102);

	Shape(Lower, EShape::Box, FVector(0, 0, 4), FVector(40, 58, 23), Pants);
	Shape(Upper, EShape::Box, FVector(0, 0, 13), FVector(43, 60, 12), Boots);
	Shape(Upper, EShape::Box, FVector(22, 0, 13), FVector(4, 12, 8), Gold);
	Shape(Upper, EShape::Box, FVector(0, 0, 44), FVector(42, 61, 54), Shirt);
	Shape(Upper, EShape::Box, FVector(22, 0, 48), FVector(4, 6, 47), FColor(103, 107, 71));
	for (double Side : {-1.0, 1.0})
	{
		Shape(Upper, EShape::Box, FVector(22, Side * 17, 48), FVector(4, 18, 19), FColor(139, 143, 98));
		Shape(Upper, EShape::Box, FVector(24, Side * 17, 56), FVector(5, 18, 4), FColor(101, 108, 75));
	}
	Shape(Upper, EShape::Cylinder, FVector(0, 0, 74), FVector(24, 25, 15), Skin);
	Shape(Upper, EShape::Ball, FVector(1, 0, 91), FVector(40, 40, 46), Skin);
	Shape(Upper, EShape::Ball, FVector(-10, 0, 98), FVector(29, 40, 31), Hair);
	Shape(Upper, EShape::Ball, FVector(21, 0, 91), FVector(9, 11, 12), Skin);
	Shape(Upper, EShape::Box, FVector(21, 0, 84), FVector(5, 19, 4), Boots);
	Shape(Upper, EShape::Ball, FVector(-2, 1, 109), FVector(52, 56, 15), Hat, FRotator(2, 0, 10));
	Shape(Upper, EShape::Ball, FVector(-5, -4, 115), FVector(47, 49, 19), Hat, FRotator(0, 0, 13));
	Shape(Upper, EShape::Cylinder, FVector(24, 0, 35), FVector(19, 19, 12), Gold, FRotator(90, 0, 0));
	Shape(Upper, EShape::Cylinder, FVector(30, 0, 35), FVector(12, 12, 4), Ink, FRotator(90, 0, 0));

	for (int32 SideIndex = 0; SideIndex < 2; ++SideIndex)
	{
		const double Side = SideIndex == 0 ? -1.0 : 1.0;
		Legs[SideIndex] = Group(Lower, FVector(0, Side * 15, -4));
		Shape(Legs[SideIndex], EShape::Box, FVector(0, 0, -20), FVector(29, 26, 41), Pants);
		Shape(Legs[SideIndex], EShape::Box, FVector(0, 0, -53), FVector(23, 21, 32), Boots);
		Shape(Legs[SideIndex], EShape::Box, FVector(8, 0, -69), FVector(39, 24, 14), Boots);
		USceneComponent* Arm = Group(Upper, FVector(1, Side * 35, 58));
		Shape(Arm, EShape::Box, FVector(19, 0, 0), FVector(39, 22, 23), Shirt);
		Shape(Arm, EShape::Box, FVector(33, 0, 0), FVector(9, 24, 25), FColor(139, 143, 98));
		Shape(Arm, EShape::Box, FVector(45, 0, 0), FVector(28, 17, 18), Skin);
		Shape(Arm, EShape::Ball, FVector(61, 0, 0), FVector(17, 18, 17), Skin);
		if (SideIndex == 0)
		{
			LeftArm = Arm;
		}
		else
		{
			RightArm = Arm;
		}
	}

	// A straight, riveted razor with a cylindrical grip and an extended cutting edge.
	Shape(RightArm, EShape::Cylinder, FVector(68, 0, 0), FVector(15, 15, 50), Hat, FRotator(90, 0, 0));
	Shape(RightArm, EShape::Box, FVector(94, 0, 0), FVector(9, 20, 53), Steel);
	Shape(RightArm, EShape::Box, FVector(159, 0, 2), FVector(123, 8, 42), FColor(122, 140, 143));
	Shape(RightArm, EShape::Box, FVector(159, 0, 22), FVector(123, 12, 5), Ink);
	Shape(RightArm, EShape::Box, FVector(159, 0, -20), FVector(123, 10, 6), FColor(236, 249, 233), FRotator::ZeroRotator, true);
	for (int32 Index = 0; Index < 6; ++Index)
	{
		Shape(RightArm, EShape::Ball, FVector(106 + 19 * Index, 4.5, 13), FVector(6, 4, 6), Steel);
	}
	BladeTrail = Group(Arena);
	for (int32 Index = 0; Index < 9; ++Index)
	{
		const float Angle = -1.10f + 0.265f * Index;
		const float Radius = 207.0f + 3.0f * Index;
		Shape(BladeTrail, EShape::Box, FVector(Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle), 65 + Index * 4),
			FVector(4 + Index * 0.5f, 54, 4), Index % 2 == 0 ? Gold : FColor(255, 234, 167),
			FRotator(0, FMath::RadiansToDegrees(Angle), 0), true);
	}
	BladeTrail->SetVisibility(false, true);
}

void ADirectorsCutStage::BuildTitleDirector()
{
	USkeletalMesh* Steven = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Player/Mesh/1/Steven1.Steven1"));
	UAnimSequence* TitlePose = LoadObject<UAnimSequence>(nullptr,
		TEXT("/Game/Player/Animation/mixamo/AS_Steven1_Slash_Pose.AS_Steven1_Slash_Pose"));
	TitleStartAnimation = LoadObject<UAnimSequence>(nullptr,
		TEXT("/Game/Player/Animation/Stable_Sword_Outward_Slash_Anim.Stable_Sword_Outward_Slash_Anim"));
	UStaticMesh* Machete = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Game/TripoModels/long_blade_machete_3d_model/long_blade_machete_3d_model.long_blade_machete_3d_model"));
	if (!Steven || !TitlePose || !Machete)
	{
		UE_LOG(LogTemp, Warning, TEXT("The Godzilla Steven title meshes could not be loaded."));
		PoseDirector(FVector2D::ZeroVector, FVector2D(-0.9, -0.43), 0.0f, 0.0f, 0.0f);
		return;
	}

	TitleDirector = NewObject<USkeletalMeshComponent>(this, TEXT("TitleSteven"));
	TitleDirector->SetupAttachment(Arena);
	TitleDirector->SetSkeletalMesh(Steven);
	TitleDirector->SetRelativeTransform(FTransform(FRotator(0, 110, 0), FVector(-90, -73, -87), FVector(2.0)));
	TitleDirector->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TitleDirector->RegisterComponent();
	TitleDirector->PlayAnimation(TitlePose, false);
	TitleDirector->SetPosition(0.0f, false);

	UStaticMeshComponent* Blade = NewObject<UStaticMeshComponent>(this, TEXT("TitleMachete"));
	Blade->SetupAttachment(TitleDirector, TEXT("hand_rSocket"));
	Blade->SetStaticMesh(Machete);
	Blade->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Blade->RegisterComponent();
	Director->SetVisibility(false, true);
}

float ADirectorsCutStage::PlayTitleStartAnimation()
{
	if (!TitleDirector || !TitleStartAnimation)
	{
		UE_LOG(LogTemp, Warning, TEXT("The Godzilla Steven title slash animation could not be played."));
		return 0.0f;
	}
	TitleDirector->PlayAnimation(TitleStartAnimation, false);
	TitleDirector->SetPosition(0.0f, false);
	return TitleStartAnimation->GetPlayLength();
}

void ADirectorsCutStage::BuildEnemy(int32 Index)
{
	FFilmEnemy& Enemy = FilmEnemies[Index];
	Enemy.Root = Group(Arena);
	Enemy.Top = Group(Enemy.Root, FVector(0, 0, 117));
	Enemy.Bottom = Group(Enemy.Root, FVector(0, 0, 117));
	const FColor Frame(34, 30, 32);
	const FColor Stock = Shots[Index].Color;
	for (int32 Half = 0; Half < 2; ++Half)
	{
		USceneComponent* Part = Half == 0 ? Enemy.Bottom : Enemy.Top;
		const double Height = Half == 0 ? -43.0 : 43.0;
		Shape(Part, EShape::Box, FVector(0, 0, Height), FVector(19, 83, 86), Frame);
		for (double Side : {-1.0, 1.0})
		{
			Shape(Part, EShape::Box, FVector(0, Side * 40, Height), FVector(23, 5, 86), Sprocket);
			for (int32 Hole = 0; Hole < 4; ++Hole)
			{
				for (double Face : {-1.0, 1.0})
				{
					Shape(Part, EShape::Box, FVector(Face * 10, Side * 32.5, Height - 30 + 20 * Hole),
						FVector(3, 8, 11), Sprocket);
				}
			}
		}
		for (double Face : {-1.0, 1.0})
		{
			for (double Row : {-21.0, 21.0})
			{
				Shape(Part, EShape::Box, FVector(Face * 10, 0, Height + Row), FVector(4, 51, 38), Stock);
				Shape(Part, EShape::Box, FVector(Face * 12.1, 0, Height + Row - 10), FVector(1, 46, 13), Teal);
				Shape(Part, EShape::Box, FVector(Face * 13, -11, Height + Row + 8), FVector(2, 10, 6), Ink);
				Shape(Part, EShape::Box, FVector(Face * 13, 11, Height + Row + 8), FVector(2, 10, 6), Ink);
			}
		}
	}
	Enemy.Tell = Shape(Enemy.Root, EShape::Box, FVector(105, 0, 3), FVector(270, 15, 3), Red, FRotator::ZeroRotator, true);
	Enemy.Root->SetVisibility(false, true);
}

void ADirectorsCutStage::SetArenaCamera(ACameraActor& Camera, float Time, bool bTitle) const
{
	if (bTitle)
	{
		const FVector Location = ArenaOrigin + FVector(-380, -410 + 30 * FMath::Sin(Time * 0.35f), 145);
		if (TitleDirector)
		{
			// The title panel covers the left 45% of the screen; center Steven in the remaining box.
			FrameCameraOffCenter(Camera, Location, ArenaOrigin + FVector(-90, -73, 84), 61.0f, 0.45f);
			return;
		}
		FrameCamera(Camera, Location, ArenaOrigin + FVector(6, -228, 94), 88.0f);
		return;
	}
	FrameCamera(Camera, ArenaOrigin + FVector(-810, 0, 1390), ArenaOrigin + FVector(85, 0, 0), 66.0f);
}

void ADirectorsCutStage::PoseDirector(FVector2D Position, FVector2D Facing, float Stride, float Slash, float Split)
{
	const float Step = FMath::Sin(Stride);
	const float Swing = Slash > 0.0f ? FMath::Lerp(-90.0f, 112.0f, Ease((1.0f - Slash) * 1.34f)) : -22.0f;
	Director->SetRelativeLocationAndRotation(FVector(Position, FMath::Abs(Step) * 3.5f), FVector(Facing, 0).Rotation());
	const float Fall = Ease(Split / 0.78f);
	Upper->SetRelativeLocationAndRotation(FVector(-96 * Fall, 29 * Fall, 80 - 71 * Fall), FRotator(-91 * Fall, 28 * Fall, -17 * Fall));
	Lower->SetRelativeLocationAndRotation(FVector(51 * Fall, -14 * Fall, 80 - 66 * Fall), FRotator(74 * Fall, -8 * Fall, 0));
	RightArm->SetRelativeRotation(FRotator(Slash > 0.0f ? 5.0f : -24.0f, Swing, 10));
	LeftArm->SetRelativeRotation(FRotator(-71 + 18 * Step, -21, 0));
	Legs[0]->SetRelativeRotation(FRotator(22 * Step, 0, 0));
	Legs[1]->SetRelativeRotation(FRotator(-22 * Step, 0, 0));
}

void ADirectorsCutStage::PoseEnemy(int32 Index, FVector2D Position, FVector2D Direction, EEnemyMove Move, float Time)
{
	FFilmEnemy& Enemy = FilmEnemies[Index];
	const bool bVisible = Move != EEnemyMove::Waiting && (Move != EEnemyMove::Cut || Time < 0.55f);
	Enemy.Root->SetVisibility(bVisible, true);
	if (!bVisible)
	{
		return;
	}
	const bool bAttack = Move == EEnemyMove::Lunge;
	const bool bTell = Move == EEnemyMove::Windup;
	const float Lift = bAttack ? 12.0f : 7.0f * FMath::Sin(Time * 10.0f);
	const float Roll = bAttack ? 0.0f : 11.0f * FMath::Sin(Time * 5.0f);
	const float Split = Move == EEnemyMove::Cut ? Ease(Time / 0.5f) : 0.0f;
	const float Angle = FVector(Direction, 0).Rotation().Yaw;
	Enemy.Root->SetRelativeLocationAndRotation(FVector(Position, Lift), FRotator(0, Angle, 0));
	Enemy.Top->SetRelativeLocationAndRotation(FVector(25 * Split, 50 * Split, 117 + 27 * Split),
		FRotator(bAttack ? -23.0f : 0.0f, bAttack ? 76.0f : 21.0f, Roll + 77.0f * Split));
	Enemy.Bottom->SetRelativeLocationAndRotation(FVector(-30 * Split, -56 * Split, 117 - 28 * Split),
		FRotator(bAttack ? -23.0f : 0.0f, bAttack ? 76.0f : 21.0f, Roll - 88.0f * Split));
	Enemy.Tell->SetVisibility(bTell);
}

void ADirectorsCutStage::PoseBladeTrail(FVector2D Position, FVector2D Direction, float Slash)
{
	BladeTrail->SetVisibility(Slash > 0.35f && Slash < 0.78f, true);
	BladeTrail->SetRelativeLocationAndRotation(FVector(Position, 0), FVector(Direction, 0).Rotation());
}
