#include "pch.h"
#include "Player.h"
#include "Abilities.h"
#include "Inventory.h"
#include "Vehicles.h"
#include "Options.h"
#include "Lategame.h"


void Player::ServerReadyToStartMatch(AFortPlayerControllerAthena* Controller)
{
	auto GameState = (AFortGameStateAthena*)UWorld::GetWorld()->GameState;
	auto PlayerState = (AFortPlayerStateAthena*)Controller->PlayerState;

	std::cout << "Hi I am NotTacs. I would like to touch you." << std::endl;

	if (GameState->CurrentPlaylistInfo.BasePlaylist->GetName().contains("Playlist_PlaygroundV2"))
	{
		AFortCreativePortalManager* PortalManager = GameState->CreativePortalManager;
		AFortAthenaCreativePortal* Portal = nullptr;
		static TArray<AFortAthenaCreativePortal*> AvailablePortals, UsedPortals;
		if (AvailablePortals.Num() == 0 && UsedPortals.Num() == 0)
			for (auto& Portal : PortalManager->AllPortals)
			{
				if (Portal->OwningPlayer.ReplicationBytes.Num() > 0) UsedPortals.Add(Portal);
				else AvailablePortals.Add(Portal);
			}

		if (AvailablePortals.Num() > 0) 
		{
			Portal = (AFortAthenaCreativePortal*)AvailablePortals[0];
			AvailablePortals.Remove(0);
			UsedPortals.Add(Portal);
		}

		AFortPlayerStateAthena* PlayerStateAthena = (AFortPlayerStateAthena*)Controller->PlayerState;
		FUniqueNetIdRepl PlayerStateUniqueId = PlayerStateAthena->UniqueId;
		Portal->OwningPlayer = PlayerStateUniqueId;
		Portal->OnRep_OwningPlayer();
		Portal->bPortalOpen = true;
		Portal->OnRep_PortalOpen();
		Portal->PlayersReady.Add(PlayerStateUniqueId);
		Portal->OnRep_PlayersReady();

		Portal->IslandInfo.CreatorName = PlayerStateAthena->GetPlayerName();
		Portal->IslandInfo.Mnemonic = L"HelloSigma";
		Portal->IslandInfo.SupportCode = L"cumwareisthegoat";
		Portal->IslandInfo.Version = 1;
		Portal->OnRep_IslandInfo();

		Portal->bIsPublishedPortal = false;
		Portal->OnRep_PublishedPortal();

		Portal->bUserInitiatedLoad = true;
		Portal->bInErrorState = false;
		Controller->OwnedPortal = Portal;

		Portal->LinkedVolume->bNeverAllowSaving = false;
		Portal->LinkedVolume->VolumeState = ESpatialLoadingState::Ready;
		Portal->LinkedVolume->OnRep_VolumeState();


		UFortPlaysetItemDefinition* IslandPlayset = Utils::FindObject<UFortPlaysetItemDefinition>(L"/Game/Playsets/PID_Playset_60x60_Composed_POI_Tilted.PID_Playset_60x60_Composed_POI_Tilted");
		UClass* FortLevelSaveComponent = Utils::FindObject<UClass>(L"/Script/FortniteGame.FortLevelSaveComponent");
		UFortLevelSaveComponent* LevelSaveComponent = (UFortLevelSaveComponent*)Portal->LinkedVolume->GetComponentByClass(FortLevelSaveComponent);

		if (!LevelSaveComponent->RestrictedPlotDefinition)
		{
			Log(L"Failed to get RestrictedPlotDefinition");
			TArray<UFortCreativeRealEstatePlotItemDefinition*> CreativeRealEstatePlots = Utils::GetAll<UFortCreativeRealEstatePlotItemDefinition>();
			std::cout << "FLippingPacketYo: " << CreativeRealEstatePlots.Num() << std::endl;
			for (auto& CreativeRealEstate : CreativeRealEstatePlots)
			{
				Log(L"CreativeRealEstatePlot: ", CreativeRealEstate->GetFullName());
			}
		}



		Portal->LinkedVolume->CurrentPlayset = IslandPlayset;
		Portal->LinkedVolume->OnRep_CurrentPlayset();


		Controller->bBuildFree = true;
		FVector SpawnLocation = Portal->LinkedVolume->K2_GetActorLocation();

		auto NewVolume = Portal->LinkedVolume;
		auto LevelStreamComponent = (UPlaysetLevelStreamComponent*)NewVolume->GetComponentByClass(UPlaysetLevelStreamComponent::StaticClass());
		if (!LevelStreamComponent || !NewVolume) return;

		Controller->CreativePlotLinkedVolume = Portal->LinkedVolume;
		Controller->OnRep_CreativePlotLinkedVolume();
		Controller->OwnedPortal = Portal;

		std::cout << "Playset: " << LevelStreamComponent->CurrentPlayset << std::endl;

		std::cout << "Owner: " << LevelStreamComponent->GetOwner()->GetFullName() << std::endl;

		((int64(*)(UPlaysetLevelStreamComponent*))(int64(Sarah::Offsets::ImageBase + 0x725fe7c)))(LevelStreamComponent);
	}

	Player::ServerReadyToStartMatchOG(Controller);
}

void Player::ServerAcknowledgePossession(UObject* Context, FFrame& Stack)
{
	APawn* Pawn;
	Stack.StepCompiledIn(&Pawn);
	Stack.IncrementCode();

	//auto PlayerController = (AFortPlayerController*)Context;
	//PlayerController->AcknowledgedPawn = Pawn;

	auto InternalServerAcknowledgePossession = (void (*)(UObject*, APawn*)) (Sarah::Offsets::ImageBase + 0x882ace0);
	InternalServerAcknowledgePossession(Context, Pawn);
}

void Player::GetPlayerViewPointInternal(APlayerController* PlayerController, FVector& Loc, FRotator& Rot)
{
	static auto SFName = FName(L"Spectating");

	if (PlayerController->StateName == SFName)
	{
		Loc = PlayerController->LastSpectatorSyncLocation;
		Rot = PlayerController->LastSpectatorSyncRotation;
	}
	/*else if (PlayerController->PlayerCameraManager && PlayerController->PlayerCameraManager->CameraCachePrivate.Timestamp > 0.f)
	{
		Loc = PlayerController->PlayerCameraManager->CameraCachePrivate.POV.Location;
		Rot = PlayerController->PlayerCameraManager->CameraCachePrivate.POV.Rotation;
	}*/
	else if (PlayerController->GetViewTarget())
	{
		Loc = PlayerController->GetViewTarget()->K2_GetActorLocation();
		Rot = PlayerController->GetViewTarget()->K2_GetActorRotation();
	}
	else return PlayerController->GetActorEyesViewPoint(&Loc, &Rot);
}

void Player::GetPlayerViewPoint(UObject *Context, FFrame& Stack)
{
	auto& Loc = Stack.StepCompiledInRef<FVector>();
	auto& Rot = Stack.StepCompiledInRef<FRotator>();
	Stack.IncrementCode();
	auto PlayerController = (AFortPlayerController*)Context;

	GetPlayerViewPointInternal(PlayerController, Loc, Rot);
}

