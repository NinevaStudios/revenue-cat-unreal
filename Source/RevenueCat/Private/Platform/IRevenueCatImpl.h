// RevenueCat UE plugin — learning skeleton.
// Platform abstraction. Each impl returns a RAW JSON string; parsing happens once in the subsystem.

#pragma once

#include "CoreMinimal.h"

class IRevenueCatImpl
{
public:
	virtual ~IRevenueCatImpl() = default;

	/** Called on the game thread. Json = serialized result (or error payload). */
	using FResultCallback = TFunction<void(const FString& /*Json*/, bool /*bIsError*/)>;

	virtual void Configure(const FString& ApiKey, const FString& AppUserId) = 0;
	virtual void GetOfferings(FResultCallback Cb) = 0;
	virtual void PurchasePackage(const FString& PackageId, const FString& OfferingId, FResultCallback Cb) = 0;
	virtual void GetCustomerInfo(FResultCallback Cb) = 0;
	virtual void RestorePurchases(FResultCallback Cb) = 0;
	virtual void LogIn(const FString& AppUserId, FResultCallback Cb) = 0;
	virtual void LogOut(FResultCallback Cb) = 0;
};

/**
 * Factory — exactly one definition is compiled per platform (see RevenueCatImpl_*.cpp/.mm).
 * Returns a raw, heap-allocated impl; the subsystem owns it and deletes it in Deinitialize().
 * (Raw pointer, not TUniquePtr, so an incomplete type in the UObject header doesn't force the
 * generated vtable-helper ctor to instantiate a destructor — avoids C4150.)
 */
IRevenueCatImpl* CreateRevenueCatImpl();
