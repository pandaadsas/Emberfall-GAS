// Copyright by person HDD  


#include "DuraGameplayTags.h"
#include "GameplayTagsManager.h"

FDuraGameplayTags FDuraGameplayTags::DuraGameplayTags;

void FDuraGameplayTags::InitializeNativeGameplayTags()
{
	/*
	 * 主属性
	 */
	DuraGameplayTags.Attributes_Primary_Strength = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Attributes.Primary.Strength"),
		FString("Increases physical damage"));

	DuraGameplayTags.Attributes_Primary_Intelligence = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Attributes.Primary.Intelligence"),
		FString("Increases magical damage"));

	DuraGameplayTags.Attributes_Primary_Resilience = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Attributes.Primary.Resilience"),
		FString("Increases Armor and Armor Penetration"));

	DuraGameplayTags.Attributes_Primary_Vigor = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Attributes.Primary.Vigor"),
		FString("Increases Health"));

	/*
	 * 次级属性
	 */
	DuraGameplayTags.Attributes_Secondary_Armor = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Attributes.Secondary.Armor"), 
		FString("Reduces damage taken, improves Block Chance"));

	DuraGameplayTags.Attributes_Secondary_Armor_Penetration = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Attributes.Secondary.Armor_Penetration"), 
		FString("Reduces damage taken, improves Block Chance"));

	DuraGameplayTags.Attributes_Secondary_Block_Chance = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Attributes.Secondary.Block_Chance"), 
		FString("Reduces damage taken, improves Block Chance"));

	DuraGameplayTags.Attributes_Secondary_CriticalHitChance = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Attributes.Secondary.CriticalHitChance"), 
		FString("Reduces damage taken, improves Block Chance"));

	DuraGameplayTags.Attributes_Secondary_CriticalHitDamage = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Attributes.Secondary.CriticalHitDamage"), 
		FString("Reduces damage taken, improves Block Chance"));

	DuraGameplayTags.Attributes_Secondary_CriticalHitResistance = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Attributes.Secondary.CriticalHitResistance"), 
		FString("Reduces damage taken, improves Block Chance"));

	DuraGameplayTags.Attributes_Secondary_HealthRegeneration = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Attributes.Secondary.HealthRegeneration"), 
		FString("Reduces damage taken, improves Block Chance"));

	DuraGameplayTags.Attributes_Secondary_ManaRegeneration = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Attributes.Secondary.ManaRegeneration"), 
		FString("Reduces damage taken, improves Block Chance"));

	DuraGameplayTags.Attributes_Secondary_MaxHealth = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Attributes.Secondary.MaxHealth"), 
		FString("Reduces damage taken, improves Block Chance"));

	DuraGameplayTags.Attributes_Secondary_MaxMana = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Attributes.Secondary.MaxMana"), 
		FString("Reduces damage taken, improves Block Chance"));


    /*
    * 元属性
    */
    DuraGameplayTags.Attributes_Meta_IncomingXP = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Attributes.Meta.IncomingXP"), 
		FString("Incoming XP Meta Attributes"));
    

	/*
	 * 输入标签
	 */

	DuraGameplayTags.InputTag_LMB = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("InputTag.LMB"),
		FString("Input Tag for Left Mouse Button")
	);

	DuraGameplayTags.InputTag_RMB = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("InputTag.RMB"),
		FString("Input Tag for Right Mouse Button")
	);

	DuraGameplayTags.InputTag_1 = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("InputTag.1"),
		FString("Input Tag for 1 key")
	);

	DuraGameplayTags.InputTag_2 = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("InputTag.2"),
		FString("Input Tag for 2 key")
	);

	DuraGameplayTags.InputTag_3 = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("InputTag.3"),
		FString("Input Tag for 3 key")
	);

	DuraGameplayTags.InputTag_4 = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("InputTag.4"),
		FString("Input Tag for 4 key")
	);

    DuraGameplayTags.InputTag_Passive_1 = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("InputTag.Passive.1"),
		FString("Input Tag for Passive 1")
	);

	DuraGameplayTags.InputTag_Passive_2 = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("InputTag.Passive.2"),
		FString("Input Tag for Passive 2")
	);

    
	

	DuraGameplayTags.Damage = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Damage"),
		FString("Damage")
	);

	/*
	* 伤害类型
	*/

	DuraGameplayTags.Damage_Fire = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Damage.Fire"),
		FString("Fire Damage Type")
	);

	DuraGameplayTags.Damage_Lightning = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Damage.Lightning"),
		FString("Lightning Damage Type")
	);

	DuraGameplayTags.Damage_Arcane = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Damage.Arcane"),
		FString("Arcane Damage Type")
	);

	DuraGameplayTags.Damage_Physical = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Damage.Physical"),
		FString("Physical Damage Type")
	);

    /*
	* 伤害抗性
	*/

	DuraGameplayTags.Attributes_Resistance_Fire = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Resistance.Fire"),
		FString("Fire Resistance Type")
	);

	DuraGameplayTags.Attributes_Resistance_Lightning = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Resistance.Lightning"),
		FString("Lightning Resistance Type")
	);

	DuraGameplayTags.Attributes_Resistance_Arcane = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Resistance.Arcane"),
		FString("Arcane Resistance Type")
	);

	DuraGameplayTags.Attributes_Resistance_Physical = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Resistance.Physical"),
		FString("Physical Resistance Type")
	);

    /* 减益 */
    DuraGameplayTags.Debuff_Burn = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Debuff.Burn"),
		FString("Debuff for fire damage")
	);

	DuraGameplayTags.Debuff_Stun = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Debuff.Stun"),
		FString("Debuff for  Lightning damage")
	);

	DuraGameplayTags.Debuff_Arcane = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Debuff.Arcane"),
		FString("Debuff for Arcane damage")
	);

	DuraGameplayTags.Debuff_Physical = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Debuff.Physical"),
		FString("Debuff for Physical damage")
	);

    DuraGameplayTags.Debuff_Chance = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Debuff.Chance"),
		FString("Debuff for Debuff_Chance")
	);

	DuraGameplayTags.Debuff_Damage = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Debuff.Damage"),
		FString("Debuff for Damage")
	);

	DuraGameplayTags.Debuff_Duration = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Debuff.Duration"),
		FString("Debuff for Duration")
	);

	DuraGameplayTags.Debuff_Frequency = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Debuff.Frequency"),
		FString("Debuff for Frequency")
	);

	
	/* 伤害类型到抗性的映射 */
	DuraGameplayTags.DamageTypesToResistances.Add(DuraGameplayTags.Damage_Arcane, DuraGameplayTags.Attributes_Resistance_Arcane);
	DuraGameplayTags.DamageTypesToResistances.Add(DuraGameplayTags.Damage_Fire, DuraGameplayTags.Attributes_Resistance_Fire);
	DuraGameplayTags.DamageTypesToResistances.Add(DuraGameplayTags.Damage_Lightning, DuraGameplayTags.Attributes_Resistance_Lightning);
	DuraGameplayTags.DamageTypesToResistances.Add(DuraGameplayTags.Damage_Physical, DuraGameplayTags.Attributes_Resistance_Physical);

    /* 伤害类型到减益的映射 */
    DuraGameplayTags.DamageTypesToDebuffs.Add(DuraGameplayTags.Damage_Arcane, DuraGameplayTags.Debuff_Arcane);
	DuraGameplayTags.DamageTypesToDebuffs.Add(DuraGameplayTags.Damage_Fire, DuraGameplayTags.Debuff_Burn);
	DuraGameplayTags.DamageTypesToDebuffs.Add(DuraGameplayTags.Damage_Lightning, DuraGameplayTags.Debuff_Stun);
	DuraGameplayTags.DamageTypesToDebuffs.Add(DuraGameplayTags.Damage_Physical, DuraGameplayTags.Debuff_Physical);


	DuraGameplayTags.Effect_HitReact = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Effects.HitReact"),
		FString("Tag granted when Hit Reacting")
	);
	
    
    DuraGameplayTags.Abilities_None = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.None"),
		FString("None Abilitie - like the nullptr for Ability Tags")
	);

	DuraGameplayTags.Abilities_Attack = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.Attack"),
		FString("Attack Abilities Tag")
	);

    DuraGameplayTags.Abilities_Summon = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.Summon"),
		FString("Summon Abilities Tag")
	);

    DuraGameplayTags.Abilities_Fire_Bolt = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.Fire.FireBolt"),
		FString("FireBolt Ability Tag")
	);

    DuraGameplayTags.Abilities_Lightning_Electrocute = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.Lightning.Electrocute"),
		FString("Electrocute Ability Tag")
	);

    DuraGameplayTags.Abilities_Arcane_ArcaneShards = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.Arcane.ArcaneShards"),
		FString("ArcaneShards Ability Tag")
	);

    DuraGameplayTags.Abilities_Fire_Blast = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.Fire.FireBlast"),
		FString("FireBlast Ability Tag")
	);
    

    /*
        被动技能
    */
    DuraGameplayTags.Abilities_Passive_HaloOfProtection = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.Passive.HaloOfProtection"),
		FString("Halo Of Protection Tag")
	);

    DuraGameplayTags.Abilities_Passive_LifeSiphon = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.Passive.LifeSiphon"),
		FString("Passive Life Siphon Tag")
	);

    DuraGameplayTags.Abilities_Passive_ManaSiphon = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.Passive.ManaSiphon"),
		FString("Passive Mana Siphon Tag")
	);


    DuraGameplayTags.Abilities_HitReact = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.HitReact"),
		FString("Attack HitReact Tag")
	);

    DuraGameplayTags.Abilities_Status_Locked = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.Status.Locked"),
		FString("Status Locked")
	);

    DuraGameplayTags.Abilities_Status_Eligible = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.Status.Eligible"),
		FString("Status Eligible")
	);

    DuraGameplayTags.Abilities_Status_UnLocked = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.Status.UnLocked"),
		FString("Status UnLocked")
	);

    DuraGameplayTags.Abilities_Status_Equipped = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.Status.Equipped"),
		FString("Status Equipped")
	);

    DuraGameplayTags.Abilities_Type_Offensive = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.Type.Offensive"),
		FString("Type Equipped")
	);

    DuraGameplayTags.Abilities_Type_Passive = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.Type.Passive"),
		FString("Type Passive")
	);

    DuraGameplayTags.Abilities_Type_None = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.Type.None"),
		FString("Type None")
	);

    /*
	* 冷却
	*/

    DuraGameplayTags.CoolDown_Fire_Bolt = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("CoolDown.Fire.FireBolt"),
		FString("CoolDown Ability Tag")
	);

    

    
	/*
	* 战斗插槽
	*/
	DuraGameplayTags.CombatSocket_Weapon = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("CombatSocket.Weapon"),
		FString("Weapon"));

	DuraGameplayTags.CombatSocket_RightHand = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("CombatSocket.RightHand"),
		FString("Right Hand"));

	DuraGameplayTags.CombatSocket_LeftHand = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("CombatSocket.LeftHand"),
		FString("Left Hand"));

    DuraGameplayTags.CombatSocket_Tail = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("CombatSocket.Tail"),
		FString("Tail"));
        

    /*
    * 蒙太奇标签
    */
    DuraGameplayTags.Montage_Attack_1 = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Montage.Attack.1"),
		FString("Attack 1"));

    DuraGameplayTags.Montage_Attack_2 = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Montage.Attack.2"),
		FString("Attack 2"));

    DuraGameplayTags.Montage_Attack_3 = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Montage.Attack.3"),
		FString("Attack 3"));

    DuraGameplayTags.Montage_Attack_4 = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Montage.Attack.4"),
		FString("Attack 4"));


    /* 玩家标签 */
    DuraGameplayTags.Player_Block_CursorTrace = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Player.Block.CursorTrace"),
		FString("Block tracing under the cursor"));

    DuraGameplayTags.Player_Block_InputPressed = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Player.Block.InputPressed"),
		FString("Block Input Pressed callback for input"));

    DuraGameplayTags.Player_Block_InputHeld= UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Player.Block.InputHeld"),
		FString("Block Input Held callback for input"));

    DuraGameplayTags.Player_Block_InputReleased = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Player.Block.InputReleased"),
		FString("Block Input Released callback for input"));

    DuraGameplayTags.GameplayCue_FireBlast = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("GameplayCue.FireBlast"),
		FString("GameplayCue FireBlast Tag"));

        
}
