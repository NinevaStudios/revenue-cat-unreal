// Copyright (c) 2026 Nineva Studios
// Desktop (Win64 / Mac / Linux): no native store SDK, so we talk to the RevenueCat REST API v1
// over HTTP (the same data the mobile SDK fetches), using the PUBLIC key. Reads (customer info /
// offerings) are direct GETs; purchases open a Web Billing checkout in the browser and then poll
// for the resulting entitlement.
//
// The App User ID is persisted per project (like the mobile SDKs do), so an anonymous desktop
// player keeps the same RevenueCat customer between launches.
//
// "Rest Api Base Url" (Project Settings -> Plugins -> RevenueCat, advanced) can point the requests
// at your own proxy; the response shape must stay identical to the v1 subscriber/offerings payloads.

#include "Platform/IRevenueCatImpl.h"

#if PLATFORM_DESKTOP

#include "RevenueCatSettings.h"
#include "RevenueCatJson.h"
#include "RevenueCatLog.h"
#include "HAL/PlatformMisc.h"
#include "Misc/App.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "HAL/PlatformProcess.h"
#include "Containers/Ticker.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/Guid.h"

class FRevenueCatImpl_Desktop : public IRevenueCatImpl
{
public:
	FRevenueCatImpl_Desktop()
		: Alive(MakeShared<bool, ESPMode::ThreadSafe>(true))
	{}

	virtual ~FRevenueCatImpl_Desktop() override
	{
		*Alive = false; // invalidate any in-flight poll continuations
	}

	virtual void Configure(const FString& InApiKey, const FString& InAppUserId, bool bInDebugLogs) override
	{
		ApiKey = InApiKey;
		bDebugLogs = bInDebugLogs;

		// Same rule as the mobile SDKs: explicit ID > last used ID > new anonymous ID.
		FString Id = InAppUserId.TrimStartAndEnd();
		if (Id.IsEmpty()) { Id = LoadStoredAppUserId(); }
		if (Id.IsEmpty()) { Id = MakeAnonymousId(); }
		SetAppUserId(Id);
	}

	virtual void GetOfferings(FResultCallback Cb) override
	{
		SendGet(BaseUrl() / TEXT("subscribers") / EncodedUserId() / TEXT("offerings"), MoveTemp(Cb));
	}

	virtual void GetCustomerInfo(FResultCallback Cb) override
	{
		SendGet(SubscribersUrl(), MoveTemp(Cb));
	}

	virtual void RestorePurchases(FResultCallback Cb) override
	{
		// On desktop "restore" == re-fetch the canonical state for this app_user_id.
		GetCustomerInfo(MoveTemp(Cb));
	}

	virtual void LogIn(const FString& InAppUserId, FResultCallback Cb) override
	{
		SetAppUserId(InAppUserId.TrimStartAndEnd());
		GetCustomerInfo(MoveTemp(Cb)); // a GET creates the subscriber if it doesn't exist yet
	}

	virtual void LogOut(FResultCallback Cb) override
	{
		if (IsAnonymous(AppUserId))
		{
			// Mirrors the SDK's LogOutWithAnonymousUserError.
			Cb(TEXT("{\"code\":22,\"message\":\"Called LogOut but the current user is anonymous.\"}"), true);
			return;
		}
		SetAppUserId(MakeAnonymousId());
		GetCustomerInfo(MoveTemp(Cb));
	}

	// PC purchases go through a Web Billing checkout in the browser. We open the configured Web
	// Purchase Link (attributed to this app_user_id), then poll until a new active entitlement
	// appears (the user finished checkout) or we time out (it may still land later via webhook).
	virtual void PurchasePackage(const FString& /*PackageId*/, const FString& /*OfferingId*/, FResultCallback Cb) override
	{
		// Desktop ignores the offering context — purchases go through the configured Web Billing
		// checkout link, not a package. (A per-offering Web Purchase Link could be selected later.)
		const URevenueCatSettings* Settings = GetDefault<URevenueCatSettings>();
		const FString Link = Settings ? Settings->WebPurchaseLinkUrl : FString();
		if (Link.IsEmpty())
		{
			Cb(TEXT("{\"code\":-2,\"message\":\"Set 'Web Purchase Link Url' in Project Settings -> Plugins -> RevenueCat to enable desktop purchases.\"}"), true);
			return;
		}

		FString Url = Link;
		while (Url.EndsWith(TEXT("/"))) { Url.LeftChopInline(1); }
		Url = Url / EncodedUserId(); // attribute the purchase to this app_user_id

		// Snapshot how many entitlements are active now, then open checkout and watch for growth.
		GetCustomerInfo([this, Url, Cb](const FString& Json, bool bIsError)
		{
			const int32 Baseline = bIsError ? 0 : RevenueCatJson::ParseCustomerInfo(Json).ActiveEntitlements.Num();
			FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);
			BeginPoll(Baseline, Cb);
		});
	}

