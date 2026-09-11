// Copyright (c) 2026 panda | Emberfall 余烬陨落 | MIT License

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "ExecCalc_Damage.generated.h"


UCLASS()
class DURA_API UExecCalc_Damage : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()
public:
	UExecCalc_Damage();

	virtual void Execute_Implementation(
		const FGameplayEffectCustomExecutionParameters& ExecutionParams, 
        FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;

    void DetermineDebuff(const FGameplayEffectSpec& Spec,
        const FGameplayEffectCustomExecutionParameters& ExecutionParams,
        FAggregatorEvaluateParameters EvaluationParameters,
        const TMap<FGameplayTag, FGameplayEffectAttributeCaptureDefinition>& InTagsToDefs) const;
};
