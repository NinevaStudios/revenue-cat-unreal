// RevenueCat UE plugin.

#include "RCDemoConversions.h"

FSlateColor URCDemoConversions::ActiveToColor(bool bActive)
{
	return FSlateColor(bActive
		? FLinearColor(0.11f, 0.62f, 0.34f)
		: FLinearColor(0.45f, 0.45f, 0.45f));
}

bool URCDemoConversions::NotBool(bool bValue)
{
	return !bValue;
}

bool URCDemoConversions::IsNotEmpty(const FString& Value)
{
	return !Value.IsEmpty();
}
