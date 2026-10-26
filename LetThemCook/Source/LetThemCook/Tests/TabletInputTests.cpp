#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "LetThemCookPlayerController.h"
#include "STabletScreen.h"
#include "TabletDevice.h"
#include "Components/WidgetComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputSubsystems.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "InputAction.h"
#include "InputKeyEventArgs.h"
#include "InputMappingContext.h"
#include "Layout/Children.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
struct FTabletInputTestState
{
    FAutomationTestBase* Test = nullptr;
    ALetThemCookPlayerController* Host = nullptr;
    ALetThemCookPlayerController* Client = nullptr;
    UInputMappingContext* Context = nullptr;
    UInputAction* ToggleAction = nullptr;
    TArray<FKey> OriginalKeys;
    bool bRemapped = false;
    double LastStepTime = 0;
    int32 Stage = 0;

    bool Check(bool Condition, const TCHAR* Message)
    {
        return Test->TestTrue(FString::Printf(TEXT("Stage %d: %s"), Stage, Message), Condition);
    }

    void Key(ALetThemCookPlayerController* Controller, FKey Key, EInputEvent Event)
    {
        static_cast<APlayerController*>(Controller)->InputKey(FInputKeyEventArgs(nullptr,
            FInputDeviceId::CreateFromInternalId(0), Key, Event, Event == IE_Released ? 0.f : 1.f, false, FPlatformTime::Cycles64()));
    }

    TSharedPtr<STabletScreen> Screen(ALetThemCookPlayerController* Controller)
    {
        for (TActorIterator<ATabletDevice> Device(Controller->GetWorld()); Device; ++Device)
        {
            if (Device->GetOwner() != Controller->GetPawn()) continue;
            const TSharedPtr<SWidget> Widget = Device->FindComponentByClass<UWidgetComponent>()->GetSlateWidget();
            if (Widget.IsValid() && Widget->GetType() == FName(TEXT("STabletScreen"))) return StaticCastSharedPtr<STabletScreen>(Widget);
        }
        return nullptr;
    }

    TSharedPtr<SWidget> FindWidget(const TSharedRef<SWidget>& Widget, FName Type)
    {
        if (Widget->GetType() == Type) return Widget;
        FChildren* Children = Widget->GetChildren();
        for (int32 Index = 0; Index < Children->Num(); ++Index)
            if (TSharedPtr<SWidget> Found = FindWidget(Children->GetChildAt(Index), Type)) return Found;
        return nullptr;
    }

