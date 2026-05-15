#include "pch.h"
#include "GameMode.h"
#include "Abilities.h"
#include "Inventory.h"
#include "Looting.h"

#include "Offsets.h"
#include "Player.h"
#include "Conversation.h"
#include "Vehicles.h"
#include "AI.h"
#include "Options.h"
#include "Backend.h"

void SetPlaylist(AFortGameModeAthena* GameMode, UFortPlaylistAthena* Playlist) {
	auto GameState = (AFortGameStateAthena*)GameMode->GameState;

	GameState->CurrentPlaylistInfo.BasePlaylist = Playlist;
	GameState->CurrentPlaylistInfo.OverridePlaylist = Playlist;
	GameState->CurrentPlaylistInfo.PlaylistReplicationKey++;
	GameState->CurrentPlaylistInfo.MarkArrayDirty();

	GameState->CurrentPlaylistId = GameMode->CurrentPlaylistId = Playlist->PlaylistId;
	GameMode->CurrentPlaylistName = Playlist->PlaylistName;
	GameState->OnRep_CurrentPlaylistInfo();
	GameState->OnRep_CurrentPlaylistId();

	GameMode->GameSession->MaxPlayers = Playlist->MaxPlayers;

	GameState->AirCraftBehavior = Playlist->AirCraftBehavior;
	GameState->WorldLevel = Playlist->LootLevel;
	GameState->CachedSafeZoneStartUp = Playlist->SafeZoneStartUp;

	GameMode->bAlwaysDBNO = Playlist->MaxSquadSize > 1;
	GameMode->bDBNOEnabled = Playlist->MaxSquadSize > 1;
	GameMode->AISettings = Playlist->AISettings;
	if (GameMode->AISettings)
		GameMode->AISettings->AIServices[1] = UAthenaAIServicePlayerBots::StaticClass();
}

