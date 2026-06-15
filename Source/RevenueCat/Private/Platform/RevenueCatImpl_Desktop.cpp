// RevenueCat UE plugin.
// Desktop (Win64 / Mac / Linux): there is no native store SDK, so we talk to the RevenueCat REST
// API v1 over HTTP (the same data the mobile SDK fetches under the hood), using the PUBLIC key.
// Purchases on desktop go through Web Billing (Stripe) in a browser — see PurchasePackage().
//
// Override the endpoint with Project Settings -> Plugins -> RevenueCat -> "Backend Base Url" to
// route through your own server (recommended source of truth) instead of hitting RevenueCat
// directly; the response shape is assumed identical to the v1 subscriber/offerings payloads.

#include "Platform/IRevenueCatImpl.h"

#if PLATFORM_DESKTOP

#include "RevenueCatSettings.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Misc/Guid.h"

class FRevenueCatImpl_Desktop : public IRevenueCatImpl
{
public:
	virtual void Configure(const FString& InApiKey, const FString& InAppUserId) override
	{
		ApiKey = InApiKey;
		AppUserId = InAppUserId.IsEmpty() ? MakeAnonymousId() : InAppUserId;
	}

	virtual void GetOfferings(FResultCallback Cb) override
	{
		SendGet(BaseUrl() / TEXT("subscribers") / EncodedUserId() / TEXT("offerings"), MoveTemp(Cb));
	}

	virtual void GetCustomerInfo(FResultCallback Cb) override
	{
		SendGet(BaseUrl() / TEXT("subscribers") / EncodedUserId(), MoveTemp(Cb));
	}

	virtual void RestorePurchases(FResultCallback Cb) override
	{
		// On desktop "restore" == re-fetch the canonical state for this app_user_id.
		GetCustomerInfo(MoveTemp(Cb));
	}

	virtual void LogIn(const FString& InAppUserId, FResultCallback Cb) override
	{
		AppUserId = InAppUserId;
		GetCustomerInfo(MoveTemp(Cb)); // a GET creates the subscriber if it doesn't exist yet
	}

	virtual void LogOut(FResultCallback Cb) override
	{
		AppUserId = MakeAnonymousId();
		Cb(TEXT("{}"), false);
	}

	virtual void PurchasePackage(const FString& /*PackageId*/, FResultCallback Cb) override
	{
		// PC purchases happen through Web Billing (Stripe) checkout in a browser, not via REST.
		// TODO: launch the Web Billing checkout URL (FPlatformProcess::LaunchURL), then poll
		//       GetCustomerInfo() or wait for your backend's redemption webhook to confirm.
		Cb(TEXT("{\"code\":-2,\"message\":\"Desktop purchases go through Web Billing checkout in a browser; not available via REST.\"}"), true);
	}

private:
	FString ApiKey;
	FString AppUserId;

	static FString MakeAnonymousId()
	{
		return FString::Printf(TEXT("$RCAnonymousID:%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	FString BaseUrl() const
	{
		const URevenueCatSettings* Settings = GetDefault<URevenueCatSettings>();
		if (Settings && !Settings->BackendBaseUrl.IsEmpty())
		{
			FString Url = Settings->BackendBaseUrl;
			while (Url.EndsWith(TEXT("/"))) { Url.LeftChopInline(1); }
			return Url;
		}
		return TEXT("https://api.revenuecat.com/v1");
	}

	FString EncodedUserId() const
	{
		return FGenericPlatformHttp::UrlEncode(AppUserId);
	}

	void SendGet(const FString& Url, FResultCallback Cb)
	{
		const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
		Request->SetVerb(TEXT("GET"));
		Request->SetURL(Url);
		Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *ApiKey));
		Request->SetHeader(TEXT("X-Platform"), TEXT("stripe")); // Web Billing products
		Request->SetHeader(TEXT("Accept"), TEXT("application/json"));

		Request->OnProcessRequestComplete().BindLambda(
			[Cb](FHttpRequestPtr /*Req*/, FHttpResponsePtr Response, bool bConnectedSuccessfully)
			{
				if (bConnectedSuccessfully && Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode()))
				{
					Cb(Response->GetContentAsString(), false);
				}
				else
				{
					const FString Body = Response.IsValid()
						? Response->GetContentAsString()
						: FString(TEXT("{\"message\":\"No response / network error\"}"));
					Cb(Body, true);
				}
			});

		Request->ProcessRequest();
	}
};

IRevenueCatImpl* CreateRevenueCatImpl()
{
	return new FRevenueCatImpl_Desktop();
}

#endif // PLATFORM_DESKTOP
