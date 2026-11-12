// Copyright (c) 2026 Nineva Studios
// Demo ViewModels (MVVM). Kept separate from the core module so the
// core stays free of UI/MVVM dependencies.

using UnrealBuildTool;

public class RevenueCatUI : ModuleRules
{
	public RevenueCatUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"SlateCore",
			"ModelViewViewModel",
			"FieldNotification",
			"RevenueCat"
		});
	}
}
