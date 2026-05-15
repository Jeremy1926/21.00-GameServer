#include "pch.h"
#include "Abilities.h"

void ConsumeAllReplicatedData(UFortAbilitySystemComponentAthena* AbilitySystemComponent, FGameplayAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey) {
    auto& AbilityTargetDataMap = *(FGameplayAbilityReplicatedDataContainer*)(__int64(AbilitySystemComponent) + offsetof(UAbilitySystemComponent, ActivatableAbilities) + sizeof(FGameplayAbilitySpecContainer));

    for (FGameplayAbilityReplicatedDataContainer::FKeyDataPair& Pair : AbilityTargetDataMap.InUseData)
        if (Pair.Key().AbilityHandle.Handle == AbilityHandle.Handle && Pair.Key().PredictionKeyAtCreation == AbilityOriginalPredictionKey.Current)
        {
            Pair.Value().Object = nullptr;
            Pair.Value().SharedReferenceCount->SharedReferenceCount = 1;
        }
}


void Abilities::InternalServerTryActivateAbility(UFortAbilitySystemComponentAthena* AbilitySystemComponent, FGameplayAbilitySpecHandle Handle, bool InputPressed, FPredictionKey& PredictionKey, FGameplayEventData* TriggerEventData) {
    auto Spec = AbilitySystemComponent->ActivatableAbilities.Items.Search([&](FGameplayAbilitySpec& item) {
        return item.Handle.Handle == Handle.Handle;
        });

    if (!Spec)
        return AbilitySystemComponent->ClientActivateAbilityFailed(Handle, PredictionKey.Current);

    ConsumeAllReplicatedData(AbilitySystemComponent, Handle, PredictionKey);
    Spec->InputPressed = true;

    UGameplayAbility* InstancedAbility = nullptr;

    auto InternalTryActivateAbility = (bool (*)(UAbilitySystemComponent*, FGameplayAbilitySpecHandle, FPredictionKey, UGameplayAbility**, void*, const FGameplayEventData*)) Sarah::Offsets::InternalTryActivateAbility;
    if (!InternalTryActivateAbility(AbilitySystemComponent, Handle, PredictionKey, &InstancedAbility, nullptr, TriggerEventData))
    {
        AbilitySystemComponent->ClientActivateAbilityFailed(Handle, PredictionKey.Current);
        Spec->InputPressed = false;
        AbilitySystemComponent->ActivatableAbilities.MarkItemDirty(*Spec);
    }
}

FGameplayAbilitySpec Abilities::GiveAbility(UAbilitySystemComponent* AbilitySystemComponent, UObject* Ability)
{
    if (!AbilitySystemComponent || !Ability)
        return {};

    FGameplayAbilitySpec Spec{};

    ((void (*)(FGameplayAbilitySpec*, UGameplayAbility*, int, int, UObject*))Sarah::Offsets::ConstructAbilitySpec)(&Spec, (UGameplayAbility*)Ability, 1, -1, nullptr);
    ((FGameplayAbilitySpecHandle * (__fastcall*)(UAbilitySystemComponent*, FGameplayAbilitySpecHandle*, FGameplayAbilitySpec)) Sarah::Offsets::InternalGiveAbility)(AbilitySystemComponent, &Spec.Handle, Spec);

    return Spec;
}

TArray<FGameplayAbilitySpec> Abilities::GiveAbilitySet(UAbilitySystemComponent *AbilitySystemComponent, UFortAbilitySet* Set)
{
    TArray<FGameplayAbilitySpec> Ret;

    if (Set)
    {
        for (auto& GameplayAbility : Set->GameplayAbilities)
            Ret.Add(GiveAbility(AbilitySystemComponent, GameplayAbility->DefaultObject));
        for (auto& GameplayEffect : Set->PassiveGameplayEffects)
            AbilitySystemComponent->BP_ApplyGameplayEffectToSelf(GameplayEffect.GameplayEffect.Get(), GameplayEffect.Level, AbilitySystemComponent->MakeEffectContext());
    }

    return Ret;
}

void Abilities::Hook() {
    Utils::HookEvery<UAbilitySystemComponent>(Sarah::Offsets::InternalServerTryActivateAbilityVft, InternalServerTryActivateAbility);
}
