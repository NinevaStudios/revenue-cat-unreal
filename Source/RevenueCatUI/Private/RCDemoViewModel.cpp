// RevenueCat UE plugin.

#include "RCDemoViewModel.h"
#include "RCPackageItemViewModel.h"
#include "RevenueCatSubsystem.h"
#include "RevenueCatSettings.h"
#include "HAL/PlatformProcess.h"
#include "Misc/DateTime.h"

namespace
{
	constexpr int32 MaxLogLines = 80;
}

// ---- setup -----------------------------------------------------------------

void URCDemoViewModel::Initialize(URevenueCatSubsystem* InSubsystem)
{
	Subsystem = InSubsystem;
	if (InSubsystem)
	{
		InSubsystem->OnCustomerInfoUpdated.AddDynamic(this, &URCDemoViewModel::HandleCustomerInfoUpdated);
	}

	StatusText = FText::FromString(TEXT("Not configured"));
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(StatusText);
	AppendLog(TEXT("ViewModel ready"));
}

void URCDemoViewModel::Shutdown()
{
	if (URevenueCatSubsystem* Pinned = Subsystem.Get())
	{
		Pinned->OnCustomerInfoUpdated.RemoveDynamic(this, &URCDemoViewModel::HandleCustomerInfoUpdated);
	}
	Subsystem.Reset();
}

// ---- commands ---------------------------------------------------------------

void URCDemoViewModel::ConfigureCmd()
{
	if (!CheckReady(TEXT("Configure"))) { return; }

	// Fail loudly when the key for THIS platform is missing — otherwise every later call just
	// reports "not configured" from the native SDK and the real cause stays hidden.
	{
		const URevenueCatSettings* Settings = GetDefault<URevenueCatSettings>();
		const TCHAR* KeyField;
		FString KeyValue;
#if PLATFORM_IOS
		KeyField = TEXT("Apple Api Key");
		KeyValue = Settings->AppleApiKey;
#elif PLATFORM_ANDROID
		KeyField = TEXT("Google Api Key");
		KeyValue = Settings->GoogleApiKey;
#else
		KeyField = TEXT("Web Billing Api Key");
		KeyValue = Settings->WebBillingApiKey;
#endif
		if (KeyValue.IsEmpty())
		{
			AppendLog(FString::Printf(
				TEXT("Configure aborted: '%s' is empty (Project Settings > Plugins > RevenueCat)"), KeyField), true);
			return;
		}
	}

	Subsystem->Configure(AppUserId);

	UE_MVVM_SET_PROPERTY_VALUE(bIsConfigured, true);
	const FString Platform = FPlatformProperties::IniPlatformName();
	StatusText = FText::FromString(FString::Printf(TEXT("Configured - %s"), *Platform));
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(StatusText);
	AppendLog(FString::Printf(TEXT("Configure(%s) on %s"), *AppUserId, *Platform));

	RefreshCustomerInfo();
	RefreshOfferings();
}

void URCDemoViewModel::LogInCmd()
{
	if (!CheckReady(TEXT("LogIn"))) { return; }
	SetBusy(true);
	AppendLog(FString::Printf(TEXT("LogIn(%s)..."), *AppUserId));

	FRCOnCustomer Done;
	Done.BindDynamic(this, &URCDemoViewModel::HandleLoginResult);
	Subsystem->LogIn(AppUserId, Done);
}

void URCDemoViewModel::LogOutCmd()
{
	if (!CheckReady(TEXT("LogOut"))) { return; }
	SetBusy(true);
	AppendLog(TEXT("LogOut..."));

	FRCOnCustomer Done;
	Done.BindDynamic(this, &URCDemoViewModel::HandleLogoutResult);
	Subsystem->LogOut(Done);
}

void URCDemoViewModel::RefreshCustomerInfo()
{
	if (!CheckReady(TEXT("GetCustomerInfo"))) { return; }
	SetBusy(true);

	FRCOnCustomer Done;
	Done.BindDynamic(this, &URCDemoViewModel::HandleCustomerInfoResult);
	Subsystem->GetCustomerInfo(Done);
}

