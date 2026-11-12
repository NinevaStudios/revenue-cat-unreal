// Copyright (c) 2026 Nineva Studios
// Android branch: C++ <-> JNI bridge into the GameActivity Java helper (added via the UPL),
// which calls com.revenuecat.purchases.hybridcommon.CommonKt. Results come back through the
// native callback nativeRevenueCatOnResult(...) and are marshalled to the game thread.
//
// NOTE: this file only compiles for Android and CANNOT be built/verified on Windows (needs the
// UE Android toolchain + an Android packaging build + a device). The exact CommonKt signatures
// are pinned by the hybrid-common Gradle version in RevenueCat_UPL_Android.xml.

#include "Platform/IRevenueCatImpl.h"

#if PLATFORM_ANDROID

#include "Android/AndroidApplication.h"
#include "Android/AndroidJavaEnv.h"
#include "Async/Async.h"
#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"

namespace
{
	FCriticalSection GCallbackLock;
	TMap<int64, IRevenueCatImpl::FResultCallback> GCallbacks;
	int64 GNextCallbackId = 1;

	int64 RegisterCallback(IRevenueCatImpl::FResultCallback Cb)
	{
		FScopeLock Lock(&GCallbackLock);
		const int64 Id = GNextCallbackId++;
		GCallbacks.Add(Id, MoveTemp(Cb));
		return Id;
	}

	// Runs on the game thread (dispatched from the JNI callback).
	void ResolveCallback(int64 Id, const FString& Json, bool bIsError)
	{
		IRevenueCatImpl::FResultCallback Cb;
		{
			FScopeLock Lock(&GCallbackLock);
			if (!GCallbacks.RemoveAndCopyValue(Id, Cb)) { return; }
		}
		if (Cb) { Cb(Json, bIsError); }
	}

	// Call a GameActivity void method "(J)V".
	void CallJavaId(const char* Method, int64 Id)
	{
		if (JNIEnv* Env = AndroidJavaEnv::GetJavaEnv())
		{
			jobject Activity = AndroidJavaEnv::GetGameActivityThis();
			jclass Clazz = Env->GetObjectClass(Activity);
			if (jmethodID Mid = Env->GetMethodID(Clazz, Method, "(J)V"))
			{
				Env->CallVoidMethod(Activity, Mid, (jlong)Id);
			}
			Env->DeleteLocalRef(Clazz);
		}
	}

	// Call a GameActivity void method "(Ljava/lang/String;J)V".
	void CallJavaStringId(const char* Method, const FString& Str, int64 Id)
	{
		if (JNIEnv* Env = AndroidJavaEnv::GetJavaEnv())
		{
			jobject Activity = AndroidJavaEnv::GetGameActivityThis();
			jclass Clazz = Env->GetObjectClass(Activity);
			if (jmethodID Mid = Env->GetMethodID(Clazz, Method, "(Ljava/lang/String;J)V"))
			{
				jstring JStr = Env->NewStringUTF(TCHAR_TO_UTF8(*Str));
				Env->CallVoidMethod(Activity, Mid, JStr, (jlong)Id);
				Env->DeleteLocalRef(JStr);
			}
			Env->DeleteLocalRef(Clazz);
		}
	}

	// Call a GameActivity void method "(Ljava/lang/String;Ljava/lang/String;J)V" (purchase).
	void CallJavaStringStringId(const char* Method, const FString& A, const FString& B, int64 Id)
	{
		if (JNIEnv* Env = AndroidJavaEnv::GetJavaEnv())
		{
			jobject Activity = AndroidJavaEnv::GetGameActivityThis();
			jclass Clazz = Env->GetObjectClass(Activity);
			if (jmethodID Mid = Env->GetMethodID(Clazz, Method, "(Ljava/lang/String;Ljava/lang/String;J)V"))
			{
				jstring JA = Env->NewStringUTF(TCHAR_TO_UTF8(*A));
				jstring JB = Env->NewStringUTF(TCHAR_TO_UTF8(*B));
				Env->CallVoidMethod(Activity, Mid, JA, JB, (jlong)Id);
				Env->DeleteLocalRef(JA);
				Env->DeleteLocalRef(JB);
			}
			Env->DeleteLocalRef(Clazz);
		}
	}

