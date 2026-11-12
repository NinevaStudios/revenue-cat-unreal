// Copyright (c) 2026 Nineva Studios
// iOS branch: Objective-C++ bridge into the purchases-hybrid-common facade (RCCommonFunctionality,
// PurchasesHybridCommon.xcframework), which wraps purchases-ios (RevenueCat.xcframework). It is the
// same facade the Android branch calls (CommonKt), so both platforms return the same maps:
// success = the facade's NSDictionary, error = RCErrorContainer.info (keys "code", "message",
// "readableErrorCode", "userCancelled", ...). They are serialized to JSON here and parsed once by
// RevenueCatJson.cpp, exactly like the Android results.
//
// Threading:
//  - Every SDK call is posted to the main queue (serial, FIFO), so Configure is always applied
//    before any call issued after it, without blocking the game thread.
//  - RevenueCat delivers completions on the main thread; results are marshalled to the game thread.
//  - RCCommonFunctionality calls fatalError() when used before configure, so every call first
//    checks [RCPurchases isConfigured] and returns an error instead of crashing.
//
// Memory management: valid under both MRC (the module default) and ARC. No explicit
// retain/release/autorelease; blocks capture only C++ values (FString, TFunction) and are passed
// straight to APIs that copy them.
//
// NOTE: compiles only for iOS and CANNOT be built/verified on Windows (needs a Mac + Xcode).
// See IOS_BUILD_NOTES.md in the plugin folder.

#include "Platform/IRevenueCatImpl.h"

#if PLATFORM_IOS

#include "RevenueCatTypes.h"
#include "RevenueCatLog.h"
#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#ifndef WITH_REVENUECAT_IOS_SDK
	#define WITH_REVENUECAT_IOS_SDK 0
#endif

namespace RevenueCatIOS
{
	/** Hand the result to the caller on the game thread (the IRevenueCatImpl contract). */
	static void Deliver(const IRevenueCatImpl::FResultCallback& Cb, const FString& Json, bool bIsError)
	{
		if (!Cb) { return; }
		AsyncTask(ENamedThreads::GameThread, [Cb, Json, bIsError]()
		{
			Cb(Json, bIsError);
		});
	}

	/** Plugin-side error payload in the same shape as the SDK's ErrorContainer.info. */
	static FString MakeErrorJson(int32 Code, const FString& Message)
	{
		const TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetNumberField(TEXT("code"), Code);
		Obj->SetStringField(TEXT("message"), Message);

		FString Out;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
		FJsonSerializer::Serialize(Obj, Writer);
		return Out;
	}
}

#if WITH_REVENUECAT_IOS_SDK

THIRD_PARTY_INCLUDES_START
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <StoreKit/StoreKit.h>
// UE compiles .mm files without -fmodules, so the "@import RevenueCat;" inside the hybrid-common
// Swift header is skipped. Import RevenueCat's Swift header explicitly first (RCPurchases, ...).
#import <RevenueCat/RevenueCat-Swift.h>
#import <PurchasesHybridCommon/PurchasesHybridCommon-Swift.h>
THIRD_PARTY_INCLUDES_END

namespace RevenueCatIOS
{
	// Must match the Android branch (RevenueCat_UPL_Android.xml): PlatformInfo("unreal", "1.0.0").
	static NSString* const PlatformFlavor = @"unreal";
	static NSString* const PlatformFlavorVersion = @"1.0.0";

	static NSString* ToNS(const FString& Str)
	{
		return [NSString stringWithUTF8String:TCHAR_TO_UTF8(*Str)];
	}

	static FString FromNS(NSString* Str)
	{
		return Str ? FString(UTF8_TO_TCHAR([Str UTF8String])) : FString();
	}

