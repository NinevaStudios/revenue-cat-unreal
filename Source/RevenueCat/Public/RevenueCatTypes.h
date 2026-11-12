// Copyright (c) 2026 Nineva Studios
// USTRUCT mirrors of the JSON that purchases-hybrid-common / REST returns,
// plus the Blueprint-facing delegates.

#pragma once

#include "CoreMinimal.h"
#include "RevenueCatTypes.generated.h"

/** How an entitlement was granted (Apple Family Sharing aware). */
UENUM(BlueprintType)
enum class ERCOwnershipType : uint8
{
	Purchased    UMETA(DisplayName = "Purchased"),
	FamilyShared UMETA(DisplayName = "Family Shared"),
	Unknown      UMETA(DisplayName = "Unknown")
};

/**
 * Error codes you can compare FRCError::Code against.
 * Positive values come from the RevenueCat SDK (same numbers on every platform);
 * negative values are produced by this plugin. Desktop REST failures carry RevenueCat backend codes (7xxx).
 */
namespace RevenueCatErrorCodes
{
	// Plugin
	constexpr int32 Internal              = -1; // native call failed / unexpected payload (see Message)
	constexpr int32 WebPurchaseLinkNotSet = -2; // desktop: "Web Purchase Link Url" is empty
	constexpr int32 CheckoutTimeout       = -3; // desktop: no new entitlement before the poll timeout
	constexpr int32 NotConfigured         = -4; // Configure was not called or the platform API key is empty

	// RevenueCat SDK (subset)
	constexpr int32 Unknown                    = 0;
	constexpr int32 PurchaseCancelled          = 1;
	constexpr int32 StoreProblem               = 2;
	constexpr int32 PurchaseNotAllowed         = 3;
	constexpr int32 PurchaseInvalid            = 4;
	constexpr int32 ProductNotAvailable        = 5;
	constexpr int32 ProductAlreadyPurchased    = 6;
	constexpr int32 ReceiptAlreadyInUse        = 7;
	constexpr int32 Network                    = 10;
	constexpr int32 InvalidCredentials         = 11;
	constexpr int32 OperationAlreadyInProgress = 15;
	constexpr int32 PaymentPending             = 20;
	constexpr int32 LogOutWithAnonymousUser    = 22;
	constexpr int32 Configuration              = 23;
}

/** Error returned by every async call. Check HasError (C++) or "Code != 0" (Blueprint) first. */
USTRUCT(BlueprintType)
struct FRCError
{
	GENERATED_BODY()

	/** 0 = no error. Positive = RevenueCat SDK code, negative = plugin code, 7xxx = RevenueCat backend (desktop). */
	UPROPERTY(BlueprintReadOnly, Category = "RevenueCat") int32 Code = 0;

	/** Human-readable description. Log it; do not show raw messages to players. */
	UPROPERTY(BlueprintReadOnly, Category = "RevenueCat") FString Message;

	/** SDK name of the code, e.g. "PurchaseCancelledError" (mobile only). */
	UPROPERTY(BlueprintReadOnly, Category = "RevenueCat") FString ReadableErrorCode;

	/** True when the player closed the store dialog. Not a real failure — do not show an error. */
	UPROPERTY(BlueprintReadOnly, Category = "RevenueCat") bool bUserCancelled = false;

	bool HasError() const { return Code != 0 || !Message.IsEmpty(); }
};

/** A single entitlement (a level of access the customer is "entitled" to). */
USTRUCT(BlueprintType)
struct FRCEntitlementInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "RevenueCat") FString Identifier;
	UPROPERTY(BlueprintReadOnly, Category = "RevenueCat") bool bIsActive = false;
	UPROPERTY(BlueprintReadOnly, Category = "RevenueCat") FString ProductIdentifier;
	UPROPERTY(BlueprintReadOnly, Category = "RevenueCat") FDateTime ExpirationDate;
	UPROPERTY(BlueprintReadOnly, Category = "RevenueCat") ERCOwnershipType OwnershipType = ERCOwnershipType::Unknown;
};

/** The whole picture of a customer's access — the object you base access decisions on. */
USTRUCT(BlueprintType)
struct FRCCustomerInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "RevenueCat") FString OriginalAppUserId;
	UPROPERTY(BlueprintReadOnly, Category = "RevenueCat") TMap<FString, FRCEntitlementInfo> ActiveEntitlements;
	UPROPERTY(BlueprintReadOnly, Category = "RevenueCat") TArray<FString> ActiveSubscriptions;
	UPROPERTY(BlueprintReadOnly, Category = "RevenueCat") FString ManagementURL;
};

/** A store SKU with localized pricing. */
USTRUCT(BlueprintType)
struct FRCStoreProduct
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "RevenueCat") FString Identifier;
	UPROPERTY(BlueprintReadOnly, Category = "RevenueCat") FString PriceString; // localized, e.g. "$9.99"
	UPROPERTY(BlueprintReadOnly, Category = "RevenueCat") float Price = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "RevenueCat") FString CurrencyCode;
	UPROPERTY(BlueprintReadOnly, Category = "RevenueCat") FString Title;
	UPROPERTY(BlueprintReadOnly, Category = "RevenueCat") FString Description;
};

/** Cross-platform bundle: one package -> equivalent product on the current store. */
USTRUCT(BlueprintType)
struct FRCPackage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "RevenueCat") FString Identifier; // $rc_monthly, $rc_annual, ...
	UPROPERTY(BlueprintReadOnly, Category = "RevenueCat") FString OfferingIdentifier; // owning offering — needed to purchase
	UPROPERTY(BlueprintReadOnly, Category = "RevenueCat") FRCStoreProduct StoreProduct;
};

/** A "paywall": a set of packages. */
USTRUCT(BlueprintType)
struct FRCOffering
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "RevenueCat") FString Identifier;
	UPROPERTY(BlueprintReadOnly, Category = "RevenueCat") TArray<FRCPackage> Packages;
};

/** All offerings + the current one (what you usually display). */
USTRUCT(BlueprintType)
struct FRCOfferings
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "RevenueCat") FRCOffering Current;
	UPROPERTY(BlueprintReadOnly, Category = "RevenueCat") TMap<FString, FRCOffering> All;
};

// --- Blueprint-facing delegates (dynamic so they can be passed from BP) ---
DECLARE_DYNAMIC_DELEGATE_TwoParams(FRCOnOfferings, const FRCOfferings&, Offerings, const FRCError&, Error);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FRCOnCustomer, const FRCCustomerInfo&, CustomerInfo, const FRCError&, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRCOnCustomerUpdated, const FRCCustomerInfo&, CustomerInfo);
