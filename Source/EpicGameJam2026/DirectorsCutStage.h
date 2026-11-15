#pragma once

#include "CoreMinimal.h"
#include "DirectorsCutGameMode.h"
#include "GameFramework/Actor.h"
#include "DirectorsCutStage.generated.h"

class ACameraActor;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPoseableMeshComponent;
class USkeletalMesh;
class UStaticMesh;
class UStaticMeshComponent;
class UTexture2D;

UCLASS()
class ADirectorsCutStage : public AActor
{
	GENERATED_BODY()

public:
	ADirectorsCutStage();

	static const FVector ArenaOrigin;
	static constexpr float ArenaDepth = 515.0f;
	static constexpr float ArenaWidth = 880.0f;

	bool Build();
	void ResetArena();
	void SetArenaCamera(ACameraActor& Camera, float Time, bool bTitle) const;
	void PoseDirector(FVector2D Position, FVector2D Facing, float Stride, float Slash, float Split);
	void PoseEnemy(int32 Index, FVector2D Position, FVector2D Direction, DirectorsCut::EEnemyMove Move, float Time);
	void PoseBladeTrail(FVector2D Position, FVector2D Direction, float Slash);
	void ShowFilm(ACameraActor& Camera, int32 Shot, float ShotTime);
	void ShowPayout(ACameraActor& Camera, int32 EarnedBags, float Time);

private:
	enum class EShape : uint8 { Box, Ball, Cylinder, Cone };

	UPROPERTY()
	TObjectPtr<USceneComponent> StageRoot;

	UPROPERTY()
	TObjectPtr<USceneComponent> Arena;

	UPROPERTY()
	TObjectPtr<USceneComponent> City;

	UPROPERTY()
	TObjectPtr<USceneComponent> CashSet;

	UPROPERTY()
	TObjectPtr<USkeletalMesh> GodzillaMesh;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMesh>> Shapes;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> PigmentMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> LightMaterial;

	UPROPERTY()
	TMap<uint32, TObjectPtr<UMaterialInstanceDynamic>> Paints;

	struct FPropBatch
	{
		USceneComponent* Parent;
		UStaticMesh* Mesh;
		FColor Color;
		UInstancedStaticMeshComponent* Instances;
	};

	struct FFilmEnemy
	{
		USceneComponent* Root = nullptr;
		USceneComponent* Top = nullptr;
		USceneComponent* Bottom = nullptr;
		UStaticMeshComponent* Tell = nullptr;
	};

	struct FBuilding
	{
		USceneComponent* Root;
		FVector Position;
		float ImpactTime;
		float Height;
	};

	USceneComponent* Director = nullptr;
	UPoseableMeshComponent* TitleDirector = nullptr;
	USceneComponent* VisibleSet = nullptr;
	USceneComponent* Upper = nullptr;
	USceneComponent* Lower = nullptr;
	USceneComponent* RightArm = nullptr;
	USceneComponent* LeftArm = nullptr;
	USceneComponent* Legs[2] = {};
	USceneComponent* BladeTrail = nullptr;
	FFilmEnemy FilmEnemies[DirectorsCut::ReelSize];
	UPoseableMeshComponent* Godzilla = nullptr;
	USceneComponent* KaijuRoot = nullptr;
	TArray<FTransform> GodzillaRestPose;
	TArray<FTransform> GodzillaComponentPose;
	TMap<FName, int32> GodzillaBones;
	float GodzillaScale = 1.0f;
	float GodzillaFootOffset = 0.0f;
	float GodzillaYaw = 0.0f;
	USceneComponent* Aircraft[4] = {};
	USceneComponent* Rotors[2] = {};
	UStaticMeshComponent* Tracers[8] = {};
	UStaticMeshComponent* Dust[20] = {};
	USceneComponent* MoneyBags[DirectorsCut::ReelSize] = {};
	TArray<FBuilding> Buildings;
	TArray<FPropBatch> PropBatches;

	void ShowSet(USceneComponent* ActiveSet);
	USceneComponent* Group(USceneComponent* Parent, FVector Location = FVector::ZeroVector);
	UMaterialInstanceDynamic* Paint(FColor Color, bool bGlow);
	UStaticMeshComponent* Shape(USceneComponent* Parent, EShape Kind, FVector Location, FVector Size,
		FColor Color, FRotator Rotation = FRotator::ZeroRotator, bool bGlow = false);
	void Prop(USceneComponent* Parent, EShape Kind, FVector Location, FVector Size,
		FColor Color, FRotator Rotation = FRotator::ZeroRotator);
	void PointLight(USceneComponent* Parent, FVector Location, FColor Color, float Intensity, float Radius);
	void BuildArena();
	void BuildDirector();
	void BuildTitleDirector();
	void BuildEnemy(int32 Index);
	void BuildCity();
	void BuildGodzilla();
	void BuildAircraft(int32 Index, bool bHelicopter);
	void BuildCashSet();
	void PoseGodzilla(float Timeline);
	void PoseAircraft(float Timeline);
	void PoseDestruction(float Timeline);
};
