// RevenueCat UE plugin.
// Single place that turns the raw JSON (RevenueCat REST v1 / hybrid-common) into USTRUCTs.

#include "RevenueCatJson.h"

#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace
{
	TSharedPtr<FJsonObject> ParseRoot(const FString& Json)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		FJsonSerializer::Deserialize(Reader, Root);
		return Root;
	}

	bool IsFutureIso8601(const FString& Iso, const FDateTime& Now, FDateTime& OutDate)
	{
		if (Iso.IsEmpty()) { return false; }
		if (FDateTime::ParseIso8601(*Iso, OutDate)) { return OutDate > Now; }
		return false;
	}
}

namespace RevenueCatJson
{
	// Parses the RevenueCat v1 "subscriber" object (also returned by POST /receipts).
	FRCCustomerInfo ParseCustomerInfo(const FString& Json)
	{
		FRCCustomerInfo Out;
		const TSharedPtr<FJsonObject> Root = ParseRoot(Json);
		if (!Root.IsValid()) { return Out; }

		const TSharedPtr<FJsonObject>* Sub = nullptr;
		if (!Root->TryGetObjectField(TEXT("subscriber"), Sub)) { return Out; }

		(*Sub)->TryGetStringField(TEXT("original_app_user_id"), Out.OriginalAppUserId);
		(*Sub)->TryGetStringField(TEXT("management_url"), Out.ManagementURL);

		const FDateTime Now = FDateTime::UtcNow();

		// entitlements: keep only the ones currently granting access.
		const TSharedPtr<FJsonObject>* Entitlements = nullptr;
		if ((*Sub)->TryGetObjectField(TEXT("entitlements"), Entitlements))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Entitlements)->Values)
			{
				const TSharedPtr<FJsonObject> E = Pair.Value->AsObject();
				if (!E.IsValid()) { continue; }

				FRCEntitlementInfo Info;
				Info.Identifier = Pair.Key;
				E->TryGetStringField(TEXT("product_identifier"), Info.ProductIdentifier);

				// No expires_date => non-expiring (lifetime / non-consumable) => active.
				bool bActive = true;
				FString ExpStr;
				if (E->TryGetStringField(TEXT("expires_date"), ExpStr) && !ExpStr.IsEmpty())
				{
					FDateTime Exp;
					FDateTime::ParseIso8601(*ExpStr, Exp);
					Info.ExpirationDate = Exp;
					bActive = Exp > Now;

					// Grace period extends access past the expiry.
					if (!bActive)
					{
						FString GraceStr;
						FDateTime Grace;
						if (E->TryGetStringField(TEXT("grace_period_expires_date"), GraceStr)
							&& IsFutureIso8601(GraceStr, Now, Grace))
						{
							bActive = true;
						}
					}
				}

				Info.bIsActive = bActive;
				if (bActive)
				{
					Out.ActiveEntitlements.Add(Pair.Key, Info);
				}
			}
		}

		// subscriptions: collect identifiers that are currently active.
		const TSharedPtr<FJsonObject>* Subscriptions = nullptr;
		if ((*Sub)->TryGetObjectField(TEXT("subscriptions"), Subscriptions))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Subscriptions)->Values)
			{
				const TSharedPtr<FJsonObject> S = Pair.Value->AsObject();
				if (!S.IsValid()) { continue; }

				bool bActive = true;
				FString ExpStr;
				FDateTime Exp;
				if (S->TryGetStringField(TEXT("expires_date"), ExpStr) && !ExpStr.IsEmpty())
				{
					bActive = IsFutureIso8601(ExpStr, Now, Exp);
				}
				if (bActive)
				{
					Out.ActiveSubscriptions.Add(Pair.Key);
				}
			}
		}

		return Out;
	}

	// Parses GET /subscribers/{id}/offerings  (REST wraps it in {"value": {...}}).
	FRCOfferings ParseOfferings(const FString& Json)
	{
		FRCOfferings Out;
		const TSharedPtr<FJsonObject> Root = ParseRoot(Json);
		if (!Root.IsValid()) { return Out; }

		TSharedPtr<FJsonObject> Container = Root;
		const TSharedPtr<FJsonObject>* ValueObj = nullptr;
		if (Root->TryGetObjectField(TEXT("value"), ValueObj))
		{
			Container = *ValueObj;
		}

		FString CurrentId;
		Container->TryGetStringField(TEXT("current_offering_id"), CurrentId);

		const TArray<TSharedPtr<FJsonValue>>* Offerings = nullptr;
		if (Container->TryGetArrayField(TEXT("offerings"), Offerings))
		{
			for (const TSharedPtr<FJsonValue>& OffVal : *Offerings)
			{
				const TSharedPtr<FJsonObject> Off = OffVal->AsObject();
				if (!Off.IsValid()) { continue; }

				FRCOffering Offering;
				Off->TryGetStringField(TEXT("identifier"), Offering.Identifier);

				const TArray<TSharedPtr<FJsonValue>>* Packages = nullptr;
				if (Off->TryGetArrayField(TEXT("packages"), Packages))
				{
					for (const TSharedPtr<FJsonValue>& PkgVal : *Packages)
					{
						const TSharedPtr<FJsonObject> Pkg = PkgVal->AsObject();
						if (!Pkg.IsValid()) { continue; }

						FRCPackage Package;
						Pkg->TryGetStringField(TEXT("identifier"), Package.Identifier);
						// REST only gives the product id here; price/title need a store/Web Billing lookup.
						Pkg->TryGetStringField(TEXT("platform_product_identifier"), Package.StoreProduct.Identifier);
						Offering.Packages.Add(Package);
					}
				}

				Out.All.Add(Offering.Identifier, Offering);
				if (Offering.Identifier == CurrentId)
				{
					Out.Current = Offering;
				}
			}
		}

		return Out;
	}

	FRCError ParseError(const FString& Json, bool bIsError)
	{
		FRCError Error;
		if (!bIsError) { return Error; }

		Error.Code = 1;
		if (const TSharedPtr<FJsonObject> Root = ParseRoot(Json))
		{
			int32 Code = 0;
			if (Root->TryGetNumberField(TEXT("code"), Code)) { Error.Code = Code; }
			Root->TryGetStringField(TEXT("message"), Error.Message);
		}
		if (Error.Message.IsEmpty()) { Error.Message = Json; }
		return Error;
	}
}
