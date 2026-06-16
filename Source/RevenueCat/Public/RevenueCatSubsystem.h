// RevenueCat UE plugin — learning skeleton.
// Platform-agnostic Blueprint/C++ facade. Routes calls to a per-platform IRevenueCatImpl.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RevenueCatTypes.h"
#include "RevenueCatSubsystem.generated.h"

class IRevenueCatImpl;

/**
 * Access via: GetGameInstance()->GetSubsystem<URevenueCatSubsystem>()
 * or in Blueprint: "Get RevenueCat Subsystem".
 */
UCLASS()
class REVENUECAT_API URevenueCatSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// UGameInstanceSubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Configure the SDK once, early. API key is chosen per-platform from URevenueCatSettings. */
	UFUNCTION(BlueprintCallable, Category = "RevenueCat")
	void Configure(const FString& AppUserId);

	/** Fetch the current offering + packages to build a paywall. */
	UFUNCTION(BlueprintCallable, Category = "RevenueCat")
	void GetOfferings(const FRCOnOfferings& OnDone);

	/** Preferred: purchase a package you got from GetOfferings (it carries its offering context). */
	UFUNCTION(BlueprintCallable, Category = "RevenueCat")
	void PurchaseStorePackage(const FRCPackage& Package, const FRCOnCustomer& OnDone);

	/** Lower-level purchase by identifiers. OfferingIdentifier is required for correct resolution. */
	UFUNCTION(BlueprintCallable, Category = "RevenueCat")
	void PurchasePackage(const FString& PackageIdentifier, const FString& OfferingIdentifier, const FRCOnCustomer& OnDone);

	/** Current entitlement / subscription state. */
	UFUNCTION(BlueprintCallable, Category = "RevenueCat")
	void GetCustomerInfo(const FRCOnCustomer& OnDone);

	/** Restore previous purchases for the current store account. */
	UFUNCTION(BlueprintCallable, Category = "RevenueCat")
	void RestorePurchases(const FRCOnCustomer& OnDone);

	/** Switch to an identified App User ID (call right after your auth succeeds). */
	UFUNCTION(BlueprintCallable, Category = "RevenueCat")
	void LogIn(const FString& AppUserId, const FRCOnCustomer& OnDone);

	/** Return to an anonymous App User ID. */
	UFUNCTION(BlueprintCallable, Category = "RevenueCat")
	void LogOut(const FRCOnCustomer& OnDone);

	/**
	 * Convenience check against the last cached CustomerInfo.
	 * UX ONLY — never gate real value on this. Gate it server-side.
	 */
	UFUNCTION(BlueprintPure, Category = "RevenueCat")
	bool HasActiveEntitlement(const FString& EntitlementId) const;

	/** Fires whenever CustomerInfo changes (after a purchase / restore / login). */
	UPROPERTY(BlueprintAssignable, Category = "RevenueCat")
	FRCOnCustomerUpdated OnCustomerInfoUpdated;

private:
	/** Per-platform backend (iOS / Android / Desktop / Null). Owned; deleted in Deinitialize(). */
	IRevenueCatImpl* Impl = nullptr;

	/** Last known customer info, refreshed on every successful call. */
	UPROPERTY()
	FRCCustomerInfo CachedCustomerInfo;

	/** Helper: cache + broadcast. */
	void UpdateCache(const FRCCustomerInfo& Info);
};
