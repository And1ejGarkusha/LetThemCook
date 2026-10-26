#include "STabletOrders.h"
#include "TabletStyle.h"
#include "Engine/Texture2D.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"

using namespace TabletPalette;

void STabletOrders::Construct(const FArguments& Args)
{
    Orders = Args._Orders;
    OpenedAt = FPlatformTime::Seconds();
    ButtonStyle = Button();
    RowStyle = ButtonStyle;
    RowStyle.SetNormal(FSlateColorBrush(FLinearColor::Transparent))
        .SetHovered(FSlateColorBrush(FLinearColor(0,0,0,.04f)))
        .SetPressed(FSlateColorBrush(FLinearColor(0,0,0,.08f)));
    ScrollBarStyle = FCoreStyle::Get().GetWidgetStyle<FScrollBarStyle>("ScrollBar");
    ScrollBarStyle.SetNormalThumbImage(FSlateColorBrush(Line))
        .SetHoveredThumbImage(FSlateColorBrush(Accent)).SetDraggedThumbImage(FSlateColorBrush(Accent));
    if (Orders.IsValid())
    {
        const TCHAR* Keys[] = {TEXT("BunTop"), TEXT("BunBottom"), TEXT("Patty"), TEXT("Iceberg"), TEXT("Pickle"),
            TEXT("Tomato"), TEXT("Cheese"), TEXT("Fries"), TEXT("Cup")};
        for (const TCHAR* Key : Keys)
        {
            if (UTexture2D* Texture = Orders->GetArtwork(FName(Key)))
            {
                FSlateBrush Brush;
                Brush.SetResourceObject(Texture);
                Brush.ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
                Brush.DrawAs = ESlateBrushDrawType::Image;
                Brushes.Add(FName(Key), Brush);
            }
        }
        ChangedHandle = Orders->OnChanged.AddLambda([Weak = TWeakPtr<STabletOrders>(SharedThis(this))]()
        { if (auto Widget = Weak.Pin()) Widget->bRebuild = true; });
    }
    ChildSlot [SAssignNew(Content, SBox)];
    Rebuild();
}

STabletOrders::~STabletOrders() { if (Orders.IsValid()) Orders->OnChanged.Remove(ChangedHandle); }

void STabletOrders::Tick(const FGeometry& Geometry, double CurrentTime, float DeltaTime)
{
    SCompoundWidget::Tick(Geometry, CurrentTime, DeltaTime);
    const bool Compact = Geometry.GetLocalSize().X < 850;
    if (bCompact != Compact) { bCompact = Compact; bRebuild = true; }
    if (Orders.IsValid() && SelectedOrder.IsValid() && !Orders->FindOrder(SelectedOrder))
    { SelectedOrder.Invalidate(); bOrderExpired = true; bRebuild = true; }
    if (Orders.IsValid() && !Orders->IsReady() && !bConnectionTimedOut && FPlatformTime::Seconds() - OpenedAt > 10)
    { bConnectionTimedOut = true; bRebuild = true; }
    if (bRebuild) Rebuild();
}

void STabletOrders::ShowList() { SelectedOrder.Invalidate(); bRebuild = true; }
void STabletOrders::OpenOrder(FGuid Id)
{
    if (Orders.IsValid() && Orders->FindOrder(Id)) { SelectedOrder = Id; bRebuild = true; bOrderExpired = false; }
}

FText STabletOrders::Time(FGuid Id) const
{
    const auto* Order = Orders.IsValid() ? Orders->FindOrder(Id) : nullptr;
    const int32 Seconds = Order ? Orders->GetSecondsRemaining(*Order) : 0;
    return FText::FromString(FString::Printf(TEXT("%02d:%02d"), Seconds / 60, Seconds % 60));
}
bool STabletOrders::IsUrgent(FGuid Id) const
{
    const auto* Order = Orders.IsValid() ? Orders->FindOrder(Id) : nullptr;
    return Order && Orders->GetSecondsRemaining(*Order) <= 30;
}

