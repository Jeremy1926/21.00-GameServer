#include "pch.h"
#include "Vehicles.h"

#include "Inventory.h"

void Vehicles::ServerMove(UObject* Context, FFrame& Stack)
{
    FReplicatedPhysicsPawnState State;
    Stack.StepCompiledIn(&State);
    Stack.IncrementCode();
    auto Pawn = (AFortPhysicsPawn*)Context;

    USkeletalMeshComponent* Mesh = (USkeletalMeshComponent*)Pawn->RootComponent;

    auto Rotation = State.Rotation.Rotator();
    Rotation.Pitch = 0.;
    Rotation.Roll = 0.;

    /*Pawn->ReplicatedMovement.AngularVelocity = State.AngularVelocity;
    Pawn->ReplicatedMovement.LinearVelocity = State.LinearVelocity;
    Pawn->ReplicatedMovement.Location = State.Translation;
    Pawn->ReplicatedMovement.Rotation = Rotation;
    Pawn->OnRep_ReplicatedMovement();*/

    Mesh->K2_SetWorldLocationAndRotation(State.Translation, Rotation, false, nullptr, true);
    Mesh->SetPhysicsLinearVelocity(State.LinearVelocity, 0, FName(0));
    Mesh->SetPhysicsAngularVelocityInDegrees(State.AngularVelocity, 0, FName(0));
}

void Vehicles::EquipVehicleWeapon(APawn* Vehicle, AFortPlayerPawn* PlayerPawn, int32 SeatIdx)
{
    Log(L"Called! %d", SeatIdx);

    UFortVehicleSeatWeaponComponent* SeatWeaponComponent = (UFortVehicleSeatWeaponComponent*)Vehicle->GetComponentByClass(UFortVehicleSeatWeaponComponent::StaticClass());

    if (!SeatWeaponComponent)
        return;

    UFortWeaponItemDefinition* Weapon = nullptr;

    for (auto& WeaponDefinition : SeatWeaponComponent->WeaponSeatDefinitions)
    {
        if (WeaponDefinition.SeatIndex != SeatIdx)
            continue;

        Weapon = WeaponDefinition.VehicleWeapon;
        break;
    }

    Log(L"KYS");
    if (Weapon)
    {
        auto PlayerController = (AFortPlayerController*)PlayerPawn->Controller;

        Inventory::GiveItem(PlayerController, Weapon, 1, Inventory::GetStats(Weapon)->ClipSize);

        auto ItemEntry = PlayerController->WorldInventory->Inventory.ReplicatedEntries.Search([&](FFortItemEntry& entry)
            { return entry.ItemDefinition == Weapon; }
        );
        auto FortWeapon = *PlayerPawn->CurrentWeaponList.Search([&](AFortWeapon* Weapon) {
            return Weapon->ItemEntryGuid == ItemEntry->ItemGuid;
           });

        PlayerController->ServerExecuteInventoryItem(ItemEntry->ItemGuid);
        PlayerController->ClientEquipItem(ItemEntry->ItemGuid, true);
    }
    Log(L"KYS");

    /*if (!PlayerPawn)
    {
        Log(L"No PlayerPawn!");
        return;
    }

    AFortPlayerController* PlayerController = PlayerPawn->Controller->Cast<AFortPlayerController>();

    if (!PlayerController)
    {
        Log(L"No PlayerController!");
        return;
    }

    UFortVehicleSeatWeaponComponent* SeatWeaponComponent = (UFortVehicleSeatWeaponComponent*)Vehicle->GetComponentByClass(UFortVehicleSeatWeaponComponent::StaticClass());

    if (!SeatWeaponComponent)
    {
        Log(L"No SeatWeaponComponent!");
        return;
    }
    else if (!SeatWeaponComponent->WeaponSeatDefinitions.IsValidIndex(SeatIdx))
    {
        Log(L"Invalid SeatIdx!");
        return;
    }

    FWeaponSeatDefinition& WeaponSeatDefinition = SeatWeaponComponent->WeaponSeatDefinitions[SeatIdx]; // ik this might look skunked, but they get filled in order of seat idx

    UFortWeaponItemDefinition* VehicleWeapon = WeaponSeatDefinition.VehicleWeapon ? WeaponSeatDefinition.VehicleWeapon : WeaponSeatDefinition.VehicleWeaponOverride;

    if (WeaponSeatDefinition.SeatIndex != SeatIdx || !VehicleWeapon) // still make sure tho
    {
        Log(L"No VehicleWeapon or invalid SeatIdx found!");
        return;
    }

    UFortWorldItem* VehicleWeaponInstance = Inventory::GiveItem(PlayerController, VehicleWeapon, 1, 99999, 0, false, true);

    if (!VehicleWeaponInstance)
    {
        Log(L"No VehicleWeaponInstance!");
        return;
    }

    AFortInventory* WorldInventory = PlayerController->WorldInventory;

    if (WorldInventory)
    {
        for (auto& ReplicatedEntry : WorldInventory->Inventory.ReplicatedEntries)
        {
            if (ReplicatedEntry.ItemGuid == VehicleWeaponInstance->GetItemGuid())
            {
                WorldInventory->Inventory.MarkItemDirty(ReplicatedEntry);
                WorldInventory->Inventory.MarkItemDirty(VehicleWeaponInstance->ItemEntry);
                WorldInventory->HandleInventoryLocalUpdate();
            }
        }
    }

    AFortWeaponRangedForVehicle* EquippedWeapon = PlayerPawn->EquipWeaponDefinition(VehicleWeapon, VehicleWeaponInstance->GetItemGuid(), VehicleWeaponInstance->GetTrackerGuid(), false)->Cast<AFortWeaponRangedForVehicle>();

    if (!EquippedWeapon)
    {
        Log(L"No EquippedWeapon!");
        return;
    }

    // PlayerController->ClientEquipItem(VehicleWeaponInstance->GetItemGuid(), true);

    FName& MuzzleSocketName = SeatWeaponComponent->MuzzleSocketNames[SeatIdx];
    TScriptInterface<IFortVehicleInterface> VehicleUInterface = PlayerPawn->GetVehicleUInterface();

    EquippedWeapon->MuzzleSocketName = MuzzleSocketName;
    EquippedWeapon->MuzzleFalloffSocketName = MuzzleSocketName;
    EquippedWeapon->WeaponHandSocketNameOverride = MuzzleSocketName;
    EquippedWeapon->LeftHandWeaponHandSocketNameOverride = MuzzleSocketName;
    EquippedWeapon->bIsEquippingWeapon = true;
    EquippedWeapon->MountedWeaponInfo.bNeedsVehicleAttachment = true;
    EquippedWeapon->MountedWeaponInfo.bTargetSourceFromVehicleMuzzle = true;
    EquippedWeapon->MountedWeaponInfoRepped.HostVehicleCached = VehicleUInterface;
    EquippedWeapon->MountedWeaponInfoRepped.HostVehicleSeatIndexCached = SeatIdx;
    EquippedWeapon->OnRep_MountedWeaponInfoRepped();
    EquippedWeapon->OnHostVehicleSetup();
    EquippedWeapon->ServerSetMuzzleTraceNearWall(false);*/
}

