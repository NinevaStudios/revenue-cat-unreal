// Copyright (c) 2026 Nineva Studios
// Pure conversion functions for MVVM view bindings.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Styling/SlateColor.h"
#include "RCDemoConversions.generated.h"

UCLASS()
class REVENUECATUI_API URCDemoConversions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Entitlement badge tint: green when active, gray otherwise. */
	UFUNCTION(BlueprintPure, Category = "RevenueCat|Demo")
	static FSlateColor ActiveToColor(bool bActive);

	/** For Buy.bIsEnabled <- NOT bIsPurchasing bindings. */
	UFUNCTION(BlueprintPure, Category = "RevenueCat|Demo")
	static bool NotBool(bool bValue);

	/** Manage button enabled only when the store returned a URL. */
	UFUNCTION(BlueprintPure, Category = "RevenueCat|Demo")
	static bool IsNotEmpty(const FString& Value);
};
