#include "pch.h"

#include "Campsite.h"
#include "Inventory.h"

void Campsite::ClearStashedItem(UObject* Context, FFrame& Stack)
{
	UFortControllerComponent_CampsiteAccountItem* CampsiteAccountItemComponent = (UFortControllerComponent_CampsiteAccountItem*)Context;

	int32 StashedItemIndex;

	Stack.StepCompiledIn(&StashedItemIndex);
	Stack.IncrementCode();

	Log(L"UFortControllerComponent_CampsiteAccountItem::ClearStashedItem StashedItemIndex: %i", StashedItemIndex);

	if (CampsiteAccountItemComponent->CurrentlyStashedItemEntries.IsValidIndex(StashedItemIndex))
		CampsiteAccountItemComponent->CurrentlyStashedItemEntries.Remove(StashedItemIndex);
	else
		Log(L"UFortControllerComponent_CampsiteAccountItem::ClearStashedItem StashedItemIndex %i is invalid index!", StashedItemIndex);
}

void Campsite::ClearStashedItemAndGiveToPlayer(UObject* Context, FFrame& Stack, bool* Ret)
{
	UFortControllerComponent_CampsiteAccountItem* CampsiteAccountItemComponent = (UFortControllerComponent_CampsiteAccountItem*)Context;

	AActor* SourceActor; // it better not be the tent
	int32 StashedItemIndex;

	Stack.StepCompiledIn(&SourceActor);
	Stack.StepCompiledIn(&StashedItemIndex);
	Stack.IncrementCode();

	Log(L"UFortControllerComponent_CampsiteAccountItem::ClearStashedItemAndGiveToPlayer SourceActor: %s", SourceActor ? SourceActor->GetFullWName().c_str() : L"None");
	Log(L"UFortControllerComponent_CampsiteAccountItem::ClearStashedItemAndGiveToPlayer StashedItemIndex: %i", StashedItemIndex);

	AFortPlayerController* PlayerController = (AFortPlayerController*)CampsiteAccountItemComponent->GetOwner();

	if (!PlayerController)
		return;

	if (CampsiteAccountItemComponent->CurrentlyStashedItemEntries.IsValidIndex(StashedItemIndex))
	{
		Inventory::GiveItem(PlayerController, CampsiteAccountItemComponent->CurrentlyStashedItemEntries[StashedItemIndex]);

		CampsiteAccountItemComponent->CurrentlyStashedItemEntries[StashedItemIndex] = FFortItemEntry{};
	}
	else
	{
		Log(L"UFortControllerComponent_CampsiteAccountItem::ClearStashedItemAndGiveToPlayer StashedItemIndex %i is invalid index!", StashedItemIndex);

		*Ret = false;
		return;
	}

	*Ret = true;
}

void Campsite::StashCurrentlyHeldItemAndRemoveFromInventory(UObject* Context, FFrame& Stack, bool* Ret)
{
	UFortControllerComponent_CampsiteAccountItem* CampsiteAccountItemComponent = (UFortControllerComponent_CampsiteAccountItem*)Context;

	int32 StashedItemIndex;

	Stack.StepCompiledIn(&StashedItemIndex);
	Stack.IncrementCode();

	Log(L"UFortControllerComponent_CampsiteAccountItem::StashCurrentlyHeldItemAndRemoveFromInventory StashedItemIndex: %i", StashedItemIndex);

	AFortPlayerController* PlayerController = (AFortPlayerController*)CampsiteAccountItemComponent->GetOwner();

	if (!PlayerController)
	{
		*Ret = false;
		return;
	}

	AFortPlayerPawn* Pawn = (AFortPlayerPawn*)PlayerController->Pawn;

	if (!Pawn)
	{
		*Ret = false;
		return;
	}

	AFortWeapon* CurrentWeapon = Pawn->CurrentWeapon;

	if (!CurrentWeapon)
	{
		*Ret = false;
		return;
	}

	if (CampsiteAccountItemComponent->CurrentlyStashedItemEntries.IsValidIndex(StashedItemIndex))
	{
		CampsiteAccountItemComponent->CurrentlyStashedItemEntries[StashedItemIndex] = UCampsiteFunctionLibraryNative::GetItemEntryCopyFromWeapon(CurrentWeapon);
		
		Inventory::Remove(PlayerController, CurrentWeapon->ItemEntryGuid);
	}
	else
	{
		Log(L"UFortControllerComponent_CampsiteAccountItem::StashCurrentlyHeldItemAndRemoveFromInventory StashedItemIndex %i is invalid index!", StashedItemIndex);
		*Ret = false;
		return;
	}

	*Ret = true;
}