void Player::ServerExecuteInventoryItem(UObject* Context, FFrame& Stack)
{
	FGuid ItemGuid;
	Stack.StepCompiledIn(&ItemGuid);
	Stack.IncrementCode();

	auto PlayerController = (AFortPlayerController*)Context;
	if (!PlayerController) 
		return;

	auto entry = PlayerController->WorldInventory->Inventory.ReplicatedEntries.Search([&](FFortItemEntry& entry) {
		return entry.ItemGuid == ItemGuid;
	});
	if (!entry || !PlayerController->MyFortPawn) 
		return;

	UFortWeaponItemDefinition* ItemDefinition = entry->ItemDefinition->IsA<UFortGadgetItemDefinition>() ? ((UFortGadgetItemDefinition*)entry->ItemDefinition)->GetWeaponItemDefinition() : (UFortWeaponItemDefinition*)entry->ItemDefinition;
	
	if (auto Deco = (UFortContextTrapItemDefinition*)ItemDefinition->Cast<UFortDecoItemDefinition>()) {
		PlayerController->MyFortPawn->PickUpActor(nullptr, Deco);
		PlayerController->MyFortPawn->CurrentWeapon->ItemEntryGuid = ItemGuid;

		if (auto ContextTrap = PlayerController->MyFortPawn->CurrentWeapon->Cast<AFortDecoTool_ContextTrap>()) ContextTrap->ContextTrapItemDefinition = Deco;
		return;
	}
	PlayerController->MyFortPawn->EquipWeaponDefinition(ItemDefinition, ItemGuid, entry->TrackerGuid, false);
}

void Player::ServerReturnToMainMenu(UObject* Context, FFrame& Stack)
{
	Stack.IncrementCode();
	return ((AFortPlayerController*)Context)->ClientReturnToMainMenu(L"");
}

void Player::ServerAttemptAircraftJump(UObject* Context, FFrame& Stack)
{
	FRotator Rotation;
	Stack.StepCompiledIn(&Rotation);
	Stack.IncrementCode();

	auto Component = (UFortControllerComponent_Aircraft*)Context;
	auto PlayerController = (AFortPlayerController*)Component->GetOwner();
	auto PlayerState = (AFortPlayerState*)PlayerController->PlayerState;
	auto GameMode = (AFortGameModeAthena*)UWorld::GetWorld()->AuthorityGameMode;
	auto GameState = (AFortGameStateAthena*)UWorld::GetWorld()->GameState;

	Component->bIsAttemptingAircraftJump = true;
	if (true)
	{
		PlayerController->StateName = L"Spectating";
		PlayerController->AirSpawnControlRotation = Rotation;
		PlayerController->ServerRestartPlayer();
		PlayerController->SetControlRotation(Rotation);
		Component->bIsAttemptingAircraftJump = false;
	}
	//GameMode->RestartPlayer(PlayerController);
	if (bLateGame && PlayerController->MyFortPawn)
		PlayerController->MyFortPawn->K2_SetActorLocation(GameState->Aircrafts[0]->K2_GetActorLocation(), false, nullptr, false);

	if (PlayerController->MyFortPawn)
	{
		//PlayerController->MyFortPawn->BeginSkydiving(true);
		PlayerController->MyFortPawn->SetHealth(100);

		if (bLateGame) 
		{
			PlayerController->MyFortPawn->SetShield(100);
		}
	}

	//Inventory::GiveItem(PlayerController, Utils::FindObject<UFortItemDefinition>(L"/CampsiteGameplay/Items/Campsite/WID_Athena_DeployableCampsite_Thrown.WID_Athena_DeployableCampsite_Thrown"), 5);
}

void Player::ServerPlayEmoteItem(UObject* Context, FFrame& Stack)
{
    UFortMontageItemDefinitionBase* Asset;
	float RandomNumber;
    Stack.StepCompiledIn(&Asset);
	Stack.StepCompiledIn(&RandomNumber);
    Stack.IncrementCode();

    auto PlayerController = (AFortPlayerController*)Context;
    if (!PlayerController || !PlayerController->MyFortPawn || !Asset) return;

    auto* AbilitySystemComponent = ((AFortPlayerStateAthena*)PlayerController->PlayerState)->AbilitySystemComponent;
    FGameplayAbilitySpec NewSpec = {};
    UObject* AbilityToUse = nullptr;
    
    if (Asset->IsA<UAthenaSprayItemDefinition>()) 
	{
		static auto SprayAbilityClass = Utils::FindObject<UBlueprintGeneratedClass>(L"/Game/Abilities/Sprays/GAB_Spray_Generic.GAB_Spray_Generic_C");
        AbilityToUse = SprayAbilityClass->DefaultObject;
    }
	else if (auto ToyAsset = Asset->Cast<UAthenaToyItemDefinition>()) {
		auto AssetPathName = ToyAsset->ToySpawnAbility.ObjectID.AssetPathName.GetRawWString();
		auto Bro = FMemory::MallocForType<wchar_t>(AssetPathName.size());
		__movsb((PBYTE)Bro, (const PBYTE) AssetPathName.c_str(), (AssetPathName.size() + 1) * sizeof(wchar_t));
		AbilityToUse = ((UClass *) Utils::InternalLoadObject(Bro, UClass::StaticClass()))->DefaultObject;
	}
    else if (auto DanceAsset = Asset->Cast<UAthenaDanceItemDefinition>())
    {
        PlayerController->MyFortPawn->bMovingEmote = DanceAsset->bMovingEmote;
		PlayerController->MyFortPawn->bMovingEmoteForwardOnly = DanceAsset->bMoveForwardOnly;
		PlayerController->MyFortPawn->bMovingEmoteFollowingOnly = DanceAsset->bMoveFollowingOnly;
        PlayerController->MyFortPawn->EmoteWalkSpeed = DanceAsset->WalkForwardSpeed;
		static auto EmoteAbilityClass = Utils::FindObject<UBlueprintGeneratedClass>(L"/Game/Abilities/Emotes/GAB_Emote_Generic.GAB_Emote_Generic_C");
        AbilityToUse = DanceAsset->CustomDanceAbility ? DanceAsset->CustomDanceAbility->DefaultObject : EmoteAbilityClass->DefaultObject;
    }
    
    if (AbilityToUse) 
	{
        ((void (*)(FGameplayAbilitySpec*, UObject*, int, int, UObject*))(Sarah::Offsets::ConstructAbilitySpec))(&NewSpec, AbilityToUse, 1, -1, Asset);
        FGameplayAbilitySpecHandle handle;
        ((void (*)(UFortAbilitySystemComponent*, FGameplayAbilitySpecHandle*, FGameplayAbilitySpec*, void*))(Sarah::Offsets::ImageBase +  0x586b458))(AbilitySystemComponent, &handle, &NewSpec, nullptr);
    }
}

