#include "DirectorsCutHUD.h"

#include "CanvasItem.h"
#include "DirectorsCutGameMode.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Fonts/SlateFontInfo.h"
#include "Kismet/GameplayStatics.h"
#include "TextureResource.h"
#include "UObject/ConstructorHelpers.h"

using namespace DirectorsCut;

namespace
{
	const FColor Paper(245, 235, 211);
	const FColor Amber(246, 180, 87);
	const FColor Muted(172, 188, 184);
	const FColor Edge(63, 79, 80);
	const FColor Void(12, 17, 20);
	const FColor Red(246, 102, 77);
	const FName ActionButton(TEXT("CutAction"));
}

ADirectorsCutHUD::ADirectorsCutHUD()
{
	static ConstructorHelpers::FObjectFinder<UFont> Roboto(TEXT("/Engine/EngineFonts/Roboto.Roboto"));
	static ConstructorHelpers::FObjectFinder<UFont> DroidMono(TEXT("/Engine/EngineFonts/DroidSansMono.DroidSansMono"));
	RegularFont = Roboto.Object;
	MonoFont = DroidMono.Object;
}

void ADirectorsCutHUD::Box(float X, float Y, float W, float H, FColor Color)
{
	DrawRect(FLinearColor::FromSRGBColor(Color), X, Y, W, H);
}

void ADirectorsCutHUD::Text(const FString& Value, float X, float Y, float Size, FColor Color, bool bHeavy,
	float Align, bool bMono)
{
	const int32 FontSize = FMath::Max(8, FMath::RoundToInt(Size * UI));
	const FSlateFontInfo Font(bMono ? MonoFont.Get() : RegularFont.Get(), FontSize,
		bMono ? NAME_None : FName(bHeavy ? TEXT("Bold") : TEXT("Regular")));
	FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(Value), Font, FLinearColor::FromSRGBColor(Color));
	Item.bCentreX = Align == 0.5f;
	Canvas->DrawItem(Item);
}

void ADirectorsCutHUD::Button(const TCHAR* Label, float X, float Y, float W)
{
	const float H = 57.0f * UI;
	float MouseX = 0.0f;
	float MouseY = 0.0f;
	const bool bHover = PlayerOwner->GetMousePosition(MouseX, MouseY) &&
		MouseX >= X && MouseX <= X + W && MouseY >= Y && MouseY <= Y + H;
	Box(X, Y, W, H, bHover ? Paper : Amber);
	Box(X, Y + H - 3 * UI, W, 3 * UI, FColor(159, 99, 48));
	Text(Label, X + W * 0.5f, Y + 13 * UI, 20, Void, true, 0.5f);
	AddHitBox(FVector2D(X, Y), FVector2D(W, H), ActionButton, true);
}

void ADirectorsCutHUD::EnsureGrainTexture()
{
	if (GrainTexture)
	{
		return;
	}
	constexpr int32 Size = 256;
	UTexture2D* Texture = UTexture2D::CreateTransient(Size, Size, PF_B8G8R8A8);
	if (!Texture || !Texture->GetPlatformData() || Texture->GetPlatformData()->Mips.Num() == 0)
	{
		return;
	}
	Texture->SRGB = false;
	Texture->Filter = TF_Bilinear;
	Texture->AddressX = TA_Wrap;
	Texture->AddressY = TA_Wrap;
	Texture->CompressionSettings = TC_VectorDisplacementmap;
	Texture->MipGenSettings = TMGS_NoMipmaps;
	Texture->NeverStream = true;

	FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
	uint8* Pixels = static_cast<uint8*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
	FRandomStream Rng(2026);
	for (int32 Index = 0; Index < Size * Size; ++Index)
	{
		const uint8 Value = static_cast<uint8>(Rng.RandRange(0, 255));
		Pixels[Index * 4 + 0] = Value;
		Pixels[Index * 4 + 1] = Value;
		Pixels[Index * 4 + 2] = Value;
		Pixels[Index * 4 + 3] = static_cast<uint8>(Rng.RandRange(70, 200));
	}
	Mip.BulkData.Unlock();
	Texture->UpdateResource();
	GrainTexture = Texture;
}

