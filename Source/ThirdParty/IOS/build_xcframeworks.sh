#!/usr/bin/env bash
# Copyright (c) 2026 Nineva Studios
#
# Builds the two iOS frameworks the RevenueCat UE plugin links against and writes them next to
# this script:
#
#   RevenueCat.xcframework.zip             purchases-ios, the version pinned by hybrid-common
#   PurchasesHybridCommon.xcframework.zip  purchases-hybrid-common (RCCommonFunctionality facade)
#
# Both are DYNAMIC Swift frameworks with two slices: ios-arm64 (device) and
# ios-arm64_x86_64-simulator. They come out of ONE CocoaPods build of the hybrid-common Xcode
# project, so PurchasesHybridCommon is guaranteed to be compiled against the exact RevenueCat
# binary that ships next to it. Do not mix in a RevenueCat.xcframework from another build.
#
# Requirements (Mac only):
#   - Xcode 15.2 - 16.x (the range UE 5.6 supports), command line tools selected (xcode-select)
#   - CocoaPods 1.16+   (brew install cocoapods)
#   - git, zip, network access to github.com and the CocoaPods CDN
#
# Usage:
#   ./build_xcframeworks.sh
#   PHC_VERSION=18.15.1 EXPECTED_RC_VERSION=5.78.0 ./build_xcframeworks.sh
#   BLFD=NO ./build_xcframeworks.sh   # fallback if Swift module-interface verification fails
#   KEEP_WORK=1 ./build_xcframeworks.sh
#
# Keep PHC_VERSION equal to the purchases-hybrid-common version in RevenueCat_UPL_Android.xml so
# Android and iOS run the same facade.

set -euo pipefail

PHC_VERSION="${PHC_VERSION:-18.15.1}"
EXPECTED_RC_VERSION="${EXPECTED_RC_VERSION:-5.78.0}"
BLFD="${BLFD:-YES}"
KEEP_WORK="${KEEP_WORK:-0}"

OUT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/rc-ue-ios.XXXXXX")"
PHC_REPO="https://github.com/RevenueCat/purchases-hybrid-common.git"

log() { printf '\n==> %s\n' "$*"; }
die() { printf '\nERROR: %s\nWork dir kept for inspection: %s\n' "$*" "$WORK_DIR" >&2; exit 1; }

[ "$(uname -s)" = "Darwin" ] || die "This script must run on macOS."
command -v xcodebuild >/dev/null || die "xcodebuild not found. Install Xcode and run xcode-select."
command -v pod >/dev/null || die "CocoaPods not found (brew install cocoapods)."
command -v zip >/dev/null || die "zip not found."

log "$(xcodebuild -version | tr '\n' ' ')"
log "purchases-hybrid-common $PHC_VERSION, expecting purchases-ios $EXPECTED_RC_VERSION, BUILD_LIBRARY_FOR_DISTRIBUTION=$BLFD"

# --- 1. Sources -------------------------------------------------------------------------------
log "Cloning $PHC_REPO @ $PHC_VERSION"
git clone --quiet --depth 1 --branch "$PHC_VERSION" "$PHC_REPO" "$WORK_DIR/phc"
PROJ_DIR="$WORK_DIR/phc/ios/PurchasesHybridCommon"

grep -q "s.dependency 'RevenueCat', '$EXPECTED_RC_VERSION'" "$WORK_DIR/phc/PurchasesHybridCommon.podspec" \
	|| die "purchases-hybrid-common $PHC_VERSION does not pin RevenueCat $EXPECTED_RC_VERSION. Check its podspec, then update EXPECTED_RC_VERSION, README.md and the docs."

log "pod install"
( cd "$PROJ_DIR" && pod install )
grep -q "RevenueCat ($EXPECTED_RC_VERSION)" "$PROJ_DIR/Podfile.lock" \
	|| die "Podfile.lock did not resolve RevenueCat $EXPECTED_RC_VERSION."

# --- 2. Archives (device + simulator) ---------------------------------------------------------
# SKIP_INSTALL=NO on the command line applies to every target, so the RevenueCat pod framework is
# installed into the archive next to PurchasesHybridCommon.framework.
# CODE_SIGNING_ALLOWED=NO: the frameworks are signed by Xcode when UE embeds them into the app.
archive() { # $1 = xcodebuild destination, $2 = archive name
	log "Archiving PurchasesHybridCommon for '$1'"
	xcodebuild archive \
		-workspace "$PROJ_DIR/PurchasesHybridCommon.xcworkspace" \
		-scheme PurchasesHybridCommon \
		-configuration Release \
		-destination "$1" \
		-archivePath "$WORK_DIR/archives/$2.xcarchive" \
		-derivedDataPath "$WORK_DIR/derived-$2" \
		SKIP_INSTALL=NO \
		BUILD_LIBRARY_FOR_DISTRIBUTION="$BLFD" \
		CODE_SIGNING_ALLOWED=NO \
		-quiet
}
archive "generic/platform=iOS" "ios"
archive "generic/platform=iOS Simulator" "ios-simulator"