void URCDemoViewModel::RefreshOfferings()
{
	if (!CheckReady(TEXT("GetOfferings"))) { return; }
	SetBusy(true);

	FRCOnOfferings Done;
	Done.BindDynamic(this, &URCDemoViewModel::HandleOfferingsResult);
	Subsystem->GetOfferings(Done);
}

void URCDemoViewModel::RestoreCmd()
{
	if (!CheckReady(TEXT("Restore"))) { return; }
	SetBusy(true);
	AppendLog(TEXT("RestorePurchases..."));

	FRCOnCustomer Done;
	Done.BindDynamic(this, &URCDemoViewModel::HandleRestoreResult);
	Subsystem->RestorePurchases(Done);
}

void URCDemoViewModel::OpenManagementUrl()
{
	if (!ManagementUrl.IsEmpty())
	{
		FPlatformProcess::LaunchURL(*ManagementUrl, nullptr, nullptr);
	}
}

void URCDemoViewModel::Purchase(URCPackageItemViewModel* Item)
{
	if (!Item || !CheckReady(TEXT("Purchase"))) { return; }
	if (PendingPurchase.IsValid())
	{
		AppendLog(TEXT("Purchase already in progress"), true);
		return;
	}

	PendingPurchase = Item;
	Item->SetIsPurchasing(true);
	SetBusy(true);
	AppendLog(FString::Printf(TEXT("Purchase %s (%s)..."),
		*Item->GetSourcePackage().Identifier, *Item->GetSourcePackage().OfferingIdentifier));

	FRCOnCustomer Done;
	Done.BindDynamic(this, &URCDemoViewModel::HandlePurchaseResult);
	Subsystem->PurchaseStorePackage(Item->GetSourcePackage(), Done);
}

void URCDemoViewModel::SetAppUserId(const FString& InValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(AppUserId, InValue);
}

// ---- handlers ----------------------------------------------------------------

void URCDemoViewModel::HandleCustomerInfoUpdated(const FRCCustomerInfo& CustomerInfo)
{
	UpdateFromCustomerInfo(CustomerInfo);
}

void URCDemoViewModel::HandleCustomerInfoResult(const FRCCustomerInfo& CustomerInfo, const FRCError& Error)
{
	SetBusy(false);
	if (Error.HasError())
	{
		AppendLog(FString::Printf(TEXT("GetCustomerInfo failed: %s"), *Error.Message), true);
		return;
	}
	AppendLog(FString::Printf(TEXT("GetCustomerInfo -> %d active entitlement(s)"),
		CustomerInfo.ActiveEntitlements.Num()));
}

void URCDemoViewModel::HandleLoginResult(const FRCCustomerInfo& CustomerInfo, const FRCError& Error)
{
	SetBusy(false);
	if (Error.HasError()) { AppendLog(FString::Printf(TEXT("LogIn failed: %s"), *Error.Message), true); return; }
	AppendLog(FString::Printf(TEXT("LogIn OK -> %s"), *CustomerInfo.OriginalAppUserId));
}

void URCDemoViewModel::HandleLogoutResult(const FRCCustomerInfo& CustomerInfo, const FRCError& Error)
{
	SetBusy(false);
	if (Error.HasError()) { AppendLog(FString::Printf(TEXT("LogOut failed: %s"), *Error.Message), true); return; }
	AppendLog(TEXT("LogOut OK (anonymous user)"));
}

void URCDemoViewModel::HandleRestoreResult(const FRCCustomerInfo& CustomerInfo, const FRCError& Error)
{
	SetBusy(false);
	if (Error.HasError()) { AppendLog(FString::Printf(TEXT("Restore failed: %s"), *Error.Message), true); return; }
	AppendLog(FString::Printf(TEXT("Restore OK -> %d active entitlement(s)"),
		CustomerInfo.ActiveEntitlements.Num()));
}

