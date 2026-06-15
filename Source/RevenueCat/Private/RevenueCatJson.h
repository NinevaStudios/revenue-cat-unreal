// RevenueCat UE plugin — learning skeleton.
// Single place that turns the raw JSON from any platform into USTRUCTs.
// Bodies are stubs for now (TODO: real parsing with FJsonObjectConverter).

#pragma once

#include "CoreMinimal.h"
#include "RevenueCatTypes.h"

namespace RevenueCatJson
{
	FRCOfferings    ParseOfferings(const FString& Json);
	FRCCustomerInfo ParseCustomerInfo(const FString& Json);
	FRCError        ParseError(const FString& Json, bool bIsError);
}