void Player::ServerSendZiplineState(UObject* Context, FFrame& Stack)
{
	FZiplinePawnState State;

	Stack.StepCompiledIn(&State);
	Stack.IncrementCode();

	auto Pawn = (AFortPlayerPawn*)Context;

	if (!Pawn)
		return;

	auto Zipline = Pawn->GetActiveZipline(); // why not State->Zipline

	Pawn->ZiplineState = State;

	((void (*)(AFortPlayerPawn*))(Sarah::Offsets::ImageBase + 0x71d931c))(Pawn);

	if (State.bJumped)
	{
		auto Velocity = Pawn->CharacterMovement->Velocity;
		auto VelocityX = Velocity.X * -0.5;
		auto VelocityY = Velocity.Y * -0.5;
		Pawn->LaunchCharacterJump({ VelocityX >= -750 ? min(VelocityX, 750) : -750, VelocityY >= -750 ? min(VelocityY, 750) : -750, 1200 }, false, false, true, true);
	}

	static auto ZipLineClass = Utils::FindObject<UClass>(L"/Ascender/Gameplay/Ascender/B_Athena_Zipline_Ascender.B_Athena_Zipline_Ascender_C");
	if (auto Ascender = Zipline->Cast<AFortAscenderZipline>(ZipLineClass))
	{
		Ascender->PawnUsingHandle = nullptr;
		Ascender->PreviousPawnUsingHandle = Pawn;
		Ascender->OnRep_PawnUsingHandle();
	}
}

void Player::ServerHandlePickupInfo(UObject* Context, FFrame& Stack)
{
	AFortPickup* Pickup;
	FFortPickupRequestInfo Params;
	Stack.StepCompiledIn(&Pickup);
	Stack.StepCompiledIn(&Params);
	Stack.IncrementCode();
	auto Pawn = (AFortPlayerPawn*)Context;

	if (!Pawn || !Pickup || Pickup->bPickedUp)
		return;

	if ((Params.bTrySwapWithWeapon || Params.bUseRequestedSwap) && Pawn->CurrentWeapon && Inventory::GetQuickbar(Pawn->CurrentWeapon->WeaponData) == EFortQuickBars::Primary && Inventory::GetQuickbar(Pickup->PrimaryPickupItemEntry.ItemDefinition) == EFortQuickBars::Primary)
	{
		auto PC = (AFortPlayerControllerAthena*)Pawn->Controller;
		auto SwapEntry = PC->WorldInventory->Inventory.ReplicatedEntries.Search([&](FFortItemEntry& entry)
			{ return entry.ItemGuid == Params.SwapWithItem; });
		PC->SwappingItemDefinition = (UFortWorldItemDefinition*)SwapEntry; // proper af
	}

	Pickup->SetNetDormancy(ENetDormancy::DORM_Awake);

	Pawn->IncomingPickups.Add(Pickup);

	Pickup->PickupLocationData.bPlayPickupSound = Params.bPlayPickupSound;
	Pickup->PickupLocationData.FlyTime = 0.4f;
	Pickup->PickupLocationData.ItemOwner = Pawn;
	Pickup->PickupLocationData.PickupGuid = Pickup->PrimaryPickupItemEntry.ItemGuid;
	Pickup->PickupLocationData.PickupTarget = Pawn;
	//Pickup->PickupLocationData.StartDirection = Params.Direction.QuantizeNormal();
	Pickup->OnRep_PickupLocationData();

	Pickup->bPickedUp = true;
	Pickup->OnRep_bPickedUp();
}

void Player::MovingEmoteStopped(UObject* Context, FFrame& Stack)
{
	Stack.IncrementCode();

	AFortPawn* Pawn = (AFortPawn*)Context;
	Pawn->bMovingEmote = false;
	Pawn->bMovingEmoteForwardOnly = false;
	Pawn->bMovingEmoteFollowingOnly = false;
}

void Player::InternalPickup(AFortPlayerControllerAthena* PlayerController, FFortItemEntry PickupEntry)
{
	auto MaxStack = (int32) Utils::EvaluateScalableFloat(PickupEntry.ItemDefinition->MaxStackSize);
	int ItemCount = 0;

	for (auto& Item : PlayerController->WorldInventory->Inventory.ReplicatedEntries)
	{
		if (Inventory::GetQuickbar(Item.ItemDefinition) == EFortQuickBars::Primary)
			ItemCount += ((UFortWorldItemDefinition*)Item.ItemDefinition)->NumberOfSlotsToTake;
	}

	auto GiveOrSwap = [&]() {
		if (ItemCount == 5 && Inventory::GetQuickbar(PickupEntry.ItemDefinition) == EFortQuickBars::Primary) 
		{
			if (Inventory::GetQuickbar(PlayerController->MyFortPawn->CurrentWeapon->WeaponData) == EFortQuickBars::Primary) 
			{
				auto itemEntry = PlayerController->WorldInventory->Inventory.ReplicatedEntries.Search([PlayerController](FFortItemEntry& entry) { 
					return entry.ItemGuid == PlayerController->MyFortPawn->CurrentWeapon->ItemEntryGuid; 
				});

				if (!itemEntry)
					return;
				Inventory::SpawnPickup(PlayerController->GetViewTarget()->K2_GetActorLocation(), *itemEntry, EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::Unset, PlayerController->MyFortPawn);
				Inventory::Remove(PlayerController, PlayerController->MyFortPawn->CurrentWeapon->ItemEntryGuid);
				Inventory::GiveItem(PlayerController, PickupEntry, PickupEntry.Count, true);
			}
			else {
				Inventory::SpawnPickup(PlayerController->GetViewTarget()->K2_GetActorLocation(), (FFortItemEntry&)PickupEntry, EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::Unset, PlayerController->MyFortPawn);
			}
		}
		else
			Inventory::GiveItem(PlayerController, PickupEntry, PickupEntry.Count, true);
		};

	auto GiveOrSwapStack = [&](int32 OriginalCount) {
		if (PickupEntry.ItemDefinition->bAllowMultipleStacks && ItemCount < 5)
			Inventory::GiveItem(PlayerController, PickupEntry, OriginalCount - MaxStack, true);
		else
			Inventory::SpawnPickup(PlayerController->GetViewTarget()->K2_GetActorLocation(), (FFortItemEntry&)PickupEntry, EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::Unset, PlayerController->MyFortPawn, OriginalCount - MaxStack);
		};

	if (PickupEntry.ItemDefinition->IsStackable()) {
		auto itemEntry = PlayerController->WorldInventory->Inventory.ReplicatedEntries.Search([PickupEntry, MaxStack](FFortItemEntry& entry)
			{ return entry.ItemDefinition == PickupEntry.ItemDefinition && entry.Count < MaxStack; });

		if (itemEntry) {
			auto State = itemEntry->StateValues.Search([](FFortItemEntryStateValue& Value)
				{ return Value.StateType == EFortItemEntryState::ShouldShowItemToast; });

			if (!State) {
				FFortItemEntryStateValue Value{};
				Value.StateType = EFortItemEntryState::ShouldShowItemToast;
				Value.IntValue = true;
				itemEntry->StateValues.Add(Value);
			}
			else State->IntValue = true;

			if ((itemEntry->Count += PickupEntry.Count) > MaxStack) {
				auto OriginalCount = itemEntry->Count;
				itemEntry->Count = MaxStack;

				GiveOrSwapStack(OriginalCount);
			}

			Inventory::ReplaceEntry(PlayerController, *itemEntry);
		}
		else {
			if (PickupEntry.Count > MaxStack) {
				auto OriginalCount = PickupEntry.Count;
				PickupEntry.Count = MaxStack;

				GiveOrSwapStack(OriginalCount);
			}

			GiveOrSwap();
		}
	}
	else {
		GiveOrSwap();
	}
}