void Campsite::SwapStashedItem(UObject* Context, FFrame& Stack, bool* Ret)
{
	UFortControllerComponent_CampsiteAccountItem* CampsiteAccountItemComponent = (UFortControllerComponent_CampsiteAccountItem*)Context;

	AActor* SourceActor; // it better not be the tent
	int32 StashedItemIndex;

	Stack.StepCompiledIn(&SourceActor);
	Stack.StepCompiledIn(&StashedItemIndex);
	Stack.IncrementCode();

	Log(L"UFortControllerComponent_CampsiteAccountItem::SwapStashedItem SourceActor: %s", SourceActor ? SourceActor->GetFullWName().c_str() : L"None");
	Log(L"UFortControllerComponent_CampsiteAccountItem::SwapStashedItem StashedItemIndex: %i", StashedItemIndex);

	AFortPlayerController* PlayerController = (AFortPlayerController*)CampsiteAccountItemComponent->GetOwner();
	if (!PlayerController)
	{
		*Ret = false;
		return;
	}

	AFortPawn* Pawn = (AFortPawn*)PlayerController->Pawn;
	if (!Pawn)
	{
		*Ret = false;
		return;
	}

	AFortWeapon* CurrentWeapon = Pawn->CurrentWeapon; // obv wrong
	if (!CurrentWeapon)
	{
		*Ret = false;
		return;
	}

	if (CampsiteAccountItemComponent->CurrentlyStashedItemEntries.IsValidIndex(StashedItemIndex))
		CampsiteAccountItemComponent->CurrentlyStashedItemEntries[StashedItemIndex] = UCampsiteFunctionLibraryNative::GetItemEntryCopyFromWeapon(CurrentWeapon); // obv wrong
	else
	{
		Log(L"UFortControllerComponent_CampsiteAccountItem::SwapStashedItem StashedItemIndex %i is invalid index!", StashedItemIndex);

		*Ret = false;
		return;
	}

	*Ret = true;
}

void Campsite::RegisterPreplacedCampsite(UObject* Context, FFrame& Stack)
{
	UFortGameStateComponent_Campsite* CampsiteGameStateComponent = (UFortGameStateComponent_Campsite*)Context;

	AAbandonedCampsitePlacedSpawner* PreplacedCampsiteSpawnPoint;

	Stack.StepCompiledIn(&PreplacedCampsiteSpawnPoint);
	Stack.IncrementCode();

	if (!CampsiteGameStateComponent->PlacedCampsiteSpawnPoints.Contains(PreplacedCampsiteSpawnPoint))
		CampsiteGameStateComponent->PlacedCampsiteSpawnPoints.Add(PreplacedCampsiteSpawnPoint);

	return PreplacedCampsiteSpawnPoint->SpawnCampsite(); // this is probably stripped, will have to do some testing
}

void Campsite::Hook()
{
	Utils::ExecHook(L"/Script/CampsiteRuntime.FortControllerComponent_CampsiteAccountItem.ClearStashedItem", ClearStashedItem);
	Utils::ExecHook(L"/Script/CampsiteRuntime.FortControllerComponent_CampsiteAccountItem.ClearStashedItemAndGiveToPlayer", ClearStashedItemAndGiveToPlayer);
	Utils::ExecHook(L"/Script/CampsiteRuntime.FortControllerComponent_CampsiteAccountItem.StashCurrentlyHeldItemAndRemoveFromInventory", StashCurrentlyHeldItemAndRemoveFromInventory);
	Utils::ExecHook(L"/Script/CampsiteRuntime.FortControllerComponent_CampsiteAccountItem.SwapStashedItem", SwapStashedItem);

	Utils::ExecHook(L"/Script/CampsiteRuntime.FortGameStateComponent_Campsite.RegisterPreplacedCampsite", RegisterPreplacedCampsite);
}