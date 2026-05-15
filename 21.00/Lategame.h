#pragma once
#include "pch.h"

enum class EAmmoType : uint8
{
    Assault = 0,
    Shotgun = 1,
    Submachine = 2,
    Rocket = 3,
    Sniper = 4
};

class Lategame 
{
public:
    static inline UEAllocatedVector<FItemAndCount> Shotguns;
    static inline UEAllocatedVector<FItemAndCount> AssaultRifles;
    static inline UEAllocatedVector<FItemAndCount> Snipers;
    static inline UEAllocatedVector<FItemAndCount> Heals;
    static inline UEAllocatedVector<UFortAmmoItemDefinition*> Ammos;
    static inline UEAllocatedVector<UFortResourceItemDefinition*> Resources;

    static void Init();

    static FItemAndCount GetShotguns();
    static FItemAndCount GetAssaultRifles();
    static FItemAndCount GetSnipers();
    static FItemAndCount GetHeals();

    static UFortAmmoItemDefinition* GetAmmo(EAmmoType);
    static UFortResourceItemDefinition* GetResource(EFortResourceType);
};