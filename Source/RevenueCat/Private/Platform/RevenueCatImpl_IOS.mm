// RevenueCat UE plugin — learning skeleton.
// iOS: Objective-C++ bridge into the hybrid-common facade (CommonFunctionality), which wraps
// purchases-ios. Stubbed for now so it compiles without the xcframeworks present.

#include "Platform/IRevenueCatImpl.h"

#if PLATFORM_IOS

// TODO: once the xcframeworks are linked (see RevenueCat.Build.cs), import the facade:
// #import <PurchasesHybridCommon/PurchasesHybridCommon-Swift.h>
// #include "Async/Async.h"
// #include "IOS/IOSAsyncTask.h"

class FRevenueCatImpl_IOS : public IRevenueCatImpl
{
public:
	virtual void Configure(const FString& /*ApiKey*/, const FString& /*AppUserId*/) override
	{
		// TODO: [CommonFunctionality configureWithAPIKey:... appUserID:...];
	}

	virtual void GetOfferings(FResultCallback Cb) override
	{
		// TODO:
		// [CommonFunctionality getOfferingsWithCompletion:^(NSDictionary* dict, RCErrorContainer* err) {
		//     const FString Json = DictToJson(err ? err.info : dict);
		//     AsyncTask(ENamedThreads::GameThread, [Cb, Json, err]() { Cb(Json, err != nil); });
		// }];
		Cb(TEXT("{}"), false);
	}

	virtual void PurchasePackage(const FString& /*PackageId*/, FResultCallback Cb) override { Cb(TEXT("{}"), false); }
	virtual void GetCustomerInfo(FResultCallback Cb) override { Cb(TEXT("{}"), false); }
	virtual void RestorePurchases(FResultCallback Cb) override { Cb(TEXT("{}"), false); }
	virtual void LogIn(const FString& /*AppUserId*/, FResultCallback Cb) override { Cb(TEXT("{}"), false); }
	virtual void LogOut(FResultCallback Cb) override { Cb(TEXT("{}"), false); }
};

IRevenueCatImpl* CreateRevenueCatImpl()
{
	return new FRevenueCatImpl_IOS();
}

#endif // PLATFORM_IOS
