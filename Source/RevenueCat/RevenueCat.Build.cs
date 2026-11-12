// Copyright (c) 2026 Nineva Studios

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

			// Swift SDK: purchases-ios (RevenueCat) + the hybrid-common facade (RCCommonFunctionality),
			// shipped as DYNAMIC, iOS-only xcframeworks (device + simulator) zipped under
			// Source/ThirdParty/IOS. Produce them on a Mac with Source/ThirdParty/IOS/build_xcframeworks.sh
			// (provenance and versions: Source/ThirdParty/IOS/README.md). Both must come from the same build.
			//
			// UBT unzips them into Engine/Intermediate/UnzippedFrameworks, adds -F/-framework for the
			// matching xcframework slice, and LinkAndCopy embeds + code-signs them into <App>.app/Frameworks
			// (UBT already links with -rpath @executable_path/Frameworks). UE 5.6 targets iOS 15+, where the
			// Swift runtime and Swift Concurrency ship with the OS, so no Swift stdlib has to be embedded.
			string ThirdPartyIOSDir = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "ThirdParty", "IOS"));
			string RevenueCatZip = Path.Combine(ThirdPartyIOSDir, "RevenueCat.xcframework.zip");
			string HybridCommonZip = Path.Combine(ThirdPartyIOSDir, "PurchasesHybridCommon.xcframework.zip");

			if (File.Exists(RevenueCatZip) && File.Exists(HybridCommonZip))
			{
				PublicAdditionalFrameworks.Add(new Framework("RevenueCat", RevenueCatZip, Framework.FrameworkMode.LinkAndCopy));
				PublicAdditionalFrameworks.Add(new Framework("PurchasesHybridCommon", HybridCommonZip, Framework.FrameworkMode.LinkAndCopy));

				// Swift library search paths (toolchain usr/lib/swift/iphoneos + /usr/lib/swift), the same
				// engine module Epic's Facebook SDK uses for its Swift xcframeworks.
				PrivateDependencyModuleNames.Add("Swift");

				PrivateDefinitions.Add("WITH_REVENUECAT_IOS_SDK=1");
			}
			else
			{
				// Keep the iOS build compiling without the SDK. Every call then fails at runtime with a clear
				// error (code -1) instead of pretending to succeed.
				PrivateDefinitions.Add("WITH_REVENUECAT_IOS_SDK=0");
				Microsoft.Extensions.Logging.LoggerExtensions.LogWarning(Logger,
					"RevenueCat: iOS SDK frameworks not found in '{Dir}'. Purchases will not work on iOS. " +
					"Run build_xcframeworks.sh there on a Mac (see README.md).", ThirdPartyIOSDir);
			}
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
