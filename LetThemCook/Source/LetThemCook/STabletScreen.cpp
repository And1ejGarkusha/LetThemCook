#include "STabletScreen.h"
#include "InputCoreTypes.h"
#include "Engine/Texture2D.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateColorBrush.h"
namespace TabletPalette
{
    const FLinearColor Paper = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("2A1D0C")));
    const FLinearColor Ink = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("FFF3C5")));
    const FLinearColor Muted = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("FFEBB1")));
    const FLinearColor Panel = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("483114")));
    const FLinearColor Line = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("916222")));
    const FLinearColor Accent = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("E8AF12")));
    const FLinearColor ActionInk = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("221704")));
    TSharedRef<STextBlock> Label(const FText& Text, int32 Size = 16, bool Bold = false)
    {
        return SNew(STextBlock).Text(Text).ColorAndOpacity(Ink).Font(FCoreStyle::GetDefaultFontStyle(Bold ? "Bold" : "Regular", Size));
    }
}
void STabletScreen::Construct(const FArguments& Args)
{
    using namespace TabletPalette;
    Shop = Args._Shop; Close = Args._OnClose;
    CommandKeyDown = Args._OnCommandKeyDown;
    CommandKeyUp = Args._OnCommandKeyUp;
    ButtonStyle.SetNormal(FSlateColorBrush(Panel)).SetHovered(FSlateColorBrush(Line)).SetPressed(FSlateColorBrush(Paper));
    ButtonStyle.SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
    ActionStyle = ButtonStyle;
    ActionStyle.SetNormal(FSlateColorBrush(Accent)).SetHovered(FSlateColorBrush(FLinearColor::FromSRGBColor(FColor(255,197,37)))).SetPressed(FSlateColorBrush(Line));
    InputStyle = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
    InputStyle.SetBackgroundImageNormal(FSlateColorBrush(Paper)).SetBackgroundImageHovered(FSlateColorBrush(Paper)).SetBackgroundImageFocused(FSlateColorBrush(Paper)).SetBackgroundImageReadOnly(FSlateColorBrush(Paper));
    InputStyle.SetForegroundColor(Ink).SetPadding(FMargin(4, 6));
    IngredientsTexture.Reset(LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/Tablet/T_Ingredients.T_Ingredients")));
    UpgradesTexture.Reset(LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/Tablet/T_Upgrades.T_Upgrades")));
    IngredientsBrush.SetResourceObject(IngredientsTexture.Get()); IngredientsBrush.ImageSize = FVector2D(64,64); IngredientsBrush.DrawAs = ESlateBrushDrawType::Image;
    UpgradesBrush.SetResourceObject(UpgradesTexture.Get()); UpgradesBrush.ImageSize = FVector2D(64,64); UpgradesBrush.DrawAs = ESlateBrushDrawType::Image;
    if (Shop.IsValid())
    {
        Status = Shop->GetLastPurchaseMessage();
        ResultHandle = Shop->OnPurchaseResult.AddLambda([Weak = TWeakPtr<STabletScreen>(SharedThis(this))](bool Success, const FText& Message)
        {
            if (auto Screen = Weak.Pin()) { Screen->bWaiting=false; Screen->bSuccess=Success; Screen->Status=Message; if (Success) Screen->Quantities.Empty(); }
        });
    }
    if (Shop.IsValid())
    {
        CatalogHandle = Shop->OnCatalogChanged.AddLambda([Weak = TWeakPtr<STabletScreen>(SharedThis(this))]()
        {
            if (auto Screen = Weak.Pin()) Screen->RefreshProducts();
        });
    }
    auto Tile = [this](const FText& Title, const FSlateBrush* Brush, bool Locked) -> TSharedRef<SWidget>
    {
        return SNew(SBox).WidthOverride(310).HeightOverride(310)
        [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Line).Padding(1)
        [SNew(SButton).ButtonStyle(&ButtonStyle).IsFocusable(false).IsEnabled(!Locked).ContentPadding(22)
            .OnClicked_Lambda([this, Locked](){ if (!Locked) ShowIngredients(); return FReply::Handled(); })
            [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[TabletPalette::Label(Title,24,true)]
                + SVerticalBox::Slot().FillHeight(1).HAlign(HAlign_Center).VAlign(VAlign_Center)
                [SNew(SBox).WidthOverride(180).HeightOverride(180)[SNew(SImage).Image(Brush).ColorAndOpacity(Locked ? FLinearColor(0.6f,0.6f,0.6f,1) : FLinearColor::White)]]
                + SVerticalBox::Slot().AutoHeight()[TabletPalette::Label(Locked ? FText::FromString(TEXT("ЗАБЛОКИРОВАНО")) : FText::FromString(TEXT("Открыть каталог  →")),14,true)]
            ]]];
    };
    ProductRows = SNew(SVerticalBox);
    RefreshProducts();
    ChildSlot
    [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Paper).Padding(32)
    [SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,24)
        [SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1)[Label(FText::FromString(TEXT("СНАБЖЕНИЕ КУХНИ")),22,true)]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,24,0)
            [SNew(STextBlock).ColorAndOpacity(Ink).Font(FCoreStyle::GetDefaultFontStyle("Bold",18)).Text_Lambda([this](){return FText::Format(FText::FromString(TEXT("Баланс: {0}")),FText::AsNumber(Shop.IsValid()?Shop->GetBalance():0));})]
            + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).ButtonStyle(&ButtonStyle).ContentPadding(FMargin(12,8)).IsFocusable(false).OnClicked(this,&STabletScreen::CloseScreen)
                [SNew(STextBlock).Text(Args._CloseHint).ColorAndOpacity(Ink).Font(FCoreStyle::GetDefaultFontStyle("Regular",14))]]
        ]
        + SVerticalBox::Slot().FillHeight(1)
        [SAssignNew(Pages,SWidgetSwitcher)
            + SWidgetSwitcher::Slot()
            [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[Label(FText::FromString(TEXT("Закупки")),32,true)]
                + SVerticalBox::Slot().FillHeight(1).HAlign(HAlign_Center).VAlign(VAlign_Center)
                [SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().Padding(0,0,22,0)[Tile(FText::FromString(TEXT("Ингредиенты")),&IngredientsBrush,false)]
                    + SHorizontalBox::Slot().AutoWidth()[Tile(FText::FromString(TEXT("Улучшения")),&UpgradesBrush,true)]]
            ]
            + SWidgetSwitcher::Slot()
            [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,20)
                [SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().Padding(0,0,20,0)[SNew(SButton).ButtonStyle(&ButtonStyle).ContentPadding(FMargin(12,8)).IsFocusable(false).OnClicked_Lambda([this](){Pages->SetActiveWidgetIndex(0);return FReply::Handled();})[Label(FText::FromString(TEXT("← Назад")),14)]]
                    + SHorizontalBox::Slot().FillWidth(1)[Label(FText::FromString(TEXT("Закупка продуктов")),28,true)]]
                + SVerticalBox::Slot().FillHeight(1)
                [SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(0.68f).Padding(0,0,24,0)[SNew(SScrollBox)+SScrollBox::Slot()[ProductRows.ToSharedRef()]]
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
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0,16,0,0)
        [SNew(STextBlock).ColorAndOpacity_Lambda([this](){return bSuccess?FSlateColor(TabletPalette::Muted):FSlateColor(FLinearColor(1,0.3f,0.1f));}).Text_Lambda([this](){return Shop.IsValid()&&Total()>Shop->GetBalance()?FText::FromString(TEXT("Недостаточно денег для этого заказа.")):Status;})]
    ]];
}
void STabletScreen::RefreshProducts()
{
    using namespace TabletPalette;
    if (!ProductRows.IsValid()) return;

    ProductRows->ClearChildren();
    ProductBrushes.Reset();
    ProductTextures.Reset();
    if (!Shop.IsValid() || Shop->GetProducts().IsEmpty())
    {
        ProductRows->AddSlot().AutoHeight()[Label(FText::FromString(TEXT("Каталог ингредиентов недоступен.")),16)];
        return;
    }

    TSet<FName> AvailableIds;
    for (const FFoodProductDefinition& Product : Shop->GetProducts()) AvailableIds.Add(Product.Id);
    for (auto Quantity = Quantities.CreateIterator(); Quantity; ++Quantity)
    {
        if (!AvailableIds.Contains(Quantity.Key())) Quantity.RemoveCurrent();
    }
    for (const FFoodProductDefinition& P : Shop->GetProducts())
    {
        const FName Id=P.Id;
        TSharedRef<FSlateBrush> IconBrush = MakeShared<FSlateBrush>();
        IconBrush->SetResourceObject(P.Icon);
        IconBrush->ImageSize = FVector2D(48, 48);
        IconBrush->DrawAs = P.Icon ? ESlateBrushDrawType::Image : ESlateBrushDrawType::NoDrawType;
        ProductBrushes.Add(IconBrush);
        ProductTextures.Emplace(P.Icon.Get());
        auto Arrow = [this, Id](const TCHAR* Caption, int32 Delta)->TSharedRef<SWidget>
        {
            return SNew(SBox).WidthOverride(34).HeightOverride(34)
            [SNew(SButton).ButtonStyle(&ButtonStyle).IsFocusable(false).HAlign(HAlign_Center)
            .IsEnabled_Lambda([this](){return !bWaiting;})
            .OnClicked_Lambda([this,Id,Delta](){Quantities.Add(Id,FMath::Clamp(QuantityFor(Id)+Delta,0,100));return FReply::Handled();})
            [TabletPalette::Label(FText::FromString(Caption),18,true)]];
        };
        ProductRows->AddSlot().AutoHeight().Padding(0,0,0,5)
        [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Line).Padding(1)
        [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Panel).Padding(FMargin(12,6))
        [SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,12,0)
            [SNew(SBox).WidthOverride(48).HeightOverride(48)[SNew(SImage).Image(&IconBrush.Get())]]
            + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Label(P.Name,16)]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6,0,16,0)[Label(FText::Format(FText::FromString(TEXT("{0} / коробку")), FText::AsNumber(P.Price)),14)]
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
}