void ADirectorsCutHUD::DrawFilmGrain(float X, float Y, float W, float H, float Opacity)
{
	EnsureGrainTexture();
	if (!Canvas || !GrainTexture || !GrainTexture->GetResource() || W <= 0.0f || H <= 0.0f)
	{
		return;
	}
	const float TileX = 3.2f;
	const float TileY = TileX * (H / W);
	const float U = FMath::FRand();
	const float V = FMath::FRand();
	FCanvasTileItem Tile(FVector2D(X, Y), GrainTexture->GetResource(), FVector2D(W, H),
		FVector2D(U, V), FVector2D(U + TileX, V + TileY), FLinearColor(1.0f, 1.0f, 1.0f, Opacity));
	Tile.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Tile);
}

void ADirectorsCutHUD::DrawHUD()
{
	Super::DrawHUD();
	const ADirectorsCutGameMode* Game = GetWorld()->GetAuthGameMode<ADirectorsCutGameMode>();
	if (!Canvas || !Game)
	{
		return;
	}
	Width = Canvas->ClipX;
	Height = Canvas->ClipY;
	UI = FMath::Min(Width / 1280.0f, Height / 720.0f);
	switch (Game->GetPhase())
	{
	case EPhase::Title:
		DrawTitle(false);
		break;
	case EPhase::TitleStart:
		DrawTitle(true);
		break;
	case EPhase::Combat:
	case EPhase::Wrap:
	case EPhase::ReelReady:
		DrawArena(*Game);
		break;
	case EPhase::Screening:
	case EPhase::GetPaid:
		DrawScreening(*Game);
		break;
	case EPhase::Payout:
	case EPhase::Reception:
		DrawPayout(*Game);
		break;
	}
	if (UGameplayStatics::IsGamePaused(GetWorld()))
	{
		DrawPaused();
	}
}

void ADirectorsCutHUD::NotifyHitBoxClick(FName BoxName)
{
	if (BoxName == ActionButton)
	{
		if (ADirectorsCutGameMode* Game = GetWorld()->GetAuthGameMode<ADirectorsCutGameMode>())
		{
			Game->Confirm();
		}
	}
	Super::NotifyHitBoxClick(BoxName);
}

void ADirectorsCutHUD::DrawTitle(bool bStarting)
{
	const float Left = 64 * UI;
	const float Top = Height * 0.135f;
	Box(0, 0, Width * 0.45f, Height, FColor(10, 15, 18, 247));
	for (int32 Index = 0; Index < 24; ++Index)
	{
		const float X = FMath::RoundToFloat(Width * (0.45f + 0.014f * Index));
		const float Next = FMath::RoundToFloat(Width * (0.45f + 0.014f * (Index + 1)));
		Box(X, 0, Next - X, Height, FColor(10, 15, 18, 220 - 9 * Index));
	}
	Box(0, 0, Width, 10 * UI, Void);
	Box(0, Height - 10 * UI, Width, 10 * UI, Void);
	Box(Left, Top, 46 * UI, 3 * UI, Amber);
	Text(TEXT("GODZILLA"), Left - 4 * UI, Top + 20 * UI, 76, Paper, true);
	Text(TEXT("STEVEN"), Left - 5 * UI, Top + 106 * UI, 100, Paper, true);
	Text(TEXT("Slice the film. Survive the take."), Left, Top + 270 * UI, 21, Paper);
	Text(TEXT("Complete the reel to screen the full movie."), Left, Top + 308 * UI, 16, Muted);
	if (bStarting)
	{
		Box(Left, Top + 370 * UI, 204 * UI, 57 * UI, Amber);
		Text(TEXT("Editing..."), Left + 102 * UI, Top + 383 * UI, 20, Void, true, 0.5f);
	}
	else
	{
		Button(TEXT("Start Editing"), Left, Top + 370 * UI, 204 * UI);
		Text(TEXT("ENTER"), Left + 223 * UI, Top + 390 * UI, 11, Muted, false, 0, true);
	}
	// Centered in the gap between the button's bottom edge and the controls line's glyph tops.
	const float GapCenter = (Top + 427 * UI + Height - 57 * UI) * 0.5f;
	Text(TEXT("As the movie director, where you point matters!"), Left, GapCenter - 13 * UI, 16, Muted);
	Text(TEXT("WASD  MOVE     CLICK  SLICE     SHIFT  DODGE     P  PAUSE     ESC  QUIT"),
		Left, Height - 61 * UI, 12, Muted, false, 0, true);
}