find_framework() { # $1 = archive name, $2 = framework name -> prints the .framework path
	local Fw="$WORK_DIR/archives/$1.xcarchive/Products/Library/Frameworks/$2.framework"
	if [ ! -d "$Fw" ]; then
		# Fallback: the build product inside DerivedData (e.g. if a pod kept SKIP_INSTALL=YES).
		Fw="$(find "$WORK_DIR/derived-$1" -type d -name "$2.framework" -path "*Release-iphone*" -not -path "*.dSYM*" | head -n 1)"
	fi
	[ -n "$Fw" ] && [ -d "$Fw" ] || die "$2.framework not found for '$1'."
	printf '%s' "$Fw"
}

# --- 3. xcframeworks + zips -------------------------------------------------------------------
STAGE="$WORK_DIR/xcframeworks"
mkdir -p "$STAGE"

for NAME in RevenueCat PurchasesHybridCommon; do
	DEVICE_FW="$(find_framework ios "$NAME")"
	SIM_FW="$(find_framework ios-simulator "$NAME")"

	# What RevenueCat.Build.cs / RevenueCatImpl_IOS.mm rely on:
	file "$DEVICE_FW/$NAME" | grep -q "dynamically linked shared library" \
		|| die "$NAME.framework is not a dynamic framework (LinkAndCopy expects an embeddable dylib)."
	[ -f "$DEVICE_FW/Headers/$NAME-Swift.h" ] \
		|| die "$NAME.framework has no Headers/$NAME-Swift.h (the Objective-C interface the .mm imports)."

	log "Creating $NAME.xcframework"
	xcodebuild -create-xcframework -framework "$DEVICE_FW" -framework "$SIM_FW" -output "$STAGE/$NAME.xcframework"

	# UnrealBuildTool reads Info.plist with an XML parser; make sure it is not a binary plist.
	plutil -convert xml1 "$STAGE/$NAME.xcframework/Info.plist"

	# UBT expects "<Name>.xcframework/" at the root of "<Name>.xcframework.zip".
	rm -f "$OUT_DIR/$NAME.xcframework.zip"
	( cd "$STAGE" && zip -qryX "$OUT_DIR/$NAME.xcframework.zip" "$NAME.xcframework" )
done

# --- 4. Licenses + provenance -----------------------------------------------------------------
cp "$WORK_DIR/phc/LICENSE" "$OUT_DIR/LICENSE-purchases-hybrid-common.txt"
RC_LICENSE="$(find "$PROJ_DIR/Pods/RevenueCat" -maxdepth 1 -iname 'LICENSE*' | head -n 1)"
if [ -n "$RC_LICENSE" ]; then
	cp "$RC_LICENSE" "$OUT_DIR/LICENSE-purchases-ios.txt"
else
	curl -fsSL "https://raw.githubusercontent.com/RevenueCat/purchases-ios/$EXPECTED_RC_VERSION/LICENSE" \
		-o "$OUT_DIR/LICENSE-purchases-ios.txt" || die "Could not obtain the purchases-ios LICENSE."
fi

(
	cd "$OUT_DIR"
	{
		echo "purchases-hybrid-common: $PHC_VERSION ($PHC_REPO)"
		echo "purchases-ios (RevenueCat): $EXPECTED_RC_VERSION (CocoaPods trunk, via hybrid-common Podfile.lock)"
		echo "Built with: $(xcodebuild -version | head -n 1), BUILD_LIBRARY_FOR_DISTRIBUTION=$BLFD"
		echo "Built at (UTC): $(date -u +%Y-%m-%dT%H:%M:%SZ)"
		echo "Slices: ios-arm64, ios-arm64_x86_64-simulator (dynamic frameworks)"
		echo
		shasum -a 256 RevenueCat.xcframework.zip PurchasesHybridCommon.xcframework.zip
	} > VERSIONS.txt
)

if [ "$KEEP_WORK" = "1" ]; then
	log "Work dir kept: $WORK_DIR"
else
	rm -rf "$WORK_DIR"
fi

log "Done. Wrote to $OUT_DIR:"
ls -lh "$OUT_DIR"/*.xcframework.zip "$OUT_DIR"/VERSIONS.txt
