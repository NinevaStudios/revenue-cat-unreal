// RevenueCat UE plugin — learning skeleton.
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

	/** Public Apple API key (used on iOS builds). */
	UPROPERTY(EditAnywhere, Config, Category = "API Keys")
	FString AppleApiKey;

	/** Public Google API key (used on Android builds). */
	UPROPERTY(EditAnywhere, Config, Category = "API Keys")
	FString GoogleApiKey;

	/** Public Web Billing API key (used on desktop / PC launcher builds). */
	UPROPERTY(EditAnywhere, Config, Category = "API Keys")
	FString WebBillingApiKey;

	/** Your backend base URL — the source of truth the PC build queries for entitlements. */
	UPROPERTY(EditAnywhere, Config, Category = "Desktop")
	FString BackendBaseUrl;

	/**
	 * X-Platform header sent on desktop REST calls. Offerings are filtered by this value.
	 * Valid: ios, android, amazon, stripe, roku, paddle. Leave empty to omit the header.
	 */
	UPROPERTY(EditAnywhere, Config, Category = "Desktop")
	FString DesktopStorePlatform = TEXT("stripe");

	/**
	 * RevenueCat Web Billing "Web Purchase Link" base URL (from the dashboard).
	 * Desktop PurchasePackage() opens this in a browser, appending the App User ID.
	 */
	UPROPERTY(EditAnywhere, Config, Category = "Desktop")
	FString WebPurchaseLinkUrl;

	/** How long (seconds) to poll for the entitlement after opening the web checkout. */
	UPROPERTY(EditAnywhere, Config, Category = "Desktop", meta = (ClampMin = "10", ClampMax = "600"))
	int32 CheckoutPollTimeoutSeconds = 180;

	/** Verbose SDK logging. */
	UPROPERTY(EditAnywhere, Config, Category = "Debug")
	bool bEnableDebugLogs = false;
};
