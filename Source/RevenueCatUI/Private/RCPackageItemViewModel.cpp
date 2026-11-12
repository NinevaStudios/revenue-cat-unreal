// Copyright (c) 2026 Nineva Studios

#include "RCPackageItemViewModel.h"
#include "RCDemoViewModel.h"

void URCPackageItemViewModel::Init(URCDemoViewModel* InOwner, const FRCPackage& InPackage)
{
	Owner = InOwner;
	SourcePackage = InPackage;

	const FRCStoreProduct& Product = InPackage.StoreProduct;

	// Title comes from the store (mobile); desktop REST has none, so fall back to the package id.
	DisplayName = FText::FromString(!Product.Title.IsEmpty() ? Product.Title : InPackage.Identifier);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(DisplayName);

	// Product identifier — good as a subtitle line.
	ProductIdText = FText::FromString(Product.Identifier);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ProductIdText);

	// Store price string is already localized (e.g. "$0.99"); "-" when unavailable (desktop REST).
	PriceText = FText::FromString(!Product.PriceString.IsEmpty() ? Product.PriceString : TEXT("-"));
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(PriceText);
}

void URCPackageItemViewModel::PurchaseThis()
{
	if (URCDemoViewModel* Pinned = Owner.Get())
	{
		Pinned->Purchase(this);
	}
}

void URCPackageItemViewModel::SetIsPurchasing(bool bInValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(bIsPurchasing, bInValue);
}
