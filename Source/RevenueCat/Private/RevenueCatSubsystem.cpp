// Copyright (c) 2026 Nineva Studios

#include "RevenueCatSubsystem.h"
#include "RevenueCatSettings.h"
#include "RevenueCatJson.h"
#include "RevenueCatLog.h"
#include "Platform/IRevenueCatImpl.h"

DEFINE_LOG_CATEGORY(LogRevenueCat);

void URevenueCatSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Impl = CreateRevenueCatImpl();
}

void URevenueCatSubsystem::Deinitialize()
{
	delete Impl;
	Impl = nullptr;
	bConfigured = false;
	Super::Deinitialize();
}

void URevenueCatSubsystem::Configure(const FString& AppUserId)
{
	const URevenueCatSettings* Settings = GetDefault<URevenueCatSettings>();

	FString ApiKey;
	const TCHAR* KeyField = nullptr;
#if PLATFORM_IOS
	ApiKey = Settings->AppleApiKey;
	KeyField = TEXT("Apple Api Key");
#elif PLATFORM_ANDROID
	ApiKey = Settings->GoogleApiKey;
	KeyField = TEXT("Google Api Key");
#else
	ApiKey = Settings->WebBillingApiKey;
	KeyField = TEXT("Web Billing Api Key");
#endif
	ApiKey.TrimStartAndEndInline();

	if (!Impl)
	{
		UE_LOG(LogRevenueCat, Error, TEXT("Configure failed: RevenueCat is not available on this platform."));
		return;
	}

	if (ApiKey.IsEmpty())
	{
		bConfigured = false;
		UE_LOG(LogRevenueCat, Error, TEXT("Configure failed: '%s' is empty. Set it in Project Settings > Plugins > RevenueCat."), KeyField);
		return;
	}

	if (ApiKey.StartsWith(TEXT("sk_")))
	{
		UE_LOG(LogRevenueCat, Error, TEXT("'%s' is a SECRET key (sk_...). Never ship secret keys in a game build - use the public key of the app."), KeyField);
	}

	Impl->Configure(ApiKey, AppUserId, Settings->bEnableDebugLogs);
	bConfigured = true;
	UE_LOG(LogRevenueCat, Log, TEXT("Configured (%s)%s"), KeyField,
		AppUserId.IsEmpty() ? TEXT(", anonymous user") : *FString::Printf(TEXT(", App User ID '%s'"), *AppUserId));
}

bool URevenueCatSubsystem::CheckReady(FRCError& OutError) const
{
	if (!Impl)
	{
		OutError.Code = RevenueCatErrorCodes::Internal;
		OutError.Message = TEXT("RevenueCat is not available on this platform.");
		return false;
	}
	if (!bConfigured)
	{
		OutError.Code = RevenueCatErrorCodes::NotConfigured;
		OutError.Message = TEXT("RevenueCat is not configured. Call Configure first and set the API key for this platform in Project Settings > Plugins > RevenueCat.");
		return false;
	}
	return true;
}

TFunction<void(const FString&, bool)> URevenueCatSubsystem::MakeCustomerHandler(const TCHAR* CallName, const FRCOnCustomer& OnDone)
{
	TWeakObjectPtr<URevenueCatSubsystem> WeakThis(this);
	const FString Name(CallName);
	return [WeakThis, Name, OnDone](const FString& Json, bool bIsError)
	{
		const FRCCustomerInfo Info = bIsError ? FRCCustomerInfo() : RevenueCatJson::ParseCustomerInfo(Json);
		const FRCError Error = RevenueCatJson::ParseError(Json, bIsError);

		if (Error.HasError())
		{
			UE_LOG(LogRevenueCat, Warning, TEXT("%s failed: [%d] %s"), *Name, Error.Code, *Error.Message);
		}
		else if (URevenueCatSubsystem* Self = WeakThis.Get())
		{
			Self->UpdateCache(Info);
		}

		OnDone.ExecuteIfBound(Info, Error);
	};
}

