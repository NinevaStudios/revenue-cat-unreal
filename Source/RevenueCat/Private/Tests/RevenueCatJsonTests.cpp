// RevenueCat UE plugin — unit tests for the JSON parser.
// Deterministic, no network / accounts / editor world required. Run from the editor's
// Session Frontend -> Automation (filter "RevenueCat"), or headless via UnrealEditor-Cmd
// -ExecCmds="Automation RunTests RevenueCat; Quit".

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "RevenueCatJson.h"
#include "RevenueCatTypes.h"

// Dates far in the future/past keep "is active now" checks deterministic regardless of clock.
// EAutomationTestFlags is an enum class in UE5, so inline the (typed) flag expression per test.
#define RC_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// ---------------------------------------------------------------------------
// CustomerInfo — active entitlement + active/expired subscriptions + top-level fields
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRevenueCatParseActiveEntitlement,
	"RevenueCat.Json.ActiveEntitlement", RC_TEST_FLAGS)
bool FRevenueCatParseActiveEntitlement::RunTest(const FString&)
{
	const TCHAR* Json = TEXT(R"JSON(
	{
	  "request_date_ms": 1,
	  "subscriber": {
	    "original_app_user_id": "test_user_42",
	    "management_url": "https://manage.example.com",
	    "entitlements": {
	      "pro": {
	        "expires_date": "2099-01-01T00:00:00Z",
	        "grace_period_expires_date": null,
	        "product_identifier": "rc_promo_pro_monthly",
	        "purchase_date": "2026-06-15T11:30:12Z"
	      }
	    },
	    "subscriptions": {
	      "rc_promo_pro_monthly": { "expires_date": "2099-01-01T00:00:00Z" },
	      "old_monthly":          { "expires_date": "2000-01-01T00:00:00Z" }
	    },
	    "non_subscriptions": {}
	  }
	}
	)JSON");

	const FRCCustomerInfo Info = RevenueCatJson::ParseCustomerInfo(Json);

	TestEqual(TEXT("original_app_user_id"), Info.OriginalAppUserId, FString(TEXT("test_user_42")));
	TestEqual(TEXT("management_url"), Info.ManagementURL, FString(TEXT("https://manage.example.com")));

	TestTrue(TEXT("pro is active"), Info.ActiveEntitlements.Contains(TEXT("pro")));
	TestEqual(TEXT("only one active entitlement"), Info.ActiveEntitlements.Num(), 1);
	if (const FRCEntitlementInfo* Pro = Info.ActiveEntitlements.Find(TEXT("pro")))
	{
		TestTrue(TEXT("pro.bIsActive"), Pro->bIsActive);
		TestEqual(TEXT("pro.product"), Pro->ProductIdentifier, FString(TEXT("rc_promo_pro_monthly")));
		TestEqual(TEXT("pro.expiry year"), Pro->ExpirationDate.GetYear(), 2099);
	}

	TestEqual(TEXT("only one active subscription"), Info.ActiveSubscriptions.Num(), 1);
	TestTrue(TEXT("active sub present"), Info.ActiveSubscriptions.Contains(TEXT("rc_promo_pro_monthly")));
	TestFalse(TEXT("expired sub excluded"), Info.ActiveSubscriptions.Contains(TEXT("old_monthly")));
	return true;
}

// ---------------------------------------------------------------------------
// CustomerInfo — expired entitlement must NOT be reported as active
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRevenueCatParseExpiredEntitlement,
	"RevenueCat.Json.ExpiredEntitlement", RC_TEST_FLAGS)
bool FRevenueCatParseExpiredEntitlement::RunTest(const FString&)
{
	const TCHAR* Json = TEXT(R"JSON(
	{ "subscriber": { "entitlements": {
	    "pro": { "expires_date": "2000-01-01T00:00:00Z", "product_identifier": "p" }
	}, "subscriptions": {}, "non_subscriptions": {} } }
	)JSON");

	const FRCCustomerInfo Info = RevenueCatJson::ParseCustomerInfo(Json);
	TestEqual(TEXT("no active entitlements"), Info.ActiveEntitlements.Num(), 0);
	TestFalse(TEXT("pro not active"), Info.ActiveEntitlements.Contains(TEXT("pro")));
	return true;
}

// ---------------------------------------------------------------------------
// CustomerInfo — null expires_date means non-expiring (lifetime) => active
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRevenueCatParseLifetimeEntitlement,
	"RevenueCat.Json.LifetimeEntitlement", RC_TEST_FLAGS)
bool FRevenueCatParseLifetimeEntitlement::RunTest(const FString&)
{
	const TCHAR* Json = TEXT(R"JSON(
	{ "subscriber": { "entitlements": {
	    "lifetime": { "expires_date": null, "product_identifier": "lifetime_pack" }
	}, "subscriptions": {}, "non_subscriptions": {} } }
	)JSON");

	const FRCCustomerInfo Info = RevenueCatJson::ParseCustomerInfo(Json);
	TestTrue(TEXT("lifetime active"), Info.ActiveEntitlements.Contains(TEXT("lifetime")));
	return true;
}