TSharedRef<SWidget> STabletOrders::Art(FName Key, float Width) const
{
    const FSlateBrush* Brush = Brushes.Find(Key);
    const float Height = Brush ? Width * Brush->ImageSize.Y / Brush->ImageSize.X : 24;
    return SNew(SBox).WidthOverride(Width).HeightOverride(Height)
        [SNew(SImage).Image(Brush).Visibility(Brush ? EVisibility::HitTestInvisible : EVisibility::Collapsed)];
}

void STabletOrders::Rebuild()
{
    if (OrderScroll.IsValid()) SavedScrollOffset = OrderScroll->GetScrollOffset();
    if (RecipeScroll.IsValid() && DisplayedRecipeOrder.IsValid()) RecipeOffsets.Add(DisplayedRecipeOrder, RecipeScroll->GetScrollOffset());
    for (auto It = RecipeOffsets.CreateIterator(); It; ++It)
        if (!Orders.IsValid() || !Orders->FindOrder(It.Key())) It.RemoveCurrent();
    RecipeScroll.Reset();
    DisplayedRecipeOrder.Invalidate();
    bRebuild = false;
    if (!Orders.IsValid()) { Content->SetContent(State(FText::FromString(TEXT("Не удалось получить заказы")), FText::FromString(TEXT("Нет подключения к списку заказов.")))); return; }
    if (!Orders->IsReady())
    {
        const bool TimedOut = FPlatformTime::Seconds() - OpenedAt > 10;
        Content->SetContent(State(FText::FromString(TimedOut ? TEXT("Не удалось получить заказы") : TEXT("Загружаем заказы…")),
            FText::FromString(TimedOut ? TEXT("Проверьте подключение и попробуйте ещё раз.") : TEXT("Получаем список заказов кухни.")), TimedOut));
        return;
    }
    const auto* Order = Orders->FindOrder(SelectedOrder);
    if (bCompact && Order) Content->SetContent(Details(*Order));
    else Content->SetContent(List());
}

TSharedRef<SWidget> STabletOrders::State(const FText& Title, const FText& Message, bool Retry)
{
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()[Text(FText::FromString(TEXT("Заказы")), 40, true)]
        + SVerticalBox::Slot().FillHeight(1).HAlign(HAlign_Center).VAlign(VAlign_Center)
        [SNew(SBox).MaxDesiredWidth(620)
            [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,16)[Text(Title, 28, true)]
                + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Message).Font(PixelFont(17)).ColorAndOpacity(Muted).AutoWrapText(true)]
                + SVerticalBox::Slot().AutoHeight().Padding(0,24,0,0)
                [SNew(SButton).ButtonStyle(&ButtonStyle).ContentPadding(FMargin(24,16))
                    .Visibility(Retry ? EVisibility::Visible : EVisibility::Collapsed)
                    .OnClicked_Lambda([this]() { if (Orders.IsValid()) Orders->RefreshOrders(); OpenedAt = FPlatformTime::Seconds(); bConnectionTimedOut = false; bRebuild = true; return FReply::Handled(); })
                    [Text(FText::FromString(TEXT("Повторить")), 17, true)]]]];
}