bool Player::CompletePickupAnimation(AFortPickup* Pickup) {
	auto Pawn = (AFortPlayerPawnAthena*)Pickup->PickupLocationData.PickupTarget;
	if (!Pawn)
		return CompletePickupAnimationOG(Pickup);

	auto PlayerController = (AFortPlayerControllerAthena*)Pawn->Controller;
	if (!PlayerController)
		return CompletePickupAnimationOG(Pickup);

	if (auto entry = (FFortItemEntry*)PlayerController->SwappingItemDefinition)
	{
		Inventory::SpawnPickup(PlayerController->GetViewTarget()->K2_GetActorLocation(), *entry, EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::Unset, PlayerController->MyFortPawn);
		// SwapEntry(PC, *entry, Pickup->PrimaryPickupItemEntry);
		Inventory::Remove(PlayerController, entry->ItemGuid);
		Inventory::GiveItem(PlayerController, Pickup->PrimaryPickupItemEntry);
		PlayerController->SwappingItemDefinition = nullptr;
	}
	else
	{
		InternalPickup(PlayerController, Pickup->PrimaryPickupItemEntry);
	}
	return CompletePickupAnimationOG(Pickup);
}

void Player::NetMulticast_Athena_BatchedDamageCues(AFortPlayerPawnAthena* Pawn, FAthenaBatchedDamageGameplayCues_Shared SharedData, FAthenaBatchedDamageGameplayCues_NonShared NonSharedData)
{
	if (!Pawn || !Pawn->Controller || !Pawn->CurrentWeapon) return;

	if (Pawn->CurrentWeapon && !Pawn->CurrentWeapon->WeaponData->bUsesPhantomReserveAmmo && Inventory::GetStats(Pawn->CurrentWeapon->WeaponData) && Inventory::GetStats(Pawn->CurrentWeapon->WeaponData)->ClipSize > 0)
	{
		auto ent = ((AFortPlayerControllerAthena*)Pawn->Controller)->WorldInventory->Inventory.ReplicatedEntries.Search([Pawn](FFortItemEntry& entry) { 
			return entry.ItemGuid == Pawn->CurrentWeapon->ItemEntryGuid; 
		});

		if (ent)
		{
			ent->LoadedAmmo = Pawn->CurrentWeapon->AmmoCount;
			Inventory::ReplaceEntry((AFortPlayerControllerAthena*)Pawn->Controller, *ent);
		}
	}
	else if (Pawn->CurrentWeapon && Pawn->CurrentWeapon->WeaponData->bUsesPhantomReserveAmmo)
	{
		auto ent = ((AFortPlayerControllerAthena*)Pawn->Controller)->WorldInventory->Inventory.ReplicatedEntries.Search([Pawn](FFortItemEntry& entry) { 
			return entry.ItemGuid == Pawn->CurrentWeapon->ItemEntryGuid; 
		});

		if (ent)
		{
			ent->LoadedAmmo = Pawn->CurrentWeapon->AmmoCount;
			Inventory::ReplaceEntry((AFortPlayerControllerAthena*)Pawn->Controller, *ent);
		}
	}

	return NetMulticast_Athena_BatchedDamageCuesOG(Pawn, SharedData, NonSharedData);
}

void Player::ReloadWeapon(AFortWeapon* Weapon, int AmmoToRemove)
{
	AFortPlayerControllerAthena* PC = (AFortPlayerControllerAthena*)((AFortPlayerPawnAthena*)Weapon->Owner)->Controller;
	AFortInventory* Inventory;
	if (auto Bot = PC->Cast<AFortAthenaAIBotController>())
	{
		Inventory = Bot->Inventory;
	}
	else
	{
		Inventory = PC->WorldInventory;
	}

	if (!PC || !Inventory || !Weapon)
		return;

	if (Weapon->WeaponData->bUsesPhantomReserveAmmo)
	{
		Weapon->PhantomReserveAmmo -= AmmoToRemove;
		Weapon->OnRep_PhantomReserveAmmo();
		return;
	}

	auto Ammo = Weapon->WeaponData->GetAmmoWorldItemDefinition_BP();
	auto ent = Inventory->Inventory.ReplicatedEntries.Search([&](FFortItemEntry& entry)
		{ return Weapon->WeaponData == Ammo ? entry.ItemGuid == Weapon->ItemEntryGuid : entry.ItemDefinition == Ammo; });
	auto WeaponEnt = Inventory->Inventory.ReplicatedEntries.Search([&](FFortItemEntry& entry)
		{ return entry.ItemGuid == Weapon->ItemEntryGuid; });
	if (!WeaponEnt)
		return;

	if (ent)
	{
		ent->Count -= AmmoToRemove;
		if (ent->Count <= 0)
			Inventory::Remove(PC, ent->ItemGuid);
		else
			Inventory::ReplaceEntry(PC, *ent);
	}
	WeaponEnt->LoadedAmmo += AmmoToRemove;
	Inventory::ReplaceEntry(PC, *WeaponEnt);
}