bool bReady = false;
void GameMode::ReadyToStartMatch(UObject* Context, FFrame& Stack, bool* Ret) {
	static auto Playlist = bCreative ? Utils::FindObject<UFortPlaylistAthena>(L"/Game/Athena/Playlists/Creative/Playlist_PlaygroundV2.Playlist_PlaygroundV2") : Utils::FindObject<UFortPlaylistAthena>(L"/Game/Athena/Playlists/Playlist_DefaultSolo.Playlist_DefaultSolo");
	Stack.IncrementCode();
	auto GameMode = Context->Cast<AFortGameModeAthena>();
	if (!GameMode) {
		*Ret = callOGWithRet(((AGameMode*)Context), Stack.CurrentNativeFunction, ReadyToStartMatch);
		return;
	}
	auto GameState = ((AFortGameStateAthena*)GameMode->GameState);
	if (GameMode->CurrentPlaylistId == -1) {
		GameMode->WarmupRequiredPlayerCount = 1;

		SetPlaylist(GameMode, Playlist);
		for (auto& Level : Playlist->AdditionalLevels)
		{
			bool Success = false;
			std::cout << "Level: " << Level.Get()->Name.ToString() << std::endl;
			ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(UWorld::GetWorld(), Level, FVector(), FRotator(), &Success, FString(), nullptr);
			FAdditionalLevelStreamed level{};
			level.bIsServerOnly = false;
			level.LevelName = Level.ObjectID.AssetPathName;
			if (Success) GameState->AdditionalPlaylistLevelsStreamed.Add(level);
		}
		for (auto& Level : Playlist->AdditionalLevelsServerOnly)
		{
			bool Success = false;
			std::cout << "Level: " << Level.Get()->Name.ToString() << std::endl;
			ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(UWorld::GetWorld(), Level, FVector(), FRotator(), &Success, FString(), nullptr);
			FAdditionalLevelStreamed level{};
			level.bIsServerOnly = true;
			level.LevelName = Level.ObjectID.AssetPathName;
			if (Success) GameState->AdditionalPlaylistLevelsStreamed.Add(level);
		}
		GameState->OnRep_AdditionalPlaylistLevelsStreamed();

		GameMode->AIDirector = Utils::SpawnActor<AAthenaAIDirector>({});
		GameMode->AIDirector->Activate();

		GameMode->AIGoalManager = Utils::SpawnActor<AFortAIGoalManager>({});

		GameMode->WarmupCountdownDuration = 120;

		*Ret = false;
		return;
	}

	if (!GameMode->bWorldIsReady) {
		auto Starts = bCreative ? (TArray<AActor*>) Utils::GetAll<AFortPlayerStartCreative>() : (TArray<AActor*>) Utils::GetAll<AFortPlayerStartWarmup>();
		auto StartsNum = Starts.Num();
		Starts.Free();
		if (StartsNum == 0 || !GameState->MapInfo) {
			*Ret = false;
			return;
		}

		GameState->DefaultParachuteDeployTraceForGroundDistance = 10000;



		void (*LoadBuiltInGameFeaturePlugins)(UFortGameFeaturePluginManager*, bool) = decltype(LoadBuiltInGameFeaturePlugins)(Sarah::Offsets::ImageBase + 0x199bbb0);
		UFortGameFeaturePluginManager* PluginManager = nullptr;
		for (int i = 0; i < UObject::GObjects->Num(); i++)
		{
			auto Object = UObject::GObjects->GetByIndex(i);
			if (Object && !Object->IsDefaultObject() && Object->Class == UFortGameFeaturePluginManager::StaticClass())
			{
				PluginManager = (UFortGameFeaturePluginManager*)Object;
				break;
			}
		}

		LoadBuiltInGameFeaturePlugins(PluginManager, true);

		AbilitySets.Add(Utils::FindObject<UFortAbilitySet>(L"/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GAS_AthenaPlayer.GAS_AthenaPlayer"));

		auto AddToTierData = [&](UDataTable* Table, UEAllocatedMap<FName, FFortLootTierData*>& TempArr) {
			if (auto CompositeTable = Table->Cast<UCompositeDataTable>())
				for (auto& ParentTable : CompositeTable->ParentTables)
					for (auto& [Key, Val] : (TMap<FName, FFortLootTierData*>) ParentTable->RowMap) {
						TempArr[Key] = Val;
					}

			for (auto& [Key, Val] : (TMap<FName, FFortLootTierData*>) Table->RowMap) {
				TempArr[Key] = Val;
			}
		};
		
		auto AddToPackages = [&](UDataTable* Table, UEAllocatedMap<FName, FFortLootPackageData*>& TempArr) {
			//Table->AddToRoot();
			if (auto CompositeTable = Table->Cast<UCompositeDataTable>())
				for (auto& ParentTable : CompositeTable->ParentTables)
					for (auto& [Key, Val] : (TMap<FName, FFortLootPackageData*>) ParentTable->RowMap) {
						TempArr[Key] = Val;
					}

			for (auto& [Key, Val] : (TMap<FName, FFortLootPackageData*>) Table->RowMap) {
				TempArr[Key] = Val;
			}
		};


		UEAllocatedMap<FName, FFortLootTierData*> LootTierDataTempArr;
		auto LootTierData = Playlist->LootTierData.Get();
		if (!LootTierData) 
			LootTierData = Utils::FindObject<UDataTable>(L"/Game/Items/Datatables/AthenaLootTierData_Client.AthenaLootTierData_Client");
		if (LootTierData) 
			AddToTierData(LootTierData, LootTierDataTempArr);
		for (auto& [_, Val] : LootTierDataTempArr)
		{
			Looting::TierDataAllGroups.Add(Val);
		}

		UEAllocatedMap<FName, FFortLootPackageData*> LootPackageTempArr;
		auto LootPackages = Playlist->LootPackages.Get();
		if (!LootPackages) LootPackages = Utils::FindObject<UDataTable>(L"/Game/Items/Datatables/AthenaLootPackages_Client.AthenaLootPackages_Client");
		if (LootPackages) 
			AddToPackages(LootPackages, LootPackageTempArr);
		for (auto& [_, Val] : LootPackageTempArr)
		{
			Looting::LPGroupsAll.Add(Val);
		}

		for (int i = 0; i < UObject::GObjects->Num(); i++)
		{
			auto Object = UObject::GObjects->GetByIndex(i);

			if (!Object || !Object->Class || Object->IsDefaultObject())
				continue;

			if (auto GameFeatureData = Object->Cast<UFortGameFeatureData>())
			{
				auto LootTableData = GameFeatureData->DefaultLootTableData;
				auto LTDFeatureData = LootTableData.LootTierData.Get();
				auto AbilitySet = GameFeatureData->PlayerAbilitySet.Get();
				auto LootPackageData = LootTableData.LootPackageData.Get();
				if (AbilitySet) {
					Log(AbilitySet->GetWName().c_str());
					AbilitySets.Add(AbilitySet);
					AbilitySet->AddToRoot();
				}
				if (LTDFeatureData) {
					UEAllocatedMap<FName, FFortLootTierData*> LTDTempData;

					AddToTierData(LTDFeatureData, LTDTempData);

					for (auto& Tag : Playlist->GameplayTagContainer.GameplayTags)
						for (auto& Override : GameFeatureData->PlaylistOverrideLootTableData)
							if (Tag.TagName == Override.First.TagName)
								AddToTierData(Override.Second.LootTierData.Get(), LTDTempData);

					for (auto& [_, Val] : LTDTempData)
					{
						Looting::TierDataAllGroups.Add(Val);
					}
				}
				if (LootPackageData) {
					UEAllocatedMap<FName, FFortLootPackageData*> LPTempData;


					AddToPackages(LootPackageData, LPTempData);

					for (auto& Tag : Playlist->GameplayTagContainer.GameplayTags)
						for (auto& Override : GameFeatureData->PlaylistOverrideLootTableData)
							if (Tag.TagName == Override.First.TagName)
								AddToPackages(Override.Second.LootPackageData.Get(), LPTempData);

					for (auto& [_, Val] : LPTempData)
					{
						Looting::LPGroupsAll.Add(Val);
					}
				}
			}
		}

		Looting::SpawnFloorLootForContainer(Utils::FindObject<UBlueprintGeneratedClass>(L"/Game/Athena/Environments/Blueprints/Tiered_Athena_FloorLoot_Warmup.Tiered_Athena_FloorLoot_Warmup_C"));
		Looting::SpawnFloorLootForContainer(Utils::FindObject<UBlueprintGeneratedClass>(L"/Game/Athena/Environments/Blueprints/Tiered_Athena_FloorLoot_01.Tiered_Athena_FloorLoot_01_C"));


		auto ConsumableSpawners = Utils::GetAll<ABGAConsumableSpawner>();
		for (auto& Spawner : ConsumableSpawners) {
			Looting::SpawnConsumableActor(Spawner);
		}


		UCurveTable* AthenaGameDataTable = GameState->AthenaGameDataTable; // this is js playlist gamedata or default gamedata if playlist doesn't have one

		if (AthenaGameDataTable)
		{
			static FName DefaultSafeZoneDamageName = FName(L"Default.SafeZone.Damage");

			for (const auto& [RowName, RowPtr] : ((UDataTable*)AthenaGameDataTable)->RowMap) // same offset
			{
				if (RowName != DefaultSafeZoneDamageName)
					continue;

				FSimpleCurve* Row = (FSimpleCurve*)RowPtr;

				if (!Row)
					continue;

				for (auto& Key : Row->Keys)
				{
					FSimpleCurveKey* KeyPtr = &Key;

					if (KeyPtr->Time == 0.f)
					{
						KeyPtr->Value = 0.f;
					}
				}

				Row->Keys.Add(FSimpleCurveKey(1.f, 0.01f), 1);
			}
		}

		ServerIsReady(true);
		SetConsoleTitleA("Sarah 21.00: Ready");
		GameMode->bWorldIsReady = true;
		*Ret = false;
		return;
	}

	auto VolumeManager = GameState->VolumeManager;
	TArray<FPlaylistStreamedLevelData>& AdditonalPlaylistLevels = *(TArray<FPlaylistStreamedLevelData>*) (__int64(GameState) + 0x3c0);

	bool bAllLevelsFinishedStreaming = true;
	for (auto& Something : AdditonalPlaylistLevels)
	{
		if (!Something.bIsFinishedStreaming)
		{
			bAllLevelsFinishedStreaming = false;
			break;
		}
	}

	bool (*AreAllAdditionalPlaylistLevelsVisible)(AFortGameStateAthena*) = decltype(AreAllAdditionalPlaylistLevelsVisible)(Sarah::Offsets::ImageBase + 0x6e908c0);
	int32 (*CountReadyPlayers)(AFortGameModeAthena*, bool) = decltype(CountReadyPlayers)(Sarah::Offsets::ImageBase + 0x6a64d44);

	*Ret = GameMode->bWorldIsReady && GameState->bPlaylistDataIsLoaded && GameMode->MatchState == L"WaitingToStart" && bAllLevelsFinishedStreaming && (!VolumeManager || !VolumeManager->bInSpawningStartup) && AreAllAdditionalPlaylistLevelsVisible(GameState) && CountReadyPlayers(GameMode, false) >= GameMode->WarmupRequiredPlayerCount;
	/*static bool bSetupAI = false;
	if (*Ret && !bSetupAI) {
		bSetupAI = true;
		for (auto& Controller : GameMode->AliveBots)
		{
			Controller->Blackboard->SetValueAsEnum(FName(L"AIEvaluator_Global_GamePhaseStep"), (uint8)EAthenaGamePhaseStep::Warmup);
			Controller->Blackboard->SetValueAsEnum(FName(L"AIEvaluator_Global_GamePhase"), (uint8)EAthenaGamePhase::Warmup);
		}
	}*/
}