TSharedRef<SWidget> STabletOrders::Row(const FTabletCustomerOrder& Order)
{
    const FGuid Id = Order.Id;
    auto Number = Text(FText::FromString(FString::Printf(TEXT("№ %03d"), Order.Number)), 20, true);
    Number->SetFont(NumberFont(20));
    Number->SetColorAndOpacity(Accent);
    auto Clock = SNew(STextBlock).Font(NumberFont(20)).Justification(ETextJustify::Right)
        .ColorAndOpacity_Lambda([this,Id]() { return FSlateColor(IsUrgent(Id) ? Urgent : Ink); })
        .Text_Lambda([this,Id]() { return Time(Id); });
    return SNew(SBox).MinDesiredHeight(166)
        [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).Padding_Lambda([this,Id]() { return FMargin(SelectedOrder == Id ? 2 : 1); })
            .BorderBackgroundColor_Lambda([this,Id]() { return SelectedOrder == Id ? Accent : Line; })
            [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).Padding(0)
                .BorderBackgroundColor_Lambda([this,Id]() { return SelectedOrder == Id ? Panel : Paper; })
                [SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                        [SNew(SBox).HeightOverride(4)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).Padding(0)
                            .BorderBackgroundColor_Lambda([this,Id]() { return SelectedOrder == Id ? Accent : Ink; })]]
                    + SVerticalBox::Slot().FillHeight(1)
                        [SNew(SButton).ButtonStyle(&RowStyle).ContentPadding(16).HAlign(HAlign_Fill).VAlign(VAlign_Fill)
                            .OnClicked_Lambda([this,Id]() { OpenOrder(Id); return FReply::Handled(); })
                            [SNew(SVerticalBox)
                                + SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
                                    + SHorizontalBox::Slot().FillWidth(1)[Number]
                                    + SHorizontalBox::Slot().AutoWidth()[Clock]]
                                + SVerticalBox::Slot().AutoHeight().Padding(0,10,0,6)
                                    [SNew(STextBlock).Text(UTabletOrdersComponent::OrderTitle(Order)).Font(PixelFont(20,true)).ColorAndOpacity(Ink).AutoWrapText(true)]
                                + SVerticalBox::Slot().FillHeight(1)
                                    [SNew(STextBlock).Font(PixelFont(16)).ColorAndOpacity(Muted).AutoWrapText(true)
                                        .Text_Lambda([this,Id]() { const auto* Current = Orders.IsValid() ? Orders->FindOrder(Id) : nullptr;
                                            if (!Current) return FText::GetEmpty();
                                            FString Caption = UTabletOrdersComponent::Description(*Current).ToString();
                                            if (IsUrgent(Id)) Caption += TEXT(" · мало времени");
                                            return FText::FromString(Caption); })]
                                + SVerticalBox::Slot().AutoHeight().Padding(0,10,0,0)
                                    [SNew(SHorizontalBox)
                                        + SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Text(FText::AsNumber(Order.Payment)).ColorAndOpacity(Muted).Font(NumberFont(18))]
                                        + SHorizontalBox::Slot().AutoWidth()[Text(FText::FromString(TEXT("›")),20)]]
                            ]]
                ]]];
}

TSharedRef<SWidget> STabletOrders::List()
{
    if (Orders->GetOrders().IsEmpty()) return State(FText::FromString(TEXT("Пока нет заказов")), FText::FromString(TEXT("Новый заказ появится здесь, когда клиент дойдёт до стойки.")));
    if (!bCompact && !Orders->FindOrder(SelectedOrder)) SelectedOrder = Orders->GetOrders()[0].Id;
    auto Rows = SNew(SVerticalBox);
    for (const auto& Order : Orders->GetOrders()) Rows->AddSlot().AutoHeight().Padding(3,3,3,13)[Row(Order)];
    SAssignNew(OrderScroll,SScrollBox).ScrollBarStyle(&ScrollBarStyle).ScrollBarAlwaysVisible(true)
        .OnUserScrolled_Lambda([this](float Offset) { SavedScrollOffset = Offset; })
        + SScrollBox::Slot()[Rows];
    OrderScroll->SetScrollOffset(SavedScrollOffset);

    TSharedRef<SWidget> Body = OrderScroll.ToSharedRef();
    if (!bCompact)
    {
        Body = SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().Padding(0,0,24,0)
                [SNew(SBox).WidthOverride(264)[OrderScroll.ToSharedRef()]]
            + SHorizontalBox::Slot().AutoWidth().Padding(0,0,28,0)
                [SNew(SBox).WidthOverride(1)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Line).Padding(0)]]
            + SHorizontalBox::Slot().FillWidth(1)[Details(*Orders->FindOrder(SelectedOrder))];
    }
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,24)
            [SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,18,0)[Text(FText::FromString(TEXT("Заказы")),32,true)]
                + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
                    [SNew(STextBlock).ColorAndOpacity(Muted).Font(PixelFont(16)).AutoWrapText(true)
                        .Text(FText::Format(FText::FromString(TEXT("Активных: {0} · оплата при выдаче")),FText::AsNumber(Orders->GetOrders().Num())))]]
        + SVerticalBox::Slot().FillHeight(1)[Body]
        + SVerticalBox::Slot().AutoHeight().Padding(0,18,0,0)
            [SNew(STextBlock).Font(PixelFont(16)).ColorAndOpacity(Muted).AutoWrapText(true)
                .Text(FText::FromString(bOrderExpired ? TEXT("Заказ больше не активен. Список обновлён.") : TEXT("Клиент ждёт у стойки выдачи.")))];
}

