// Copyright (c) 2026 Nineva Studios
// Single place that turns the raw JSON from any platform into USTRUCTs.
// Understands both the REST v1 shape (desktop) and the SDK / hybrid-common shape (mobile).

#pragma once

#include "CoreMinimal.h"
#include "RevenueCatTypes.h"

namespace RevenueCatJson
{
	FRCOfferings    ParseOfferings(const FString& Json);
	FRCCustomerInfo ParseCustomerInfo(const FString& Json);
	FRCError        ParseError(const FString& Json, bool bIsError);
}
