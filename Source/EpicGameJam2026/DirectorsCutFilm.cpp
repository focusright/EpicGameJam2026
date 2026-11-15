#include "DirectorsCutStage.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

using namespace DirectorsCut;

namespace
{
	const FColor Asphalt(42, 46, 48);
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
	const FColor Pavement(76, 78, 75);
	const FColor Curb(123, 124, 114);
	const FColor RoadShoulder(54, 55, 52);
	const FColor RoadRepair[] = {FColor(58, 58, 54), FColor(49, 51, 51), FColor(34, 38, 40)};
	const FColor FadedPaint(119, 116, 96);
	const FColor Crack(24, 27, 28);
	const FColor RubbleStone[] = {FColor(100, 95, 84), FColor(135, 130, 116), FColor(68, 71, 68)};
	UMaterialInterface* WeatheredPaving = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/Props/CitySurface/M_WeatheredPaving.M_WeatheredPaving"));
	TMap<uint32, UMaterialInstanceDynamic*> PavingMaterials;
	const auto AddGround = [&](FVector Location, FVector Size, FColor Color, FRotator Rotation = FRotator::ZeroRotator)
	{
		UStaticMeshComponent* Surface = Shape(City, EShape::Box, Location, Size, Color, Rotation);
		if (WeatheredPaving)
		{
			UMaterialInstanceDynamic*& Material = PavingMaterials.FindOrAdd(Color.ToPackedARGB());
			if (!Material)
			{
				Material = UMaterialInstanceDynamic::Create(WeatheredPaving, this);
				Material->SetVectorParameterValue(TEXT("PavingTint"), FLinearColor::FromSRGBColor(Color) * 2.7f);
			}
			Surface->SetMaterial(0, Material);
		}
	};
	AddGround(FVector(0, 0, -106), FVector(14500, 13700, 200), Pavement);
	AddGround(FVector(0, 0, -3.5), FVector(12500, 650, 7), Asphalt);
	for (double Side : {-1.0, 1.0})
	{
		AddGround(FVector(0, Side * 3112.5, -3.5), FVector(650, 5575, 7), Asphalt);
	}
	FRandomStream Random(8415);
	FRandomStream StreetRandom(3671);
	FRandomStream FacadeRandom(1745);
	const FColor Walls[] = {FColor(78, 98, 109), FColor(117, 109, 97), FColor(132, 132, 118), FColor(86, 98, 94)};
	const FColor WallTrim[] = {FColor(112, 132, 141), FColor(151, 139, 121), FColor(156, 155, 141)};
	const FColor RoofMembrane(52, 58, 61);
	const FColor RoofSeam(64, 69, 71);
	const FColor Galvanized(112, 124, 124);
	const FColor UnlitWindows[] = {FColor(34, 63, 78), FColor(63, 101, 117)};
	const FColor LitWindows[] = {FColor(234, 188, 111), FColor(168, 209, 218)};
	UStaticMesh* GlassPlane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	UMaterialInterface* Glass = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Props/FilmEnemy/M_FilmStock.M_FilmStock"));