TSharedRef<SWidget> STabletOrders::Details(const FTabletCustomerOrder& Order)
{
    const FGuid Id = Order.Id;
    const auto Items = UTabletOrdersComponent::GetItems(Order);
    auto Dishes = SNew(SVerticalBox);
    for (int32 Index = 0; Index < Items.Num(); ++Index)
    {
        const auto& Item = Items[Index];
        const auto Parts = UTabletOrdersComponent::ItemRecipe(Item);
        auto Stack = SNew(SVerticalBox);
        for (const auto& Part : Parts) Stack->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0,0,0,6)
            [Art(Part.Artwork, Item.Dish == ETabletDish::Burger ? 260.f : 300.f)];
        auto Picture = SNew(SBox).HeightOverride(240)
            [SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)[Stack]];
        auto Composition = SNew(SVerticalBox);
        Composition->AddSlot().AutoHeight().Padding(0,0,0,12)[Text(FText::FromString(TEXT("ЧТО ВНУТРИ")),24,true)];
        const FName ListOrder[] = {TEXT("BunTop"), TEXT("Patty"), TEXT("Iceberg"), TEXT("Pickle"), TEXT("Tomato"), TEXT("Cheese"), TEXT("Fries"), TEXT("Cup")};
        TArray<const FTabletRecipePart*> ListedParts;
        for (FName Key : ListOrder) if (const auto* Part = Parts.FindByPredicate([Key](const FTabletRecipePart& P) { return P.Artwork == Key; }))
            ListedParts.Add(Part);
        auto IngredientGrid = SNew(SUniformGridPanel).SlotPadding(FMargin(0,0,16,0));
        const int32 RowCount = FMath::Max(1, FMath::DivideAndRoundUp(ListedParts.Num(), 2));
        for (int32 PartIndex = 0; PartIndex < ListedParts.Num(); ++PartIndex)
        {
            const auto* Part = ListedParts[PartIndex];
            IngredientGrid->AddSlot(PartIndex / RowCount, PartIndex % RowCount)
                [SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                        [SNew(SBox).MinDesiredHeight(68)[SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,10,0)
                                [SNew(SBox).WidthOverride(36).HeightOverride(36)[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[Art(Part->Artwork,36)]]]
                            + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
                                [SNew(SVerticalBox)
                                    + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Part->Name).ColorAndOpacity(Ink).Font(PixelFont(24,true)).AutoWrapText(true)]
                                    + SVerticalBox::Slot().AutoHeight().Padding(0,5,0,0)[SNew(STextBlock).Text(Part->Description).AutoWrapText(true).ColorAndOpacity(Muted).Font(PixelFont(20))]]]]
                    + SVerticalBox::Slot().AutoHeight()
                        [SNew(SBox).HeightOverride(1)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Line).Padding(0)]]];
        }
        Composition->AddSlot().AutoHeight()[IngredientGrid];
        TSharedRef<SWidget> DishBody = SNullWidget::NullWidget;
        if (bCompact) DishBody = SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[Picture]
            + SVerticalBox::Slot().AutoHeight().Padding(0,20,0,0)[Composition];
        else DishBody = SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(.32f).VAlign(VAlign_Top).Padding(0,0,24,0)[Picture]
            + SHorizontalBox::Slot().FillWidth(.68f).VAlign(VAlign_Top)[Composition];
        if (Index > 0) Dishes->AddSlot().AutoHeight().Padding(0,12,0,28)
            [SNew(SBox).HeightOverride(2)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Line).Padding(0)]];
        if (Index > 0 || Item.Quantity > 1) Dishes->AddSlot().AutoHeight().Padding(0,0,0,20)
            [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                    [Text(FText::Format(FText::FromString(TEXT("{0} × {1}")), UTabletOrdersComponent::DishName(Item.Dish), FText::AsNumber(Item.Quantity)),28,true)]
                + SVerticalBox::Slot().AutoHeight().Padding(0,6,0,0)
                    [SNew(STextBlock).Text(UTabletOrdersComponent::ItemDescription(Item)).ColorAndOpacity(Muted).Font(PixelFont(18)).AutoWrapText(true)]];
        Dishes->AddSlot().AutoHeight().Padding(0,0,12,28)[DishBody];
    }
    SAssignNew(RecipeScroll,SScrollBox).ScrollBarStyle(&ScrollBarStyle).ScrollBarAlwaysVisible(true).AllowOverscroll(EAllowOverscroll::No)
        .OnUserScrolled_Lambda([this,Id](float Offset) { RecipeOffsets.Add(Id,Offset); })
        + SScrollBox::Slot()[Dishes];
    DisplayedRecipeOrder = Id;
    RecipeScroll->SetScrollOffset(RecipeOffsets.FindRef(Id));
    TSharedRef<SWidget> RecipeBody = RecipeScroll.ToSharedRef();

    auto Meta = [this,Id](const TCHAR* Title, FText Value, bool Clock) -> TSharedRef<SWidget>
    {
        return SNew(SBox).MinDesiredWidth(86)
            [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).ColorAndOpacity(Muted).Font(PixelFont(13))
                    .Text_Lambda([this,Id,Clock,Title]() { return FText::FromString(Clock && IsUrgent(Id) ? TEXT("МАЛО ВРЕМЕНИ") : Title); })]
                + SVerticalBox::Slot().AutoHeight().Padding(0,6,0,0)
                    [SNew(STextBlock).Font(NumberFont(24))
                        .ColorAndOpacity_Lambda([this,Id,Clock]() { return FSlateColor(Clock && IsUrgent(Id) ? Urgent : Ink); })
                        .Text_Lambda([this,Id,Clock,Value]() { return Clock ? Time(Id) : Value; })]];
    };
    auto Identity = SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("ЗАКАЗ / %03d"),Order.Number))).ColorAndOpacity(Muted).Font(PixelFont(14))]
        + SVerticalBox::Slot().AutoHeight().Padding(0,8,0,0)[SNew(STextBlock).Text(UTabletOrdersComponent::OrderTitle(Order)).ColorAndOpacity(Ink).Font(PixelFont(32,true)).AutoWrapText(true)]
        + SVerticalBox::Slot().AutoHeight().Padding(0,8,0,0)[SNew(STextBlock).Text(UTabletOrdersComponent::Description(Order)).ColorAndOpacity(Muted).Font(PixelFont(16)).AutoWrapText(true)];
    auto Metadata = SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().Padding(0,0,18,0)[Meta(TEXT("ОПЛАТА"),FText::AsNumber(Order.Payment),false)]
        + SHorizontalBox::Slot().AutoWidth()[Meta(TEXT("ОСТАЛОСЬ"),FText::GetEmpty(),true)];
    TSharedRef<SWidget> Heading = SNullWidget::NullWidget;
    if (bCompact) Heading = SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()[Identity]
        + SVerticalBox::Slot().AutoHeight().Padding(0,16,0,0)[Metadata];
    else Heading = SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1).Padding(0,0,18,0)[Identity]
        + SHorizontalBox::Slot().AutoWidth()[Metadata];
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,20)[Heading]
        + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,20)
            [SNew(SBox).HeightOverride(1)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Line).Padding(0)]]
        + SVerticalBox::Slot().FillHeight(1)[RecipeBody];
}