    bool UIKey(ALetThemCookPlayerController* Controller, FKey Key)
    {
        TSharedPtr<STabletScreen> Tablet = Screen(Controller);
        if (!Check(Tablet.IsValid(), TEXT("Tablet owns a Slate screen"))) return false;
        Tablet->ShowIngredients();
        const TSharedPtr<SWidget> Input = FindWidget(Tablet.ToSharedRef(), TEXT("SEditableTextBox"));
        if (!Check(Input.IsValid(), TEXT("Catalog contains quantity fields"))) return false;
        const TSharedPtr<FSlateUser> User = Controller->GetLocalPlayer()->GetSlateUser();
        if (!Check(User.IsValid(), TEXT("Local player has a Slate user"))) return false;
        FSlateApplication::Get().SetUserFocus(User->GetUserIndex(), Input, EFocusCause::SetDirectly);
        if (!Check(Input->HasUserFocus(User->GetUserIndex()) || Input->HasUserFocusedDescendants(User->GetUserIndex()), TEXT("Quantity field receives keyboard focus"))) return false;
        return Check(FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(Key, FModifierKeysState(),
            FInputDeviceId::CreateFromInternalId(0), false, 0, 0, User->GetUserIndex())), TEXT("Focused quantity field routes tablet command through Slate"));
    }

    void RebuildMappings()
    {
        for (ALetThemCookPlayerController* Controller : {Host, Client})
        {
            UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Controller->GetLocalPlayer());
            Subsystem->RequestRebuildControlMappings(FModifyContextOptions(), EInputMappingRebuildType::RebuildWithFlush);
        }
    }

    void RestoreMappings()
    {
        if (!bRemapped) return;
        Context->UnmapAllKeysFromAction(ToggleAction);
        for (FKey Key : OriginalKeys) Context->MapKey(ToggleAction, Key);
        RebuildMappings();
        bRemapped = false;
    }

    bool RunStage()
    {
        switch (Stage)
        {
        case 0:
            for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
            {
                UWorld* World = WorldContext.World();
                if (!World || World->WorldType != EWorldType::PIE) continue;
                if (World->GetNetMode() == NM_ListenServer) Host = Cast<ALetThemCookPlayerController>(World->GetFirstPlayerController());
                if (World->GetNetMode() == NM_Client) Client = Cast<ALetThemCookPlayerController>(World->GetFirstPlayerController());
            }
            if (!Check(Host && Client, TEXT("Run two-player PIE as listen server"))) return false;
            Context = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Input/IMC_Tablet.IMC_Tablet"));
            ToggleAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_ToggleTablet.IA_ToggleTablet"));
            if (!Check(Context && ToggleAction && !Host->IsTabletOpen() && !Client->IsTabletOpen(), TEXT("Input assets exist and both tablets start closed"))) return false;
            Key(Host, EKeys::T, IE_Pressed);
            return true;
        case 1:
            if (!Check(Host->IsTabletOpen() && !Client->IsTabletOpen(), TEXT("T opens only host tablet through Enhanced Input"))) return false;
            Key(Host, EKeys::T, IE_Released);
            Key(Host, EKeys::SpaceBar, IE_Pressed);
            if (!Check(!Host->IsInputKeyDown(EKeys::SpaceBar), TEXT("Open tablet blocks gameplay input"))) return false;
            return UIKey(Host, EKeys::Escape);
        case 2:
            if (!Check(!Host->IsTabletOpen() && !Client->IsTabletOpen(), TEXT("Escape closes tablet while quantity field has focus"))) return false;
            Key(Host, EKeys::Escape, IE_Released);
            Key(Client, EKeys::T, IE_Pressed);
            return true;
        case 3:
            if (!Check(Client->IsTabletOpen() && !Host->IsTabletOpen(), TEXT("Client T opens only client tablet"))) return false;
            Key(Client, EKeys::T, IE_Released);
            return true;
        case 4:
            return UIKey(Client, EKeys::T);
        case 5:
            if (!Check(!Client->IsTabletOpen() && !Host->IsTabletOpen(), TEXT("T closes focused client tablet through Enhanced Input"))) return false;
            Key(Client, EKeys::T, IE_Released);
            Key(Host, EKeys::T, IE_Pressed);
            Key(Client, EKeys::T, IE_Pressed);
            return true;
        case 6:
            if (!Check(Host->IsTabletOpen() && Client->IsTabletOpen(), TEXT("Both players can independently open tablets"))) return false;
            Key(Host, EKeys::T, IE_Released);
            Key(Client, EKeys::T, IE_Released);
            for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
                if (Mapping.Action == ToggleAction) OriginalKeys.Add(Mapping.Key);
            Context->UnmapAllKeysFromAction(ToggleAction);
            Context->MapKey(ToggleAction, EKeys::Y);
            bRemapped = true;
            RebuildMappings();
            return true;
        case 7:
            return UIKey(Host, EKeys::Y);
        case 8:
            if (!Check(!Host->IsTabletOpen() && Client->IsTabletOpen(), TEXT("Remapped Y closes only the focused host tablet"))) return false;
            Key(Host, EKeys::Y, IE_Released);
            Key(Host, EKeys::T, IE_Pressed);
            return true;
        case 9:
            if (!Check(!Host->IsTabletOpen(), TEXT("Old T binding is inactive after remapping"))) return false;
            Key(Host, EKeys::T, IE_Released);
            return UIKey(Client, EKeys::Escape);
        case 10:
            if (!Check(!Host->IsTabletOpen() && !Client->IsTabletOpen(), TEXT("Escape still closes client after remapping toggle"))) return false;
            Key(Client, EKeys::Escape, IE_Released);
            Key(Host, EKeys::Y, IE_Pressed);
            return true;
        case 11:
            if (!Check(Host->IsTabletOpen(), TEXT("Remapped Y opens tablet from gameplay"))) return false;
            Key(Host, EKeys::Y, IE_Repeat);
            return true;
        case 12:
            if (!Check(Host->IsTabletOpen(), TEXT("Keyboard auto-repeat does not toggle tablet again"))) return false;
            Key(Host, EKeys::Y, IE_Released);
            Host->CloseTablet();
            RestoreMappings();
            return true;
        default:
            return false;
        }
    }
};
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FRunTabletInputScenario, TSharedRef<FTabletInputTestState>, State);

bool FRunTabletInputScenario::Update()
{
    const double Now = FPlatformTime::Seconds();
    if (Now - State->LastStepTime < 0.8) return false;
    State->LastStepTime = Now;
    UE_LOG(LogTemp, Display, TEXT("TABLET_INPUT_STATE stage=%d host=%d client=%d"), State->Stage, State->Host ? State->Host->IsTabletOpen() : false, State->Client ? State->Client->IsTabletOpen() : false);
    if (!State->RunStage())
    {
        State->RestoreMappings();
        UE_LOG(LogTemp, Error, TEXT("TABLET_INPUT_NETWORK_RESULT False stage=%d"), State->Stage);
        return true;
    }
    if (++State->Stage == 13)
    {
        UE_LOG(LogTemp, Display, TEXT("TABLET_INPUT_NETWORK_RESULT True; two players; Enhanced Input open/close; Slate quantity focus; gameplay blocking; remapped Y works in gameplay and UI; original context restored"));
        return true;
    }
    return false;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTabletEnhancedInputTest, "LetThemCook.Tablet.EnhancedInput",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FTabletEnhancedInputTest::RunTest(const FString& Parameters)
{
    TSharedRef<FTabletInputTestState> State = MakeShared<FTabletInputTestState>();
    State->Test = this;
    ADD_LATENT_AUTOMATION_COMMAND(FRunTabletInputScenario(State));
    return true;
}

#endif
