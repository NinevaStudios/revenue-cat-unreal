# RevenueCat iOS SDK binaries

`RevenueCat.Build.cs` links the iOS build against two dynamic Swift xcframeworks from this folder:

| File | What it is | Version | Source | License |
|---|---|---|---|---|
| `RevenueCat.xcframework.zip` | purchases-ios, the RevenueCat iOS SDK | 5.78.0 | CocoaPods trunk `RevenueCat` 5.78.0 = https://github.com/RevenueCat/purchases-ios tag `5.78.0` | MIT (`LICENSE-purchases-ios.txt`) |
| `PurchasesHybridCommon.xcframework.zip` | purchases-hybrid-common iOS facade (`RCCommonFunctionality`, `RCErrorContainer`) | 18.15.1 | https://github.com/RevenueCat/purchases-hybrid-common tag `18.15.1`, `ios/PurchasesHybridCommon` | MIT (`LICENSE-purchases-hybrid-common.txt`) |

Android uses the same hybrid-common version (`RevenueCat_UPL_Android.xml`). purchases-hybrid-common 18.15.1 pins purchases-ios 5.78.0 in its podspec (`s.dependency 'RevenueCat', '5.78.0'`).

## How they are produced

RevenueCat publishes no prebuilt `PurchasesHybridCommon.xcframework`; it is distributed only as source (CocoaPods / SPM). Both binaries are therefore built on a Mac by `build_xcframeworks.sh`:

```bash
cd Plugins/RevenueCat/Source/ThirdParty/IOS
bash build_xcframeworks.sh
```

If the script was copied from a Windows checkout and fails with `$'\r': command not found`, strip the CRLFs first with `sed -i '' $'s/\r$//' build_xcframeworks.sh`. The `.gitattributes` in this folder keeps it LF in git.

The script:

1. Clones purchases-hybrid-common at the pinned tag and runs `pod install`, which resolves RevenueCat 5.78.0 from the repository's `Podfile.lock`.
2. Archives the `PurchasesHybridCommon` scheme for `generic/platform=iOS` and `generic/platform=iOS Simulator` with `SKIP_INSTALL=NO` and `BUILD_LIBRARY_FOR_DISTRIBUTION=YES`. This puts both `PurchasesHybridCommon.framework` and the `RevenueCat.framework` pod in each archive.
3. Runs `xcodebuild -create-xcframework` for each framework. Only the iOS slices `ios-arm64` and `ios-arm64_x86_64-simulator` are included.
4. Converts the xcframework `Info.plist` to XML, because UnrealBuildTool parses it with an XML reader.
5. Zips each xcframework as `<Name>.xcframework.zip` with `<Name>.xcframework/` at the zip root. This is the layout UBT expects.
6. Copies both MIT licenses and writes `VERSIONS.txt` with the versions, Xcode version and SHA-256 of the zips.

Both frameworks come out of **one** build, so `PurchasesHybridCommon` is compiled against the exact `RevenueCat` binary shipped next to it. Do not replace `RevenueCat.xcframework.zip` with the 166 MB all-platform `RevenueCat.xcframework.zip` from the purchases-ios GitHub release. That release asset is the right SDK version, but it is a separate build and contains tvOS/macOS/watchOS/visionOS slices the plugin does not need.

## Why zips and dynamic frameworks

- UE 5.6 supports `.xcframework.zip` in `PublicAdditionalFrameworks`. UBT unzips it into `Engine/Intermediate/UnzippedFrameworks/<Name>/` and picks the right slice from `Info.plist` (device `arm64`, or the `simulator` variant). A single zip also survives Windows checkouts, Perforce and Fab packaging intact.
- `Framework.FrameworkMode.LinkAndCopy` links each framework and embeds it into `<App>.app/Frameworks`. Modern Xcode adds an "Embed Frameworks" phase with `CodeSignOnCopy`, so Xcode signs the frameworks with the app's identity.
- UE 5.6 requires iOS 15+. The Swift runtime and Swift Concurrency ship with the OS there, so no Swift standard library has to be embedded. The game executable contains no Swift code. The Build.cs also depends on the engine's `Swift` module (toolchain `usr/lib/swift/iphoneos` and `/usr/lib/swift` search paths), the same setup Epic uses for its Swift Facebook xcframeworks.

## Updating the SDK

1. Choose the new purchases-hybrid-common version and update it in `RevenueCat_UPL_Android.xml` (Android) **and** here.
2. Look up the purchases-ios version it pins in `PurchasesHybridCommon.podspec`.
3. Run `PHC_VERSION=<x> EXPECTED_RC_VERSION=<y> bash build_xcframeworks.sh` on a Mac.
4. Update the version table above and the docs, then re-test on a device (see `IOS_BUILD_NOTES.md` in the plugin root).
5. If the build fails during Swift module-interface verification, rerun with `BLFD=NO`. Both frameworks still ship from the same build, so ABI resilience is not needed for this pairing.

If the zips are missing, the iOS build still compiles, with a UBT warning. Every RevenueCat call on iOS then fails with error code `-1` and a message that points here.
