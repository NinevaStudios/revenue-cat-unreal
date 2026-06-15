// RevenueCat UE plugin — learning skeleton.
// Fallback for any platform that is not iOS / Android / Desktop (e.g. unsupported targets,
// or a console). Guarantees CreateRevenueCatImpl() always has exactly one definition.

#include "Platform/IRevenueCatImpl.h"

#if !PLATFORM_IOS && !PLATFORM_ANDROID && !PLATFORM_DESKTOP

class FRevenueCatImpl_Null : public IRevenueCatImpl
{
public:
	virtual void Configure(const FString& /*ApiKey*/, const FString& /*AppUserId*/) override {}
	virtual void GetOfferings(FResultCallback Cb) override { Cb(TEXT("{}"), false); }
	virtual void PurchasePackage(const FString& /*PackageId*/, FResultCallback Cb) override { Cb(TEXT("{}"), false); }
	virtual void GetCustomerInfo(FResultCallback Cb) override { Cb(TEXT("{}"), false); }
	virtual void RestorePurchases(FResultCallback Cb) override { Cb(TEXT("{}"), false); }
	virtual void LogIn(const FString& /*AppUserId*/, FResultCallback Cb) override { Cb(TEXT("{}"), false); }
	virtual void LogOut(FResultCallback Cb) override { Cb(TEXT("{}"), false); }
};

IRevenueCatImpl* CreateRevenueCatImpl()
{
	return new FRevenueCatImpl_Null();
}

#endif