void Vehicles::ServerOnExitVehicle(UObject* Context, FFrame& Stack)
{
    ETryExitVehicleBehavior ExitForceBehavior;
    bool bDestroyVehicleWhenForced;
    Stack.StepCompiledIn(&ExitForceBehavior);
    Stack.StepCompiledIn(&bDestroyVehicleWhenForced);
    Stack.IncrementCode();

    auto Pawn = (AFortPlayerPawn*)Context;
    auto PlayerController = (AFortPlayerController*)Pawn->Controller;
    auto Vehicle = Pawn->BP_GetVehicle();
    auto SeatIdx = Vehicle->FindSeatIndex(PlayerController->MyFortPawn);

    UFortVehicleSeatWeaponComponent* SeatWeaponComponent = (UFortVehicleSeatWeaponComponent*)Vehicle->GetComponentByClass(UFortVehicleSeatWeaponComponent::StaticClass());

    if (SeatWeaponComponent)
    {
        UFortWeaponItemDefinition* Weapon = nullptr;
        for (auto& WeaponDefinition : SeatWeaponComponent->WeaponSeatDefinitions)
        {
            if (WeaponDefinition.SeatIndex != SeatIdx)
                continue;

            Weapon = WeaponDefinition.VehicleWeapon;
            break;
        }

        auto ItemEntry = PlayerController->WorldInventory->Inventory.ReplicatedEntries.Search([Weapon](FFortItemEntry& entry)
            { return entry.ItemDefinition == Weapon; });

        if (ItemEntry)
        {
            auto LastItem = Pawn->PreviousWeapon;
            PlayerController->ServerExecuteInventoryItem(LastItem->ItemEntryGuid);
            PlayerController->ClientEquipItem(LastItem->ItemEntryGuid, true);
            Inventory::Remove(PlayerController, ItemEntry->ItemGuid);
        }
    }

    callOG(Pawn, Stack.CurrentNativeFunction, ServerOnExitVehicle, ExitForceBehavior, bDestroyVehicleWhenForced);
}