void URCDemoViewModel::HandlePurchaseResult(const FRCCustomerInfo& CustomerInfo, const FRCError& Error)
{
	SetBusy(false);
	if (URCPackageItemViewModel* Item = PendingPurchase.Get())
	{
		Item->SetIsPurchasing(false);
	}
	PendingPurchase.Reset();

	if (Error.HasError())
	{
		const TCHAR* Suffix = Error.bUserCancelled ? TEXT(" (user cancelled)") : TEXT("");
		AppendLog(FString::Printf(TEXT("Purchase failed: %s%s"), *Error.Message, Suffix), true);
		return;
	}
	AppendLog(FString::Printf(TEXT("Purchase OK -> %d active entitlement(s)"),
		CustomerInfo.ActiveEntitlements.Num()));
}

void URCDemoViewModel::HandleOfferingsResult(const FRCOfferings& Offerings, const FRCError& Error)
{
	SetBusy(false);
	if (Error.HasError())
	{
		AppendLog(FString::Printf(TEXT("GetOfferings failed: %s"), *Error.Message), true);
		return;
	}

	TArray<TObjectPtr<URCPackageItemViewModel>> NewItems;
	for (const FRCPackage& Pkg : Offerings.Current.Packages)
	{
		URCPackageItemViewModel* Item = NewObject<URCPackageItemViewModel>(this);
		Item->Init(this, Pkg);
		NewItems.Add(Item);
	}
	UE_MVVM_SET_PROPERTY_VALUE(Packages, NewItems);

	AppendLog(FString::Printf(TEXT("GetOfferings -> '%s' (%d package(s))"),
		*Offerings.Current.Identifier, NewItems.Num()));
}

// ---- helpers ------------------------------------------------------------------

void URCDemoViewModel::UpdateFromCustomerInfo(const FRCCustomerInfo& Info)
{
	const FRCEntitlementInfo* Found = Info.ActiveEntitlements.Find(WatchedEntitlementId);
	UE_MVVM_SET_PROPERTY_VALUE(bHasEntitlement, Found != nullptr);

	FString EntitlementStr;
	if (Found)
	{
		EntitlementStr = (Found->ExpirationDate == FDateTime())
			? FString::Printf(TEXT("%s - active (non-expiring)"), *WatchedEntitlementId)
			: FString::Printf(TEXT("%s - active until %s"),
				*WatchedEntitlementId, *Found->ExpirationDate.ToString(TEXT("%Y-%m-%d")));
	}
	else
	{
		EntitlementStr = FString::Printf(TEXT("%s - inactive"), *WatchedEntitlementId);
	}
	EntitlementText = FText::FromString(EntitlementStr);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(EntitlementText);

	SubscriptionsText = FText::FromString(Info.ActiveSubscriptions.Num() > 0
		? FString::Join(Info.ActiveSubscriptions, TEXT(", "))
		: FString(TEXT("-")));
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SubscriptionsText);

	UE_MVVM_SET_PROPERTY_VALUE(ManagementUrl, Info.ManagementURL);
}

void URCDemoViewModel::AppendLog(const FString& Line, bool bError)
{
	const FString Stamped = FString::Printf(TEXT("[%s]%s %s"),
		*FDateTime::Now().ToString(TEXT("%H:%M:%S")),
		bError ? TEXT(" [ERROR]") : TEXT(""),
		*Line);

	LogLines.Insert(Stamped, 0); // newest first — no autoscroll needed
	if (LogLines.Num() > MaxLogLines)
	{
		LogLines.RemoveAt(LogLines.Num() - 1);
	}

	LogText = FText::FromString(FString::Join(LogLines, TEXT("\n")));
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(LogText);
}

void URCDemoViewModel::SetBusy(bool bInBusy)
{
	UE_MVVM_SET_PROPERTY_VALUE(bIsBusy, bInBusy);
}

bool URCDemoViewModel::CheckReady(const TCHAR* Op)
{
	if (!Subsystem.IsValid())
	{
		AppendLog(FString::Printf(TEXT("%s skipped: subsystem not set (call Initialize)"), Op), true);
		return false;
	}
	return true;
}