	// Call a GameActivity void method "(Ljava/lang/String;Ljava/lang/String;Z)V" (configure).
	void CallJavaTwoStringsBool(const char* Method, const FString& A, const FString& B, bool bFlag)
	{
		if (JNIEnv* Env = AndroidJavaEnv::GetJavaEnv())
		{
			jobject Activity = AndroidJavaEnv::GetGameActivityThis();
			jclass Clazz = Env->GetObjectClass(Activity);
			if (jmethodID Mid = Env->GetMethodID(Clazz, Method, "(Ljava/lang/String;Ljava/lang/String;Z)V"))
			{
				jstring JA = Env->NewStringUTF(TCHAR_TO_UTF8(*A));
				jstring JB = Env->NewStringUTF(TCHAR_TO_UTF8(*B));
				Env->CallVoidMethod(Activity, Mid, JA, JB, (jboolean)(bFlag ? JNI_TRUE : JNI_FALSE));
				Env->DeleteLocalRef(JA);
				Env->DeleteLocalRef(JB);
			}
			Env->DeleteLocalRef(Clazz);
		}
	}
}

class FRevenueCatImpl_Android : public IRevenueCatImpl
{
public:
	virtual void Configure(const FString& ApiKey, const FString& AppUserId, bool bDebugLogs) override
	{
		CallJavaTwoStringsBool("AndroidThunkJava_RevenueCat_Configure", ApiKey, AppUserId, bDebugLogs);
	}

	virtual void GetOfferings(FResultCallback Cb) override
	{
		CallJavaId("AndroidThunkJava_RevenueCat_GetOfferings", RegisterCallback(MoveTemp(Cb)));
	}

	virtual void PurchasePackage(const FString& PackageId, const FString& OfferingId, FResultCallback Cb) override
	{
		CallJavaStringStringId("AndroidThunkJava_RevenueCat_PurchasePackage", PackageId, OfferingId, RegisterCallback(MoveTemp(Cb)));
	}

	virtual void GetCustomerInfo(FResultCallback Cb) override
	{
		CallJavaId("AndroidThunkJava_RevenueCat_GetCustomerInfo", RegisterCallback(MoveTemp(Cb)));
	}

	virtual void RestorePurchases(FResultCallback Cb) override
	{
		CallJavaId("AndroidThunkJava_RevenueCat_RestorePurchases", RegisterCallback(MoveTemp(Cb)));
	}

	virtual void LogIn(const FString& AppUserId, FResultCallback Cb) override
	{
		CallJavaStringId("AndroidThunkJava_RevenueCat_LogIn", AppUserId, RegisterCallback(MoveTemp(Cb)));
	}

	virtual void LogOut(FResultCallback Cb) override
	{
		CallJavaId("AndroidThunkJava_RevenueCat_LogOut", RegisterCallback(MoveTemp(Cb)));
	}
};

IRevenueCatImpl* CreateRevenueCatImpl()
{
	return new FRevenueCatImpl_Android();
}

// Java -> C++ callback (declared as `native` in GameActivity by the UPL).
extern "C" JNIEXPORT void JNICALL
Java_com_epicgames_unreal_GameActivity_nativeRevenueCatOnResult(
	JNIEnv* Env, jobject /*thiz*/, jlong CallbackId, jstring Json, jboolean bIsError)
{
	FString Result;
	if (Json)
	{
		const char* Chars = Env->GetStringUTFChars(Json, nullptr);
		Result = UTF8_TO_TCHAR(Chars);
		Env->ReleaseStringUTFChars(Json, Chars);
	}
	const int64 Id = (int64)CallbackId;
	const bool bError = (bIsError == JNI_TRUE);
	AsyncTask(ENamedThreads::GameThread, [Id, Result, bError]()
	{
		ResolveCallback(Id, Result, bError);
	});
}

#endif // PLATFORM_ANDROID
