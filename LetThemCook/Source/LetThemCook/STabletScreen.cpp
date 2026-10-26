#include "STabletScreen.h"
#include "STabletOrders.h"
#include "TabletStyle.h"
#include "TabletOrdersComponent.h"
#include "InputCoreTypes.h"
#include "Engine/Texture2D.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateColorBrush.h"
void STabletScreen::Construct(const FArguments& Args)
{
    using namespace TabletPalette;
    Shop = Args._Shop; Orders = Args._Orders; Close = Args._OnClose;
    ButtonStyle.SetNormal(FSlateColorBrush(Panel)).SetHovered(FSlateColorBrush(Line)).SetPressed(FSlateColorBrush(Paper));
    ButtonStyle.SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
    TileStyle = ButtonStyle;
    LockedStyle = ButtonStyle;
    LockedStyle.SetNormal(FSlateColorBrush(Inset)).SetHovered(FSlateColorBrush(Inset)).SetPressed(FSlateColorBrush(Inset));
    ActionStyle = ButtonStyle;
    ActionStyle.SetNormal(FSlateColorBrush(Accent)).SetHovered(FSlateColorBrush(AccentHover)).SetPressed(FSlateColorBrush(AccentHover));
    InputStyle = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
    InputStyle.SetBackgroundImageNormal(FSlateColorBrush(Paper)).SetBackgroundImageHovered(FSlateColorBrush(Paper)).SetBackgroundImageFocused(FSlateColorBrush(Paper)).SetBackgroundImageReadOnly(FSlateColorBrush(Paper));
    InputStyle.SetForegroundColor(Ink).SetPadding(FMargin(4, 6));
    IngredientsTexture.Reset(LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/Tablet/T_Ingredients.T_Ingredients")));
    UpgradesTexture.Reset(LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/Tablet/T_Upgrades.T_Upgrades")));
    IngredientsBrush.SetResourceObject(IngredientsTexture.Get()); IngredientsBrush.ImageSize = FVector2D(64,64); IngredientsBrush.DrawAs = ESlateBrushDrawType::Image;
    UpgradesBrush.SetResourceObject(UpgradesTexture.Get()); UpgradesBrush.ImageSize = FVector2D(64,64); UpgradesBrush.DrawAs = ESlateBrushDrawType::Image;
    auto LoadBrush = [this](FSlateBrush& Brush, FName Key)
    {
        if (Orders.IsValid()) if (UTexture2D* Texture = Orders->GetArtwork(Key))
        {
            Brush.SetResourceObject(Texture);
            Brush.ImageSize = FVector2D(Texture->GetSizeX(),Texture->GetSizeY());
            Brush.DrawAs = ESlateBrushDrawType::Image;
        }
    };
    LoadBrush(OrdersBrush, TEXT("Orders")); LoadBrush(LockedBrush, TEXT("Locked")); LoadBrush(BunBrush, TEXT("Iceberg"));
    if (Shop.IsValid())
    {
        Status = Shop->GetLastPurchaseMessage();
        ResultHandle = Shop->OnPurchaseResult.AddLambda([Weak = TWeakPtr<STabletScreen>(SharedThis(this))](bool Success, const FText& Message)
        {
            if (auto Screen = Weak.Pin()) { Screen->bWaiting=false; Screen->bSuccess=Success; Screen->Status=Message; if (Success) Screen->Quantities.Empty(); }
        });
    }
    auto Tile = [this](const FText& Title, const FSlateBrush* Brush, int32 Page) -> TSharedRef<SWidget>
    {
        const bool Locked = Page < 0;
        return SNew(SBox).WidthOverride(320).HeightOverride(320)
        [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Line).Padding(1)
        [SNew(SButton).ButtonStyle(Locked ? &LockedStyle : &TileStyle).ContentPadding(24)
            .OnClicked_Lambda([this,Page](){ if (Page == 2) ShowOrders(); else if (Page == 1) ShowIngredients(); return FReply::Handled(); })
            [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Title).ColorAndOpacity(Locked ? Muted : Ink).Font(PixelFont(28,true))]
                + SVerticalBox::Slot().FillHeight(1).HAlign(HAlign_Center).VAlign(VAlign_Center)
                [SNew(SBox).WidthOverride(Page == 1 ? 195 : 112).HeightOverride(Page == 1 ? 47.45f : 112)[SNew(SImage).Image(Brush)]]
                + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).ColorAndOpacity(Locked ? Muted : Ink).Font(PixelFont(17))
                    .Text_Lambda([this,Page]() { if (Page < 0) return FText::FromString(TEXT("Заблокировано"));
                        if (Page == 1) return FText::FromString(TEXT("Закупка продуктов"));
                        const int32 Count = Orders.IsValid() ? Orders->GetOrders().Num() : 0;
                        return FText::FromString(Count > 0 ? FString::Printf(TEXT("Ждут выдачи: %d"), Count) : TEXT("Пока нет заказов")); })]
            ]]];
    };
    TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
    if (Shop.IsValid()) for (const FTabletProduct& P : Shop->GetProducts())
    {
        const FName Id=P.Id;
        auto Arrow = [this, Id](const TCHAR* Caption, int32 Delta)->TSharedRef<SWidget>
        {
            return SNew(SBox).WidthOverride(34).HeightOverride(34)
            [SNew(SButton).ButtonStyle(&ButtonStyle).IsFocusable(false).HAlign(HAlign_Center)
            .IsEnabled_Lambda([this](){return !bWaiting;})
            .OnClicked_Lambda([this,Id,Delta](){Quantities.Add(Id,FMath::Clamp(QuantityFor(Id)+Delta,0,100));return FReply::Handled();})
            [TabletPalette::Label(FText::FromString(Caption),18,true)]];
        };
        Rows->AddSlot().AutoHeight().Padding(0,0,0,5)
        [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Line).Padding(1)
        [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Panel).Padding(FMargin(12,6))
        [SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Label(P.Name,16)]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6,0,16,0)[Label(FText::Format(FText::FromString(TEXT("{0} / шт.")), FText::AsNumber(P.Price)),14)]
            + SHorizontalBox::Slot().AutoWidth()[Arrow(TEXT("‹"),-1)]
            + SHorizontalBox::Slot().AutoWidth().Padding(3,0)
            [SNew(SBox).WidthOverride(48)[SNew(SEditableTextBox).Style(&InputStyle).Justification(ETextJustify::Center)
                .IsReadOnly_Lambda([this](){return bWaiting;})
                .Text_Lambda([this,Id](){return FText::AsNumber(QuantityFor(Id));})
                .OnVerifyTextChanged_Lambda([](const FText& Text,FText& Error){const FString S=Text.ToString(); bool Valid=S.Len()<=3;for(TCHAR C:S)Valid&=(C>=TEXT('0')&&C<=TEXT('9'));if(!Valid){Error=FText::FromString(TEXT("Введите число от 0 до 100"));return false;}return true;})
                .OnTextChanged_Lambda([this,Id](const FText& Text){if(!bWaiting) Quantities.Add(Id,FMath::Clamp(FCString::Atoi(*Text.ToString()),0,100));})
                .OnTextCommitted_Lambda([this,Id](const FText& Text,ETextCommit::Type){if(!bWaiting) Quantities.Add(Id,FMath::Clamp(FCString::Atoi(*Text.ToString()),0,100));})]]
            + SHorizontalBox::Slot().AutoWidth()[Arrow(TEXT("›"),1)]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(14,0,0,0)
            [SNew(SBox).WidthOverride(65)[SNew(STextBlock).Justification(ETextJustify::Right).ColorAndOpacity(Ink).Font(FCoreStyle::GetDefaultFontStyle("Bold",16)).Text_Lambda([this,Id,Price=P.Price](){return FText::AsNumber(int64(Price)*QuantityFor(Id));})]]
        ]]];
    }
    ChildSlot
    [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Paper).Padding(32)
    [SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,24)
        [SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
                [SNew(SButton).ButtonStyle(FCoreStyle::Get(),"NoBorder").ContentPadding(0)
                    .OnClicked_Lambda([this]() { if (Pages->GetActiveWidgetIndex() == 2 && OrdersScreen->IsShowingDetails()) OrdersScreen->ShowList(); else Pages->SetActiveWidgetIndex(0); return FReply::Handled(); })
                    [SNew(STextBlock).ColorAndOpacity(Muted).Font(PixelFont(15))
                        .Text_Lambda([this]() { if (!Pages.IsValid() || Pages->GetActiveWidgetIndex() == 0) return FText::FromString(TEXT("КУХОННЫЙ ПЛАНШЕТ"));
                            return FText::FromString(Pages->GetActiveWidgetIndex() == 2 && OrdersScreen->IsShowingDetails() ? TEXT("← Заказы") : TEXT("← Приложения")); })]]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,24,0)
            [SNew(STextBlock).ColorAndOpacity(Ink).Font(PixelFont(17)).Text_Lambda([this](){return FText::Format(FText::FromString(TEXT("Баланс: {0}")),FText::AsNumber(Shop.IsValid()?Shop->GetBalance():0));})]
            + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).ButtonStyle(&ButtonStyle).ContentPadding(FMargin(12,8)).IsFocusable(false).OnClicked(this,&STabletScreen::CloseScreen)[TabletPalette::Text(FText::FromString(TEXT("T / Esc — убрать")),15)]]
        ]
        + SVerticalBox::Slot().FillHeight(1)
        [SAssignNew(Pages,SWidgetSwitcher)
            + SWidgetSwitcher::Slot()
            [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[TabletPalette::Text(FText::FromString(TEXT("Приложения")),40,true)]
                + SVerticalBox::Slot().AutoHeight().Padding(0,4,0,24)[SNew(STextBlock).Text(FText::FromString(TEXT("Кухня · рабочий планшет"))).ColorAndOpacity(Muted).Font(PixelFont(17))]
                + SVerticalBox::Slot().FillHeight(1).VAlign(VAlign_Center)
                    [SNew(SScrollBox)+SScrollBox::Slot()[SNew(SWrapBox).UseAllottedSize(true).HAlign(HAlign_Center).InnerSlotPadding(FVector2D(24,24))
                        + SWrapBox::Slot()[Tile(FText::FromString(TEXT("Заказы")),&OrdersBrush,2)]
                        + SWrapBox::Slot()[Tile(FText::FromString(TEXT("Ингредиенты")),&BunBrush,1)]
                        + SWrapBox::Slot()[Tile(FText::FromString(TEXT("Улучшения")),&LockedBrush,-1)]]]
                + SVerticalBox::Slot().AutoHeight().Padding(0,24,0,0)[TabletPalette::Text(FText::FromString(TEXT("Выбери приложение.")),17)]
            ]
            + SWidgetSwitcher::Slot()
            [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,20)
                [SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().Padding(0,0,20,0)[SNew(SButton).ButtonStyle(&ButtonStyle).ContentPadding(FMargin(12,8)).IsFocusable(false).OnClicked_Lambda([this](){Pages->SetActiveWidgetIndex(0);return FReply::Handled();})[Label(FText::FromString(TEXT("← Назад")),14)]]
                    + SHorizontalBox::Slot().FillWidth(1)[Label(FText::FromString(TEXT("Закупка продуктов")),28,true)]]
                + SVerticalBox::Slot().FillHeight(1)
                [SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(0.68f).Padding(0,0,24,0)[SNew(SScrollBox)+SScrollBox::Slot()[Rows]]
                    + SHorizontalBox::Slot().FillWidth(0.32f)
                    [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Panel).Padding(20)
                    [SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,18)[Label(FText::FromString(TEXT("Заказ")),24,true)]
                        + SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot()[SNew(STextBlock).ColorAndOpacity(Ink).Font(FCoreStyle::GetDefaultFontStyle("Regular",16)).AutoWrapText(true).Text(this,&STabletScreen::Summary)]]
                        + SVerticalBox::Slot().AutoHeight().Padding(0,14)[SNew(STextBlock).ColorAndOpacity(Ink).Font(FCoreStyle::GetDefaultFontStyle("Bold",24)).Text_Lambda([this](){return FText::Format(FText::FromString(TEXT("Итого: {0}")),FText::AsNumber(Total()));})]
                        + SVerticalBox::Slot().AutoHeight()
                        [SNew(SButton).ButtonStyle(&ActionStyle).IsFocusable(false).ContentPadding(FMargin(12,14)).HAlign(HAlign_Center).IsEnabled(this,&STabletScreen::CanBuy).OnClicked(this,&STabletScreen::Buy)
                        [SNew(STextBlock).ColorAndOpacity(ActionInk).Font(FCoreStyle::GetDefaultFontStyle("Bold",16)).Text_Lambda([this](){return FText::FromString(bWaiting?TEXT("Оформляем…"):TEXT("Заказать доставку"));})]]
                    ]]
                ]
            ]
            + SWidgetSwitcher::Slot()[SAssignNew(OrdersScreen,STabletOrders).Orders(Orders.Get())]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0,16,0,0)
        [SNew(STextBlock).Visibility_Lambda([this]() { return Pages->GetActiveWidgetIndex() == 1 ? EVisibility::Visible : EVisibility::Collapsed; }).ColorAndOpacity_Lambda([this](){return bSuccess?FSlateColor(TabletPalette::Muted):FSlateColor(TabletPalette::Urgent);}).Text_Lambda([this](){return Shop.IsValid()&&Total()>Shop->GetBalance()?FText::FromString(TEXT("Недостаточно денег для этого заказа.")):Status;})]
    ]];
}
STabletScreen::~STabletScreen(){if(Shop.IsValid())Shop->OnPurchaseResult.Remove(ResultHandle);}
void STabletScreen::ShowIngredients(){Pages->SetActiveWidgetIndex(1);}
void STabletScreen::ShowOrders(){OrdersScreen->ShowList();Pages->SetActiveWidgetIndex(2);}
int32 STabletScreen::QuantityFor(FName Id)const{return Quantities.FindRef(Id);}
int64 STabletScreen::Total()const{int64 Cost=0;if(Shop.IsValid())for(const auto& P:Shop->GetProducts())Cost+=int64(P.Price)*QuantityFor(P.Id);return Cost;}
FText STabletScreen::Summary()const
{
    FString Result; int32 Count=0;
    if(Shop.IsValid())for(const auto& P:Shop->GetProducts())if(int32 Q=QuantityFor(P.Id);Q>0){Result+=FString::Printf(TEXT("%s  × %d     %lld\n\n"),*P.Name.ToString(),Q,int64(P.Price)*Q);Count+=Q;}
    if(!Count)return FText::FromString(TEXT("Добавьте ингредиенты стрелками или введите количество."));
    Result+=FString::Printf(TEXT("Всего: %d шт."),Count);return FText::FromString(Result);
}
bool STabletScreen::CanBuy()const{return !bWaiting && Shop.IsValid() && Total()>0 && Total()<=Shop->GetBalance();}
FReply STabletScreen::Buy(){if(CanBuy()){TArray<FTabletOrderItem> Items;for(const auto& P:Shop->GetProducts())if(QuantityFor(P.Id)>0){FTabletOrderItem I;I.ProductId=P.Id;I.Quantity=QuantityFor(P.Id);Items.Add(I);}bWaiting=true;Status=FText::FromString(TEXT("Отправляем заказ…"));Shop->PurchaseCart(Items);}return FReply::Handled();}
FReply STabletScreen::CloseScreen(){Close.ExecuteIfBound();return FReply::Handled();}
FReply STabletScreen::OnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Event){if(Event.GetKey()==EKeys::T||Event.GetKey()==EKeys::Escape)return CloseScreen();return SCompoundWidget::OnPreviewKeyDown(Geometry,Event);}
