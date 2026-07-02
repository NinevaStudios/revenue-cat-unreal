// RevenueCat UE plugin.

#include "RCPackageItemViewModel.h"
#include "RCDemoViewModel.h"

void URCPackageItemViewModel::Init(URCDemoViewModel* InOwner, const FRCPackage& InPackage)
{
	Owner = InOwner;
	SourcePackage = InPackage;

	const FRCStoreProduct& Product = InPackage.StoreProduct;

	DisplayName = FText::FromString(!Product.Title.IsEmpty() ? Product.Title : InPackage.Identifier);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(DisplayName);

	FString Price = Product.PriceString;
	if (Price.IsEmpty())
	{
		Price = !Product.Identifier.IsEmpty() ? Product.Identifier : TEXT("-");
	}
	PriceText = FText::FromString(Price);
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
