// Copyright (c) 2026 Nineva Studios
// Single place that turns raw JSON into USTRUCTs. Handles BOTH response shapes:
//  - REST v1 (desktop branch): { "subscriber": { "entitlements": {...}, "subscriptions": {...} } }
//  - SDK / purchases-hybrid-common (iOS & Android): { "entitlements": { "active": {...} }, ... }

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

	ERCOwnershipType MapOwnership(const FString& Raw)
	{
		if (Raw.Equals(TEXT("PURCHASED"), ESearchCase::IgnoreCase)) { return ERCOwnershipType::Purchased; }
		if (Raw.Equals(TEXT("FAMILY_SHARED"), ESearchCase::IgnoreCase)) { return ERCOwnershipType::FamilyShared; }
		return ERCOwnershipType::Unknown;
	}

	// --- REST v1 (desktop) ---------------------------------------------------
	void ParseRestSubscriber(const TSharedPtr<FJsonObject>& Sub, FRCCustomerInfo& Out)
	{
		Sub->TryGetStringField(TEXT("original_app_user_id"), Out.OriginalAppUserId);
		Sub->TryGetStringField(TEXT("management_url"), Out.ManagementURL);

		const FDateTime Now = FDateTime::UtcNow();

		const TSharedPtr<FJsonObject>* Entitlements = nullptr;
		if (Sub->TryGetObjectField(TEXT("entitlements"), Entitlements))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Entitlements)->Values)
			{
				const TSharedPtr<FJsonObject> E = Pair.Value->AsObject();
				if (!E.IsValid()) { continue; }

				FRCEntitlementInfo Info;
				Info.Identifier = Pair.Key;
				E->TryGetStringField(TEXT("product_identifier"), Info.ProductIdentifier);

				bool bActive = true; // no expires_date => non-expiring => active
				FString ExpStr;
				if (E->TryGetStringField(TEXT("expires_date"), ExpStr) && !ExpStr.IsEmpty())
				{
					FDateTime Exp;
					FDateTime::ParseIso8601(*ExpStr, Exp);
					Info.ExpirationDate = Exp;
					bActive = Exp > Now;
					if (!bActive)
					{
						FString GraceStr; FDateTime Grace;
						if (E->TryGetStringField(TEXT("grace_period_expires_date"), GraceStr)
							&& IsFutureIso8601(GraceStr, Now, Grace))
						{
							bActive = true;
						}
					}
				}
				Info.bIsActive = bActive;
				if (bActive) { Out.ActiveEntitlements.Add(Pair.Key, Info); }
			}
		}

		const TSharedPtr<FJsonObject>* Subscriptions = nullptr;
		if (Sub->TryGetObjectField(TEXT("subscriptions"), Subscriptions))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Subscriptions)->Values)
			{
				const TSharedPtr<FJsonObject> S = Pair.Value->AsObject();
				if (!S.IsValid()) { continue; }
				bool bActive = true;
				FString ExpStr; FDateTime Exp;
				if (S->TryGetStringField(TEXT("expires_date"), ExpStr) && !ExpStr.IsEmpty())
				{
					bActive = IsFutureIso8601(ExpStr, Now, Exp);
				}
				if (bActive) { Out.ActiveSubscriptions.Add(Pair.Key); }
			}
		}
	}

	// --- SDK / hybrid-common (iOS & Android) ---------------------------------
	void ParseSdkCustomerInfo(const TSharedPtr<FJsonObject>& CI, FRCCustomerInfo& Out)
	{
		if (!CI.IsValid()) { return; }

		CI->TryGetStringField(TEXT("originalAppUserId"), Out.OriginalAppUserId);
		CI->TryGetStringField(TEXT("managementURL"), Out.ManagementURL);

		const TSharedPtr<FJsonObject>* Entitlements = nullptr;
		if (CI->TryGetObjectField(TEXT("entitlements"), Entitlements))
		{
			const TSharedPtr<FJsonObject>* Active = nullptr;
			if ((*Entitlements)->TryGetObjectField(TEXT("active"), Active)) // already-active map
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Active)->Values)
				{
					const TSharedPtr<FJsonObject> E = Pair.Value->AsObject();
					if (!E.IsValid()) { continue; }

					FRCEntitlementInfo Info;
					Info.Identifier = Pair.Key;
					Info.bIsActive = true;
					E->TryGetBoolField(TEXT("isActive"), Info.bIsActive);
					E->TryGetStringField(TEXT("productIdentifier"), Info.ProductIdentifier);
					FString ExpStr;
					if (E->TryGetStringField(TEXT("expirationDate"), ExpStr) && !ExpStr.IsEmpty())
					{
						FDateTime::ParseIso8601(*ExpStr, Info.ExpirationDate);
					}
					FString Own;
					E->TryGetStringField(TEXT("ownershipType"), Own);
					Info.OwnershipType = MapOwnership(Own);

					Out.ActiveEntitlements.Add(Pair.Key, Info);
				}
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* Subs = nullptr;
		if (CI->TryGetArrayField(TEXT("activeSubscriptions"), Subs))
		{
			for (const TSharedPtr<FJsonValue>& V : *Subs)
			{
				FString S;
				if (V->TryGetString(S)) { Out.ActiveSubscriptions.Add(S); }
			}
		}
	}

	void ParseRestOfferings(const TSharedPtr<FJsonObject>& Container,
		const TArray<TSharedPtr<FJsonValue>>& Offerings, FRCOfferings& Out)
	{
		FString CurrentId;
		Container->TryGetStringField(TEXT("current_offering_id"), CurrentId);

		for (const TSharedPtr<FJsonValue>& OffVal : Offerings)
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
					Package.OfferingIdentifier = Offering.Identifier;
					Pkg->TryGetStringField(TEXT("identifier"), Package.Identifier);
					Pkg->TryGetStringField(TEXT("platform_product_identifier"), Package.StoreProduct.Identifier);
					Offering.Packages.Add(Package);
				}
			}
			Out.All.Add(Offering.Identifier, Offering);
			if (Offering.Identifier == CurrentId) { Out.Current = Offering; }
		}
	}

	FRCOffering ParseSdkOffering(const TSharedPtr<FJsonObject>& O)
	{
		FRCOffering Offering;
		if (!O.IsValid()) { return Offering; }
		O->TryGetStringField(TEXT("identifier"), Offering.Identifier);

		const TArray<TSharedPtr<FJsonValue>>* Packages = nullptr;
		if (O->TryGetArrayField(TEXT("availablePackages"), Packages))
		{
			for (const TSharedPtr<FJsonValue>& PkgVal : *Packages)
			{
				const TSharedPtr<FJsonObject> Pkg = PkgVal->AsObject();
				if (!Pkg.IsValid()) { continue; }
				FRCPackage Package;
				Package.OfferingIdentifier = Offering.Identifier;
				Pkg->TryGetStringField(TEXT("identifier"), Package.Identifier);

				const TSharedPtr<FJsonObject>* Prod = nullptr;
				if (Pkg->TryGetObjectField(TEXT("product"), Prod))
				{
					(*Prod)->TryGetStringField(TEXT("identifier"), Package.StoreProduct.Identifier);
					(*Prod)->TryGetStringField(TEXT("priceString"), Package.StoreProduct.PriceString);
					double Price = 0.0;
					if ((*Prod)->TryGetNumberField(TEXT("price"), Price)) { Package.StoreProduct.Price = static_cast<float>(Price); }
					(*Prod)->TryGetStringField(TEXT("currencyCode"), Package.StoreProduct.CurrencyCode);
					(*Prod)->TryGetStringField(TEXT("title"), Package.StoreProduct.Title);
					(*Prod)->TryGetStringField(TEXT("description"), Package.StoreProduct.Description);
				}
				Offering.Packages.Add(Package);
			}
		}
		return Offering;
	}
}

