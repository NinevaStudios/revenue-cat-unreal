// RevenueCat UE plugin — learning skeleton.

#include "RevenueCatSubsystem.h"
#include "RevenueCatSettings.h"
#include "RevenueCatJson.h"
#include "Platform/IRevenueCatImpl.h"

void URevenueCatSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Impl = CreateRevenueCatImpl();
}

void URevenueCatSubsystem::Deinitialize()
{
	delete Impl;
	Impl = nullptr;
	Super::Deinitialize();
}

void URevenueCatSubsystem::Configure(const FString& AppUserId)
{
	const URevenueCatSettings* Settings = GetDefault<URevenueCatSettings>();

	FString ApiKey;
#if PLATFORM_IOS
	ApiKey = Settings->AppleApiKey;
#elif PLATFORM_ANDROID
	ApiKey = Settings->GoogleApiKey;
#else
	ApiKey = Settings->WebBillingApiKey;
#endif

	if (Impl)
	{
		Impl->Configure(ApiKey, AppUserId);
	}
}

void URevenueCatSubsystem::GetOfferings(const FRCOnOfferings& OnDone)
{
	if (!Impl)
	{
		FRCError Err; Err.Code = -1; Err.Message = TEXT("RevenueCat not initialized");
		OnDone.ExecuteIfBound(FRCOfferings(), Err);
		return;
	}

	FRCOnOfferings Callback = OnDone;
	Impl->GetOfferings([Callback](const FString& Json, bool bIsError)
	{
		Callback.ExecuteIfBound(RevenueCatJson::ParseOfferings(Json), RevenueCatJson::ParseError(Json, bIsError));
	});
}

void URevenueCatSubsystem::PurchaseStorePackage(const FRCPackage& Package, const FRCOnCustomer& OnDone)
{
	PurchasePackage(Package.Identifier, Package.OfferingIdentifier, OnDone);
}

void URevenueCatSubsystem::PurchasePackage(const FString& PackageIdentifier, const FString& OfferingIdentifier, const FRCOnCustomer& OnDone)
{
	if (!Impl)
	{
		FRCError Err; Err.Code = -1; Err.Message = TEXT("RevenueCat not initialized");
		OnDone.ExecuteIfBound(FRCCustomerInfo(), Err);
		return;
	}

	FRCOnCustomer Callback = OnDone;
	Impl->PurchasePackage(PackageIdentifier, OfferingIdentifier, [this, Callback](const FString& Json, bool bIsError)
	{
		const FRCCustomerInfo Info = RevenueCatJson::ParseCustomerInfo(Json);
		if (!bIsError) { UpdateCache(Info); }
		Callback.ExecuteIfBound(Info, RevenueCatJson::ParseError(Json, bIsError));
	});
}

void URevenueCatSubsystem::GetCustomerInfo(const FRCOnCustomer& OnDone)
{
	if (!Impl)
	{
		FRCError Err; Err.Code = -1; Err.Message = TEXT("RevenueCat not initialized");
		OnDone.ExecuteIfBound(FRCCustomerInfo(), Err);
		return;
	}

	FRCOnCustomer Callback = OnDone;
	Impl->GetCustomerInfo([this, Callback](const FString& Json, bool bIsError)
	{
		const FRCCustomerInfo Info = RevenueCatJson::ParseCustomerInfo(Json);
		if (!bIsError) { UpdateCache(Info); }
		Callback.ExecuteIfBound(Info, RevenueCatJson::ParseError(Json, bIsError));
	});
}

void URevenueCatSubsystem::RestorePurchases(const FRCOnCustomer& OnDone)
{
	if (!Impl)
	{
		FRCError Err; Err.Code = -1; Err.Message = TEXT("RevenueCat not initialized");
		OnDone.ExecuteIfBound(FRCCustomerInfo(), Err);
		return;
	}

	FRCOnCustomer Callback = OnDone;
	Impl->RestorePurchases([this, Callback](const FString& Json, bool bIsError)
	{
		const FRCCustomerInfo Info = RevenueCatJson::ParseCustomerInfo(Json);
		if (!bIsError) { UpdateCache(Info); }
		Callback.ExecuteIfBound(Info, RevenueCatJson::ParseError(Json, bIsError));
	});
}

void URevenueCatSubsystem::LogIn(const FString& AppUserId, const FRCOnCustomer& OnDone)
{
	if (!Impl)
	{
		FRCError Err; Err.Code = -1; Err.Message = TEXT("RevenueCat not initialized");
		OnDone.ExecuteIfBound(FRCCustomerInfo(), Err);
		return;
	}

	FRCOnCustomer Callback = OnDone;
	Impl->LogIn(AppUserId, [this, Callback](const FString& Json, bool bIsError)
	{
		const FRCCustomerInfo Info = RevenueCatJson::ParseCustomerInfo(Json);
		if (!bIsError) { UpdateCache(Info); }
		Callback.ExecuteIfBound(Info, RevenueCatJson::ParseError(Json, bIsError));
	});
}

void URevenueCatSubsystem::LogOut(const FRCOnCustomer& OnDone)
{
	if (!Impl)
	{
		FRCError Err; Err.Code = -1; Err.Message = TEXT("RevenueCat not initialized");
		OnDone.ExecuteIfBound(FRCCustomerInfo(), Err);
		return;
	}

	FRCOnCustomer Callback = OnDone;
	Impl->LogOut([this, Callback](const FString& Json, bool bIsError)
	{
		const FRCCustomerInfo Info = RevenueCatJson::ParseCustomerInfo(Json);
		if (!bIsError) { UpdateCache(Info); }
		Callback.ExecuteIfBound(Info, RevenueCatJson::ParseError(Json, bIsError));
	});
}

bool URevenueCatSubsystem::HasActiveEntitlement(const FString& EntitlementId) const
{
	if (const FRCEntitlementInfo* Found = CachedCustomerInfo.ActiveEntitlements.Find(EntitlementId))
	{
		return Found->bIsActive;
	}
	return false;
}

void URevenueCatSubsystem::UpdateCache(const FRCCustomerInfo& Info)
{
	CachedCustomerInfo = Info;
	OnCustomerInfoUpdated.Broadcast(Info);
}
