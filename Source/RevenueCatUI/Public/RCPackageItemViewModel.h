// Copyright (c) 2026 Nineva Studios
// MVVM item viewmodel for one purchasable package (a ListView entry).

#pragma once

#include "MVVMViewModelBase.h"
#include "RevenueCatTypes.h"
#include "RCPackageItemViewModel.generated.h"

class URCDemoViewModel;

UCLASS(BlueprintType)
class REVENUECATUI_API URCPackageItemViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Called by URCDemoViewModel when building the package list. */
	void Init(URCDemoViewModel* InOwner, const FRCPackage& InPackage);

	/** Bind the row's Buy button OnClicked to this. */
	UFUNCTION(BlueprintCallable, Category = "RevenueCat|Demo")
	void PurchaseThis();

	void SetIsPurchasing(bool bInValue);
	const FRCPackage& GetSourcePackage() const { return SourcePackage; }

	/** "Monthly" — product title, falls back to the package identifier. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RevenueCat|Demo")
	FText DisplayName;

	/** Localized price ("$0.99") from the store; "-" when the branch has no price (e.g. desktop REST). */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RevenueCat|Demo")
	FText PriceText;

	/** Store product identifier ("test" / "com.game.pro.monthly") — use as a subtitle. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RevenueCat|Demo")
	FText ProductIdText;

	/** True while this package's purchase is in flight — bind Buy.bIsEnabled through NOT. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RevenueCat|Demo")
	bool bIsPurchasing = false;

	/** Raw package (identifiers) in case the row wants extra fields. */
	UPROPERTY(BlueprintReadOnly, Category = "RevenueCat|Demo")
	FRCPackage SourcePackage;

private:
	TWeakObjectPtr<URCDemoViewModel> Owner;
};