void Player::ClientOnPawnDied(AFortPlayerControllerAthena* PlayerController, FFortPlayerDeathReport& DeathReport)
{
	if (!PlayerController)
		return ClientOnPawnDiedOG(PlayerController, DeathReport);
	auto GameMode = (AFortGameModeAthena*)UWorld::GetWorld()->AuthorityGameMode;
	auto GameState = (AFortGameStateAthena*)GameMode->GameState;
	auto PlayerState = (AFortPlayerStateAthena*)PlayerController->PlayerState;


	if (!GameState->IsRespawningAllowed(PlayerState) && PlayerController->WorldInventory && PlayerController->MyFortPawn)
	{
		bool bHasMats = false;
		for (auto& entry : PlayerController->WorldInventory->Inventory.ReplicatedEntries)
		{
			if (!entry.ItemDefinition->IsA<UFortWeaponMeleeItemDefinition>() && (entry.ItemDefinition->IsA<UFortResourceItemDefinition>() || entry.ItemDefinition->IsA<UFortWeaponRangedItemDefinition>() || entry.ItemDefinition->IsA<UFortConsumableItemDefinition>() || entry.ItemDefinition->IsA<UFortAmmoItemDefinition>()))
			{
				Inventory::SpawnPickup(PlayerController->MyFortPawn->K2_GetActorLocation(), entry, EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::PlayerElimination, PlayerController->MyFortPawn);
			}
		}

		AFortAthenaMutator_ItemDropOnDeath* Mutator = (AFortAthenaMutator_ItemDropOnDeath*)GameState->GetMutatorByClass(GameMode, AFortAthenaMutator_ItemDropOnDeath::StaticClass());

		if (Mutator)
		{
			for (FItemsToDropOnDeath& Items : Mutator->ItemsToDrop)
			{
				Inventory::SpawnPickup(PlayerState->DeathInfo.DeathLocation, Items.ItemToDrop, (int) Utils::EvaluateScalableFloat(Items.NumberToDrop), 0, EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::PlayerElimination, PlayerController->MyFortPawn);
			}
		}
	}

	auto KillerPlayerState = (AFortPlayerStateAthena*)DeathReport.KillerPlayerState;
	auto KillerPawn = (AFortPlayerPawnAthena*)DeathReport.KillerPawn;

	PlayerState->PawnDeathLocation = PlayerController->MyFortPawn ? PlayerController->MyFortPawn->K2_GetActorLocation() : FVector();
	PlayerState->DeathInfo.bDBNO = PlayerController->MyFortPawn ? PlayerController->MyFortPawn->IsDBNO() : false;
	PlayerState->DeathInfo.DeathLocation = PlayerState->PawnDeathLocation;
	PlayerState->DeathInfo.DeathTags = PlayerController->MyFortPawn ? *(FGameplayTagContainer*)(__int64(PlayerController->MyFortPawn) + 0x20a8) : DeathReport.Tags;
	PlayerState->DeathInfo.DeathCause = AFortPlayerStateAthena::ToDeathCause(PlayerState->DeathInfo.DeathTags, PlayerState->DeathInfo.bDBNO);
	if (PlayerState->DeathInfo.bDBNO)
		PlayerState->DeathInfo.Downer = KillerPlayerState;
	PlayerState->DeathInfo.FinisherOrDowner = KillerPlayerState;
	PlayerState->DeathInfo.Distance = PlayerController->MyFortPawn ? (PlayerState->DeathInfo.DeathCause != EDeathCause::FallDamage ? (KillerPawn ? KillerPawn->GetDistanceTo(PlayerController->MyFortPawn) : 0) : PlayerController->MyFortPawn->Cast<AFortPlayerPawnAthena>()->LastFallDistance) : 0;
	PlayerState->DeathInfo.bInitialized = true;
	PlayerState->OnRep_DeathInfo();

	if (KillerPlayerState && KillerPawn && KillerPawn->Controller && KillerPawn->Controller->IsA<AFortPlayerControllerAthena>() && KillerPawn->Controller != PlayerController)
	{
		KillerPlayerState->KillScore++;
		KillerPlayerState->OnRep_Kills();
		KillerPlayerState->TeamKillScore++;
		KillerPlayerState->OnRep_TeamKillScore();

		KillerPlayerState->ClientReportKill(PlayerState);
		KillerPlayerState->ClientReportTeamKill(KillerPlayerState->TeamKillScore);

		auto KillerPC = (AFortPlayerControllerAthena*)KillerPlayerState->Owner;

		Log(L"Player %s killed %s", KillerPlayerState->GetPlayerName().ToWString().c_str(), PlayerController->PlayerState->GetPlayerName().ToWString().c_str());
	}

	if (!GameState->IsRespawningAllowed(PlayerState) && (PlayerController->MyFortPawn ? !PlayerController->MyFortPawn->IsDBNO() : true))
	{
		PlayerState->Place = GameState->PlayersLeft;
		PlayerState->OnRep_Place();
		FAthenaMatchStats& Stats = PlayerController->MatchReport->MatchStats;
		FAthenaMatchTeamStats& TeamStats = PlayerController->MatchReport->TeamStats;

		Stats.Stats[3] = PlayerState->KillScore;
		Stats.Stats[8] = PlayerState->SquadId;
		PlayerController->ClientSendMatchStatsForPlayer(Stats);

		TeamStats.Place = PlayerState->Place;
		TeamStats.TotalPlayers = GameState->TotalPlayers;
		PlayerController->ClientSendTeamStatsForPlayer(TeamStats);


		AFortWeapon* DamageCauser = nullptr;
		if (auto Projectile = DeathReport.DamageCauser ? DeathReport.DamageCauser->Cast<AFortProjectileBase>() : nullptr)
			DamageCauser = Projectile->GetOwnerWeapon();
		else if (auto Weapon = DeathReport.DamageCauser ? DeathReport.DamageCauser->Cast<AFortWeapon>() : nullptr)
			DamageCauser = Weapon;

		((void (*)(AFortGameModeAthena*, AFortPlayerController*, APlayerState*, AFortPawn*, UFortWeaponItemDefinition*, EDeathCause, char))(Sarah::Offsets::ImageBase + 0x6a74af4))(GameMode, PlayerController, KillerPlayerState == PlayerState ? nullptr : KillerPlayerState, KillerPawn, DamageCauser ? DamageCauser->WeaponData : nullptr, PlayerState->DeathInfo.DeathCause, 0);

		PlayerController->ClientSendEndBattleRoyaleMatchForPlayer(true, PlayerController->MatchReport->EndOfMatchResults);

		if (PlayerController->MyFortPawn && KillerPlayerState && KillerPawn && KillerPawn->Controller != PlayerController)
		{
			auto Handle = KillerPlayerState->AbilitySystemComponent->MakeEffectContext();
			FGameplayTag Tag;
			static auto Cue = FName(L"GameplayCue.Shield.PotionConsumed");
			Tag.TagName = Cue;
			KillerPlayerState->AbilitySystemComponent->NetMulticast_InvokeGameplayCueAdded(Tag, FPredictionKey(), Handle);
			KillerPlayerState->AbilitySystemComponent->NetMulticast_InvokeGameplayCueExecuted(Tag, FPredictionKey(), Handle);

			auto Health = KillerPawn->GetHealth();
			auto Shield = KillerPawn->GetShield();

			if (Health == 100)
			{
				Shield += Shield + 50;
			}
			else if (Health + 50 > 100)
			{
				Health = 100;
				Shield += (Health + 50) - 100;
			}
			else if (Health + 50 <= 100)
			{
				Health += 50;
			}

			KillerPawn->SetHealth(Health);
			KillerPawn->SetShield(Shield);
			//forgot to add this back
		}
		if (PlayerController->MyFortPawn && ((KillerPlayerState && KillerPlayerState->Place == 1) || PlayerState->Place == 1))
		{
			if (PlayerState->Place == 1)
			{
				KillerPlayerState = PlayerState;
				KillerPawn = (AFortPlayerPawnAthena*)PlayerController->MyFortPawn;
			}
			auto KillerPlayerController = (AFortPlayerControllerAthena*)KillerPlayerState->Owner;
			auto KillerWeapon = DamageCauser ? DamageCauser->WeaponData : nullptr;

			KillerPlayerController->PlayWinEffects(KillerPawn, KillerWeapon, PlayerState->DeathInfo.DeathCause, false);
			KillerPlayerController->ClientNotifyWon(KillerPawn, KillerWeapon, PlayerState->DeathInfo.DeathCause);
			KillerPlayerController->ClientNotifyTeamWon(KillerPawn, KillerWeapon, PlayerState->DeathInfo.DeathCause);

			if (KillerPlayerState != PlayerState)
			{
				KillerPlayerController->ClientSendEndBattleRoyaleMatchForPlayer(true, KillerPlayerController->MatchReport->EndOfMatchResults);

				FAthenaMatchStats& KillerStats = KillerPlayerController->MatchReport->MatchStats;
				FAthenaMatchTeamStats& KillerTeamStats = KillerPlayerController->MatchReport->TeamStats;


				KillerStats.Stats[3] = KillerPlayerState->KillScore;
				KillerStats.Stats[8] = KillerPlayerState->SquadId;
				KillerPlayerController->ClientSendMatchStatsForPlayer(KillerStats);

				KillerTeamStats.Place = KillerPlayerState->Place;
				KillerTeamStats.TotalPlayers = GameState->TotalPlayers;
				KillerPlayerController->ClientSendTeamStatsForPlayer(KillerTeamStats);
			}

			GameState->WinningTeam = KillerPlayerState->TeamIndex;
			GameState->OnRep_WinningTeam();
			GameState->WinningPlayerState = KillerPlayerState;
			GameState->OnRep_WinningPlayerState();
		}
	}

	static UClass* Class = UGAB_AthenaDBNO_C::StaticClass();

	if (!Class) return ClientOnPawnDiedOG(PlayerController, DeathReport);

	if (PlayerState->DeathInfo.bDBNO) //no point in remove an ability that no no need to
	{
		for (auto& Item : PlayerState->AbilitySystemComponent->ActivatableAbilities.Items)
		{
			if (!Item.Ability) continue; //Prob safer ngl
			if (Item.Ability->Class == Class/*Lowkey doesn't say if active or not*/)
			{
				PlayerState->AbilitySystemComponent->ClientCancelAbility(Item.Handle, Item.ActivationInfo);
				PlayerState->AbilitySystemComponent->ClientEndAbility(Item.Handle, Item.ActivationInfo);
				PlayerState->AbilitySystemComponent->ServerEndAbility(Item.Handle, Item.ActivationInfo, FPredictionKey());
			}
		}
	}


	return ClientOnPawnDiedOG(PlayerController, DeathReport);
}


