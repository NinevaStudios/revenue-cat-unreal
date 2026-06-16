// RevenueCat UE plugin — learning skeleton.
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

/** Normalized error returned by every async call. */
USTRUCT(BlueprintType)
struct FRCError
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "RevenueCat") int32 Code = 0;
	UPROPERTY(BlueprintReadOnly, Category = "RevenueCat") FString Message;
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