STabletScreen::~STabletScreen()
{
    if (Shop.IsValid())
    {
        Shop->OnPurchaseResult.Remove(ResultHandle);
        Shop->OnCatalogChanged.Remove(CatalogHandle);
    }
}
void STabletScreen::ShowIngredients(){Pages->SetActiveWidgetIndex(1);}
int32 STabletScreen::QuantityFor(FName Id)const{return Quantities.FindRef(Id);}
int64 STabletScreen::Total()const{int64 Cost=0;if(Shop.IsValid())for(const auto& P:Shop->GetProducts())Cost+=int64(P.Price)*QuantityFor(P.Id);return Cost;}
FText STabletScreen::Summary()const
{
    FString Result; int32 Count=0;
    if(Shop.IsValid())for(const auto& P:Shop->GetProducts())if(int32 Q=QuantityFor(P.Id);Q>0){Result+=FString::Printf(TEXT("%s  × %d     %lld\n\n"),*P.Name.ToString(),Q,int64(P.Price)*Q);Count+=Q;}
    if(!Count)return FText::FromString(TEXT("Добавьте ингредиенты стрелками или введите количество."));
    Result+=FString::Printf(TEXT("Всего коробок: %d"),Count);return FText::FromString(Result);
}
bool STabletScreen::CanBuy()const{return !bWaiting && Shop.IsValid() && Total()>0 && Total()<=Shop->GetBalance();}
FReply STabletScreen::Buy(){if(CanBuy()){TArray<FTabletOrderItem> Items;for(const auto& P:Shop->GetProducts())if(QuantityFor(P.Id)>0){FTabletOrderItem I;I.ProductId=P.Id;I.Quantity=QuantityFor(P.Id);Items.Add(I);}bWaiting=true;Status=FText::FromString(TEXT("Отправляем заказ…"));Shop->PurchaseCart(Items);}return FReply::Handled();}
FReply STabletScreen::CloseScreen(){Close.ExecuteIfBound();return FReply::Handled();}
FReply STabletScreen::OnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (CommandKeyDown.IsBound())
    {
        const FReply Reply = CommandKeyDown.Execute(Geometry, Event);
        if (Reply.IsEventHandled()) return Reply;
    }
    return SCompoundWidget::OnPreviewKeyDown(Geometry, Event);
}

FReply STabletScreen::OnKeyUp(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (CommandKeyUp.IsBound())
    {
        const FReply Reply = CommandKeyUp.Execute(Geometry, Event);
        if (Reply.IsEventHandled()) return Reply;
    }
    return SCompoundWidget::OnKeyUp(Geometry, Event);
}