// ---------------------------------------------------------------------------
// CustomerInfo — expired but within grace period => still active
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRevenueCatParseGracePeriod,
	"RevenueCat.Json.GracePeriod", RC_TEST_FLAGS)
bool FRevenueCatParseGracePeriod::RunTest(const FString&)
{
	const TCHAR* Json = TEXT(R"JSON(
	{ "subscriber": { "entitlements": {
	    "pro": {
	      "expires_date": "2000-01-01T00:00:00Z",
	      "grace_period_expires_date": "2099-01-01T00:00:00Z",
	      "product_identifier": "p"
	    }
	}, "subscriptions": {}, "non_subscriptions": {} } }
	)JSON");

	const FRCCustomerInfo Info = RevenueCatJson::ParseCustomerInfo(Json);
	TestTrue(TEXT("grace keeps pro active"), Info.ActiveEntitlements.Contains(TEXT("pro")));
	return true;
}

// ---------------------------------------------------------------------------
// Offerings — REST {"value": {...}} wrapper, current selection, package mapping
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRevenueCatParseOfferings,
	"RevenueCat.Json.Offerings", RC_TEST_FLAGS)
bool FRevenueCatParseOfferings::RunTest(const FString&)
{
	const TCHAR* Json = TEXT(R"JSON(
	{ "value": {
	    "current_offering_id": "default",
	    "offerings": [
	      { "identifier": "default", "packages": [
	          { "identifier": "$rc_monthly", "platform_product_identifier": "monthly_prod" },
	          { "identifier": "$rc_annual",  "platform_product_identifier": "yearly_prod"  }
	      ]},
	      { "identifier": "promo", "packages": [] }
	    ]
	} }
	)JSON");

	const FRCOfferings Offerings = RevenueCatJson::ParseOfferings(Json);

	TestEqual(TEXT("two offerings"), Offerings.All.Num(), 2);
	TestTrue(TEXT("has default"), Offerings.All.Contains(TEXT("default")));
	TestTrue(TEXT("has promo"), Offerings.All.Contains(TEXT("promo")));
	TestEqual(TEXT("current is default"), Offerings.Current.Identifier, FString(TEXT("default")));
	TestEqual(TEXT("current packages"), Offerings.Current.Packages.Num(), 2);
	if (Offerings.Current.Packages.Num() == 2)
	{
		TestEqual(TEXT("pkg0 id"), Offerings.Current.Packages[0].Identifier, FString(TEXT("$rc_monthly")));
		TestEqual(TEXT("pkg0 product"), Offerings.Current.Packages[0].StoreProduct.Identifier, FString(TEXT("monthly_prod")));
		TestEqual(TEXT("pkg0 offering"), Offerings.Current.Packages[0].OfferingIdentifier, FString(TEXT("default")));
	}
	return true;
}

// ---------------------------------------------------------------------------
// Error parsing — code/message extracted; no error when bIsError == false
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRevenueCatParseError,
	"RevenueCat.Json.Error", RC_TEST_FLAGS)
bool FRevenueCatParseError::RunTest(const FString&)
{
	const TCHAR* Json = TEXT(R"JSON({ "code": 7218, "message": "Invalid API Key." })JSON");

	const FRCError Err = RevenueCatJson::ParseError(Json, /*bIsError*/ true);
	TestEqual(TEXT("error code"), Err.Code, 7218);
	TestEqual(TEXT("error message"), Err.Message, FString(TEXT("Invalid API Key.")));
	TestTrue(TEXT("HasError true"), Err.HasError());

	const FRCError NoErr = RevenueCatJson::ParseError(Json, /*bIsError*/ false);
	TestFalse(TEXT("HasError false on success"), NoErr.HasError());
	return true;
}

// ---------------------------------------------------------------------------
// Robustness — malformed / empty JSON must not crash and yields empty structs
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRevenueCatParseGarbage,
	"RevenueCat.Json.Garbage", RC_TEST_FLAGS)
bool FRevenueCatParseGarbage::RunTest(const FString&)
{
	const FRCCustomerInfo Info = RevenueCatJson::ParseCustomerInfo(TEXT("not-json{"));
	TestEqual(TEXT("garbage -> no entitlements"), Info.ActiveEntitlements.Num(), 0);

	const FRCCustomerInfo Empty = RevenueCatJson::ParseCustomerInfo(TEXT("{}"));
	TestEqual(TEXT("empty -> no entitlements"), Empty.ActiveEntitlements.Num(), 0);

	const FRCOfferings Offerings = RevenueCatJson::ParseOfferings(TEXT("not-json"));
	TestEqual(TEXT("garbage -> no offerings"), Offerings.All.Num(), 0);
	return true;
}

