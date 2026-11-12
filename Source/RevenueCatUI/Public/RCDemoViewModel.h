// Copyright (c) 2026 Nineva Studios
// Main MVVM viewmodel for the demo screen.
// View (WBP_RevenueCatDemo) binds to the FieldNotify properties and calls the command UFUNCTIONs;
// it contains no logic of its own. Model = URevenueCatSubsystem.

#pragma once

#include "MVVMViewModelBase.h"
#include "RevenueCatTypes.h"
#include "RCDemoViewModel.generated.h"

class URevenueCatSubsystem;
class URCPackageItemViewModel;

UCLASS(BlueprintType)
class REVENUECATUI_API URCDemoViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	// ---- setup (call from the widget) ------------------------------------

	/** Event Construct: pass GetGameInstance -> GetSubsystem(RevenueCatSubsystem). */
	UFUNCTION(BlueprintCallable, Category = "RevenueCat|Demo")
	void Initialize(URevenueCatSubsystem* InSubsystem);

	/** Event Destruct: unbinds from the subsystem. */
	UFUNCTION(BlueprintCallable, Category = "RevenueCat|Demo")
	void Shutdown();

	// ---- commands (bind buttons' OnClicked to these) ----------------------

	UFUNCTION(BlueprintCallable, Category = "RevenueCat|Demo")
	void ConfigureCmd();

	UFUNCTION(BlueprintCallable, Category = "RevenueCat|Demo")
	void LogInCmd();

	UFUNCTION(BlueprintCallable, Category = "RevenueCat|Demo")
	void LogOutCmd();

	UFUNCTION(BlueprintCallable, Category = "RevenueCat|Demo")
	void RefreshCustomerInfo();

	UFUNCTION(BlueprintCallable, Category = "RevenueCat|Demo")
	void RefreshOfferings();

	UFUNCTION(BlueprintCallable, Category = "RevenueCat|Demo")
	void RestoreCmd();

	UFUNCTION(BlueprintCallable, Category = "RevenueCat|Demo")
	void OpenManagementUrl();

	/** Called by URCPackageItemViewModel::PurchaseThis(). */
	UFUNCTION(BlueprintCallable, Category = "RevenueCat|Demo")
	void Purchase(URCPackageItemViewModel* Item);

	// ---- two-way field ----------------------------------------------------

	/** App User ID used by ConfigureCmd / LogInCmd. Two-way bindable. */
	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter, Getter, Category = "RevenueCat|Demo")
	FString AppUserId = TEXT("test_user_42");

	void SetAppUserId(const FString& InValue);
	const FString& GetAppUserId() const { return AppUserId; }

	// ---- one-way fields (View reads these) --------------------------------

	/** "Not configured" / "Configured - Windows". */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RevenueCat|Demo")
	FText StatusText;

	/** Gates the action buttons. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RevenueCat|Demo")
	bool bIsConfigured = false;

	/** Whether WatchedEntitlementId is currently active. Drives the badge color. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RevenueCat|Demo")
	bool bHasEntitlement = false;

	/** "pro - active until 2026-07-16" / "pro - inactive". */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RevenueCat|Demo")
	FText EntitlementText;

	/** Comma-joined active subscription ids, or "-". */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RevenueCat|Demo")
	FText SubscriptionsText;

	/** Empty when the store returned none; gates the Manage button. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RevenueCat|Demo")
	FString ManagementUrl;

	/** ListView items for the paywall. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RevenueCat|Demo")
	TArray<TObjectPtr<URCPackageItemViewModel>> Packages;

	/** True while any request is in flight — drives the global spinner. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RevenueCat|Demo")
	bool bIsBusy = false;

	/** Newest-first log, one line per callback. Bind to a TextBlock inside a ScrollBox. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RevenueCat|Demo")
	FText LogText;

	/** Which entitlement the badge watches. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RevenueCat|Demo")
	FString WatchedEntitlementId = TEXT("pro");

private:
	// Subsystem push: fires after every successful call — the ONLY place fields get updated from.
	UFUNCTION()
	void HandleCustomerInfoUpdated(const FRCCustomerInfo& CustomerInfo);

	// Per-operation result handlers: logging + busy/error handling only.
	UFUNCTION()
	void HandleCustomerInfoResult(const FRCCustomerInfo& CustomerInfo, const FRCError& Error);
	UFUNCTION()
	void HandleLoginResult(const FRCCustomerInfo& CustomerInfo, const FRCError& Error);
	UFUNCTION()
	void HandleLogoutResult(const FRCCustomerInfo& CustomerInfo, const FRCError& Error);
	UFUNCTION()
	void HandleRestoreResult(const FRCCustomerInfo& CustomerInfo, const FRCError& Error);
	UFUNCTION()
	void HandlePurchaseResult(const FRCCustomerInfo& CustomerInfo, const FRCError& Error);
	UFUNCTION()
	void HandleOfferingsResult(const FRCOfferings& Offerings, const FRCError& Error);

	void UpdateFromCustomerInfo(const FRCCustomerInfo& Info);
	void AppendLog(const FString& Line, bool bError = false);
	void SetBusy(bool bInBusy);
	bool CheckReady(const TCHAR* Op);

	TWeakObjectPtr<URevenueCatSubsystem> Subsystem;
	TWeakObjectPtr<URCPackageItemViewModel> PendingPurchase;
	TArray<FString> LogLines;
};