APawn* GameMode::SpawnDefaultPawnFor(UObject* Context, FFrame& Stack, APawn** Ret) {
	AController* NewPlayer;
	AActor* StartSpot;
	Stack.StepCompiledIn(&NewPlayer);
	Stack.StepCompiledIn(&StartSpot);
	Stack.IncrementCode();
	auto GameMode = (AFortGameModeAthena*)Context;
	auto Transform = StartSpot->GetTransform();
	auto Pawn = GameMode->SpawnDefaultPawnAtTransform(NewPlayer, Transform);

	static bool IsFirstPlayer = false;

	if (!IsFirstPlayer)
	{
		Utils::Patch<uint8_t>(Sarah::Offsets::ImageBase + 0x154b864, 0xc3);
		for (ABuildingProp_ConversationCompatible* ConversationProp : Utils::GetAll<ABuildingProp_ConversationCompatible>())
		{
			if (!ConversationProp)
				continue;

			//Log(ConversationProp->GetFullWName().c_str());

			UFortNonPlayerConversationParticipantComponent* ParticipantComponent =
				(UFortNonPlayerConversationParticipantComponent*)
				ConversationProp->GetComponentByClass(UFortNonPlayerConversationParticipantComponent::StaticClass());

			if (!ParticipantComponent)
				continue;

			if (ParticipantComponent->ServiceProviderIDTag.TagName == FName(0))
			{
				ParticipantComponent->ServiceProviderIDTag.TagName = ParticipantComponent->ConversationEntryTag.TagName;
			}

			if (ParticipantComponent->GiftItemInventory)
			{
				for (const auto& [RowName, RowPtr] : ParticipantComponent->GiftItemInventory->RowMap)
				{
					FNPCGiftItemInventoryRow* Row = (FNPCGiftItemInventoryRow*)RowPtr;

					if (!RowName.IsValid() || !Row)
						continue;

					if (Row->NPC.TagName == FName(L"None") || Row->NPC.TagName == ParticipantComponent->ServiceProviderIDTag.TagName)
					{
						ParticipantComponent->SupportedGiftItems.Add(*Row);
					}
				}
			}

			if (ParticipantComponent->Gifts)
			{
				for (const auto& [RowName, RowPtr] : ParticipantComponent->Gifts->RowMap)
				{
					FNPCConversationGiftRow* Row = (FNPCConversationGiftRow*)RowPtr;

					if (!RowName.IsValid() || !Row)
						continue;

					if (Row->NPC.TagName == FName(L"None") || Row->NPC.TagName == ParticipantComponent->ServiceProviderIDTag.TagName)
					{
						ParticipantComponent->SupportedGiftTypes.Add(*Row);
					}
				}
			}

			if (ParticipantComponent->SalesInventory)
			{
				for (const auto& [RowName, RowPtr] : ParticipantComponent->SalesInventory->RowMap)
				{
					FNPCSaleInventoryRow* Row = (FNPCSaleInventoryRow*)RowPtr;

					if (!RowName.IsValid() || !Row)
						continue;

					if (Row->NPC.TagName == FName(L"None") || Row->NPC.TagName == ParticipantComponent->ServiceProviderIDTag.TagName)
					{
						ParticipantComponent->SupportedSales.Add(*Row);
					}
				}
			}

			if (ParticipantComponent->Services)
			{
				for (const auto& [RowName, RowPtr] : ParticipantComponent->Services->RowMap)
				{
					FNPCDynamicServiceRow* Row = (FNPCDynamicServiceRow*)RowPtr;

					if (!RowName.IsValid() || !Row)
						continue;

					if (Row->NPC.TagName == FName(L"None") || Row->NPC == ParticipantComponent->ServiceProviderIDTag)
					{
						ParticipantComponent->SupportedServices.GameplayTags.Add(Row->ServiceTag);
					}
				}
			}
		}
		if (GameMode->AISettings)
			AI::SpawnAIs();

		

		/*UAthenaAIServicePlayerBots* AIServicePlayerBots = UAthenaAIBlueprintLibrary::GetAIServicePlayerBots(UWorld::GetWorld());

		AIServicePlayerBots->DefaultBotAISpawnerData = Utils::FindObject<UClass>(L"/Game/Athena/AI/Phoebe/BP_AISpawnerData_Phoebe.BP_AISpawnerData_Phoebe_C");

		Log(L"%s", AIServicePlayerBots->DefaultBotAISpawnerData->GetFullWName().c_str());

		FMMRSpawningInfo NewSpawningInfo{};
		NewSpawningInfo.SpawnerData = (UFortAthenaAISpawnerData*)AIServicePlayerBots->DefaultBotAISpawnerData->DefaultObject;
		NewSpawningInfo.AmountToSpawn = 70;

		AIServicePlayerBots->CachedMMRSpawningInfo.SpawningInfos.Add(NewSpawningInfo);

		static void (*StartSpawning)(UAthenaAIServicePlayerBots* AthenaAIServicePlayerBots, const wchar_t *) = decltype(StartSpawning)(Sarah::Offsets::ImageBase + 0x8D0D55C);

		StartSpawning(AIServicePlayerBots, L"WarmUp");

		static void (*DumpStackTraceToLog)() = decltype(DumpStackTraceToLog)(Sarah::Offsets::ImageBase + 0x77B73F0);

		DumpStackTraceToLog();*/

		

		IsFirstPlayer = true;
	}

	auto PlayerController = NewPlayer->Cast<AFortPlayerController>();
	if (!PlayerController) return *Ret = Pawn;

	auto Num = PlayerController->WorldInventory->Inventory.ReplicatedEntries.Num();
	if (Num != 0) {
		PlayerController->WorldInventory->Inventory.ReplicatedEntries.ResetNum();
		PlayerController->WorldInventory->Inventory.ItemInstances.ResetNum();
	}
	Inventory::GiveItem(PlayerController, PlayerController->CosmeticLoadoutPC.Pickaxe->WeaponDefinition);
	for (auto& StartingItem : ((AFortGameModeAthena*)GameMode)->StartingItems)
	{
		if (StartingItem.Count && !StartingItem.Item->IsA<UFortSmartBuildingItemDefinition>())
		{
			Inventory::GiveItem(PlayerController, StartingItem.Item, StartingItem.Count);
		}
	}
	if (bLateGame && Num != 0)
	{
	    	auto Shotgun = Lategame::GetShotguns();
		auto AssaultRifle = Lategame::GetAssaultRifles();
		auto Sniper = Lategame::GetSnipers();
		auto Heal = Lategame::GetHeals();
		auto HealSlot2 = Lategame::GetHeals();

		int ShotgunClipSize = Inventory::GetStats((UFortWeaponItemDefinition*)Shotgun.Item)->ClipSize;
		int AssaultRifleClipSize = Inventory::GetStats((UFortWeaponItemDefinition*)AssaultRifle.Item)->ClipSize;
		int SniperClipSize = Inventory::GetStats((UFortWeaponItemDefinition*)Sniper.Item)->ClipSize;
		// for grappler
		int HealClipSize = Heal.Item->IsA<UFortWeaponItemDefinition>() ? Inventory::GetStats((UFortWeaponItemDefinition*)Heal.Item)->ClipSize : 0;
		int HealSlot2ClipSize = HealSlot2.Item->IsA<UFortWeaponItemDefinition>() ? Inventory::GetStats((UFortWeaponItemDefinition*)HealSlot2.Item)->ClipSize : 0;

		Inventory::GiveItem(PlayerController, Lategame::GetResource(EFortResourceType::Wood), 500);
		Inventory::GiveItem(PlayerController, Lategame::GetResource(EFortResourceType::Stone), 500);
		Inventory::GiveItem(PlayerController, Lategame::GetResource(EFortResourceType::Metal), 500);

		Inventory::GiveItem(PlayerController, Lategame::GetAmmo(EAmmoType::Assault), 250);
		Inventory::GiveItem(PlayerController, Lategame::GetAmmo(EAmmoType::Shotgun), 50);
		Inventory::GiveItem(PlayerController, Lategame::GetAmmo(EAmmoType::Submachine), 400);
		Inventory::GiveItem(PlayerController, Lategame::GetAmmo(EAmmoType::Rocket), 6);
		Inventory::GiveItem(PlayerController, Lategame::GetAmmo(EAmmoType::Sniper), 20);

		Inventory::GiveItem(PlayerController, AssaultRifle.Item, AssaultRifle.Count, AssaultRifleClipSize, true);
		Inventory::GiveItem(PlayerController, Shotgun.Item, Shotgun.Count, ShotgunClipSize, true);
		Inventory::GiveItem(PlayerController, Sniper.Item, Sniper.Count, SniperClipSize, true);
		Inventory::GiveItem(PlayerController, Heal.Item, Heal.Count, HealClipSize, true);
		Inventory::GiveItem(PlayerController, HealSlot2.Item, HealSlot2.Count, HealSlot2ClipSize, true);
	}


	if (Num == 0) 
	{
		auto PlayerState = (AFortPlayerStateAthena*)PlayerController->PlayerState;

		for (auto& AbilitySet : AbilitySets)
			Abilities::GiveAbilitySet(PlayerState->AbilitySystemComponent, AbilitySet);

		auto SprintCompClass = Utils::FindObject<UClass>(L"/TacticalSprint/Gameplay/TacticalSprintControllerComponent.TacticalSprintControllerComponent_C");
		auto EnergyCompClass = Utils::FindObject<UClass>(L"/TacticalSprint/Gameplay/TacticalSprintEnergyComponent.TacticalSprintEnergyComponent_C");

		UFortPlayerControllerComponent_TacticalSprint* SprintComp = (UFortPlayerControllerComponent_TacticalSprint*)PlayerController->GetComponentByClass(SprintCompClass);
		UFortComponent_Energy* EnergyComp = (UFortComponent_Energy*)Pawn->GetComponentByClass(EnergyCompClass);

		/*SprintComp->CurrentBoundPlayerPawn = (AFortPlayerPawn*)Pawn;
		SprintComp->OnPawnChanged(SprintComp->CurrentBoundPlayerPawn);

		Pawn->Tags.Add(SprintComp->TacticalSprintAbilityGameplayTag.TagName);
		PlayerController->Tags.Add(SprintComp->TacticalSprintAbilityGameplayTag.TagName);

		EnergyComp->HandleControllerChanged(Pawn, nullptr, NewPlayer);*/
		//SprintComp->SetIsSprinting(true);

		Log(L"%p", SprintComp);
		Log(L"%p", SprintComp->CurrentBoundPlayerPawn);
		Log(L"%d", bool(SprintComp->bTacticalSprintEnabled));
		Log(L"%ls", SprintComp->TacticalSprintAbilityGameplayTag.TagName.ToWString().c_str());
		Log(L"%ls", SprintComp->DisableTacticalSprintAbilityGameplayTag.TagName.ToWString().c_str());
		Log(L"%d", bool(SprintComp->bIsSprinting));

		Log(L"%p", EnergyComp);
		Log(L"%f", EnergyComp->CurrentEnergy);

		((void (*)(APlayerState*, APawn*)) Sarah::Offsets::ApplyCharacterCustomization)(PlayerController->PlayerState, Pawn);

		auto AthenaController = (AFortPlayerControllerAthena*)PlayerController;
		PlayerState->SeasonLevelUIDisplay = AthenaController->XPComponent->CurrentLevel;
		PlayerState->OnRep_SeasonLevelUIDisplay();
		AthenaController->XPComponent->bRegisteredWithQuestManager = true;
		AthenaController->XPComponent->OnRep_bRegisteredWithQuestManager();

		AthenaController->GetQuestManager(ESubGame::Athena)->InitializeQuestAbilities(AthenaController->Pawn);
	}

	return *Ret = Pawn;
}

