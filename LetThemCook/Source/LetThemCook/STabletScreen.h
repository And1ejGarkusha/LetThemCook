#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "TabletShopComponent.h"
#include "UObject/StrongObjectPtr.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
class UTexture2D;
class SWidgetSwitcher;
class SVerticalBox;
class STabletScreen : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(STabletScreen) {}
        SLATE_ARGUMENT(UTabletShopComponent*, Shop)
        SLATE_EVENT(FSimpleDelegate, OnClose)
        SLATE_EVENT(FOnKeyDown, OnCommandKeyDown)
        SLATE_EVENT(FOnKeyDown, OnCommandKeyUp)
        SLATE_ATTRIBUTE(FText, CloseHint)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    virtual ~STabletScreen() override;
    virtual bool SupportsKeyboardFocus() const override { return true; }
    virtual FReply OnPreviewKeyDown(const FGeometry&, const FKeyEvent&) override;
    virtual FReply OnKeyUp(const FGeometry&, const FKeyEvent&) override;
    void ShowIngredients();
private:
    TWeakObjectPtr<UTabletShopComponent> Shop;
    FSimpleDelegate Close;
    FOnKeyDown CommandKeyDown;
    FOnKeyDown CommandKeyUp;
    FDelegateHandle ResultHandle;
    FDelegateHandle CatalogHandle;
    TMap<FName, int32> Quantities;
    TSharedPtr<SWidgetSwitcher> Pages;
    TSharedPtr<SVerticalBox> ProductRows;
    TArray<TStrongObjectPtr<UTexture2D>> ProductTextures;
    TArray<TSharedPtr<FSlateBrush>> ProductBrushes;
    TStrongObjectPtr<UTexture2D> IngredientsTexture, UpgradesTexture;
    FSlateBrush IngredientsBrush, UpgradesBrush;
    FButtonStyle ButtonStyle, ActionStyle;
    FEditableTextBoxStyle InputStyle;
    bool bWaiting = false;
    bool bSuccess = true;
    FText Status;
    void RefreshProducts();
    int32 QuantityFor(FName Id) const;
    int64 Total() const;
    FText Summary() const;
    bool CanBuy() const;
    FReply Buy();
    FReply CloseScreen();
};