namespace RevenueCatJson
{
	FRCCustomerInfo ParseCustomerInfo(const FString& Json)
	{
		FRCCustomerInfo Out;
		const TSharedPtr<FJsonObject> Root = ParseRoot(Json);
		if (!Root.IsValid()) { return Out; }

		const TSharedPtr<FJsonObject>* RestSub = nullptr;
		if (Root->TryGetObjectField(TEXT("subscriber"), RestSub))
		{
			ParseRestSubscriber(*RestSub, Out); // REST v1 (desktop)
		}
		else
		{
			// SDK shape. Purchase results nest the customer info under "customerInfo".
			TSharedPtr<FJsonObject> CI = Root;
			const TSharedPtr<FJsonObject>* Nested = nullptr;
			if (Root->TryGetObjectField(TEXT("customerInfo"), Nested)) { CI = *Nested; }
			ParseSdkCustomerInfo(CI, Out);
		}
		return Out;
	}

	FRCOfferings ParseOfferings(const FString& Json)
	{
		FRCOfferings Out;
		const TSharedPtr<FJsonObject> Root = ParseRoot(Json);
		if (!Root.IsValid()) { return Out; }

		// REST wraps in {"value":...}; SDK does not.
		TSharedPtr<FJsonObject> Container = Root;
		const TSharedPtr<FJsonObject>* ValueObj = nullptr;
		if (Root->TryGetObjectField(TEXT("value"), ValueObj)) { Container = *ValueObj; }

		const TArray<TSharedPtr<FJsonValue>>* RestOfferings = nullptr;
		if (Container->TryGetArrayField(TEXT("offerings"), RestOfferings))
		{
			ParseRestOfferings(Container, *RestOfferings, Out); // REST v1
		}
		else
		{
			// SDK shape: "all" (object) + "current" (object).
			const TSharedPtr<FJsonObject>* All = nullptr;
			if (Root->TryGetObjectField(TEXT("all"), All))
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*All)->Values)
				{
					const TSharedPtr<FJsonObject> O = Pair.Value->AsObject();
					if (O.IsValid()) { Out.All.Add(Pair.Key, ParseSdkOffering(O)); }
				}
			}
			const TSharedPtr<FJsonObject>* Cur = nullptr;
			if (Root->TryGetObjectField(TEXT("current"), Cur))
			{
				Out.Current = ParseSdkOffering(*Cur);
			}
		}
		return Out;
	}

	FRCError ParseError(const FString& Json, bool bIsError)
	{
		FRCError Error;
		if (!bIsError) { return Error; }

		// Default for payloads without a code (non-JSON body, unexpected shape).
		Error.Code = RevenueCatErrorCodes::Internal;
		if (const TSharedPtr<FJsonObject> Root = ParseRoot(Json))
		{
			int32 Code = 0;
			if (Root->TryGetNumberField(TEXT("code"), Code)) { Error.Code = Code; }
			Root->TryGetStringField(TEXT("message"), Error.Message);
			Root->TryGetStringField(TEXT("readableErrorCode"), Error.ReadableErrorCode);

			bool bCancelled = false;
			if (Root->TryGetBoolField(TEXT("userCancelled"), bCancelled)) { Error.bUserCancelled = bCancelled; }
		}
		if (Error.Code == RevenueCatErrorCodes::PurchaseCancelled) { Error.bUserCancelled = true; }
		if (Error.Message.IsEmpty()) { Error.Message = Json.IsEmpty() ? FString(TEXT("Unknown error")) : Json; }
		return Error;
	}
}