private:
	FString ApiKey;
	FString AppUserId;
	bool bDebugLogs = false;
	TSharedRef<bool, ESPMode::ThreadSafe> Alive;

	struct FPollState
	{
		int32 Baseline = 0;
		float Elapsed = 0.f;
		float Interval = 3.f;
		int32 TimeoutSec = 180;
		IRevenueCatImpl::FResultCallback Cb;
	};

	static FString MakeAnonymousId()
	{
		return FString::Printf(TEXT("$RCAnonymousID:%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower());
	}

	static bool IsAnonymous(const FString& Id)
	{
		return Id.StartsWith(TEXT("$RCAnonymousID:"));
	}

	// Per-user, per-project storage (registry on Windows, ini under the user folder on Mac/Linux).
	static FString StoreSection()
	{
		return FString::Printf(TEXT("RevenueCat_%s"), FApp::GetProjectName());
	}

	static FString LoadStoredAppUserId()
	{
		FString Value;
		FPlatformMisc::GetStoredValue(TEXT("Nineva Studios"), StoreSection(), TEXT("AppUserId"), Value);
		return Value;
	}

	void SetAppUserId(const FString& Id)
	{
		AppUserId = Id;
		FPlatformMisc::SetStoredValue(TEXT("Nineva Studios"), StoreSection(), TEXT("AppUserId"), Id);
		if (bDebugLogs)
		{
			UE_LOG(LogRevenueCat, Log, TEXT("App User ID: %s"), *Id);
		}
	}

	FString BaseUrl() const
	{
		const URevenueCatSettings* Settings = GetDefault<URevenueCatSettings>();
		if (Settings && !Settings->RestApiBaseUrl.IsEmpty())
		{
			FString Url = Settings->RestApiBaseUrl;
			while (Url.EndsWith(TEXT("/"))) { Url.LeftChopInline(1); }
			return Url;
		}
		return TEXT("https://api.revenuecat.com/v1");
	}

	FString SubscribersUrl() const
	{
		return BaseUrl() / TEXT("subscribers") / EncodedUserId();
	}

	FString EncodedUserId() const
	{
		return FGenericPlatformHttp::UrlEncode(AppUserId);
	}

	void SendGet(const FString& Url, FResultCallback Cb)
	{
		const URevenueCatSettings* Settings = GetDefault<URevenueCatSettings>();

		const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
		Request->SetVerb(TEXT("GET"));
		Request->SetURL(Url);
		Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *ApiKey));
		Request->SetHeader(TEXT("Accept"), TEXT("application/json"));

		const FString Platform = Settings ? Settings->DesktopStorePlatform : FString(TEXT("stripe"));
		if (!Platform.IsEmpty())
		{
			Request->SetHeader(TEXT("X-Platform"), Platform);
		}

		if (bDebugLogs)
		{
			UE_LOG(LogRevenueCat, Log, TEXT("GET %s (X-Platform: %s)"), *Url, Platform.IsEmpty() ? TEXT("-") : *Platform);
		}

		const bool bLog = bDebugLogs;
		Request->OnProcessRequestComplete().BindLambda(
			[Cb, bLog, Url](FHttpRequestPtr /*Req*/, FHttpResponsePtr Response, bool bConnectedSuccessfully)
			{
				const bool bHasResponse = bConnectedSuccessfully && Response.IsValid();
				if (bLog)
				{
					UE_LOG(LogRevenueCat, Log, TEXT("GET %s -> %d"), *Url, bHasResponse ? Response->GetResponseCode() : 0);
				}

				if (bHasResponse && EHttpResponseCodes::IsOk(Response->GetResponseCode()))
				{
					Cb(Response->GetContentAsString(), false);
				}
				else if (bHasResponse)
				{
					Cb(Response->GetContentAsString(), true); // RevenueCat error body: {"code":7xxx,"message":...}
				}
				else
				{
					// Same code as the SDK's NetworkError.
					Cb(TEXT("{\"code\":10,\"message\":\"Network error: no response from RevenueCat.\"}"), true);
				}
			});

		Request->ProcessRequest();
	}

	void BeginPoll(int32 Baseline, FResultCallback Cb)
	{
		const URevenueCatSettings* Settings = GetDefault<URevenueCatSettings>();

		const TSharedRef<FPollState> State = MakeShared<FPollState>();
		State->Baseline = Baseline;
		State->TimeoutSec = Settings ? FMath::Clamp(Settings->CheckoutPollTimeoutSeconds, 10, 600) : 180;
		State->Cb = Cb;
		ArmPoll(State);
	}

	void ArmPoll(TSharedRef<FPollState> State)
	{
		TWeakPtr<bool, ESPMode::ThreadSafe> WeakAlive = Alive;
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
			[this, State, WeakAlive](float) -> bool
			{
				const TSharedPtr<bool, ESPMode::ThreadSafe> Pinned = WeakAlive.Pin();
				if (Pinned.IsValid() && *Pinned)
				{
					PollOnce(State);
				}
				return false; // one-shot; re-armed from PollOnce's completion
			}), State->Interval);
	}

	void PollOnce(TSharedRef<FPollState> State)
	{
		TWeakPtr<bool, ESPMode::ThreadSafe> WeakAlive = Alive;
		SendGet(SubscribersUrl(), [this, State, WeakAlive](const FString& Json, bool bIsError)
		{
			const TSharedPtr<bool, ESPMode::ThreadSafe> Pinned = WeakAlive.Pin();
			if (!Pinned.IsValid() || !*Pinned)
			{
				return; // impl was destroyed; drop silently
			}

			if (!bIsError && RevenueCatJson::ParseCustomerInfo(Json).ActiveEntitlements.Num() > State->Baseline)
			{
				State->Cb(Json, false); // checkout produced a new entitlement
				return;
			}

			State->Elapsed += State->Interval;
			if (State->Elapsed >= State->TimeoutSec)
			{
				State->Cb(TEXT("{\"code\":-3,\"message\":\"Checkout was not completed before the timeout. If the player pays later, call Get Customer Info to refresh.\"}"), true);
				return;
			}

			ArmPoll(State); // keep watching
		});
	}
};

IRevenueCatImpl* CreateRevenueCatImpl()
{
	return new FRevenueCatImpl_Desktop();
}

#endif // PLATFORM_DESKTOP