void URevenueCatSubsystem::GetOfferings(const FRCOnOfferings& OnDone)
{
	FRCError NotReady;
	if (!CheckReady(NotReady))
	{
		UE_LOG(LogRevenueCat, Warning, TEXT("GetOfferings failed: [%d] %s"), NotReady.Code, *NotReady.Message);
		OnDone.ExecuteIfBound(FRCOfferings(), NotReady);
		return;
	}

	Impl->GetOfferings([OnDone](const FString& Json, bool bIsError)
	{
		const FRCError Error = RevenueCatJson::ParseError(Json, bIsError);
		if (Error.HasError())
		{
			UE_LOG(LogRevenueCat, Warning, TEXT("GetOfferings failed: [%d] %s"), Error.Code, *Error.Message);
		}
		OnDone.ExecuteIfBound(bIsError ? FRCOfferings() : RevenueCatJson::ParseOfferings(Json), Error);
	});
}

void URevenueCatSubsystem::PurchaseStorePackage(const FRCPackage& Package, const FRCOnCustomer& OnDone)
{
	PurchasePackage(Package.Identifier, Package.OfferingIdentifier, OnDone);
}

void URevenueCatSubsystem::PurchasePackage(const FString& PackageIdentifier, const FString& OfferingIdentifier, const FRCOnCustomer& OnDone)
{
	FRCError NotReady;
	if (!CheckReady(NotReady))
	{
		MakeCustomerHandler(TEXT("PurchasePackage"), OnDone)(FString::Printf(TEXT("{\"code\":%d,\"message\":\"%s\"}"), NotReady.Code, *NotReady.Message), true);
		return;
	}
	Impl->PurchasePackage(PackageIdentifier, OfferingIdentifier, MakeCustomerHandler(TEXT("PurchasePackage"), OnDone));
}

void URevenueCatSubsystem::GetCustomerInfo(const FRCOnCustomer& OnDone)
{
	FRCError NotReady;
	if (!CheckReady(NotReady))
	{
		MakeCustomerHandler(TEXT("GetCustomerInfo"), OnDone)(FString::Printf(TEXT("{\"code\":%d,\"message\":\"%s\"}"), NotReady.Code, *NotReady.Message), true);
		return;
	}
	Impl->GetCustomerInfo(MakeCustomerHandler(TEXT("GetCustomerInfo"), OnDone));
}

void URevenueCatSubsystem::RestorePurchases(const FRCOnCustomer& OnDone)
{
	FRCError NotReady;
	if (!CheckReady(NotReady))
	{
		MakeCustomerHandler(TEXT("RestorePurchases"), OnDone)(FString::Printf(TEXT("{\"code\":%d,\"message\":\"%s\"}"), NotReady.Code, *NotReady.Message), true);
		return;
	}
	Impl->RestorePurchases(MakeCustomerHandler(TEXT("RestorePurchases"), OnDone));
}

void URevenueCatSubsystem::LogIn(const FString& AppUserId, const FRCOnCustomer& OnDone)
{
	FRCError NotReady;
	if (!CheckReady(NotReady))
	{
		MakeCustomerHandler(TEXT("LogIn"), OnDone)(FString::Printf(TEXT("{\"code\":%d,\"message\":\"%s\"}"), NotReady.Code, *NotReady.Message), true);
		return;
	}
	if (AppUserId.TrimStartAndEnd().IsEmpty())
	{
		MakeCustomerHandler(TEXT("LogIn"), OnDone)(TEXT("{\"code\":14,\"message\":\"LogIn requires a non-empty App User ID.\"}"), true);
		return;
	}
	Impl->LogIn(AppUserId, MakeCustomerHandler(TEXT("LogIn"), OnDone));
}

void URevenueCatSubsystem::LogOut(const FRCOnCustomer& OnDone)
{
	FRCError NotReady;
	if (!CheckReady(NotReady))
	{
		MakeCustomerHandler(TEXT("LogOut"), OnDone)(FString::Printf(TEXT("{\"code\":%d,\"message\":\"%s\"}"), NotReady.Code, *NotReady.Message), true);
		return;
	}
	Impl->LogOut(MakeCustomerHandler(TEXT("LogOut"), OnDone));
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