void Player::ServerAttemptInventoryDrop(UObject* Context, FFrame& Stack)
{
	FGuid Guid;
	int32 Count;
	bool bTrash;
	Stack.StepCompiledIn(&Guid);
	Stack.StepCompiledIn(&Count);
	Stack.StepCompiledIn(&bTrash);
	Stack.IncrementCode();
	auto PlayerController = (AFortPlayerControllerAthena*)Context;

	if (!PlayerController || !PlayerController->Pawn)
		return;

	auto ItemEntry = PlayerController->WorldInventory->Inventory.ReplicatedEntries.Search([&](FFortItemEntry& entry)
		{ return entry.ItemGuid == Guid; });
	if (!ItemEntry || (ItemEntry->Count - Count) < 0)
		return;

	ItemEntry->Count -= Count;
	Inventory::SpawnPickup(PlayerController->Pawn->K2_GetActorLocation() + PlayerController->Pawn->GetActorForwardVector() * 70.f + FVector(0, 0, 50), *ItemEntry, EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::Unset, PlayerController->MyFortPawn, Count);
	if (ItemEntry->Count == 0)
		Inventory::Remove(PlayerController, Guid);
	else
		Inventory::ReplaceEntry(PlayerController, *ItemEntry);
}

void Player::OnCapsuleBeginOverlap(UObject *Context, FFrame& Stack)
{
	UPrimitiveComponent* OverlappedComp;
	AActor* OtherActor;
	UPrimitiveComponent* OtherComp;
	int32 OtherBodyIndex;
	bool bFromSweep;
	FHitResult SweepResult;
	Stack.StepCompiledIn(&OverlappedComp);
	Stack.StepCompiledIn(&OtherActor);
	Stack.StepCompiledIn(&OtherComp);
	Stack.StepCompiledIn(&OtherBodyIndex);
	Stack.StepCompiledIn(&bFromSweep);
	Stack.StepCompiledIn(&SweepResult);
	Stack.IncrementCode();

	auto Pawn = (AFortPlayerPawn*)Context;
	if (!Pawn || !Pawn->Controller || Pawn->Controller->IsA<AFortAthenaAIBotController>())
		return callOG(Pawn, Stack.CurrentNativeFunction, OnCapsuleBeginOverlap, OverlappedComp, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);

	auto Pickup = OtherActor->Cast<AFortPickup>();
	if (!Pickup || !Pickup->PrimaryPickupItemEntry.ItemDefinition)
		return callOG(Pawn, Stack.CurrentNativeFunction, OnCapsuleBeginOverlap, OverlappedComp, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);

	auto MaxStack = (int32) Utils::EvaluateScalableFloat(Pickup->PrimaryPickupItemEntry.ItemDefinition->MaxStackSize);
	auto itemEntry = ((AFortPlayerControllerAthena*)Pawn->Controller)->WorldInventory->Inventory.ReplicatedEntries.Search([&](FFortItemEntry& entry)
		{ return entry.ItemDefinition == Pickup->PrimaryPickupItemEntry.ItemDefinition && entry.Count <= MaxStack; });

	if (Pickup && Pickup->PawnWhoDroppedPickup != Pawn)
	{
		if ((!itemEntry && Inventory::GetQuickbar(Pickup->PrimaryPickupItemEntry.ItemDefinition) == EFortQuickBars::Secondary) || (itemEntry && itemEntry->Count < MaxStack))
			Pawn->ServerHandlePickup(Pickup, 0.4f, FVector(), true);
	}
	return callOG(Pawn, Stack.CurrentNativeFunction, OnCapsuleBeginOverlap, OverlappedComp, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);
}

void Player::ServerAttemptInteract(UFortControllerComponent_Interaction* InteractionComp, class AFortPawn* InteractingPawn, float InInteractDistance, class AActor* ReceivingActor, class UPrimitiveComponent* InteractComponent, ETInteractionType InteractType, class UObject* OptionalObjectData, EInteractionBeingAttempted InteractionBeingAttempted)
{
	Log(L"mate");

	if (!ReceivingActor)
	{
		Log(L"No ReceivingActor!");
		return;
	}

	AController* Controller = InteractionComp->GetOwner()->Cast<AController>();

	if (!Controller)
	{
		Log(L"No Controller!");
		return;
	}

	AFortPlayerPawn* PlayerPawn = InteractingPawn->Cast<AFortPlayerPawn>();

	if (!PlayerPawn)
	{
		Log(L"No PlayerPawn!");
		return;
	}

	if (InteractionBeingAttempted == EInteractionBeingAttempted::FirstInteraction)
	{
		Log(L"wow");
		if (auto ParticipantComponent = (UFortNonPlayerConversationParticipantComponent*)ReceivingActor->GetComponentByClass(UFortNonPlayerConversationParticipantComponent::StaticClass()))
		{
			auto ConversationComponent = (UFortPlayerConversationComponent*)Controller->GetComponentByClass(UFortPlayerConversationComponent::StaticClass());
			UConversationLibrary::StartConversation(ParticipantComponent->ConversationEntryTag, ConversationComponent->GetOwner(), ParticipantComponent->InteractorParticipantTag, ReceivingActor, ParticipantComponent->SelfParticipantTag);
		}
	}
	else
	{
		Log(L"oh");
	}

	return ServerAttemptInteractOG(InteractionComp, InteractingPawn, InInteractDistance, ReceivingActor, InteractComponent, InteractType, OptionalObjectData, InteractionBeingAttempted);
}

