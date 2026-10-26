// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LetThemCook : ModuleRules
{
	public LetThemCook(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"NavigationSystem",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"SlateCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"LetThemCook",
			"LetThemCook/Variant_Horror",
			"LetThemCook/Variant_Horror/UI",
			"LetThemCook/Variant_Shooter",
			"LetThemCook/Variant_Shooter/AI",
			"LetThemCook/Variant_Shooter/UI",
			"LetThemCook/Variant_Shooter/Weapons"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
