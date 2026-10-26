#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "TabletOrdersComponent.h"
#include "Styling/SlateTypes.h"

class SBox;
class SScrollBox;

class STabletOrders : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(STabletOrders) : _Orders(nullptr) {}
        SLATE_ARGUMENT(UTabletOrdersComponent*, Orders)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    virtual ~STabletOrders() override;
    bool IsShowingDetails() const { return bCompact && SelectedOrder.IsValid(); }
    void ShowList();
    void OpenOrder(FGuid Id);
    virtual void Tick(const FGeometry& Geometry, double CurrentTime, float DeltaTime) override;
private:
    TWeakObjectPtr<UTabletOrdersComponent> Orders;
    FDelegateHandle ChangedHandle;
    FGuid SelectedOrder;
    TSharedPtr<SBox> Content;
    TSharedPtr<SScrollBox> OrderScroll;
    TSharedPtr<SScrollBox> RecipeScroll;
    FGuid DisplayedRecipeOrder;
    TMap<FGuid, float> RecipeOffsets;
    float SavedScrollOffset = 0;
    TMap<FName, FSlateBrush> Brushes;
    FButtonStyle ButtonStyle;
    FButtonStyle RowStyle;
    FScrollBarStyle ScrollBarStyle;
    bool bCompact = false;
    bool bRebuild = true;
    bool bOrderExpired = false;
    bool bConnectionTimedOut = false;
    double OpenedAt = 0;
    void Rebuild();
    TSharedRef<SWidget> List();
    TSharedRef<SWidget> Details(const FTabletCustomerOrder& Order);
    TSharedRef<SWidget> Row(const FTabletCustomerOrder& Order);
    TSharedRef<SWidget> State(const FText& Title, const FText& Message, bool Retry = false);
    TSharedRef<SWidget> Art(FName Key, float Width) const;
    FText Time(FGuid Id) const;
    bool IsUrgent(FGuid Id) const;
};