void Player::ServerAddMapMarker(UObject* Context, FFrame& Stack)
{
	FFortClientMarkerRequest MarkerRequest;
	Stack.StepCompiledIn(&MarkerRequest);
	Stack.IncrementCode();

	auto Comp = (UAthenaMarkerComponent*)Context;

	AFortPlayerControllerAthena* FortPlayerControllerAthena = Comp->GetOwner()->Cast<AFortPlayerControllerAthena>();

	if (!FortPlayerControllerAthena)
		return;

	AFortPlayerStateAthena* PlayerState = FortPlayerControllerAthena->PlayerState->Cast<AFortPlayerStateAthena>();

	if (!PlayerState || !PlayerState->PlayerTeam)
		return;

	//if (MarkerRequest.MarkedActor)
	//{
		FMarkerID MarkerID = Comp->MarkActorOnClient(MarkerRequest.MarkedActor, MarkerRequest.bIncludeSquad, MarkerRequest.bUseHoveredMarkerDetail);

		/*UFortWorldMarker* WorldMarker = Comp->FindMarkerByID(MarkerID);

		if (WorldMarker)
		{*/
			// WorldMarker->MarkerComponent = Comp;

		FFortWorldMarkerData MarkerData{};

		MarkerData.MostRecentArrayReplicationKey = -1;
		MarkerData.ReplicationID = -1;
		MarkerData.ReplicationKey = -1;
		MarkerData.MarkerType = MarkerRequest.MarkerType;
		MarkerData.Owner = PlayerState;
		MarkerData.BasePosition = MarkerRequest.BasePosition;
		MarkerData.BasePositionOffset = MarkerRequest.BasePositionOffset;
		MarkerData.WorldNormal = MarkerRequest.WorldNormal;
		MarkerData.MarkerID = MarkerID;
		MarkerData.bIncludeSquad = MarkerRequest.bIncludeSquad;
		MarkerData.bUseHoveredMarkerDetail = MarkerRequest.bUseHoveredMarkerDetail;

		if (MarkerRequest.MarkedActor)
		{
			MarkerData.MarkedActor = MarkerRequest.MarkedActor;
			MarkerData.MarkedActorClass = TSoftClassPtr<UClass>(MarkerRequest.MarkedActor->Class);
			int32 MarkerDisplayOffset = MarkerRequest.MarkedActor->GetOffset(FName(L"MarkerDisplay"));

			if (MarkerDisplayOffset != -1)
			{
				const FMarkedActorDisplayInfo& MarkerDisplay = MarkerRequest.MarkedActor->Get<FMarkedActorDisplayInfo>(MarkerDisplayOffset);

				MarkerData.CustomDisplayInfo = MarkerDisplay;
				MarkerData.bHasCustomDisplayInfo = true;
			}
			auto Pickup = MarkerRequest.MarkedActor->Cast<AFortPickup>();
			switch (MarkerRequest.MarkerType)
			{
			case EFortWorldMarkerType::Item:
				if (Pickup)
				{
					MarkerData.ItemDefinition = Pickup->PrimaryPickupItemEntry.ItemDefinition;
					MarkerData.ItemCount = Pickup->PrimaryPickupItemEntry.Count;
				}
			}

		}

		// WorldMarker->MarkerDataCache = FortWorldMarkerData;

		for (auto& TeamMember : PlayerState->PlayerTeam->TeamMembers)
		{
			if (!TeamMember)
				continue;

			//if (Controller == FortPlayerControllerAthena)
			//	continue;

			AFortPlayerControllerAthena* MemberController = TeamMember->Cast<AFortPlayerControllerAthena>();

			if (!MemberController || !MemberController->MarkerComponent)
				continue;

			MemberController->OnServerMarkerAdded(MarkerData);
		}
		//}
	//}
}

void Player::ServerRemoveMapMarker(UObject *Context, FFrame& Stack)
{
	FMarkerID MarkerID;
	ECancelMarkerReason CancelReason;
	Stack.StepCompiledIn(&MarkerID);
	Stack.StepCompiledIn(&CancelReason);
	Stack.IncrementCode();

	auto Comp = (UAthenaMarkerComponent*)Context;

	AFortPlayerControllerAthena* PlayerController = Comp->GetOwner()->Cast<AFortPlayerControllerAthena>();

	if (!PlayerController)
		return;

	AFortPlayerStateAthena* PlayerState = PlayerController->PlayerState->Cast<AFortPlayerStateAthena>();

	if (!PlayerState || !PlayerState->PlayerTeam)
		return;

	for (auto& TeamMember : PlayerState->PlayerTeam->TeamMembers)
	{
		if (!TeamMember)
			continue;

		//if (Controller == PlayerController)
		//	continue;

		AFortPlayerControllerAthena* MemberController = TeamMember->Cast<AFortPlayerControllerAthena>();

		if (!MemberController || !MemberController->MarkerComponent)
			continue;

		MemberController->MarkerComponent->UnmarkActorOnClient(MarkerID);
	}
}

void Player::OnPlayImpactFX(AFortWeapon* Weapon, FHitResult& HitResult, EPhysicalSurface ImpactPhysicalSurface, UFXSystemComponent* SpawnedPSC)
{
	Log(L"%s", Weapon->WeaponData->GetWName().c_str());
	auto Pawn = (AFortPlayerPawnAthena*) Weapon->GetOwner();
	auto Controller = (AFortPlayerControllerAthena*)Pawn->Controller;
	auto ent = Controller->WorldInventory->Inventory.ReplicatedEntries.Search([&](FFortItemEntry& entry) {
		return entry.ItemGuid == Pawn->CurrentWeapon->ItemEntryGuid;
	});

	if (ent)
	{
		ent->LoadedAmmo = Pawn->CurrentWeapon->AmmoCount;
		Inventory::ReplaceEntry(Controller, *ent);
	}
	return OnPlayImpactFXOG(Weapon, HitResult, ImpactPhysicalSurface, SpawnedPSC);
}

void Player::TeleportPlayerToLinkedVolume(AFortAthenaCreativePortal* Portal, FFrame& Frame)
{
	AFortPlayerPawn* PlayerPawn;
	bool bUseSpawnTags;

	Frame.StepCompiledIn(&PlayerPawn);
	Frame.StepCompiledIn(&bUseSpawnTags);
	Frame.IncrementCode();

	if (!PlayerPawn)
		return;

	auto Volume = Portal->LinkedVolume;
	auto Location = Volume->K2_GetActorLocation();
	Location.Z = 10000;

	PlayerPawn->K2_TeleportTo(Location, FRotator());
	PlayerPawn->BeginSkydiving(false);
}

void Player::TeleportPlayerForPlotLoadComplete(UObject* Portal, FFrame& Frame)
{
	AFortPlayerPawn* PlayerPawn;
	Frame.StepCompiledIn(&PlayerPawn);
	Frame.IncrementCode();

	if (!PlayerPawn)
		return;

	auto Portal2 = (AFortAthenaCreativePortal*)Portal;

	PlayerPawn->K2_TeleportTo(Portal2->GetLinkedVolume()->K2_GetActorLocation(), Portal2->GetLinkedVolume()->K2_GetActorRotation());
}

void Player::TeleportPlayer(UObject* Portal, FFrame& Frame)
{
	AFortPlayerPawn* PlayerPawn;
	FRotator TeleportRotation;
	Frame.StepCompiledIn(&PlayerPawn);
	Frame.StepCompiledIn(&TeleportRotation);
	Frame.IncrementCode();

	if (!PlayerPawn)
		return;

	PlayerPawn->K2_TeleportTo(Portal->Cast<AFortAthenaCreativePortal>()->TeleportLocation, TeleportRotation);
}


