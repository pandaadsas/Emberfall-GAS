// Copyright by person HDD  


#include "AbilitySystem/ModMagCalc/MMC_MaxMana.h"
#include "AbilitySystem/DuraAttributeSet.h"
#include "Interaction/CombatInterface.h"

UMMC_MaxMana::UMMC_MaxMana()
{
	IntelligenceDef.AttributeToCapture = UDuraAttributeSet::GetIntelligenceAttribute();
	IntelligenceDef.AttributeSource = EGameplayEffectAttributeCaptureSource::Target;
	IntelligenceDef.bSnapshot = false;

	RelevantAttributesToCapture.Add(IntelligenceDef);
}

float UMMC_MaxMana::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	// 收集来源和目标的标签
	const FGameplayTagContainer* SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	const FGameplayTagContainer* TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	FAggregatorEvaluateParameters EvaluationParameters;
	EvaluationParameters.SourceTags = SourceTags;
	EvaluationParameters.TargetTags = TargetTags;

	float Intelligence = 0.f;
	GetCapturedAttributeMagnitude(IntelligenceDef, Spec, EvaluationParameters, Intelligence);
	Intelligence = FMath::Max<float>(Intelligence, 0.f);

    int32 PlayerLevel = 1;
    if(const UObject* SourceObject = Spec.GetContext().GetSourceObject())
    {
        if(SourceObject->Implements<UCombatInterface>())
        {
            PlayerLevel = ICombatInterface::Execute_GetPlayerLevel(SourceObject);
        }
    }

    return 50.f + 2.0f * Intelligence + 10.f * PlayerLevel;
}