	/**
	 * NSJSONSerialization raises (and UE builds with -fno-objc-exceptions) on anything that is not
	 * NSString / NSNumber / NSNull / NSArray / NSDictionary, or on NaN / Inf. The hybrid-common maps
	 * already use JSON types (dates are ISO-8601 strings + millis), so this is only a safety net.
	 * Never returns nil.
	 */
	static id Sanitize(id Value)
	{
		if (Value == nil)
		{
			return [NSNull null];
		}
		if ([Value isKindOfClass:[NSString class]] || [Value isKindOfClass:[NSNull class]])
		{
			return Value;
		}
		if ([Value isKindOfClass:[NSNumber class]])
		{
			const double D = [(NSNumber*)Value doubleValue];
			return (FMath::IsNaN(D) || !FMath::IsFinite(D)) ? (id)[NSNull null] : Value;
		}
		if ([Value isKindOfClass:[NSDictionary class]])
		{
			NSDictionary* Dict = (NSDictionary*)Value;
			NSMutableDictionary* Out = [NSMutableDictionary dictionaryWithCapacity:Dict.count];
			for (id Key in Dict)
			{
				NSString* KeyStr = [Key isKindOfClass:[NSString class]] ? (NSString*)Key : [Key description];
				Out[(KeyStr != nil) ? KeyStr : @""] = Sanitize(Dict[Key]);
			}
			return Out;
		}
		if ([Value isKindOfClass:[NSArray class]])
		{
			NSMutableArray* Out = [NSMutableArray arrayWithCapacity:[(NSArray*)Value count]];
			for (id Item in (NSArray*)Value)
			{
				[Out addObject:Sanitize(Item)];
			}
			return Out;
		}
		if ([Value isKindOfClass:[NSSet class]])
		{
			return Sanitize([(NSSet*)Value allObjects]);
		}
		if ([Value isKindOfClass:[NSDate class]])
		{
			// Same format as the hybrid-common date strings. "new" returns an owned object -> release under MRC.
			NSDateFormatter* Formatter = [NSDateFormatter new];
			Formatter.locale = [NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"];
			Formatter.timeZone = [NSTimeZone timeZoneForSecondsFromGMT:0];
			Formatter.dateFormat = @"yyyy-MM-dd'T'HH:mm:ss.SSS'Z'";
			NSString* Iso = [Formatter stringFromDate:(NSDate*)Value];
#if !__has_feature(objc_arc)
			[Formatter release];
#endif
			return (Iso != nil) ? (id)Iso : (id)[NSNull null];
		}
		if ([Value isKindOfClass:[NSURL class]])
		{
			NSString* Url = [(NSURL*)Value absoluteString];
			return (Url != nil) ? (id)Url : (id)[NSNull null];
		}
		NSString* Description = [Value description];
		return (Description != nil) ? (id)Description : (id)[NSNull null];
	}

	/** Serialize a dictionary to a JSON FString. Returns false if it cannot be serialized. */
	static bool ToJson(NSDictionary* Dict, FString& OutJson)
	{
		const id Clean = Sanitize(Dict);
		if (![NSJSONSerialization isValidJSONObject:Clean])
		{
			return false;
		}
		NSError* JsonError = nil;
		NSData* Data = [NSJSONSerialization dataWithJSONObject:Clean options:0 error:&JsonError];
		if (Data == nil)
		{
			return false;
		}
		TArray<ANSICHAR> Utf8;
		Utf8.Append(static_cast<const ANSICHAR*>([Data bytes]), static_cast<int32>([Data length]));
		Utf8.Add('\0');
		OutJson = FString(UTF8_TO_TCHAR(Utf8.GetData()));
		return true;
	}

	/** Shared body of every hybrid-common completion block (runs on the main thread). */
	static void HandleResult(const IRevenueCatImpl::FResultCallback& Cb, const TCHAR* CallName,
		NSDictionary* Result, RCErrorContainer* Error)
	{
		FString Json;
		if (Error != nil)
		{
			if (!ToJson(Error.info, Json))
			{
				Json = MakeErrorJson(static_cast<int32>(Error.code), FromNS(Error.message));
			}
			Deliver(Cb, Json, true);
			return;
		}
		if (Result == nil)
		{
			UE_LOG(LogRevenueCat, Error, TEXT("%s: the RevenueCat SDK returned neither a result nor an error."), CallName);
			Deliver(Cb, MakeErrorJson(RevenueCatErrorCodes::Internal,
				FString::Printf(TEXT("%s: empty result from the RevenueCat SDK."), CallName)), true);
			return;
		}
		if (!ToJson(Result, Json))
		{
			UE_LOG(LogRevenueCat, Error, TEXT("%s: could not serialize the RevenueCat SDK result to JSON."), CallName);
			Deliver(Cb, MakeErrorJson(RevenueCatErrorCodes::Internal,
				FString::Printf(TEXT("%s: could not serialize the RevenueCat SDK result."), CallName)), true);
			return;
		}
		Deliver(Cb, Json, false);
	}

	/** Run on the main queue. FIFO order keeps Configure ahead of every later call. */
	static void RunOnMainQueue(TFunction<void()> Work)
	{
		dispatch_async(dispatch_get_main_queue(), ^{
			@autoreleasepool
			{
				Work();
			}
		});
	}

	/** Run on the main queue once the SDK is configured; otherwise fail the call (avoids the SDK's fatalError). */
	static void RunWhenConfigured(const TCHAR* CallName, IRevenueCatImpl::FResultCallback Cb, TFunction<void()> Work)
	{
		const FString Name(CallName);
		RunOnMainQueue([Name, Cb, Work]()
		{
			if (![RCPurchases isConfigured])
			{
				UE_LOG(LogRevenueCat, Warning, TEXT("%s: the RevenueCat iOS SDK is not configured."), *Name);
				Deliver(Cb, MakeErrorJson(RevenueCatErrorCodes::NotConfigured,
					FString::Printf(TEXT("%s: the RevenueCat iOS SDK is not configured. Call Configure first and set 'Apple Api Key' in Project Settings > Plugins > RevenueCat."), *Name)), true);
				return;
			}
			Work();
		});
	}
}

class FRevenueCatImpl_IOS : public IRevenueCatImpl
{
public:
	virtual void Configure(const FString& ApiKey, const FString& AppUserId, bool bDebugLogs) override
	{
		RevenueCatIOS::RunOnMainQueue([ApiKey, AppUserId, bDebugLogs]()
		{
			if (ApiKey.IsEmpty())
			{
				UE_LOG(LogRevenueCat, Error, TEXT("Configure failed: API key is empty. Set 'Apple Api Key' in Project Settings > Plugins > RevenueCat."));
				return;
			}

			if (bDebugLogs)
			{
				[RCCommonFunctionality setLogLevel:@"DEBUG"];
			}

			RCPurchases* Purchases = [RCPurchases configureWithAPIKey:RevenueCatIOS::ToNS(ApiKey)
				appUserID:(AppUserId.IsEmpty() ? nil : RevenueCatIOS::ToNS(AppUserId))
				purchasesAreCompletedBy:nil                 // default: RevenueCat finishes transactions
				userDefaultsSuiteName:nil
				platformFlavor:RevenueCatIOS::PlatformFlavor
				platformFlavorVersion:RevenueCatIOS::PlatformFlavorVersion
				storeKitVersion:@"DEFAULT"                  // SDK default (StoreKit 2 where available)
				dangerousSettings:nil
				shouldShowInAppMessagesAutomatically:YES
				verificationMode:nil
				diagnosticsEnabled:NO
				automaticDeviceIdentifierCollectionEnabled:YES
				preferredLocale:nil];

			if (Purchases != nil)
			{
				UE_LOG(LogRevenueCat, Log, TEXT("iOS SDK configured (purchases-hybrid-common %s), appUserId=%s"),
					*RevenueCatIOS::FromNS([RCCommonFunctionality hybridCommonVersion]),
					AppUserId.IsEmpty() ? TEXT("<anonymous>") : *AppUserId);
			}
			else
			{
				UE_LOG(LogRevenueCat, Error, TEXT("Configure failed: the RevenueCat iOS SDK returned no instance."));
			}
		});
	}

