#pragma once

#include "CoreMinimal.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Brushes/SlateColorBrush.h"
#include "Widgets/Text/STextBlock.h"

namespace TabletPalette
{
    inline FSlateFontInfo PixelFont(int32 Pixels, bool Bold = false)
    {
        return FCoreStyle::GetDefaultFontStyle(Bold ? "Bold" : "Regular", FMath::RoundToInt(Pixels * .75f));
    }
    inline FSlateFontInfo NumberFont(int32 Pixels)
    {
        FSlateFontInfo Font = FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("MonospacedText").Font;
        Font.Size = FMath::RoundToInt(Pixels * .75f);
        return Font;
    }
    inline const FLinearColor Paper = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("F6E5B9")));
    inline const FLinearColor Ink = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("302416")));
    inline const FLinearColor Muted = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("665437")));
    inline const FLinearColor Panel = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("E8CC91")));
    inline const FLinearColor Inset = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("F0D79D")));
    inline const FLinearColor Line = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("B9A277")));
    inline const FLinearColor Accent = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("A3311A")));
    inline const FLinearColor AccentHover = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("8E2916")));
    inline const FLinearColor ActionInk = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("FFF5DB")));
    inline const FLinearColor Urgent = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("A3311A")));
    inline const FLinearColor UrgentBg = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("F2CF9F")));

    inline TSharedRef<STextBlock> Label(const FText& Text, int32 Size = 16, bool Bold = false)
    {
        return SNew(STextBlock).Text(Text).ColorAndOpacity(Ink)
            .Font(FCoreStyle::GetDefaultFontStyle(Bold ? "Bold" : "Regular", Size));
    }
    inline FButtonStyle Button()
    {
        FButtonStyle Style;
        Style.SetNormal(FSlateColorBrush(Panel)).SetHovered(FSlateColorBrush(Line)).SetPressed(FSlateColorBrush(Paper));
        Style.SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
        return Style;
    }
    inline TSharedRef<STextBlock> Text(const FText& Value, int32 Pixels, bool Bold = false)
    {
        return SNew(STextBlock).Text(Value).ColorAndOpacity(Ink).Font(PixelFont(Pixels,Bold));
    }
}