void ADirectorsCutHUD::DrawPaused()
{
	Box(0, 0, Width, Height, FColor(12, 17, 20, 180));
	const float PanelW = 360 * UI;
	const float PanelH = 150 * UI;
	const float PanelX = (Width - PanelW) * 0.5f;
	const float PanelY = (Height - PanelH) * 0.5f;
	Box(PanelX, PanelY, PanelW, PanelH, FColor(12, 17, 20, 236));
	Box(PanelX, PanelY, 5 * UI, PanelH, Amber);
	Text(TEXT("PAUSED"), Width * 0.5f, PanelY + 36 * UI, 42, Paper, true, 0.5f);
	Text(TEXT("P  RESUME      ESC  QUIT"), Width * 0.5f, PanelY + 100 * UI, 14, Muted, false, 0.5f, true);
}

void ADirectorsCutHUD::DrawArena(const ADirectorsCutGameMode& Game)
{
	Box(0, 0, Width, 63 * UI, FColor(12, 17, 20, 230));
	Box(0, 63 * UI, Width, 1 * UI, Edge);
	Box(26 * UI, 18 * UI, 5 * UI, 28 * UI, Amber);
	Text(TEXT("GODZILLA STEVEN"), 43 * UI, 17 * UI, 23, Paper, true);
	Text(TEXT("WASD  MOVE   /   CLICK  SLICE   /   SHIFT  DODGE   /   P  PAUSE"), Width * 0.53f, 24 * UI, 12, Muted, false, 0.5f, true);
	Text(TEXT("DODGE"), Width - 130 * UI, 14 * UI, 10, Muted, false, 0, true);
	Box(Width - 130 * UI, 37 * UI, 102 * UI, 5 * UI, Edge);
	Box(Width - 130 * UI, 37 * UI, 102 * UI * Game.GetDashCharge(), 5 * UI, Amber);
	DrawReel(Game);
	// Stack above the film strip: banner, then Play Reel, then the REEL count label.
	const float ButtonY = Height * 0.90f - 98 * UI;
	const float BannerY = ButtonY - 78 * UI;
	if (Game.GetPhase() == EPhase::Wrap || Game.GetPhase() == EPhase::ReelReady)
	{
		const bool bWon = Game.Won();
		const float W = 384 * UI;
		Box((Width - W) / 2, BannerY, W, 58 * UI, FColor(12, 17, 20, 236));
		Box((Width - W) / 2, BannerY, 5 * UI, 58 * UI, bWon ? Amber : Red);
		Text(bWon ? TEXT("REEL COMPLETE") : TEXT("DIRECTOR CUT"), Width / 2, BannerY + 12 * UI,
			24, Paper, true, 0.5f);
	}
	if (Game.GetPhase() == EPhase::ReelReady)
	{
		Button(TEXT("Play Reel"), Width / 2 - 110 * UI, ButtonY, 220 * UI);
	}
}

void ADirectorsCutHUD::DrawReel(const ADirectorsCutGameMode& Game)
{
	const float H = Height * 0.10f;
	const float Y = Height - H;
	const float Cell = Width / ReelSize;
	const float Rail = H * 0.18f;
	Box(0, Y, Width, H, Void);
	Box(0, Y, Width, 2 * UI, Edge);
	Text(FString::Printf(TEXT("REEL   %d / %d"), Game.GetReelCount(), ReelSize), 22 * UI, Y - 29 * UI,
		14, Paper, true, 0, true);
	for (int32 Index = 0; Index < ReelSize; ++Index)
	{
		const float X = Index * Cell;
		const bool bEarned = Game.HasShot(Index);
		const bool bFlash = Game.GetLastSplice() == Index && Game.GetSpliceFlash() > 0.24f;
		const FColor Stock = bFlash ? Paper : (bEarned ? Shots[Index].Color : Edge);
		const FColor Base = bEarned ? FColor(38, 53, 56) : FColor(24, 33, 38);
		Box(X + 2 * UI, Y + 3 * UI, Cell - 4 * UI, H - 6 * UI, Base);
		for (int32 Hole = 0; Hole < 9; ++Hole)
		{
			const float HoleX = X + Cell * (0.055f + Hole * 0.111f);
			Box(HoleX, Y + Rail * 0.45f, Cell * 0.047f, Rail * 0.45f, Stock);
			Box(HoleX, Height - Rail * 0.91f, Cell * 0.047f, Rail * 0.45f, Stock);
		}
		Box(X + 10 * UI, Y + Rail + 2 * UI, Cell - 20 * UI, H - 2 * Rail - 4 * UI, bEarned ? Stock : FColor(29, 39, 43));
		const FString Label = bEarned ? FString(Shots[Index].Title) : FString::Printf(TEXT("%02d"), Index + 1);
		Text(Label, X + Cell / 2, Y + H * 0.5f - 9 * UI, 12, bEarned ? Void : Muted, true, 0.5f, true);
	}
}