EFortTeam GameMode::PickTeam(AFortGameModeAthena* GameMode, uint8_t PreferredTeam, AFortPlayerControllerAthena* Controller) {
	uint8_t ret = CurrentTeam;

	if (++PlayersOnCurTeam >= ((AFortGameStateAthena*)GameMode->GameState)->CurrentPlaylistInfo.BasePlaylist->MaxSquadSize) {
		CurrentTeam++;
		PlayersOnCurTeam = 0;
	}

	return EFortTeam(ret);
}

void Player::MakeNewCreativePlot(AFortPlayerControllerAthena* thisPtr, UFortCreativeRealEstatePlotItemDefinition* PlotType, const class FString& Locale, const class FString& Title)
{
	Log(L"MakeNewCreativePlot");
	FCreativeIslandData Info{};
	Info.IslandName = UKismetTextLibrary::Conv_StringToText(Title);
	Info.McpId = L"rtr2t426";
	Info.bIsDeleted = false;

	thisPtr->CreativeIslands.Add(Info);
	thisPtr->OnRep_CreativeIslands();
	Player::MakeNewCreativePlotOG(thisPtr, PlotType, Locale, Title);



	if (auto Portal = thisPtr->OwnedPortal)
	{
		if (auto Playset = PlotType->BasePlayset.Get())
		{
			if (Portal->LinkedVolume)
			{
				Portal->bInErrorState = false;
				Portal->bUserInitiatedLoad = true;
				if (Portal->LinkedVolume->VolumeState != ESpatialLoadingState::Ready)
				{
					Portal->LinkedVolume->VolumeState = ESpatialLoadingState::Ready;
					Portal->LinkedVolume->OnRep_VolumeState();
				}

				std::cout << "Playset: " << Playset->GetFullName() << std::endl;

				Portal->LinkedVolume->CurrentPlayset = Playset;
				Portal->LinkedVolume->bNeverAllowSaving = false;
				Portal->LinkedVolume->VolumeState = ESpatialLoadingState::Ready;
				Portal->LinkedVolume->CurrentPlayset = Playset;
				Portal->LinkedVolume->OverridePlayset = Playset;
				Portal->LinkedVolume->OnRep_VolumeState();
				Portal->LinkedVolume->OnRep_CurrentPlayset();



				bool bWorked = false;
				for (auto& Component : Portal->LinkedVolume->K2_GetComponentsByClass(UActorComponent::StaticClass()))
				{
					std::cout << "Component: " << Component->GetFullName() << std::endl;
					if (Component->IsA(UFortLevelSaveComponent::StaticClass()))
					{
						auto FortLevelSaveComponnet = (UFortLevelSaveComponent*)Component;
						FortLevelSaveComponnet->AccountIdOfOwner = ((AFortPlayerStateAthena*)thisPtr->PlayerState)->UniqueId;
						FortLevelSaveComponnet->bIsLoaded = true;
						FortLevelSaveComponnet->bLoadPlaysetFromPlot = true;
						FortLevelSaveComponnet->bAutoLoadFromRestrictedPlotDefinition = true;
						FortLevelSaveComponnet->RestrictedPlotDefinition = PlotType;
					}
				}

				//				UFortMinigameVolumeComponent
				//              UFortPropertyOverrideReplComponent


				auto PlaysetStreamComponent = (UPlaysetLevelStreamComponent*)Portal->LinkedVolume->GetComponentByClass(UPlaysetLevelStreamComponent::StaticClass());
				std::cout << "Owner: " << PlaysetStreamComponent->GetOwner()->GetFullName() << std::endl;
				PlaysetStreamComponent->CurrentPlayset = Playset;
				PlaysetStreamComponent->SetPlayset(Playset);
				PlaysetStreamComponent->ClientPlaysetData.bValid = true;
				PlaysetStreamComponent->OnRep_ClientPlaysetData();
				PlaysetStreamComponent->bAutoLoadLevel = true;
				PlaysetStreamComponent->bAutoActivate = true;


				auto LoadPlayset = (void (*)(UPlaysetLevelStreamComponent*)) (Sarah::Offsets::ImageBase + 0x725fe7c);
				LoadPlayset(PlaysetStreamComponent);

				thisPtr->CreativePlotLinkedVolume = Portal->LinkedVolume;
				thisPtr->OnRep_CreativePlotLinkedVolume();
			}
		}
		else
		{
			Log(L"Playset is null");
		}
	}
}