	virtual void GetOfferings(FResultCallback Cb) override
	{
		RevenueCatIOS::RunWhenConfigured(TEXT("GetOfferings"), Cb, [Cb]()
		{
			[RCCommonFunctionality getOfferingsWithCompletionBlock:^(NSDictionary* Result, RCErrorContainer* Error)
			{
				RevenueCatIOS::HandleResult(Cb, TEXT("GetOfferings"), Result, Error);
			}];
		});
	}

	virtual void PurchasePackage(const FString& PackageId, const FString& OfferingId, FResultCallback Cb) override
	{
		RevenueCatIOS::RunWhenConfigured(TEXT("PurchasePackage"), Cb, [Cb, PackageId, OfferingId]()
		{
			// Same presentedOfferingContext map the Android bridge builds. hybrid-common looks the
			// package up inside this offering, so an empty offering id yields "Couldn't find package".
			NSDictionary* Context = OfferingId.IsEmpty()
				? @{}
				: @{ @"offeringIdentifier": RevenueCatIOS::ToNS(OfferingId) };

			[RCCommonFunctionality purchasePackage:RevenueCatIOS::ToNS(PackageId)
				presentedOfferingContext:Context
				signedDiscountTimestamp:nil
				completionBlock:^(NSDictionary* Result, RCErrorContainer* Error)
			{
				RevenueCatIOS::HandleResult(Cb, TEXT("PurchasePackage"), Result, Error);
			}];
		});
	}