void ADirectorsCutHUD::DrawScreening(const ADirectorsCutGameMode& Game)
{
	const float Matte = Height * 0.105f;
	if (Game.GetPhase() == EPhase::Screening)
	{
		// Engine film-grain PP is easy to miss (texture streaming / scalability). Overlay is reliable.
		DrawFilmGrain(0.0f, Matte, Width, Height - Matte * 2.0f, 0.28f);
	}
	Box(0, 0, Width, Matte, Void);
	Box(0, Height - Matte, Width, Matte, Void);
	Box(26 * UI, Matte - 16 * UI, 5 * UI, 5 * UI, Amber);
	Text(TEXT("TOKYO, JAPAN"), 43 * UI, Matte - 27 * UI, 12, Paper, false, 0, true);
	if (Game.GetPhase() == EPhase::GetPaid)
	{
		Box(0, Matte, Width, Height - Matte * 2, FColor(12, 17, 20, 174));
		Text(TEXT("END OF REEL"), Width / 2, Height * 0.36f, 51, Paper, true, 0.5f);
		Text(FString::Printf(TEXT("%d of %d shots screened"), Game.GetReelCount(), ReelSize), Width / 2,
			Height * 0.46f, 16, Muted, false, 0.5f, true);
		Button(TEXT("Get Paid"), Width / 2 - 110 * UI, Height * 0.56f, 220 * UI);
		return;
	}
	const int32 Shot = Game.GetFilmShot();
	if (Shot == INDEX_NONE)
	{
		Text(TEXT("GODZILLA"), 53 * UI, Height - Matte - 113 * UI, 62, Paper, true);
		Text(TEXT("TOKYO UNDER SIEGE"), 58 * UI, Height - Matte - 39 * UI, 17, Amber, false, 0, true);
	}
	else
	{
		Text(FString::Printf(TEXT("%02d   %s"), Shot + 1, Shots[Shot].Title), 42 * UI, Height - Matte + 23 * UI,
			14, Paper, true, 0, true);
	}
	if (Game.GetShotTime() < 0.10f)
	{
		Box(0, Matte, Width, Height - Matte * 2, FColor(238, 223, 186, 105));
	}
}

void ADirectorsCutHUD::DrawPayout(const ADirectorsCutGameMode& Game)
{
	const bool bReception = Game.GetPhase() == EPhase::Reception;
	const float Left = 58 * UI;
	const float Top = Height * 0.18f;
	Box(0, 0, Width * 0.36f, Height, FColor(12, 17, 20, 244));
	for (int32 Index = 0; Index < 16; ++Index)
	{
		const float X = FMath::RoundToFloat(Width * (0.36f + 0.012f * Index));
		const float Next = FMath::RoundToFloat(Width * (0.36f + 0.012f * (Index + 1)));
		Box(X, 0, Next - X, Height, FColor(12, 17, 20, 207 - 12 * Index));
	}
	Box(Left, Top, 46 * UI, 3 * UI, Amber);
	Text(bReception ? (Game.Won() ? TEXT("Good Reception") : TEXT("Bad Reception")) : TEXT("Your Cut"),
		Left, Top + 20 * UI, 43, Paper, true);
	Text(FString::Printf(TEXT("%d%%"), Game.GetPayoutPercent()), Left, Top + 107 * UI, 95, Amber, true);
	Text(FString::Printf(TEXT("%d / %d MONEY BAGS"), Game.GetReelCount(), ReelSize), Left,
		Top + 228 * UI, 15, Muted, false, 0, true);
	if (bReception)
	{
		Button(TEXT("Play Again"), Left, Top + 298 * UI, 208 * UI);
	}
}
