// Copyright (c) 2026 Nineva Studios
// Project Settings page (Project Settings -> Plugins -> RevenueCat) holding the API keys.
// NOTE: only PUBLIC / SDK keys belong here. Secret keys (sk_...) must live ONLY on your server.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RevenueCatSettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "RevenueCat"))
class REVENUECAT_API URevenueCatSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return FName(TEXT("Plugins")); }

	/** Public API key of your App Store app in RevenueCat (starts with "appl_"). Used on iOS builds. Never use a secret "sk_" key. */
	UPROPERTY(EditAnywhere, Config, Category = "API Keys")
	FString AppleApiKey;

	/** Public API key of your Google Play app in RevenueCat (starts with "goog_"). Used on Android builds. Never use a secret "sk_" key. */
	UPROPERTY(EditAnywhere, Config, Category = "API Keys")
	FString GoogleApiKey;

	/** Public API key of your Web Billing app in RevenueCat (starts with "rcb_"). Used on Windows / Mac / Linux builds and in the editor. Never use a secret "sk_" key. */
	UPROPERTY(EditAnywhere, Config, Category = "API Keys")
	FString WebBillingApiKey;

	/**
	 * Advanced. Base URL of the RevenueCat REST API used by desktop builds. Leave empty to use https://api.revenuecat.com/v1.
	 * Set it only if you route desktop requests through your own proxy that returns the same v1 responses.
	 */
	UPROPERTY(EditAnywhere, Config, Category = "Desktop", AdvancedDisplay)
	FString RestApiBaseUrl;

	/**
	 * Store whose offerings desktop builds receive (sent as the X-Platform header).
	 * "stripe" for Web Billing. Valid: ios, android, amazon, stripe, roku, paddle. Empty = header omitted.
	 */
	UPROPERTY(EditAnywhere, Config, Category = "Desktop")
	FString DesktopStorePlatform = TEXT("stripe");

	/**
	 * Web Purchase Link from the RevenueCat dashboard (Web -> Web Purchase Links), e.g. https://pay.rev.cat/xxxx.
	 * Desktop purchases open it in the browser with the current App User ID appended.
	 */
	UPROPERTY(EditAnywhere, Config, Category = "Desktop")
	FString WebPurchaseLinkUrl;

	/** Desktop: how long (seconds) to wait for the browser checkout to grant a new entitlement before returning a timeout error. */
	UPROPERTY(EditAnywhere, Config, Category = "Desktop", meta = (ClampMin = "10", ClampMax = "600"))
	int32 CheckoutPollTimeoutSeconds = 180;

	/** Verbose logging: RevenueCat SDK debug logs on mobile, request logs (LogRevenueCat) on desktop. Disable for release builds. */
	UPROPERTY(EditAnywhere, Config, Category = "Debug")
	bool bEnableDebugLogs = false;
};