	virtual void GetCustomerInfo(FResultCallback Cb) override
	{
		RevenueCatIOS::RunWhenConfigured(TEXT("GetCustomerInfo"), Cb, [Cb]()
		{
			[RCCommonFunctionality getCustomerInfoWithCompletionBlock:^(NSDictionary* Result, RCErrorContainer* Error)
			{
				RevenueCatIOS::HandleResult(Cb, TEXT("GetCustomerInfo"), Result, Error);
			}];
		});
	}

	virtual void RestorePurchases(FResultCallback Cb) override
	{
		RevenueCatIOS::RunWhenConfigured(TEXT("RestorePurchases"), Cb, [Cb]()
		{
			[RCCommonFunctionality restorePurchasesWithCompletionBlock:^(NSDictionary* Result, RCErrorContainer* Error)
			{
				RevenueCatIOS::HandleResult(Cb, TEXT("RestorePurchases"), Result, Error);
			}];
		});
	}

	virtual void LogIn(const FString& AppUserId, FResultCallback Cb) override
	{
		RevenueCatIOS::RunWhenConfigured(TEXT("LogIn"), Cb, [Cb, AppUserId]()
		{
			[RCCommonFunctionality logInWithAppUserID:RevenueCatIOS::ToNS(AppUserId)
				completionBlock:^(NSDictionary* Result, RCErrorContainer* Error)
			{
				RevenueCatIOS::HandleResult(Cb, TEXT("LogIn"), Result, Error);
			}];
		});
	}

	virtual void LogOut(FResultCallback Cb) override
	{
		RevenueCatIOS::RunWhenConfigured(TEXT("LogOut"), Cb, [Cb]()
		{
			[RCCommonFunctionality logOutWithCompletionBlock:^(NSDictionary* Result, RCErrorContainer* Error)
			{
				RevenueCatIOS::HandleResult(Cb, TEXT("LogOut"), Result, Error);
			}];
		});
	}
};

#else // !WITH_REVENUECAT_IOS_SDK

// Built without Source/ThirdParty/IOS/*.xcframework.zip (see RevenueCat.Build.cs): keep the game
// compiling, but fail every call loudly instead of reporting fake successes.
class FRevenueCatImpl_IOS : public IRevenueCatImpl
{
public:
	virtual void Configure(const FString& /*ApiKey*/, const FString& /*AppUserId*/, bool /*bDebugLogs*/) override
	{
		UE_LOG(LogRevenueCat, Error, TEXT("Configure failed: this iOS build does not contain the RevenueCat SDK frameworks. %s"), MissingSdkHint());
	}

	virtual void GetOfferings(FResultCallback Cb) override { Fail(Cb); }
	virtual void PurchasePackage(const FString& /*PackageId*/, const FString& /*OfferingId*/, FResultCallback Cb) override { Fail(Cb); }
	virtual void GetCustomerInfo(FResultCallback Cb) override { Fail(Cb); }
	virtual void RestorePurchases(FResultCallback Cb) override { Fail(Cb); }
	virtual void LogIn(const FString& /*AppUserId*/, FResultCallback Cb) override { Fail(Cb); }
	virtual void LogOut(FResultCallback Cb) override { Fail(Cb); }

private:
	static const TCHAR* MissingSdkHint()
	{
		return TEXT("Add RevenueCat.xcframework.zip and PurchasesHybridCommon.xcframework.zip to Plugins/RevenueCat/Source/ThirdParty/IOS (see README.md there) and rebuild.");
	}

	static void Fail(const FResultCallback& Cb)
	{
		RevenueCatIOS::Deliver(Cb, RevenueCatIOS::MakeErrorJson(RevenueCatErrorCodes::Internal,
			FString::Printf(TEXT("The RevenueCat iOS SDK is not linked into this build. %s"), MissingSdkHint())), true);
	}
};

#endif // WITH_REVENUECAT_IOS_SDK

IRevenueCatImpl* CreateRevenueCatImpl()
{
	return new FRevenueCatImpl_IOS();
}

#endif // PLATFORM_IOS
