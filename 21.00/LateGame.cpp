#include "pch.h"
#include "Lategame.h"
#include "Utils.h"

void Lategame::Init()
{
    Shotguns = 
    {
        FItemAndCount(1, {}, Utils::FindObject<UFortWeaponRangedItemDefinition>(L"/FlipperGameplay/Items/Weapons/BurstShotgun/WID_Shotgun_CoreBurst_Athena_SR.WID_Shotgun_CoreBurst_Athena_SR")), // striker 
            FItemAndCount(1, {}, Utils::FindObject<UFortWeaponRangedItemDefinition>(L"/FlipperGameplay/Items/Weapons/BurstShotgun/WID_Shotgun_CoreBurst_Athena_VR.WID_Shotgun_CoreBurst_Athena_VR")), // striker
            FItemAndCount(1, {}, Utils::FindObject<UFortWeaponRangedItemDefinition>(L"/FlipperGameplay/Items/Weapons/DPSShotgun/WID_Shotgun_CoreDPS_Athena_SR.WID_Shotgun_CoreDPS_Athena_SR")), // auto 
            FItemAndCount(1, {}, Utils::FindObject<UFortWeaponRangedItemDefinition>(L"/FlipperGameplay/Items/Weapons/DPSShotgun/WID_Shotgun_CoreDPS_Athena_VR.WID_Shotgun_CoreDPS_Athena_VR")), // auto 
            FItemAndCount(1, {}, Utils::FindObject<UFortWeaponRangedItemDefinition>(L"/DaisyWeaponGameplay/Items/Weapons/Shotguns/TwoShotShotgun/WID_Shotgun_TwoShot_Pump_Athena_SR.WID_Shotgun_TwoShot_Pump_Athena_SR")), // two-shot 
            FItemAndCount(1, {}, Utils::FindObject<UFortWeaponRangedItemDefinition>(L"/DaisyWeaponGameplay/Items/Weapons/Shotguns/TwoShotShotgun/WID_Shotgun_TwoShot_Pump_Athena_VR.WID_Shotgun_TwoShot_Pump_Athena_VR")), // two-shot
    };

    AssaultRifles =
    {
        FItemAndCount(1, {}, Utils::FindObject<UFortWeaponRangedItemDefinition>(L"/FlipperGameplay/Items/Weapons/CoreAR/WID_Assault_CoreAR_Athena_SR.WID_Assault_CoreAR_Athena_SR")), // ranger 
        FItemAndCount(1, {}, Utils::FindObject<UFortWeaponRangedItemDefinition>(L"/FlipperGameplay/Items/Weapons/CoreAR/WID_Assault_CoreAR_Athena_VR.WID_Assault_CoreAR_Athena_VR")), // ranger
        FItemAndCount(1, {}, Utils::FindObject<UFortWeaponRangedItemDefinition>(L"/ResolveGameplay/Items/Guns/RedDotBurst/WID_Assault_RedDotBurstAR_Athena_SR.WID_Assault_RedDotBurstAR_Athena_SR")), // striker burst
        FItemAndCount(1, {}, Utils::FindObject<UFortWeaponRangedItemDefinition>(L"/ResolveGameplay/Items/Guns/RedDotBurst/WID_Assault_RedDotBurstAR_Athena_VR.WID_Assault_RedDotBurstAR_Athena_VR")), // striker burst
        FItemAndCount(1, {}, Utils::FindObject<UFortWeaponRangedItemDefinition>(L"/DaisyWeaponGameplay/Items/Weapons/Assault/WID_Assault_Heavy_Recoil_Athena_SR.WID_Assault_Heavy_Recoil_Athena_SR")), // hammer
        FItemAndCount(1, {}, Utils::FindObject<UFortWeaponRangedItemDefinition>(L"/DaisyWeaponGameplay/Items/Weapons/Assault/WID_Assault_Heavy_Recoil_Athena_VR.WID_Assault_Heavy_Recoil_Athena_VR")), // hammer
    };

    Snipers =
    {
        FItemAndCount(1, {}, Utils::FindObject<UFortWeaponRangedItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Sniper_Heavy_Athena_VR_Ore_T03.WID_Sniper_Heavy_Athena_VR_Ore_T03")), // heavy sniper
        FItemAndCount(1, {}, Utils::FindObject<UFortWeaponRangedItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Sniper_Heavy_Athena_SR_Ore_T03.WID_Sniper_Heavy_Athena_SR_Ore_T03")), // heavy sniper
    };

    Heals =
    {
        FItemAndCount(3, {}, Utils::FindObject<UFortWeaponRangedItemDefinition>(L"/Game/Athena/Items/Consumables/Shields/Athena_Shields.Athena_Shields")), // big pots
        FItemAndCount(3, {}, Utils::FindObject<UFortWeaponRangedItemDefinition>(L"/Game/Athena/Items/Consumables/Medkit/Athena_Medkit.Athena_Medkit")), // medkit
        FItemAndCount(6, {}, Utils::FindObject<UFortWeaponRangedItemDefinition>(L"/Game/Athena/Items/Consumables/ShieldSmall/Athena_ShieldSmall.Athena_ShieldSmall")), // minis
        FItemAndCount(1, {}, Utils::FindObject<UFortWeaponRangedItemDefinition>(L"/Game/Athena/Items/Weapons/LTM/WID_Hook_Gun_Slide.WID_Hook_Gun_Slide")), // grappler
        FItemAndCount(6, {}, Utils::FindObject<UFortWeaponRangedItemDefinition>(L"/Game/Athena/Items/Consumables/ShockwaveGrenade/Athena_ShockGrenade.Athena_ShockGrenade")), // shockwave
        FItemAndCount(1, {}, Utils::FindObject<UFortWeaponRangedItemDefinition>(L"/FlipperGameplay/Items/HealSpray/WID_Athena_HealSpray.WID_Athena_HealSpray")) // med mist
    };

    Ammos =
    {
        Utils::FindObject<UFortAmmoItemDefinition>(L"/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsLight.AthenaAmmoDataBulletsLight"),
        Utils::FindObject<UFortAmmoItemDefinition>(L"/Game/Athena/Items/Ammo/AthenaAmmoDataShells.AthenaAmmoDataShells"),
        Utils::FindObject<UFortAmmoItemDefinition>(L"/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsMedium.AthenaAmmoDataBulletsMedium"),
        Utils::FindObject<UFortAmmoItemDefinition>(L"/Game/Athena/Items/Ammo/AmmoDataRockets.AmmoDataRockets"),
        Utils::FindObject<UFortAmmoItemDefinition>(L"/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsHeavy.AthenaAmmoDataBulletsHeavy")
    };

    Resources =
    {
        Utils::FindObject<UFortResourceItemDefinition>(L"/Game/Items/ResourcePickups/WoodItemData.WoodItemData"),
        Utils::FindObject<UFortResourceItemDefinition>(L"/Game/Items/ResourcePickups/StoneItemData.StoneItemData"),
        Utils::FindObject<UFortResourceItemDefinition>(L"/Game/Items/ResourcePickups/MetalItemData.MetalItemData")
    };
}

FItemAndCount Lategame::GetShotguns()
{
    return Shotguns[rand() % (Shotguns.size() - 1)];
}

FItemAndCount Lategame::GetAssaultRifles()
{
    return AssaultRifles[rand() % (AssaultRifles.size() - 1)];
}


FItemAndCount Lategame::GetSnipers()
{
    return Snipers[rand() % (Heals.size() - 1)];
}

FItemAndCount Lategame::GetHeals()
{
    return Heals[rand() % (Heals.size() - 1)];
}

UFortAmmoItemDefinition* Lategame::GetAmmo(EAmmoType AmmoType)
{
    return Ammos[(uint8)AmmoType];
}

UFortResourceItemDefinition* Lategame::GetResource(EFortResourceType ResourceType) 
{
    return Resources[(uint8)ResourceType];
}