void GameMode::HandleStartingNewPlayer(UObject* Context, FFrame& Stack) {
	AFortPlayerControllerAthena* NewPlayer;
	Stack.StepCompiledIn(&NewPlayer);
	Stack.IncrementCode();
	auto GameMode = (AFortGameModeAthena*)Context;
	auto GameState = (AFortGameStateAthena*)GameMode->GameState;
	AFortPlayerStateAthena* PlayerState = (AFortPlayerStateAthena*)NewPlayer->PlayerState;

	PlayerState->SquadId = PlayerState->TeamIndex - 3;
	PlayerState->OnRep_SquadId();

	FGameMemberInfo Member;
	Member.MostRecentArrayReplicationKey = -1;
	Member.ReplicationID = -1;
	Member.ReplicationKey = -1;
	Member.TeamIndex = PlayerState->TeamIndex;
	Member.SquadId = PlayerState->SquadId;
	Member.MemberUniqueId = PlayerState->UniqueId;

	GameState->GameMemberInfoArray.Members.Add(Member);
	GameState->GameMemberInfoArray.MarkItemDirty(Member);


	if (!NewPlayer->MatchReport)
	{
		NewPlayer->MatchReport = reinterpret_cast<UAthenaPlayerMatchReport*>(UGameplayStatics::SpawnObject(UAthenaPlayerMatchReport::StaticClass(), NewPlayer));
	}

	if (bCreative)
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

		if (AvailablePortals.Num() > 0) {
			Portal = (AFortAthenaCreativePortal*)AvailablePortals[0];
			AvailablePortals.Remove(0);
			UsedPortals.Add(Portal);
		}

		Portal->OwningPlayer = PlayerState->UniqueId;
		Portal->OnRep_OwningPlayer();
		Portal->bPortalOpen = true;
		Portal->OnRep_PortalOpen();
		Portal->PlayersReady.Add(PlayerState->UniqueId);
		Portal->OnRep_PlayersReady();

		Portal->IslandInfo.CreatorName = PlayerState->GetPlayerName();
		Portal->IslandInfo.SupportCode = L"sarah";
		Portal->IslandInfo.Version = 1;
		Portal->OnRep_IslandInfo();

		Portal->bIsPublishedPortal = false;
		Portal->OnRep_PublishedPortal();

		Portal->bUserInitiatedLoad = true;
		Portal->bInErrorState = false;
		NewPlayer->OwnedPortal = Portal;

		Portal->LinkedVolume->bNeverAllowSaving = false;
		Portal->LinkedVolume->VolumeState = ESpatialLoadingState::Ready;
		Portal->LinkedVolume->OnRep_VolumeState();

		auto LevelSaveComponent = (UFortLevelSaveComponent*) Portal->LinkedVolume->GetComponentByClass(UFortLevelSaveComponent::StaticClass());

		UFortPlaysetItemDefinition* IslandPlayset = Utils::FindObject<UFortPlaysetItemDefinition>(L"/Game/Playsets/PID_Playset_60x60_Composed_POI_Tilted.PID_Playset_60x60_Composed_POI_Tilted");
		Portal->LinkedVolume->CurrentPlayset = IslandPlayset;
		Portal->LinkedVolume->OverridePlayset = IslandPlayset;
		Portal->LinkedVolume->OnRep_CurrentPlayset();

		if (!LevelSaveComponent->RestrictedPlotDefinition)
		{
			Log(L"Failed to get RestrictedPlotDefinition");
			LevelSaveComponent->AccountIdOfOwner = PlayerState->UniqueId;
			LevelSaveComponent->bIsLoaded = true;
			LevelSaveComponent->bLoadPlaysetFromPlot = true;
			LevelSaveComponent->bAutoLoadFromRestrictedPlotDefinition = true;
			LevelSaveComponent->RestrictedPlotDefinition = Utils::FindObject<UFortCreativeRealEstatePlotItemDefinition>(L"/Game/Playgrounds/Items/Plots/POI_Tilted_Medium.POI_Tilted_Medium");
		}

		auto LevelStreamComponent = (UPlaysetLevelStreamComponent*)Portal->LinkedVolume->GetComponentByClass(UPlaysetLevelStreamComponent::StaticClass());

		LevelStreamComponent->ClearPlayset();
		LevelStreamComponent->CurrentPlayset = IslandPlayset;
		LevelStreamComponent->SetPlayset(IslandPlayset);
		LevelStreamComponent->ClientPlaysetData.bValid = true;
		LevelStreamComponent->OnRep_ClientPlaysetData();
		LevelStreamComponent->bAutoLoadLevel = true;
		LevelStreamComponent->bAutoActivate = true;


		NewPlayer->CreativePlotLinkedVolume = Portal->LinkedVolume;
		NewPlayer->OnRep_CreativePlotLinkedVolume();


		auto LoadPlayset = (void (*)(UPlaysetLevelStreamComponent*)) (Sarah::Offsets::ImageBase + 0x725fe7c);
		LoadPlayset(LevelStreamComponent);

		for (int i = 0; i < UObject::GObjects->Num(); i++)
		{
			auto Object = UObject::GObjects->GetByIndex(i);
			if (Object && Object->IsA<UFortCreativeRealEstatePlotItemDefinition>())
			{
				Log(Object->GetWName().c_str());
			}
		}
	}

	return callOG(GameMode, Stack.CurrentNativeFunction, HandleStartingNewPlayer, NewPlayer);
}


