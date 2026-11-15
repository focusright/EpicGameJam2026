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
	Prop(Arena, EShape::Box, FVector(0, 0, -29), FVector(7000, 7400, 48), Ink);
	Prop(Arena, EShape::Box, FVector(0, 0, -9), FVector(DeckX, DeckY, 18), Floor);
	for (int32 X = -5; X <= 5; ++X)
	{
		Prop(Arena, EShape::Box, FVector(X * 100, 0, 0.3), FVector(1.2, 1925, 0.6), FColor(77, 84, 87));
	}
	for (int32 Y = -9; Y <= 9; ++Y)
	{
		Prop(Arena, EShape::Box, FVector(0, Y * 100, 0.3), FVector(1205, 1.2, 0.6), FColor(77, 84, 87));
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

	// Sideline clusters just outside the teal border.
	for (double Side : {-1.0, 1.0})
	{
		CanStack(FVector(-420, Side * 1085, 0), 0.52f, 3, Side * 20.0f);
		CanStack(FVector(-180, Side * 1110, 0), 0.44f, 2, Side * -35.0f);
		CanStack(FVector(210, Side * 1095, 0), 0.48f, 2, Side * 55.0f);
		StandingReel(FVector(-300, Side * 1060, 95 * 0.58f), 0.58f, Side > 0 ? 12.0f : -18.0f);
		StandingReel(FVector(40, Side * 1075, 95 * 0.50f), 0.50f, Side > 0 ? -40.0f : 55.0f);
		StandingReel(FVector(340, Side * 1055, 95 * 0.62f), 0.62f, Side > 0 ? 70.0f : -65.0f);
		FlatReel(FVector(-70, Side * 1145, 14 * 0.40f), 0.40f, Side * 18.0f);
		FlatReel(FVector(480, Side * 1120, 14 * 0.36f), 0.36f, Side * -50.0f);
		Dress(Arena, CanTop, FVector(-510, Side * 1040, 12 * 0.42f), 0.42f, FRotator(0, Side * 80.0f, 0));
		Dress(Arena, Tape, FVector(120, Side * 1160, 11 * 0.34f), 0.34f, FRotator(0, Side * 110.0f, 0));

		// Soft cases and cable spools between the lights.
		Prop(Arena, EShape::Box, FVector(-250, Side * 1180, 28), FVector(110, 64, 54), Ink, FRotator(0, Side * 12, 0));
		Prop(Arena, EShape::Box, FVector(-250, Side * 1180, 58), FVector(104, 58, 8), Steel);
		Prop(Arena, EShape::Cylinder, FVector(160, Side * 1205, 42), FVector(70, 70, 84), Sprocket);
		Prop(Arena, EShape::Cylinder, FVector(160, Side * 1205, 88), FVector(28, 28, 18), Steel);
		Prop(Arena, EShape::Box, FVector(430, Side * 1175, 22), FVector(88, 52, 42), Ink, FRotator(0, Side * -18, 0));
	}

	// Foreground apron (camera side): keep low so it frames the deck.
	CanStack(FVector(-690, -420, 0), 0.40f, 2, -25.0f);
	CanStack(FVector(-705, 380, 0), 0.38f, 2, 40.0f);
	FlatReel(FVector(-660, -120, 12), 0.34f, 15.0f);
	FlatReel(FVector(-675, 160, 12), 0.32f, -55.0f);
	StandingReel(FVector(-640, 520, 78), 0.46f, 28.0f);
	Dress(Arena, CanTop, FVector(-650, -560, 10), 0.36f, FRotator(12, -30, 8));
	Prop(Arena, EShape::Box, FVector(-720, 0, 18), FVector(46, 260, 8), Sprocket, FRotator(0, 6, 0));
	Prop(Arena, EShape::Box, FVector(-730, -250, 24), FVector(70, 48, 36), Ink, FRotator(0, -12, 0));
	Prop(Arena, EShape::Box, FVector(-730, 250, 24), FVector(64, 44, 36), Ink, FRotator(0, 16, 0));

	// Background against the tower wall: denser reel dumps and open cans.
	CanStack(FVector(640, -620, 0), 0.55f, 3, 10.0f);
	CanStack(FVector(655, -280, 0), 0.50f, 2, -20.0f);
	CanStack(FVector(640, 120, 0), 0.58f, 3, 35.0f);
	CanStack(FVector(650, 480, 0), 0.46f, 2, -45.0f);
	StandingReel(FVector(610, -450, 110), 0.70f, 8.0f);
	StandingReel(FVector(600, -80, 100), 0.64f, -22.0f);
	StandingReel(FVector(615, 300, 115), 0.72f, 48.0f);
	StandingReel(FVector(605, 620, 95), 0.58f, -70.0f);
	FlatReel(FVector(680, -780, 16), 0.42f, 20.0f);
	FlatReel(FVector(690, 740, 16), 0.40f, -35.0f);
	Dress(Arena, Tape, FVector(670, -40, 14), 0.38f, FRotator(0, 70, 0));
	Dress(Arena, CanTop, FVector(700, 220, 14), 0.44f, FRotator(-6, 15, 10));

	// Film-strip ribbons and marker wedges around the apron.
	for (int32 Index = 0; Index < 6; ++Index)
	{
		const double Y = Index * 280 - 700;
		Prop(Arena, EShape::Box, FVector(-780, Y, 4), FVector(18, 210, 2.4), Gold, FRotator(0, Index % 2 ? 4 : -5, 0));
		Prop(Arena, EShape::Box, FVector(780, Y, 4), FVector(18, 210, 2.4), Sprocket, FRotator(0, Index % 2 ? -3 : 6, 0));
	}
	for (double Side : {-1.0, 1.0})
	{
		Prop(Arena, EShape::Cone, FVector(-540, Side * 980, 28), FVector(28, 28, 56), Gold);
		Prop(Arena, EShape::Cone, FVector(540, Side * 980, 28), FVector(28, 28, 56), Red);
		Prop(Arena, EShape::Box, FVector(Side * 20, Side * 1240, 36), FVector(150, 40, 70), Ink, FRotator(0, 90, 0));
		Prop(Arena, EShape::Cylinder, FVector(Side * 280, Side * 1230, 55), FVector(54, 54, 110), Steel);
	}

	// Simple camera bodies on sticks just off the short ends.
	for (double Side : {-1.0, 1.0})
	{
		USceneComponent* Camera = Group(Arena, FVector(Side * 20, Side * 1280, 0));
		Prop(Camera, EShape::Cylinder, FVector(0, 0, 70), FVector(10, 10, 140), Steel);
		Prop(Camera, EShape::Box, FVector(18, 0, 150), FVector(70, 42, 36), Ink, FRotator(0, 0, -12));
		Prop(Camera, EShape::Cylinder, FVector(58, 0, 150), FVector(22, 22, 40), Steel, FRotator(0, 0, 90));
		Prop(Camera, EShape::Box, FVector(18, 0, 128), FVector(40, 70, 8), Sprocket);
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
