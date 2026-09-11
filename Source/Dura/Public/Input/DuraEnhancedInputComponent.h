// Copyright (c) 2026 panda | Emberfall 余烬陨落 | MIT License

#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "DuraEnhancedInputComponent.generated.h"

/**
 * 
 */
UCLASS()
class DURA_API UDuraEnhancedInputComponent : public UEnhancedInputComponent
{
	GENERATED_BODY()

public:
	template<class UserClass, typename PressedFuncType, typename ReleasedFuncType, typename HeldFuncType>
	void BindAbilityActions(const UInputAction* Action, ETriggerEvent TriggerEvent, UserClass* Object, PressedFuncType PressedFunc, ReleasedFuncType ReleasedFunc, HeldFuncType HeldFunc);
};

template<class UserClass, typename PressedFuncType, typename ReleasedFuncType, typename HeldFuncType>
void UDuraEnhancedInputComponent::BindAbilityActions(const UInputAction* Action, ETriggerEvent TriggerEvent, UserClass* Object, PressedFuncType PressedFunc, ReleasedFuncType ReleasedFunc, HeldFuncType HeldFunc)
{
	BindActionValueLambda(Action, ETriggerEvent::Started, [Object, PressedFunc](const FInputActionValue&)
	{
		(Object->*PressedFunc)();
	});
	BindActionValueLambda(Action, ETriggerEvent::Completed, [Object, ReleasedFunc](const FInputActionValue&)
	{
		(Object->*ReleasedFunc)();
	});
	BindActionValueLambda(Action, ETriggerEvent::Triggered, [Object, HeldFunc](const FInputActionValue&)
	{
		(Object->*HeldFunc)();
	});
}