// ---------------------------------------------------------------------------
// SDK shape (iOS/Android via hybrid-common): entitlements.active + ownership
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRevenueCatParseSdkCustomerInfo,
	"RevenueCat.Json.SdkCustomerInfo", RC_TEST_FLAGS)
bool FRevenueCatParseSdkCustomerInfo::RunTest(const FString&)
{
	const TCHAR* Json = TEXT(R"JSON(
	{
	  "originalAppUserId": "test_user_42",
	  "managementURL": "https://manage.example.com",
	  "entitlements": {
	    "active": {
	      "pro": { "isActive": true, "productIdentifier": "rc_monthly",
	               "expirationDate": "2099-01-01T00:00:00Z", "ownershipType": "FAMILY_SHARED" }
	    },
	    "all": { "pro": { "isActive": true } }
	  },
	  "activeSubscriptions": [ "rc_monthly" ]
	}
	)JSON");

	const FRCCustomerInfo Info = RevenueCatJson::ParseCustomerInfo(Json);
	TestEqual(TEXT("originalAppUserId"), Info.OriginalAppUserId, FString(TEXT("test_user_42")));
	TestTrue(TEXT("pro active"), Info.ActiveEntitlements.Contains(TEXT("pro")));
	if (const FRCEntitlementInfo* Pro = Info.ActiveEntitlements.Find(TEXT("pro")))
	{
		TestTrue(TEXT("pro.bIsActive"), Pro->bIsActive);
		TestEqual(TEXT("pro.product"), Pro->ProductIdentifier, FString(TEXT("rc_monthly")));
		TestEqual(TEXT("pro.expiry year"), Pro->ExpirationDate.GetYear(), 2099);
		TestEqual(TEXT("pro ownership"), (int32)Pro->OwnershipType, (int32)ERCOwnershipType::FamilyShared);
	}
	TestTrue(TEXT("active sub"), Info.ActiveSubscriptions.Contains(TEXT("rc_monthly")));
	return true;
}

// ---------------------------------------------------------------------------
// SDK shape — offerings carry localized price/title (REST does not)
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRevenueCatParseSdkOfferings,
	"RevenueCat.Json.SdkOfferings", RC_TEST_FLAGS)
bool FRevenueCatParseSdkOfferings::RunTest(const FString&)
{
	const TCHAR* Json = TEXT(R"JSON(
	{
	  "current": {
	    "identifier": "default",
	    "availablePackages": [
	      { "identifier": "$rc_monthly", "product": {
	          "identifier": "p_monthly", "priceString": "$0.99", "price": 0.99,
	          "currencyCode": "USD", "title": "Monthly" } }
	    ]
	  },
	  "all": { "default": { "identifier": "default", "availablePackages": [] } }
	}
	)JSON");

	const FRCOfferings Offerings = RevenueCatJson::ParseOfferings(Json);
	TestEqual(TEXT("current id"), Offerings.Current.Identifier, FString(TEXT("default")));
	TestTrue(TEXT("all has default"), Offerings.All.Contains(TEXT("default")));
	TestEqual(TEXT("current packages"), Offerings.Current.Packages.Num(), 1);
	if (Offerings.Current.Packages.Num() == 1)
	{
		const FRCPackage& P = Offerings.Current.Packages[0];
		TestEqual(TEXT("pkg id"), P.Identifier, FString(TEXT("$rc_monthly")));
		TestEqual(TEXT("pkg offering"), P.OfferingIdentifier, FString(TEXT("default")));
		TestEqual(TEXT("product id"), P.StoreProduct.Identifier, FString(TEXT("p_monthly")));
		TestEqual(TEXT("price string"), P.StoreProduct.PriceString, FString(TEXT("$0.99")));
		TestEqual(TEXT("currency"), P.StoreProduct.CurrencyCode, FString(TEXT("USD")));
		TestTrue(TEXT("price > 0"), P.StoreProduct.Price > 0.0f);
	}
	return true;
}

// ---------------------------------------------------------------------------
// SDK shape — purchase result nests the customer info under "customerInfo"
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRevenueCatParseSdkPurchaseResult,
	"RevenueCat.Json.SdkPurchaseResult", RC_TEST_FLAGS)
bool FRevenueCatParseSdkPurchaseResult::RunTest(const FString&)
{
	const TCHAR* Json = TEXT(R"JSON(
	{ "productIdentifier": "p_monthly",
	  "customerInfo": { "entitlements": { "active": {
	      "pro": { "isActive": true, "productIdentifier": "p_monthly" } } } } }
	)JSON");

	const FRCCustomerInfo Info = RevenueCatJson::ParseCustomerInfo(Json);
	TestTrue(TEXT("pro active from purchase result"), Info.ActiveEntitlements.Contains(TEXT("pro")));
	return true;
}

#undef RC_TEST_FLAGS

#endif // WITH_DEV_AUTOMATION_TESTS
