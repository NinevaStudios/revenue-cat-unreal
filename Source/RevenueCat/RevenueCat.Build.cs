// RevenueCat UE plugin — learning skeleton (stubbed implementation).

using UnrealBuildTool;
using System.IO;

public class RevenueCat : ModuleRules
{
	public RevenueCat(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Json",
			"JsonUtilities",
			"HTTP",
			"DeveloperSettings"
		});

		if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			// System framework — always present on iOS.
			PublicFrameworks.Add("StoreKit");

			// TODO: drop the real Swift xcframeworks into ThirdParty/IOS and uncomment.
			// They bundle purchases-ios + the hybrid-common facade (CommonFunctionality).
			// PublicAdditionalFrameworks.Add(new Framework("RevenueCat",
			//     "ThirdParty/IOS/RevenueCat.xcframework.zip", null, true));
			// PublicAdditionalFrameworks.Add(new Framework("PurchasesHybridCommon",
			//     "ThirdParty/IOS/PurchasesHybridCommon.xcframework.zip", null, true));
		}
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			// JNI helpers (FAndroidApplication::GetJavaEnv()) live in ApplicationCore.
			PrivateDependencyModuleNames.Add("ApplicationCore");

			// UPL injects Gradle deps + manifest + proguard. See the XML for the TODO version pin.
			AdditionalPropertiesForReceipt.Add("AndroidPlugin",
				Path.Combine(ModuleDirectory, "RevenueCat_UPL_Android.xml"));
		}
		// Win64 / Mac / Linux -> desktop REST branch (no native store SDK). Nothing extra to link.
	}
}
