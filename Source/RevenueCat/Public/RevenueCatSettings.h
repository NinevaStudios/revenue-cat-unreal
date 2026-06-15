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

	/** Verbose SDK logging. */
	UPROPERTY(EditAnywhere, Config, Category = "Debug")
	bool bEnableDebugLogs = false;
};