void GameMode::OnAircraftEnteredDropZone(UObject* Context, FFrame& Stack)
{
	AFortAthenaAircraft* Aircraft;
	Stack.StepCompiledIn(&Aircraft);
	Stack.IncrementCode();
	auto GameMode = (AFortGameModeAthena*)Context;

	/*for (auto& Controller : GameMode->AliveBots)
	{
		Controller->Blackboard->SetValueAsBool(FName(L"AIEvaluator_Global_IsInBus"), true);
		Controller->Blackboard->SetValueAsEnum(FName(L"AIEvaluator_Global_GamePhaseStep"), (uint8)EAthenaGamePhaseStep::BusFlying);
		Controller->Blackboard->SetValueAsEnum(FName(L"AIEvaluator_Global_GamePhase"), (uint8)EAthenaGamePhase::SafeZones);
	}*/
	callOG(GameMode, Stack.CurrentNativeFunction, OnAircraftEnteredDropZone, Aircraft);
}

void GameMode::OnAircraftExitedDropZone(UObject* Context, FFrame& Stack)
{
	AFortAthenaAircraft* Aircraft;
	Stack.StepCompiledIn(&Aircraft);
	Stack.IncrementCode();
	auto GameMode = (AFortGameModeAthena*)Context;
	auto GameState = (AFortGameStateAthena*) GameMode->GameState;
	for (auto& Player : GameMode->AlivePlayers)
	{
		if (Player->IsInAircraft())
		{
			Player->GetAircraftComponent()->ServerAttemptAircraftJump({});
		}
	}


	GameState->GamePhase = EAthenaGamePhase::SafeZones;
	GameState->GamePhaseStep = EAthenaGamePhaseStep::StormHolding;
	GameState->OnRep_GamePhase(EAthenaGamePhase::Aircraft);


	/*for (auto& Controller : GameMode->AliveBots)
	{
		Controller->Blackboard->SetValueAsBool(FName(L"AIEvaluator_Global_IsInBus"), false);
		Controller->Blackboard->SetValueAsEnum(FName(L"AIEvaluator_Global_GamePhaseStep"), (uint8)EAthenaGamePhaseStep::StormForming);
		Controller->Blackboard->SetValueAsEnum(FName(L"AIEvaluator_Global_GamePhase"), (uint8)EAthenaGamePhase::SafeZones);
	}*/

	callOG(GameMode, Stack.CurrentNativeFunction, OnAircraftExitedDropZone, Aircraft);
}