void Player::ServerTeleportToPlaygroundLobbyIsland(AFortPlayerControllerAthena* Controller, FFrame& Frame)
{
	auto GameMode = (AFortGameModeAthena*)UWorld::GetWorld()->AuthorityGameMode;
	auto GameState = (AFortGameStateAthena*)GameMode->GameState;

	Log(L"Skunked");
	if (Controller->WarmupPlayerStart)
		Controller->GetPlayerPawn()->K2_TeleportTo(Controller->WarmupPlayerStart->K2_GetActorLocation(), Controller->GetPlayerPawn()->K2_GetActorRotation());
	else 
	{
		AActor* Actor = GameMode->ChoosePlayerStart(Controller);
		Controller->GetPlayerPawn()->K2_TeleportTo(Actor->K2_GetActorLocation(), Actor->K2_GetActorRotation());
	}
	Controller->MyFortPawn->ForceKill(FGameplayTag(), Controller, nullptr);
}

void Player::ServerRestartPlayer(APlayerController *PlayerController)
{
	Log(L"nill yourself kigga");
	static auto ServerRestartPlayerZone = (void(*)(APlayerController*)) AFortPlayerControllerZone::GetDefaultObj()->VTable[0x11e];
	return ServerRestartPlayerZone(PlayerController);
}

template<typename ...Args>
static void LOG(Args && ...args)
{
	std::cout << "Log: ";
	(std::cout << ... << args);
	std::cout << std::endl;
}

static inline FString(*InitNewPlayerOG)(AGameModeBase* GameMode, APlayerController* NewPlayerController, FUniqueNetIdRepl UniqueId, FString Options, FString Portal);
static FString InitNewPlayerHook(AGameModeBase* GameMode, APlayerController* NewPlayerController, FUniqueNetIdRepl UniqueId, FString Options, FString Portal)
{
	Log(L"gee gee");

	if (!Options.Data)
	{
		Log(L"Skidder");
		return InitNewPlayerOG(GameMode, NewPlayerController, UniqueId, Options, Portal);
	}

	UEAllocatedString OptionsStr = Options.ToString();
	UEAllocatedString PortalStr = Portal.ToString();

	std::cout << "Options: " + OptionsStr << std::endl;
	std::cout << "Portal: " + PortalStr << std::endl;

#ifndef NO_CPR

	if (OptionsStr.contains("?AuthTicket=lawinstokenlol")
		|| OptionsStr.contains("?RequiredVoiceChatImpl="))
	{
		NewPlayerController->CustomTimeDilation = 4.7f;
		NewPlayerController->Tags.Add(FName(L"Funny.Guy"));
		LOG("Exploiter, Added Flag!");

		if (OptionsStr.contains("?RequiredVoiceChatImpl=")) LOG("Contained VoiceChat, Flagged!");
		if (OptionsStr.contains("?AuthTicket=lawinstokenlol")) LOG("Contained LawinsToken, Flagged!");
	}

#endif // !NO_CPR
	if (!NewPlayerController->Tags.Contains(FName(L"Funny.guy"))) {

		return InitNewPlayerOG(GameMode, NewPlayerController, UniqueId, Options, Portal);
	}
}

void Player::Hook()
{
	//Utils::ExecHook("/Script/FortniteGame.FortPlayerController.ServerLoadingScreenDropped", ServerLoadingScreenDropped, ServerLoadingScreenDroppedOG);
	Utils::ExecHook(L"/Script/Engine.PlayerController.ServerAcknowledgePossession", ServerAcknowledgePossession);
	Utils::ExecHook(L"/Script/Engine.Controller.GetPlayerViewPoint", GetPlayerViewPoint);
	Utils::ExecHook(L"/Script/FortniteGame.FortPlayerController.ServerPlayEmoteItem", ServerPlayEmoteItem);
	Utils::ExecHook(L"/Script/FortniteGame.FortPlayerController.ServerExecuteInventoryItem", ServerExecuteInventoryItem);
	Utils::ExecHook(L"/Script/FortniteGame.FortPlayerController.ServerReturnToMainMenu", ServerReturnToMainMenu);
	Utils::ExecHook(L"/Script/FortniteGame.FortPawn.MovingEmoteStopped", MovingEmoteStopped);
	Utils::ExecHook(L"/Script/FortniteGame.FortControllerComponent_Aircraft.ServerAttemptAircraftJump", ServerAttemptAircraftJump, ServerAttemptAircraftJumpOG);
	Utils::ExecHook(L"/Script/FortniteGame.FortPlayerPawn.ServerSendZiplineState", ServerSendZiplineState);
	Utils::ExecHook(L"/Script/FortniteGame.FortPlayerPawn.ServerHandlePickupInfo", ServerHandlePickupInfo);
	Utils::Hook(Sarah::Offsets::ImageBase + 0x6fa4854, CompletePickupAnimation, CompletePickupAnimationOG);
	Utils::Hook<AFortPlayerPawnAthena>(uint32(0x130), NetMulticast_Athena_BatchedDamageCues, NetMulticast_Athena_BatchedDamageCuesOG);
	Utils::Hook(Sarah::Offsets::ImageBase + 0x104c0fc, OnPlayImpactFX, OnPlayImpactFXOG);
	Utils::Hook(Sarah::Offsets::ImageBase + 0x744cf64, ReloadWeapon);
	Utils::Hook(Sarah::Offsets::ImageBase + 0x76c94b8, ClientOnPawnDied, ClientOnPawnDiedOG);
	//Utils::Hook(Sarah::Offsets::ImageBase + 0x19A40E8, InitNewPlayerHook, InitNewPlayerOG);
	Utils::ExecHook(L"/Script/FortniteGame.FortPlayerController.ServerAttemptInventoryDrop", ServerAttemptInventoryDrop);
	Utils::ExecHook(L"/Script/FortniteGame.FortPlayerPawn.OnCapsuleBeginOverlap", OnCapsuleBeginOverlap, OnCapsuleBeginOverlapOG);
	Utils::Hook(Sarah::Offsets::ImageBase + 0x6d1ea68, ServerAttemptInteract, ServerAttemptInteractOG);
	Utils::ExecHook(L"/Script/FortniteGame.AthenaMarkerComponent.ServerAddMapMarker", ServerAddMapMarker);
	Utils::ExecHook(L"/Script/FortniteGame.AthenaMarkerComponent.ServerRemoveMapMarker", ServerRemoveMapMarker);
	Utils::ExecHook(L"/Script/FortniteGame.FortAthenaCreativePortal.TeleportPlayerToLinkedVolume", TeleportPlayerToLinkedVolume);
	Utils::ExecHook(L"/Script/FortniteGame.FortAthenaCreativePortal.TeleportPlayer", TeleportPlayer);
	Utils::ExecHook(L"/Script/FortniteGame.FortAthenaCreativePortal.TeleportPlayerForPlotLoadComplete", TeleportPlayerForPlotLoadComplete);
	Utils::ExecHook(L"/Script/FortniteGame.FortPlayerControllerAthena.ServerTeleportToPlaygroundLobbyIsland", ServerTeleportToPlaygroundLobbyIsland);
	//Utils::Hook<AFortPlayerControllerAthena>(uint32(5312 / 8), ServerReadyToStartMatch, ServerReadyToStartMatchOG);
	Utils::Hook<AFortPlayerControllerAthena>(uint32(0x547), MakeNewCreativePlot, MakeNewCreativePlotOG);
	Utils::Hook<AFortPlayerControllerAthena>(uint32(0x11e), ServerRestartPlayer);
}
