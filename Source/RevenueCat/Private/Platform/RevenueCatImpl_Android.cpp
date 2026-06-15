// RevenueCat UE plugin — learning skeleton.
// Android: JNI bridge into com.revenuecat.purchases.hybridcommon.CommonKt (pulled in by the UPL).
// Stubbed for now — wire the real JNI calls + a callback registry when ready.

#include "Platform/IRevenueCatImpl.h"

#if PLATFORM_ANDROID

// #include "Android/AndroidApplication.h"
// #include "Android/AndroidJava.h"
// #include "Android/AndroidJavaEnv.h"
// #include "Async/Async.h"

class FRevenueCatImpl_Android : public IRevenueCatImpl
{
public:
	virtual void Configure(const FString& /*ApiKey*/, const FString& /*AppUserId*/) override
	{
		// TODO: AndroidThunkJava_RC_Configure(ApiKey, AppUserId) via FJavaWrapper.
	}

	virtual void GetOfferings(FResultCallback Cb) override
	{
		// TODO: int64 Id = RegisterCallback(MoveTemp(Cb));
		//       call AndroidThunkJava_RC_GetOfferings(Id); native callback resolves it.
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
	return new FRevenueCatImpl_Android();
}

// TODO: JNI entry point called by the Java helper (declared in the UPL <gameActivityClassAdditions>):
//
// extern "C" JNIEXPORT void JNICALL
// Java_com_epicgames_unreal_GameActivity_nativeRC_1OnResult(JNIEnv* Env, jobject, jlong CallbackId, jstring Json, jboolean bIsError)
// {
//     const FString Result = FJavaHelper::FStringFromParam(Env, Json);
//     AsyncTask(ENamedThreads::GameThread, [CallbackId, Result, bIsError]()
//     {
//         ResolveCallback(CallbackId, Result, (bool)bIsError);
//     });
// }

#endif // PLATFORM_ANDROID