UClass** GetGameSessionClass(AFortGameMode*, UClass** OutClass) {
	*OutClass = AFortGameSessionDedicatedAthena::StaticClass();
	return OutClass;
}

void ProcessServerTravel(AFortGameModeAthena* GameMode, FString& URL, bool bAbsolute)
{
	Log(L"ProcessServerTravel");

	auto SetMatchState = (void(*)(AGameMode*, FName)) (Sarah::Offsets::ImageBase + 0x1203ab0);
	SetMatchState(GameMode, L"LeavingMap");


	auto World = UWorld::GetWorld();
	auto Engine = UEngine::GetEngine();
	FWorldContext* Context = ((FWorldContext * (*)(UEngine*, UWorld*)) (Sarah::Offsets::ImageBase + 0xebd6a8))(Engine, World);


	FURL NextURL;
	auto FURLConstructor = (FURL * (*)(FURL*, FURL*, const TCHAR*, ETravelType)) (Sarah::Offsets::ImageBase + 0x190de58);
	FURLConstructor(&NextURL, &Context->LastURL, URL.CStr(), bAbsolute ? ETravelType::TRAVEL_Absolute : ETravelType::TRAVEL_Relative);
	FString URLMod;
	auto FURLToString = (FString&(*)(FURL*, FString&, bool)) (Sarah::Offsets::ImageBase + 0x1207b1c);
	FURLToString(&NextURL, URLMod, false);

	Log(URLMod.CStr());

	bool bSeamless = GameMode->bUseSeamlessTravel && UGameplayStatics::GetTimeSeconds(World) < 172800.0f;

	auto ProcessClientTravel = (APlayerController*(*)(AGameModeBase*, FString&, bool, bool)) (Sarah::Offsets::ImageBase + 0x86e3998);
	ProcessClientTravel(GameMode, URLMod, bSeamless, bAbsolute);

	*(FString*)(__int64(World) + 0x670) = URLMod;

	if (bSeamless)
	{
		auto SeamlessTravel = (void (*)(UWorld*, FString&, bool)) (Sarah::Offsets::ImageBase + 0x892f948);
		SeamlessTravel(World, *(FString*)(__int64(World) + 0x670), bAbsolute);
		*(FString*)(__int64(World) + 0x670) = FString(L"");
	}
}

void GameMode::Hook()
{
	Utils::ExecHook(L"/Script/Engine.GameMode.ReadyToStartMatch", ReadyToStartMatch, ReadyToStartMatchOG);
	Utils::ExecHook(L"/Script/Engine.GameModeBase.SpawnDefaultPawnFor", SpawnDefaultPawnFor);
	Utils::Hook(Sarah::Offsets::PickTeam, PickTeam, PickTeamOG);
	Utils::ExecHook(L"/Script/Engine.GameModeBase.HandleStartingNewPlayer", HandleStartingNewPlayer, HandleStartingNewPlayerOG);
	Utils::ExecHook(L"/Script/FortniteGame.FortGameModeAthena.OnAircraftEnteredDropZone", OnAircraftEnteredDropZone, OnAircraftEnteredDropZoneOG);
	Utils::ExecHook(L"/Script/FortniteGame.FortGameModeAthena.OnAircraftExitedDropZone", OnAircraftExitedDropZone, OnAircraftExitedDropZoneOG);
	Utils::Hook<AFortGameModeAthena>(uint32(0xf4), ProcessServerTravel);
	if (bGameSessions)
	{
		Utils::Hook(Sarah::Offsets::ImageBase + 0x19A2D44, GetGameSessionClass);
	}
}
