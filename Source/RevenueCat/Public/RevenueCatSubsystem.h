// Copyright (c) 2026 Nineva Studios
// Platform-agnostic Blueprint/C++ facade. Routes calls to a per-platform IRevenueCatImpl.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RevenueCatTypes.h"
#include "RevenueCatSubsystem.generated.h"

class IRevenueCatImpl;

/**
 * Entry point of the plugin. One instance per Game Instance.
 * C++: GetGameInstance()->GetSubsystem<URevenueCatSubsystem>()
 * Blueprint: "Get RevenueCat Subsystem" (or "Get Game Instance Subsystem").
 * All callbacks are invoked on the Game Thread.
 */
UCLASS()
class REVENUECAT_API URevenueCatSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// UGameInstanceSubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * Initializes RevenueCat. Call once, as early as possible, before any other function.
	 * The API key is taken from Project Settings -> Plugins -> RevenueCat for the current platform.
	 * @param AppUserId Your own user ID. Leave empty to use an anonymous ID (kept between launches).
	 */
	UFUNCTION(BlueprintCallable, Category = "RevenueCat")
	void Configure(const FString& AppUserId);

	/** True after a successful Configure call (an API key was found for this platform). */
	UFUNCTION(BlueprintPure, Category = "RevenueCat")
	bool IsConfigured() const { return bConfigured; }

	/** Fetches the offerings configured in the dashboard. Use Offerings.Current to build your paywall. */
	UFUNCTION(BlueprintCallable, Category = "RevenueCat")
	void GetOfferings(const FRCOnOfferings& OnDone);

	/** Purchases a package received from Get Offerings (recommended way to purchase). */
	UFUNCTION(BlueprintCallable, Category = "RevenueCat")
	void PurchaseStorePackage(const FRCPackage& Package, const FRCOnCustomer& OnDone);

	/** Purchases a package by identifiers, e.g. "$rc_monthly" in offering "default". Both identifiers are required. */
	UFUNCTION(BlueprintCallable, Category = "RevenueCat")
	void PurchasePackage(const FString& PackageIdentifier, const FString& OfferingIdentifier, const FRCOnCustomer& OnDone);

	/** Fetches the latest Customer Info: active entitlements and subscriptions of the current user. */
	UFUNCTION(BlueprintCallable, Category = "RevenueCat")
	void GetCustomerInfo(const FRCOnCustomer& OnDone);

	/** Restores purchases made with the current store account. Call it from a "Restore Purchases" button. */
	UFUNCTION(BlueprintCallable, Category = "RevenueCat")
	void RestorePurchases(const FRCOnCustomer& OnDone);

	/** Switches to your own user ID. Call right after the player signs in to your account system. */
	UFUNCTION(BlueprintCallable, Category = "RevenueCat")
	void LogIn(const FString& AppUserId, const FRCOnCustomer& OnDone);

	/** Switches back to a new anonymous user. Call when the player signs out. Fails if the current user is already anonymous. */
	UFUNCTION(BlueprintCallable, Category = "RevenueCat")
	void LogOut(const FRCOnCustomer& OnDone);

	/**
	 * Checks the last received Customer Info for an active entitlement. Does not make a request.
	 * Use it for UI only — protect valuable content on your server.
	 */
	UFUNCTION(BlueprintPure, Category = "RevenueCat")
	bool HasActiveEntitlement(const FString& EntitlementId) const;

	/** Last Customer Info received by any call. Empty until the first successful request. */
	UFUNCTION(BlueprintPure, Category = "RevenueCat")
	FRCCustomerInfo GetCachedCustomerInfo() const { return CachedCustomerInfo; }

	/** Fires every time new Customer Info is received (purchase, restore, log in / out, refresh). */
	UPROPERTY(BlueprintAssignable, Category = "RevenueCat")
	FRCOnCustomerUpdated OnCustomerInfoUpdated;

private:
	/** Per-platform backend (iOS / Android / Desktop / Null). Owned; deleted in Deinitialize(). */
	IRevenueCatImpl* Impl = nullptr;

	bool bConfigured = false;

	/** Last known customer info, refreshed on every successful call. */
	UPROPERTY()
	FRCCustomerInfo CachedCustomerInfo;

	/** Fills OutError and returns false when calls cannot be made yet. */
	bool CheckReady(FRCError& OutError) const;

	/** Wraps a customer-returning call: parse, cache, broadcast, log errors, invoke the user's delegate. */
	TFunction<void(const FString&, bool)> MakeCustomerHandler(const TCHAR* CallName, const FRCOnCustomer& OnDone);

	/** Helper: cache + broadcast. */
	void UpdateCache(const FRCCustomerInfo& Info);
};