void Vehicles::ServerUpdateTowhook(UObject *Context, FFrame& Stack)
{
    FVector_NetQuantizeNormal InNetTowhookAimDir;
    Stack.StepCompiledIn(&InNetTowhookAimDir);
    Stack.IncrementCode();

    auto Vehicle = (AFortOctopusVehicle*)Context;
    Vehicle->NetTowhookAimDir = InNetTowhookAimDir;
    Vehicle->OnRep_NetTowhookAimDir();
}

void Vehicles::OnPawnEnterVehicle(__int64 VehicleInterfaceWHY, AFortPlayerPawn* PlayerPawn, int32 SeatIdx) 
{
    auto Vehicle = (AFortAthenaVehicle*)(VehicleInterfaceWHY - 1176);
    UFortVehicleSeatWeaponComponent* SeatWeaponComponent = (UFortVehicleSeatWeaponComponent*)Vehicle->GetComponentByClass(UFortVehicleSeatWeaponComponent::StaticClass());

    Log(Vehicle->GetWName().c_str());
    if (SeatWeaponComponent)
    {
        UFortWeaponItemDefinition* Weapon = nullptr;

        for (auto& WeaponDefinition : SeatWeaponComponent->WeaponSeatDefinitions)
        {
            if (WeaponDefinition.SeatIndex != SeatIdx)
                continue;

            Weapon = WeaponDefinition.VehicleWeapon;
            break;
        }

        Log(L"KYS");
        if (Weapon)
        {
            auto PlayerController = (AFortPlayerController*)PlayerPawn->Controller;

            Log(Weapon->GetWName().c_str());
            Inventory::GiveItem(PlayerController, Weapon, 1, Inventory::GetStats(Weapon)->ClipSize);

            auto ItemEntry = PlayerController->WorldInventory->Inventory.ReplicatedEntries.Search([&](FFortItemEntry& entry)
                { return entry.ItemDefinition == Weapon; }
            );

            PlayerPawn->PreviousWeapon = PlayerPawn->CurrentWeapon;
            //PlayerController->ServerExecuteInventoryItem(ItemEntry->ItemGuid);
            //PlayerController->ClientEquipItem(ItemEntry->ItemGuid, true);
        }
        Log(L"KYS");
    }

    OnPawnEnterVehicleOG(VehicleInterfaceWHY, PlayerPawn, SeatIdx);
}

void Vehicles::Hook()
{
    Utils::ExecHook(L"/Script/FortniteGame.FortPhysicsPawn.ServerMove", ServerMove);
    Utils::ExecHook(L"/Script/FortniteGame.FortPhysicsPawn.ServerMoveReliable", ServerMove);

    Utils::ExecHook(L"/Script/FortniteGame.FortPlayerPawn.ServerOnExitVehicle", ServerOnExitVehicle, ServerOnExitVehicleOG);
    Utils::ExecHook(L"/Script/FortniteGame.FortOctopusVehicle.ServerUpdateTowhook", ServerUpdateTowhook);
    Utils::Hook(Sarah::Offsets::ImageBase + 0x2084394, OnPawnEnterVehicle, OnPawnEnterVehicleOG);
    // Utils::ExecHook("/Script/FortniteGame.FortAthenaVehicle.OnPawnEnterVehicle", OnPawnEnterVehicle, OnPawnEnterVehicleOriginal);
    //Utils::ExecHook("/Script/FortniteGame.FortCharacterVehicle.OnPawnEnterVehicle", OnPawnEnterVehicle, OnPawnEnterVehicleOriginal);
    //Utils::ExecHookEvery<AFortCharacterVehicle>("OnPawnEnterVehicle", OnPawnEnterVehicle, OnPawnEnterVehicleOG);
}
