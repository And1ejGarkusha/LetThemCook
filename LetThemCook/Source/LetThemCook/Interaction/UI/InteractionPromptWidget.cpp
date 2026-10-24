#include "InteractionPromptWidget.h"
#include "Interaction/InteractorComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

void UInteractionPromptWidget::NativeConstruct()
{
    Super::NativeConstruct();
    ForceVolatile(true);
}

int32 UInteractionPromptWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry,
    const FSlateRect& CullingRect, FSlateWindowElementList& Elements, int32 Layer,
    const FWidgetStyle& Style, bool bParentEnabled) const
{
    Layer = Super::NativePaint(Args, Geometry, CullingRect, Elements, Layer, Style, bParentEnabled);
    const APlayerController* PC = GetOwningPlayer();
    const APawn* Pawn = GetOwningPlayerPawn();
    if (!PC || !PC->IsLocalController() || PC->bShowMouseCursor || !Pawn) return Layer;
    const UInteractorComponent* User = Pawn->FindComponentByClass<UInteractorComponent>();
    if (!User) return Layer;

    const FVector2f Center = FVector2f(Geometry.GetLocalSize()) * 0.5f;
    const FLinearColor Tint = Style.GetColorAndOpacityTint();
    const FLinearColor Color = (User->GetFocusedTarget() || User->GetHeldItem() ? FLinearColor(0.2f, 1.f, 0.4f) : FLinearColor::White) * Tint;
    TArray<FVector2f> Horizontal = {Center - FVector2f(5.f, 0.f), Center + FVector2f(5.f, 0.f)};
    TArray<FVector2f> Vertical = {Center - FVector2f(0.f, 5.f), Center + FVector2f(0.f, 5.f)};
    FSlateDrawElement::MakeLines(Elements, ++Layer, Geometry.ToPaintGeometry(), Horizontal, ESlateDrawEffect::None, Color, true, 1.5f);
    FSlateDrawElement::MakeLines(Elements, Layer, Geometry.ToPaintGeometry(), Vertical, ESlateDrawEffect::None, Color, true, 1.5f);

    const FText Prompt = User->GetInteractionPrompt();
    if (!Prompt.IsEmpty())
    {
        const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 14);
        const FVector2f Size(FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Prompt, Font));
        const FVector2f Position(Center.X - Size.X * 0.5f, Center.Y + 24.f);
        FSlateDrawElement::MakeText(Elements, ++Layer,
            Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Position + FVector2f(1.f, 1.f))),
            Prompt, Font, ESlateDrawEffect::None, FLinearColor::Black * Tint);
        FSlateDrawElement::MakeText(Elements, ++Layer,
            Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Position)),
            Prompt, Font, ESlateDrawEffect::None, Color);
    }
    if (User->IsChargingThrow())
    {
        const FSlateBrush* Brush = FCoreStyle::Get().GetBrush("WhiteBrush");
        const FVector2f Position(Center.X - 90.f, Center.Y + 52.f);
        FSlateDrawElement::MakeBox(Elements, ++Layer,
            Geometry.ToPaintGeometry(FVector2f(180.f, 10.f), FSlateLayoutTransform(Position)),
            Brush, ESlateDrawEffect::None, FLinearColor(0.03f, 0.03f, 0.03f, 0.9f) * Tint);
        FSlateDrawElement::MakeBox(Elements, ++Layer,
            Geometry.ToPaintGeometry(FVector2f(180.f * User->GetThrowCharge(), 10.f), FSlateLayoutTransform(Position)),
            Brush, ESlateDrawEffect::None, FLinearColor(1.f, 0.65f, 0.05f) * Tint);
    }
    return Layer;
}
