// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class miniTPS : ModuleRules
{
	public miniTPS(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"miniTPS",
			"miniTPS/Variant_Platforming",
			"miniTPS/Variant_Platforming/Animation",
			"miniTPS/Variant_Combat",
			"miniTPS/Variant_Combat/AI",
			"miniTPS/Variant_Combat/Animation",
			"miniTPS/Variant_Combat/Gameplay",
			"miniTPS/Variant_Combat/Interfaces",
			"miniTPS/Variant_Combat/UI",
			"miniTPS/Variant_SideScrolling",
			"miniTPS/Variant_SideScrolling/AI",
			"miniTPS/Variant_SideScrolling/Gameplay",
			"miniTPS/Variant_SideScrolling/Interfaces",
			"miniTPS/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