	// Each façade has a different mix of occupied rooms, with smoked glass over the closest buildings.
	const auto AddFacade = [&](USceneComponent* Parent, FVector Base, double Width, double Depth, double Height,
		int32 Style, bool bGlass)
	{
		const double FloorSpacing = Style == 1 ? 59.0 : 70.0;
		const double FirstFloor = 88.0;
		const int32 Rows = FMath::Max(1, 1 + FMath::FloorToInt((Height - 32.0 - FirstFloor) / FloorSpacing));
		const double PaneHeight = Style == 0 ? 31.0 : 42.0;
		for (bool bXFace : {false, true})
		{
			const double Span = bXFace ? Depth : Width;
			const double FaceOffset = (bXFace ? Width : Depth) / 2;
			const int32 Columns = FMath::Max(2, FMath::RoundToInt(Span / (Style == 2 ? 84.0 : 66.0)));
			const double Pitch = Span * 0.86 / Columns;
			const double PaneWidth = Pitch * (Style == 2 ? 0.59 : 0.77);
			for (double Side : {-1.0, 1.0})
			{
				const auto Position = [&](double Across, double Out, double Z)
				{
					return Base + (bXFace ? FVector(Side * (FaceOffset + Out), Across, Z)
						: FVector(Across, Side * (FaceOffset + Out), Z));
				};
				const auto Size = [bXFace](double Across, double Out, double Z)
				{
					return bXFace ? FVector(Out, Across, Z) : FVector(Across, Out, Z);
				};
				const FColor Trim = WallTrim[Style];
				for (double Edge : {-1.0, 1.0})
				{
					Prop(Parent, EShape::Box, Position(Edge * (Span / 2 - 10), 1, Height / 2),
						Size(15, 9, Height), Trim);
				}
				Prop(Parent, EShape::Box, Position(0, 2, 21), Size(Span - 20, 7, 40), Steel);
				Prop(Parent, EShape::Box, Position(0, 4, 80 - PaneHeight / 2), Size(Span - 20, 12, 5), Trim);
				for (int32 Row = 0; Row < Rows; ++Row)
				{
					const double Z = FirstFloor + Row * FloorSpacing - PaneHeight / 2 - 6;
					Prop(Parent, EShape::Box, Position(0, Style == 2 ? 5 : 2, Z),
						Size(Span - 26, Style == 2 ? 12 : 5, Style == 0 ? 3 : 5), Trim);
				}
				for (int32 Column = 0; Column < Columns; ++Column)
				{
					const double Across = (Column - (Columns - 1) * 0.5) * Pitch;
					const double GlazingHeight = (Rows - 1) * FloorSpacing + PaneHeight;
					const double MiddleZ = FirstFloor + (Rows - 1) * FloorSpacing / 2;
					Prop(Parent, EShape::Box, Position(Across, 1, MiddleZ), Size(PaneWidth + 7, 5, GlazingHeight + 10), Black);
					for (int32 Row = 0; Row < Rows; ++Row)
					{
						const bool bLit = FacadeRandom.FRand() < (Style == 1 ? 0.40f : 0.26f);
						const int32 ColorIndex = FacadeRandom.RandRange(0, 1);
						Prop(Parent, EShape::Box, Position(Across, 4, FirstFloor + Row * FloorSpacing),
							Size(PaneWidth, 5, PaneHeight), bLit ? LitWindows[ColorIndex] : UnlitWindows[ColorIndex]);
					}
					if (bGlass && Glass && GlassPlane)
					{
						UStaticMeshComponent* Pane = NewObject<UStaticMeshComponent>(this);
						Pane->SetupAttachment(Parent);
						Pane->SetStaticMesh(GlassPlane);
						Pane->SetMaterial(0, Glass);
						Pane->SetCollisionEnabled(ECollisionEnabled::NoCollision);
						Pane->SetCastShadow(false);
						Pane->SetRelativeTransform(FTransform(FRotator(90, bXFace ? 0 : 90, 0),
							Position(Across, 7, MiddleZ), FVector(GlazingHeight / 100, PaneWidth / 100, 1)));
						Pane->RegisterComponent();
					}
				}
				Prop(Parent, EShape::Box, Position(0, 7, 30), Size(Span * 0.39, 7, 44), Black);
				for (double Door : {-1.0, 1.0})
				{
					Prop(Parent, EShape::Box, Position(Door * Span * 0.083, 11, 30), Size(Span * 0.15, 3, 37), UnlitWindows[1]);
				}
				Prop(Parent, EShape::Box, Position(0, 14, 60), Size(Span * 0.47, 28, 6), Trim);
				Prop(Parent, EShape::Box, Position(0, 19, 7), Size(Span * 0.48, 32, 7), Concrete);
			}
		}
	};
	const auto AddRoof = [&](USceneComponent* Parent, FVector Base, double Width, double Depth, double Height, int32 Style)
	{
		const FVector Roof = Base + FVector(0, 0, Height);
		const FColor Coping = WallTrim[Style];
		Prop(Parent, EShape::Box, Roof + FVector(0, 0, 6), FVector(Width + 8, Depth + 8, 12), Steel);
		Prop(Parent, EShape::Box, Roof + FVector(0, 0, 13), FVector(Width - 8, Depth - 8, 3), RoofMembrane);
		for (double Side : {-1.0, 1.0})
		{
			Prop(Parent, EShape::Box, Roof + FVector(Side * Width / 2, 0, 20), FVector(8, Depth + 8, 24), Coping);
			Prop(Parent, EShape::Box, Roof + FVector(0, Side * Depth / 2, 20), FVector(Width - 8, 8, 24), Coping);
			Prop(Parent, EShape::Box, Roof + FVector(Side * Width / 2, 0, 33), FVector(14, Depth + 14, 4), Steel);
			Prop(Parent, EShape::Box, Roof + FVector(0, Side * Depth / 2, 33), FVector(Width - 14, 14, 4), Steel);
		}
		for (double Seam = -Width / 2 + 32; Seam < Width / 2 - 12; Seam += 48)
		{
			Prop(Parent, EShape::Box, Roof + FVector(Seam, 0, 15), FVector(1.5, Depth - 18, 1.5), RoofSeam);
		}

		const FVector Service = Roof + FVector(-Width * 0.22, -Depth * 0.16, 0);
		const double ServiceWidth = Width * 0.27;
		const double ServiceDepth = Depth * 0.37;
		Prop(Parent, EShape::Box, Service + FVector(0, 0, 34), FVector(ServiceWidth, ServiceDepth, 39), Coping);
		Prop(Parent, EShape::Box, Service + FVector(0, 0, 55), FVector(ServiceWidth + 6, ServiceDepth + 6, 5), Steel);
		Prop(Parent, EShape::Box, Service + FVector(ServiceWidth / 2 + 1, 0, 29),
			FVector(3, ServiceDepth * 0.42, 27), Steel);

		const FVector Vent = Roof + FVector(Width * 0.24, Depth * 0.19, 0);
		const double VentWidth = Width * 0.23;
		const double VentDepth = Depth * 0.24;
		Prop(Parent, EShape::Box, Vent + FVector(0, 0, 17), FVector(VentWidth + 7, VentDepth + 7, 5), Concrete);
		Prop(Parent, EShape::Box, Vent + FVector(0, 0, 29), FVector(VentWidth, VentDepth, 20), Galvanized);
		Prop(Parent, EShape::Cylinder, Vent + FVector(0, 0, 40),
			FVector(FMath::Min(VentWidth, VentDepth) * 0.75, FMath::Min(VentWidth, VentDepth) * 0.75, 3), Black);
		for (double Slat : {-6.0, 0.0, 6.0})
		{
			Prop(Parent, EShape::Box, Vent + FVector(VentWidth / 2 + 1, 0, 29 + Slat),
				FVector(2, VentDepth * 0.85, 2), Steel);
		}
		if (Style == 1)
		{
			Prop(Parent, EShape::Box, Service + FVector(0, 0, 86), FVector(7, 7, 58), Galvanized);
			Prop(Parent, EShape::Box, Service + FVector(0, 0, 103), FVector(5, 55, 5), Steel);
		}
		else
		{
			const FVector Stack = Roof + FVector(-Width * 0.25, Depth * 0.30, 0);
			Prop(Parent, EShape::Cylinder, Stack + FVector(0, 0, 31), FVector(12, 12, 33), Galvanized);
			Prop(Parent, EShape::Cylinder, Stack + FVector(0, 0, 49), FVector(19, 19, 4), Steel);
		}
	};
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
			const int32 Style = FMath::Abs(X * 2 + Y) % 3;
			Prop(City, EShape::Box, Location + FVector(0, 0, 0), FVector(700, 680, 23), Concrete);
			Prop(City, EShape::Box, Location + FVector(0, 0, Height / 2), FVector(Width, Depth, Height), Wall);
			AddFacade(City, Location, Width, Depth, Height, Style, FMath::Abs(Y) == 1 && FMath::Abs(X) <= 2);
			AddRoof(City, Location, Width, Depth, Height, Style);
		}
	}
	for (bool bCrossStreet : {false, true})
	{
		const auto RoadPoint = [bCrossStreet](double Along, double Across, double Height)
		{
			return bCrossStreet ? FVector(Across, Along, Height) : FVector(Along, Across, Height);
		};
		const FRotator RoadRotation(0, bCrossStreet ? 90 : 0, 0);
		for (int32 Index = -27; Index <= 27; ++Index)
		{
			if (FMath::Abs(Index) < 3)
			{
				continue;
			}
			const double Along = Index * 180;
			Prop(City, EShape::Box, RoadPoint(Along, 0, 0.6), FVector(76, 8, 1),
				Index % 4 == 0 ? StreetPaint : FadedPaint, RoadRotation);
			for (double Side : {-1.0, 1.0})
			{
				Prop(City, EShape::Box, RoadPoint(Along, Side * 308, 0.6), FVector(179, 32, 1), RoadShoulder, RoadRotation);
				Prop(City, EShape::Box, RoadPoint(Along, Side * 332, 3), FVector(178, 15, 12), Curb, RoadRotation);
				Prop(City, EShape::Box, RoadPoint(Along, Side * 369, 0), FVector(176, 62, 15),
					Index % 3 == 0 ? Concrete : Pavement, RoadRotation);
				if (Index % 4 == 0)
				{
					const FVector Drain = RoadPoint(Along + 38, Side * 307, 1);
					Prop(City, EShape::Box, Drain, FVector(35, 19, 2), Black, RoadRotation);
					for (double Bar : {-10.0, 0.0, 10.0})
					{
						Prop(City, EShape::Box, RoadPoint(Along + 38 + Bar, Side * 307, 2.2),
							FVector(2.5, 17, 1), Curb, RoadRotation);
					}
				}
			}
			if (Index % 3 == 0)
			{
				const FVector Cover = RoadPoint(Along + 49, 101, 0.4);
				Prop(City, EShape::Cylinder, Cover, FVector(41, 41, 2), Black);
				Prop(City, EShape::Cylinder, Cover + FVector(0, 0, 1), FVector(34, 34, 1.5), RoadShoulder);
			}
		}
		for (int32 Patch = 0; Patch < 65; ++Patch)
		{
			const double Along = StreetRandom.FRandRange(-4600, 4600);
			const double Across = StreetRandom.FRandRange(54, 278) * (Patch % 2 == 0 ? -1 : 1);
			const double Length = StreetRandom.FRandRange(29, 160);
			const double Width = StreetRandom.FRandRange(16, 59);
			const FRotator PatchRotation(0, RoadRotation.Yaw + StreetRandom.FRandRange(-12, 12), 0);
			Prop(City, EShape::Box, RoadPoint(Along, Across, 0.2), FVector(Length, Width, 0.7),
				RoadRepair[Patch % UE_ARRAY_COUNT(RoadRepair)], PatchRotation);
			if (Patch % 3 == 0)
			{
				FVector Start = RoadPoint(Along - Length / 2, Across, 1.1);
				for (int32 Segment = 1; Segment <= 4; ++Segment)
				{
					const FVector End = RoadPoint(Along + Length * (Segment / 4.0 - 0.5),
						Across + StreetRandom.FRandRange(-17, 17), 1.1);
					const FVector Span = End - Start;
					Prop(City, EShape::Box, (Start + End) / 2, FVector(Span.Size() + 1, 2.8, 0.7), Crack, Span.Rotation());
					Start = End;
				}
			}
		}
	}
	for (int32 Index = 0; Index < 10; ++Index)
	{
		for (double Side : {-1.0, 1.0})
		{
			const FColor Paint = Index % 3 == 0 ? FadedPaint : StreetPaint;
			Prop(City, EShape::Box, FVector(Side * 355, Index * 43 - 195, 0.7), FVector(99, 26, 1), Paint);
			Prop(City, EShape::Box, FVector(Index * 43 - 195, Side * 355, 0.7), FVector(26, 99, 1), Paint);
		}
	}
	// Extend the avenue into the pale apron visible behind the last buildings in the opening shot.
	constexpr double PerimeterJunction = -6600.0;
	AddGround(FVector(-6425, 0, -3.5), FVector(350, 650, 7), Asphalt);
	AddGround(FVector(PerimeterJunction - 140, 0, -3.5), FVector(280, 3600, 7), Asphalt);
	const FColor CarColors[] = {FColor(174, 174, 158), Rust, FColor(83, 113, 127), FColor(186, 147, 87)};
	const FColor CarGlass(28, 44, 51);
	const auto AddCar = [&](double X, double Y, double Yaw, int32 ColorIndex, bool bVan)
	{
		const FRotator Heading(0, Yaw, 0);
		const FVector Origin(X, Y, 0);
		const FColor Body = CarColors[ColorIndex];
		const double RoofHeight = bVan ? 50 : 39;
		const auto Part = [&](EShape Mesh, FVector Offset, FVector Size, FColor Color, FRotator Rotation = FRotator::ZeroRotator)
		{
			Prop(City, Mesh, Origin + Heading.RotateVector(Offset), Size, Color,
				(Heading.Quaternion() * Rotation.Quaternion()).Rotator());
		};
		Part(EShape::Box, FVector(0, 0, 17), FVector(121, 49, 20), Body);
		Part(EShape::Box, FVector(-5, 0, (RoofHeight + 25) / 2),
			FVector(bVan ? 76 : 57, 44, RoofHeight - 25), CarGlass);
		Part(EShape::Box, FVector(-8, 0, RoofHeight), FVector(bVan ? 76 : 45, 45, 5), Body);
		for (double Side : {-1.0, 1.0})
		{
			Part(EShape::Box, FVector(-8, Side * 22, (RoofHeight + 25) / 2), FVector(4, 2, RoofHeight - 25), Body);
			for (double Axle : {-38.0, 38.0})
			{
				Part(EShape::Cylinder, FVector(Axle, Side * 24, 10), FVector(19, 19, 8), Black, FRotator(0, 0, 90));
				Part(EShape::Cylinder, FVector(Axle, Side * 29, 10), FVector(9, 9, 2), Concrete, FRotator(0, 0, 90));
			}
			Part(EShape::Box, FVector(61, Side * 16, 21), FVector(3, 11, 5), StreetPaint);
			Part(EShape::Box, FVector(-61, Side * 16, 21), FVector(3, 9, 5), Rust);
		}
	};
	for (double Side : {-1.0, 1.0})
	{
		for (int32 Block = 0; Block < 7; ++Block)
		{
			const double Y = Side * (323 + Block * 213);
			Prop(City, EShape::Box, FVector(-6895, Y, -0.5), FVector(25, 208, 9), Curb);
			Prop(City, EShape::Box, FVector(-6985, Y, -3.5), FVector(150, 210, 6), Block % 2 ? RoadShoulder : Concrete);
			Prop(City, EShape::Box, FVector(-6740, Y, 0.7), FVector(7, 96, 1), FadedPaint);
			if (Block % 2 == 0)
			{
				Prop(City, EShape::Box, FVector(-6895, Y, 51), FVector(10, 10, 102), Steel);
				Prop(City, EShape::Box, FVector(-6869, Y, 100), FVector(58, 10, 8), Steel);
				Prop(City, EShape::Box, FVector(-6842, Y, 97), FVector(18, 13, 6), Lamp);
			}
		}
		for (int32 Stripe = 0; Stripe < 9; ++Stripe)
		{
			Prop(City, EShape::Box, FVector(-6521, Side * (21 + Stripe * 31), 0.7),
				FVector(93, 14, 1), Stripe % 3 == 0 ? FadedPaint : StreetPaint);
		}
		AddCar(-3930, Side * 225, Side < 0 ? 180 : 0, 2, false);
		AddCar(-4790, Side * 229, Side < 0 ? 174 : 7, 3, false);
		AddCar(-5610, Side * 224, Side < 0 ? 185 : -9, 1, false);
		AddCar(-6130, Side * 202, Side < 0 ? 170 : 14, 0, true);
		AddCar(-6730, Side * 420, Side * 90 + 14, 2, true);
		AddCar(-6795, Side * 720, Side * 90 - 12, Side < 0 ? 1 : 0, false);
		AddCar(-6705, Side * 1075, Side * 90 + 8, 3, true);
	}
	for (int32 Patch = 0; Patch < 70; ++Patch)
	{
		const double X = StreetRandom.FRandRange(-6872, -6610);
		const double Side = Patch % 2 == 0 ? 1.0 : -1.0;
		const double Y = Side * StreetRandom.FRandRange(340, 1630);
		const double Size = StreetRandom.FRandRange(15, 53);
		Prop(City, EShape::Box, FVector(X, Y, 0.4), FVector(Size, Size * 1.6, 0.6),
			RoadRepair[Patch % UE_ARRAY_COUNT(RoadRepair)], FRotator(0, StreetRandom.FRandRange(-28, 28), 0));
		if (Patch % 2 == 0)
		{
			const double Edge = Patch % 4 == 0 ? -6852 : -6622;
			Prop(City, EShape::Box, FVector(Edge + StreetRandom.FRandRange(-14, 14), Y, Size * 0.12),
				FVector(Size * 0.8, Size * 0.5, Size * 0.34), RubbleStone[Patch % UE_ARRAY_COUNT(RubbleStone)],
				FRotator(11, StreetRandom.FRandRange(0, 180), 13));
		}
	}

	const FColor LotSurface(55, 58, 58);
	const FColor Landscaping(70, 77, 54);
	const FColor SignBlue(45, 100, 140);
	const auto AddSign = [&](FVector Ground, double Yaw, bool bParking)
	{
		const FRotator Rotation(0, Yaw, 0);
		const auto SignPart = [&](FVector Offset, FVector Size, FColor Color)
		{
			Prop(City, EShape::Box, Ground + Rotation.RotateVector(Offset), Size, Color, Rotation);
		};
		SignPart(FVector(0, 0, 36), FVector(6, 6, 72), Steel);
		SignPart(FVector(0, 0, 69), FVector(33, 6, 34), bParking ? SignBlue : Rust);
		// A parking P or a no-entry bar appears on both faces.
		for (double Face : {-1.0, 1.0})
		{
			if (bParking)
			{
				SignPart(FVector(-5, Face * 3.5, 69), FVector(4, 1, 21), StreetPaint);
				SignPart(FVector(1, Face * 3.5, 77), FVector(12, 1, 4), StreetPaint);
				SignPart(FVector(1, Face * 3.5, 69), FVector(12, 1, 4), StreetPaint);
				SignPart(FVector(5, Face * 3.5, 73), FVector(4, 1, 8), StreetPaint);
			}
			else
			{
				SignPart(FVector(0, Face * 3.5, 69), FVector(23, 1, 6), StreetPaint);
			}
		}
	};
	const auto AddStreetlight = [&](FVector Ground, double Yaw)
	{
		const FRotator Heading(0, Yaw, 0);
		Prop(City, EShape::Box, Ground + FVector(0, 0, 67), FVector(9, 9, 135), Steel);
		Prop(City, EShape::Box, Ground + Heading.RotateVector(FVector(26, 0, 131)), FVector(59, 8, 8), Steel, Heading);
		Prop(City, EShape::Box, Ground + Heading.RotateVector(FVector(54, 0, 128)), FVector(16, 14, 5), Lamp, Heading);
	};

	// Collector streets connect the ends of the two main streets. Parking courts and planted
	// islands sit outside the collector, where the city previously gave way to bare ground.
	for (bool bCrossAxis : {false, true})
	{
		const double RoadOffset = bCrossAxis ? 5120 : 5510;
		const double RoadLength = bCrossAxis ? 10788 : 10470;
		const double RoadYaw = bCrossAxis ? 0 : 90;
		const FRotator RoadHeading(0, RoadYaw, 0);
		for (double Side : {-1.0, 1.0})
		{
			const auto AtEdge = [bCrossAxis, Side](double Along, double Out, double Z)
			{
				return bCrossAxis ? FVector(Along, Side * Out, Z) : FVector(Side * Out, Along, Z);
			};
			AddGround(AtEdge(0, RoadOffset, -2.9), FVector(RoadLength, 232, 6), Asphalt, RoadHeading);
			for (int32 Segment = -10; Segment <= 10; ++Segment)
			{
				if (Segment == 0)
				{
					continue;
				}
				const double Along = Segment * 475.0;
				Prop(City, EShape::Box, AtEdge(Along, RoadOffset, 0.7), FVector(101, 8, 1), FadedPaint, RoadHeading);
				Prop(City, EShape::Box, AtEdge(Along, RoadOffset + 121, 0), FVector(465, 14, 10), Curb, RoadHeading);
				Prop(City, EShape::Box, AtEdge(Along, RoadOffset + 163, -1), FVector(459, 68, 10), Pavement, RoadHeading);
				if (Segment % 2 == 0)
				{
					const double LampYaw = bCrossAxis ? -Side * 90 : (Side > 0 ? 180 : 0);
					AddStreetlight(AtEdge(Along, RoadOffset + 151, 4), LampYaw);
					const FVector Parked = AtEdge(Along + 140, RoadOffset + 76, 0);
					AddCar(Parked.X, Parked.Y, RoadYaw + (Side < 0 ? 180 : 0), FMath::Abs(Segment / 2) % 4, Segment % 4 == 0);
				}
				if (Segment % 3 == 0)
				{
					const FVector Mark = AtEdge(Along - 180, RoadOffset + 140, 4);
					AddSign(Mark, RoadYaw + 90, Segment > 0);
					for (double Delta : {-25.0, 25.0})
					{
						Prop(City, EShape::Cylinder, AtEdge(Along - 180 + Delta, RoadOffset + 140, 17),
							FVector(7, 7, 26), Segment > 0 ? Steel : Lamp);
					}
				}
			}
			for (int32 Court = -2; Court <= 2; ++Court)
			{
				const double Along = Court * 1740.0 + (Court == 0 ? 800.0 : 0.0);
				const double CourtOut = RoadOffset + 645;
				AddGround(AtEdge(Along, CourtOut, -2.8), FVector(940, 670, 5), LotSurface, RoadHeading);
				AddGround(AtEdge(Along, RoadOffset + 239, -2.4), FVector(160, 210, 5), LotSurface, RoadHeading);
				const FVector Entrance = AtEdge(Along + 91, RoadOffset + 299, 0);
				AddSign(Entrance, RoadYaw, true);
				for (int32 Stall = 0; Stall < 9; ++Stall)
				{
					const double Bay = Along + (Stall - 4) * 87.0;
					for (double Row : {-1.0, 1.0})
					{
						const double Out = CourtOut + Row * 195;
						Prop(City, EShape::Box, AtEdge(Bay - 43, Out, 0.4), FVector(4, 142, 1), FadedPaint, RoadHeading);
						if ((Stall + Court + (Row > 0 ? 2 : 0)) % 3 == 0)
						{
							const FVector Parked = AtEdge(Bay, Out, 0);
							const double ParkYaw = RoadYaw + Row * 90 + (Stall % 2 == 0 ? 5 : -3);
							AddCar(Parked.X, Parked.Y, ParkYaw, (Stall + Court + 2) % 4, Stall % 4 == 0);
						}
					}
				}
				for (double End : {-1.0, 1.0})
				{
					const double EndAlong = Along + End * 425;
					Prop(City, EShape::Box, AtEdge(EndAlong, CourtOut, 2), FVector(75, 427, 12), Curb, RoadHeading);
					Prop(City, EShape::Box, AtEdge(EndAlong, CourtOut, 8), FVector(65, 414, 4), Landscaping, RoadHeading);
					for (double Tree : {-125.0, 0.0, 125.0})
					{
						Prop(City, EShape::Cylinder, AtEdge(EndAlong, CourtOut + Tree, 25), FVector(9, 9, 35), CashDark);
						Prop(City, EShape::Ball, AtEdge(EndAlong, CourtOut + Tree, 57), FVector(54, 52, 60), Landscaping);
					}
				}
			}
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
		AddFacade(Building, FVector::ZeroVector, 136, 180, Height, Index % 3, true);
		AddRoof(Building, FVector::ZeroVector, 136, 180, Height, Index % 3);
		USceneComponent* Rubble = Group(City, Location);
		for (int32 Chunk = 0; Chunk < 13; ++Chunk)
		{
			const double Size = StreetRandom.FRandRange(11, 36);
			Prop(Rubble, EShape::Box,
				FVector(StreetRandom.FRandRange(-65, 175), Side * StreetRandom.FRandRange(-115, 165), Size * 0.22),
				FVector(Size * 1.35, Size, Size * 0.60), RubbleStone[Chunk % UE_ARRAY_COUNT(RubbleStone)],
				FRotator(StreetRandom.FRandRange(-17, 17), StreetRandom.FRandRange(0, 180), 13));
		}
		Rubble->SetVisibility(false, true);
		Buildings.Add({Building, Rubble, Location, Time, Height});
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
		const bool bRubbleVisible = Collapse > 0.25f;
		if (Building.Rubble->IsVisible() != bRubbleVisible)
		{
			Building.Rubble->SetVisibility(bRubbleVisible, true);
		}
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
