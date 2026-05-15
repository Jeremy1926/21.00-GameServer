#include "pch.h"
#include "Utils.h"
#include "Conversation.h"
#include "Inventory.h"
#include "Abilities.h"
#include "Replication.h"

UConversationParticipantComponent* Conversation::GetParticipantComponent(const FConversationParticipantEntry& Entry)
{
	return Entry.Actor ? (UConversationParticipantComponent*)Entry.Actor->GetComponentByClass(UConversationParticipantComponent::StaticClass()) : nullptr;
}

void Conversation::ServerNotifyConversationStarted(UConversationParticipantComponent* Comp, UConversationInstance* Conversation, FGameplayTag AsParticipant)
{
	AActor* Owner = Comp->GetOwner();
	if (Owner->GetLocalRole() == ENetRole::ROLE_Authority)
	{
		Comp->Auth_CurrentConversation = Conversation;
		Comp->Auth_Conversations.Add(Conversation);

		//@TODO: CONVERSATION: ClientUpdateParticipants, we need to do this immediately so when we tell the client a task has been
		// executed, the client has knowledge of what participants, before any client side task effects need to execute.
		Comp->ClientUpdateParticipants(Comp->Auth_CurrentConversation->Participants);

		if (Comp->ConversationsActive == 0)
		{
			//OnEnterConversationState(); // this is just null in ue
		}

		Comp->ConversationsActive++;

		//Comp->OnServerConversationStarted(Conversation, AsParticipant); // also null in ue
		Comp->ClientStartConversation(AsParticipant);

		if (Owner->GetRemoteRole() == ENetRole::ROLE_AutonomousProxy)
		{
			Comp->ClientUpdateConversations(Comp->ConversationsActive);
		}
	}
}

void Conversation::ServerNotifyConversationEnded(UConversationParticipantComponent *Comp, UConversationInstance* Conversation)
{
	AActor* Owner = Comp->GetOwner();
	if (Owner->GetLocalRole() == ENetRole::ROLE_Authority)
	{
		if (Comp->Auth_CurrentConversation == Conversation)
		{
			Comp->Auth_CurrentConversation = nullptr;
			auto ConversationIdx = Comp->Auth_Conversations.SearchIndex([&](UConversationInstance*& Instance) {
				return Instance == Conversation;
			});
			Comp->Auth_Conversations.Remove(ConversationIdx);

			Comp->ConversationsActive--;

			//Comp->OnServerConversationEnded(Conversation); // also null in ue

			if (Owner->GetRemoteRole() == ENetRole::ROLE_AutonomousProxy)
			{
				Comp->ClientUpdateConversations(Comp->ConversationsActive);
			}

			if (Comp->ConversationsActive == 0)
			{
				//Comp->OnLeaveConversationState(); // also null in ue
			}
		}
	}
}

void Conversation::ServerRemoveParticipant(UConversationInstance* Instance, FGameplayTag ParticipantID)
{
	for (int i = 0; i < Instance->Participants.List.Num(); i++)
	{
		auto& Participant = Instance->Participants.List[i];
		if (Participant.ParticipantID.TagName == ParticipantID.TagName)
		{
			if (UConversationParticipantComponent* OldParticipant = GetParticipantComponent(Participant))
			{
				if (Instance->bConversationStarted)
				{
					ServerNotifyConversationEnded(OldParticipant, Instance);
				}
			}
			Instance->Participants.List.Remove(i);
			break;
		}
	}
}

void Conversation::ServerAssignParticipant(UConversationInstance* Instance, FGameplayTag ParticipantID, AActor* ParticipantActor)
{
	if (ParticipantID.TagName.ComparisonIndex <= 0 || (ParticipantActor == nullptr))
	{
		return;
	}

	ServerRemoveParticipant(Instance, ParticipantID);

	FConversationParticipantEntry NewEntry;
	NewEntry.ParticipantID = ParticipantID;
	NewEntry.Actor = ParticipantActor;
	Instance->Participants.List.Add(NewEntry);

	if (Instance->bConversationStarted)
	{
		if (UConversationParticipantComponent* ParticipantComponent = GetParticipantComponent(NewEntry))
		{
			ServerNotifyConversationStarted(ParticipantComponent, Instance, ParticipantID);
		}
	}
}

void Conversation::Helpers::MakeConversationParticipant(UObject *, FFrame& Stack)
{
	auto& ConversationContext = Stack.StepCompiledInRef<FConversationContext>();
	AActor* ParticipantActor;
	FGameplayTag ParticipantTag;
	Stack.StepCompiledIn(&ParticipantActor);
	Stack.StepCompiledIn(&ParticipantTag);
	Stack.IncrementCode();
	if (!ParticipantActor || ParticipantTag.TagName.ComparisonIndex <= 0) 
		return;

	if (UConversationInstance* Conversation = UConversationContextHelpers::GetConversationInstance(ConversationContext))
	{
		ServerAssignParticipant(Conversation, ParticipantTag, ParticipantActor);
	}
}


void Conversation::Helpers::AdvanceConversation(UObject*, FFrame& Stack, FConversationTaskResult* Ret)
{
	auto& ConversationContext = Stack.StepCompiledInRef<FConversationContext>();
	Stack.IncrementCode();

	FConversationTaskResult OutResult;
	OutResult.Type = EConversationTaskResultType::AdvanceConversation;
	OutResult.AdvanceToChoice = FAdvanceConversationRequest();
	OutResult.Message = FClientConversationMessage();
	*Ret = OutResult;
}

void Conversation::Helpers::AdvanceConversationWithChoice(UObject*, FFrame& Stack, FConversationTaskResult* Ret)
{
	auto& ConversationContext = Stack.StepCompiledInRef<FConversationContext>();
	auto& Choice = Stack.StepCompiledInRef<FAdvanceConversationRequest>();
	Stack.IncrementCode();

	FConversationTaskResult OutResult;
	OutResult.Type = EConversationTaskResultType::AdvanceConversationWithChoice;
	OutResult.AdvanceToChoice = Choice;
	OutResult.Message = FClientConversationMessage();
	*Ret = OutResult;
}

void Conversation::Helpers::PauseConversationAndSendClientChoices(UObject*, FFrame& Stack, FConversationTaskResult* Ret)
{
	auto& ConversationContext = Stack.StepCompiledInRef<FConversationContext>();
	auto& Message = Stack.StepCompiledInRef<FClientConversationMessage>();
	Stack.IncrementCode();

	FConversationTaskResult OutResult;
	OutResult.Type = EConversationTaskResultType::PauseConversationAndSendClientChoices;
	OutResult.AdvanceToChoice = FAdvanceConversationRequest();
	OutResult.Message = Message;
	*Ret = OutResult;
}

void Conversation::Helpers::ReturnToLastClientChoice(UObject*, FFrame& Stack, FConversationTaskResult* Ret)
{
	auto& ConversationContext = Stack.StepCompiledInRef<FConversationContext>();
	Stack.IncrementCode();

	FConversationTaskResult OutResult;
	OutResult.Type = EConversationTaskResultType::ReturnToLastClientChoice;
	OutResult.AdvanceToChoice = FAdvanceConversationRequest();
	OutResult.Message = FClientConversationMessage();
	*Ret = OutResult;
}

void Conversation::Helpers::ReturnToCurrentClientChoice(UObject*, FFrame& Stack, FConversationTaskResult* Ret)
{
	auto& ConversationContext = Stack.StepCompiledInRef<FConversationContext>();
	Stack.IncrementCode();

	FConversationTaskResult OutResult;
	OutResult.Type = EConversationTaskResultType::ReturnToCurrentClientChoice;
	OutResult.AdvanceToChoice = FAdvanceConversationRequest();
	OutResult.Message = FClientConversationMessage();
	*Ret = OutResult;
}

void Conversation::Helpers::ReturnToConversationStart(UObject*, FFrame& Stack, FConversationTaskResult* Ret)
{
	auto& ConversationContext = Stack.StepCompiledInRef<FConversationContext>();
	Stack.IncrementCode();

	FConversationTaskResult OutResult;
	OutResult.Type = EConversationTaskResultType::ReturnToConversationStart;
	OutResult.AdvanceToChoice = FAdvanceConversationRequest();
	OutResult.Message = FClientConversationMessage();
	*Ret = OutResult;
}

void Conversation::Helpers::AbortConversation(UObject*, FFrame& Stack, FConversationTaskResult* Ret)
{
	auto& ConversationContext = Stack.StepCompiledInRef<FConversationContext>();
	Stack.IncrementCode();

	FConversationTaskResult OutResult;
	OutResult.Type = EConversationTaskResultType::AbortConversation;
	OutResult.AdvanceToChoice = FAdvanceConversationRequest();
	OutResult.Message = FClientConversationMessage();
	*Ret = OutResult;
}

TArray<FGuid> Conversation::GetEntryPointGUIDs(UConversationRegistry* Registry, FGameplayTag EntryPoint)
{
	TArray<FGuid> EntryPointGUIDs;

	if (const auto& FoundGUIDs = Registry->EntryTagToEntryList.Search([&](FGameplayTag& Tag, TArray<FGuid>&) {
		return Tag.TagName == EntryPoint.TagName;
		}))
	{
		EntryPointGUIDs = *FoundGUIDs;
	}

	return EntryPointGUIDs;
}

TArray<FGuid> Conversation::GetOutputLinkGUIDs(UConversationRegistry* Registry, TArray<FGuid>& SourceGUIDs)
{
	void (*BuildDependenciesGraph)(UConversationRegistry*) = decltype(BuildDependenciesGraph)(Sarah::Offsets::BuildDependenciesGraph);
	UConversationDatabase* (*GetConversationFromNodeGUID)(UConversationRegistry*, FGuid&) = decltype(GetConversationFromNodeGUID)(Sarah::Offsets::GetConversationFromNodeGUID);
	
	BuildDependenciesGraph(Registry);

	TArray<FGuid> Result;

	for (FGuid& SourceGUID : SourceGUIDs)
	{
		if (UConversationDatabase* SourceConversation = GetConversationFromNodeGUID(Registry, SourceGUID))
		{
			UConversationNode** SourceNode = SourceConversation->ReachableNodeMap.Search([&](FGuid& Guid, UConversationNode*&) {
				return Guid == SourceGUID;
			});

			if (SourceNode)
			{
				if (UConversationNodeWithLinks* SourceNodeWithLinks = (*SourceNode)->Cast<UConversationNodeWithLinks>())
				{
					for (FGuid& Connection : SourceNodeWithLinks->OutputConnections)
					{
						Result.Add(Connection);
					}
				}
			}
		}
	}

	return Result;
}

TArray<FGuid> Conversation::GetOutputLinkGUIDs(UConversationRegistry* Registry, FGameplayTag EntryPoint)
{
	TArray<FGuid> SourceGUIDs = GetEntryPointGUIDs(Registry, EntryPoint);
	return GetOutputLinkGUIDs(Registry, SourceGUIDs);
}

void Conversation::ResetConversationProgress(UConversationInstance* Instance)
{
	Instance->StartingEntryGameplayTag = FGameplayTag();
	Instance->StartingBranchPoint = FConversationBranchPoint();
	Instance->CurrentBranchPoint = FConversationBranchPoint();
	Instance->CurrentBranchPoints.ResetNum();
	Instance->ClientBranchPoints.ResetNum();
	Instance->CurrentUserChoices.ResetNum();
}

void Conversation::ServerAbortConversation(UConversationInstance* Instance)
{
	if (!Instance)
		return;

	if (Instance->bConversationStarted)
	{
		TArray<FConversationParticipantEntry> ListCopy = Instance->Participants.List;
		for (const FConversationParticipantEntry& ParticipantEntry : ListCopy)
		{
			ServerRemoveParticipant(Instance, ParticipantEntry.ParticipantID);
		}
	}

	ResetConversationProgress(Instance);

	Instance->bConversationStarted = false;
}

FConversationContext Conversation::CreateServerContext(UConversationInstance* InActiveConversation, UConversationTaskNode* InTaskBeingConsidered)
{
	FConversationContext Context;
	Context.ActiveConversation = InActiveConversation;
	Context.TaskBeingConsidered = InTaskBeingConsidered;
	Context.bServer = true;
	UConversationRegistry* (*GetConversationRegistryFromWorld)(UWorld*) = decltype(GetConversationRegistryFromWorld)(Sarah::Offsets::GetConversationRegistryFromWorld);
	Context.ConversationRegistry = GetConversationRegistryFromWorld(UWorld::GetWorld());

	return Context;
}

TArray<FGuid> Conversation::DetermineBranches(UConversationInstance* Instance, const TArray<FGuid>& SourceList, EConversationRequirementResult MaximumRequirementResult)
{
	UConversationNode* (*GetRuntimeNodeFromGUID)(UConversationRegistry*, FGuid&) = decltype(GetRuntimeNodeFromGUID)(Sarah::Offsets::GetRuntimeNodeFromGUID);
	UConversationRegistry* (*GetConversationRegistryFromWorld)(UWorld*) = decltype(GetConversationRegistryFromWorld)(Sarah::Offsets::GetConversationRegistryFromWorld);
	EConversationRequirementResult(*CheckRequirements)(UConversationTaskNode*, FConversationContext&) = decltype(CheckRequirements)(Sarah::Offsets::CheckRequirements);
	FConversationContext Context = CreateServerContext(Instance, nullptr);

	TArray<FGuid> EnabledPaths;
	for (FGuid& TestGUID : SourceList)
	{
		UConversationNode* TestNode = GetRuntimeNodeFromGUID(GetConversationRegistryFromWorld(UWorld::GetWorld()), TestGUID);
		if (UConversationTaskNode* TaskNode = TestNode->Cast<UConversationTaskNode>())
		{
			const EConversationRequirementResult RequirementResult = CheckRequirements(TaskNode, Context);

			if ((int64)RequirementResult <= (int64)MaximumRequirementResult)
			{
				EnabledPaths.Add(TestGUID);
			}
		}
	}

	return EnabledPaths;
}

UConversationNode* Conversation::TryToResolve(FConversationContext& Context, FGuid& NodeGUID)
{
	UConversationNode* (*GetRuntimeNodeFromGUID)(UConversationRegistry*, FGuid&) = decltype(GetRuntimeNodeFromGUID)(Sarah::Offsets::GetRuntimeNodeFromGUID);
	return GetRuntimeNodeFromGUID(Context.ConversationRegistry, NodeGUID);
}

void Conversation::SetNextChoices(UConversationInstance *Instance, TArray<FConversationBranchPoint>& InAllChoices)
{
	Instance->CurrentUserChoices.ResetNum();
	Instance->CurrentBranchPoints = InAllChoices;

	for (const FConversationBranchPoint& UserBranchPoint : Instance->CurrentBranchPoints)
	{
		if (UserBranchPoint.ClientChoice.ChoiceType != EConversationChoiceType::ServerOnly)
		{
			Instance->CurrentUserChoices.Add(UserBranchPoint.ClientChoice);
		}
	}

	if (Instance->CurrentBranchPoints.Num() > 0 || Instance->ScopeStack.Num() > 0)
	{
		if (Instance->CurrentUserChoices.Num() == 0)
		{
			FClientConversationOptionEntry DefaultChoice;
			DefaultChoice.ChoiceReference = FConversationChoiceReference();
			//FText ChoiceText;
			//UKismetTextLibrary::FindTextInLocalizationTable(L"ConversationInstance", L"ConversationInstance_DefaultText", &ChoiceText);
			DefaultChoice.ChoiceText = UKismetTextLibrary::Conv_StringToText(L"Continue");
			DefaultChoice.ChoiceType = EConversationChoiceType::UserChoiceAvailable;
			Instance->CurrentUserChoices.Add(DefaultChoice);
		}
	}
}

void Conversation::GenerateChoicesForDestinations(FConversationBranchPointBuilder& BranchBuilder, FConversationContext& InContext, const TArray<FGuid>& CandidateDestinations)
{
	UConversationNode* (*GetRuntimeNodeFromGUID)(UConversationRegistry*, FGuid&) = decltype(GetRuntimeNodeFromGUID)(Sarah::Offsets::GetRuntimeNodeFromGUID);
	EConversationRequirementResult (*CheckRequirements)(UConversationTaskNode*, FConversationContext&) = decltype(CheckRequirements)(Sarah::Offsets::CheckRequirements);
	
	for (FGuid& DestinationGUID : CandidateDestinations)
	{
		if (UConversationTaskNode* DestinationTaskNode = GetRuntimeNodeFromGUID(InContext.ConversationRegistry, DestinationGUID)->Cast<UConversationTaskNode>())
		{
			TGuardValue<UObject*> Swapper(DestinationTaskNode->EvalWorldContextObj, UWorld::GetWorld());

			FConversationContext DestinationContext = InContext;
			DestinationContext.TaskBeingConsidered = DestinationTaskNode;

			const int32 StartingNumber = BranchBuilder.BranchPoints.Num();

			if (auto LinkNode = DestinationTaskNode->Cast<UConversationLinkNode>())
			{
				TArray<FGuid> PotentialStartingPoints = GetOutputLinkGUIDs(InContext.ConversationRegistry, LinkNode->RemoteEntryTag);

				if (PotentialStartingPoints.Num() > 0)
				{
					TArray<FGuid> LegalStartingPoints = DetermineBranches(InContext.ActiveConversation, PotentialStartingPoints, EConversationRequirementResult::FailedButVisible);
					InContext.ReturnScopeStack.Add(FConversationNodeHandle(LinkNode->Compiled_NodeGUID));
					GenerateChoicesForDestinations(BranchBuilder, InContext, LegalStartingPoints);
				}
			}
			else
			{
				void (*GatherChoices)(UConversationTaskNode*, FConversationBranchPointBuilder&, FConversationContext&) = decltype(GatherChoices)(DestinationTaskNode->VTable[0x5f]);
				GatherChoices(DestinationTaskNode, BranchBuilder, DestinationContext);
			}

			// If a node has no choices, but we're generating the choices, we need to have this node as 'a' choice, even if
			// it's not something we're ever sending to the client, we just need to know this is a valid path for the
			// conversation to flow.
			if (BranchBuilder.BranchPoints.Num() == StartingNumber)
			{
				const EConversationRequirementResult RequirementResult = CheckRequirements(DestinationTaskNode, InContext);

				if (RequirementResult == EConversationRequirementResult::Passed)
				{
					FClientConversationOptionEntry DefaultChoice;
					DefaultChoice.ChoiceReference.NodeReference.NodeGUID = DestinationGUID;
					DefaultChoice.ChoiceType = EConversationChoiceType::ServerOnly;
					BranchBuilder.AddChoice(DestinationContext, DefaultChoice);
				}
			}
		}
	}
}

void Conversation::UpdateNextChoices(UConversationInstance* Instance, FConversationContext& Context)
{
	TArray<FConversationBranchPoint> AllChoices;

	if (UConversationTaskNode* TaskNode = TryToResolve(Context, Instance->CurrentBranchPoint.ClientChoice.ChoiceReference.NodeReference.NodeGUID)->Cast<UConversationTaskNode>())
	{
		FConversationContext ChoiceContext = Context;
		ChoiceContext.TaskBeingConsidered = TaskNode;

		TArray<FGuid> ArrayWithGUID;
		ArrayWithGUID.Add(Instance->CurrentBranchPoint.ClientChoice.ChoiceReference.NodeReference.NodeGUID);
		const TArray<FGuid> CandidateDestinations = GetOutputLinkGUIDs(ChoiceContext.ConversationRegistry, ArrayWithGUID);

		FConversationBranchPointBuilder BranchBuilder;
		GenerateChoicesForDestinations(BranchBuilder, ChoiceContext, CandidateDestinations);
		AllChoices = BranchBuilder.BranchPoints;
	}

	SetNextChoices(Instance, AllChoices);
}

void Conversation::ApplyEffect(UObject* Context, FFrame& Stack)
{
	auto& EffectRequestContext = Stack.StepCompiledInRef<FEffectRequestContext>();
	Stack.IncrementCode();
	
	auto ControllerEffect = (UFortControllerEffect*)Context;
	auto PlayerController = (AFortPlayerController*)EffectRequestContext.EffectReceivingController;

	if (!PlayerController)
		return;

	auto PlayerState = (AFortPlayerState*)PlayerController->PlayerState;

	if (!PlayerState)
		return;

	auto Pawn = (AFortPlayerPawn*)PlayerController->Pawn;

	if (!Pawn)
		return;

	if (auto ApplyGameplayEffectBase = ControllerEffect->Cast<UFortControllerEffect_ApplyGameplayEffectBase>())
	{
		auto AbilitySystemComponent = PlayerState->AbilitySystemComponent;

		FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
		FGameplayEffectSpecHandle EffectSpecHandle = AbilitySystemComponent->MakeOutgoingSpec(ApplyGameplayEffectBase->GameplayEffect, ApplyGameplayEffectBase->EffectLevel, EffectContext);

		if (ApplyGameplayEffectBase->bSetMagnitudeFromParameter)
		{
			UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(EffectSpecHandle, ApplyGameplayEffectBase->ParameterName, ApplyGameplayEffectBase->EffectLevel);
		}

		AbilitySystemComponent->BP_ApplyGameplayEffectSpecToSelf(EffectSpecHandle);
	}
	else if (auto GiveResource = ControllerEffect->Cast<UFortControllerEffect_GiveResource>())
	{
		int32 Quantity = int32(Utils::EvaluateScalableFloat(GiveResource->Quantity));

		UFortWorldItem* GivenItem = Inventory::GiveItem(PlayerController, GiveResource->ResourceItemDefinition, Quantity);
		return Inventory::TriggerInventoryUpdate(PlayerController, GivenItem ? &GivenItem->ItemEntry : nullptr);
	}
	
	if (auto HealPlayer = ControllerEffect->Cast<UFortControllerEffect_HealPlayer>())
	{
		float HealthToRestore = Utils::EvaluateScalableFloat(HealPlayer->HealthRestoreQuantity);

		float CurrentHealth = Pawn->GetHealth();

		Pawn->SetHealth(CurrentHealth + HealthToRestore);
	}
	else if (auto ShieldPlayer = ControllerEffect->Cast<UFortControllerEffect_ShieldPlayer>())
	{
		float ShieldToRestore = Utils::EvaluateScalableFloat(ShieldPlayer->ShieldRestoreQuantity);

		float CurrentShield = Pawn->GetHealth();

		Pawn->SetHealth(CurrentShield + ShieldToRestore);
	}
}

FConversationTaskResult Conversation::ExecuteTaskNode(UConversationTaskNode* TaskNode, FConversationContext& InContext)
{
	Log(L"ExecuteTaskNode TaskNode->Class: %s", TaskNode->Class->GetFullWName().c_str());

	Params::ConversationTaskNode_ExecuteTaskNode ExecuteTaskNode_Params{};
	ExecuteTaskNode_Params.Context = std::move(InContext);

	TaskNode->ProcessEvent(TaskNode->Class->FindFunction("ExecuteTaskNode"), &ExecuteTaskNode_Params);

	FConversationTaskResult Result = ExecuteTaskNode_Params.ReturnValue;

	if (auto SpeechTaskNode = TaskNode->Cast<UFortConversationTaskNode_Speech>())
	{
		FContextualMessageQueryResult OutResult;

		if (Conversation::ResolveMessageForContext(SpeechTaskNode, InContext.ActiveConversation->Participants.List[0].ParticipantID, InContext, &OutResult))
		{
			Result.Message.Text = OutResult.Message;
			Result.Message.MetadataParameters.Append(OutResult.MetadataParameters);
		}
	}
	else if (auto UpgradeItem = TaskNode->Cast<UFortConversationTaskNode_UpgradeItem>())
	{
		auto PlayerController = (AFortPlayerController*)InContext.ActiveConversation->Participants.List[1].Actor;
		auto ItemDef = PlayerController->MyFortPawn->CurrentWeapon->WeaponData;

		auto UpgradedWeaponDef = UFortKismetLibrary::GetUpgradedWeaponItemVerticalToRarity(ItemDef, EFortRarity(uint8(ItemDef->Rarity) + 1) /* crazy ass cast */);

		if (UpgradedWeaponDef && UpgradedWeaponDef != ItemDef)
		{
			auto ItemEntry = PlayerController->WorldInventory->Inventory.ReplicatedEntries.Search([&](FFortItemEntry& Entry)
				{ return Entry.ItemGuid == PlayerController->MyFortPawn->CurrentWeapon->ItemEntryGuid; });
			auto NewItemEntry = Inventory::MakeItemEntry(UpgradedWeaponDef, 1, 0);
			NewItemEntry->LoadedAmmo = ItemEntry->LoadedAmmo;
			Inventory::Remove(PlayerController, ItemEntry->ItemGuid);
			Inventory::GiveItem(PlayerController, *NewItemEntry);
		}
	}
	else if (auto DataDrivenService = TaskNode->Cast<UFortConversationTaskNode_DataDrivenService>())
	{
		auto PlayerController = (AFortPlayerController*)InContext.ActiveConversation->Participants.List[1].Actor;
		auto OtherActor = InContext.ActiveConversation->Participants.List[0].Actor;

		for (const auto& EffectRecipientConfig : DataDrivenService->EffectRecipientConfigs)
		{
			if (EffectRecipientConfig.Recipient != EDataDrivenEffectRecipient::Player)
				continue;

			FEffectRequestContext EffectRequestContext{};
			EffectRequestContext.EffectReceivingController = PlayerController;
			EffectRequestContext.EffectReceivingActor = PlayerController->Pawn;
			EffectRequestContext.InstigatingActor = OtherActor;

			for (const auto& Effect : EffectRecipientConfig.Effects)
			{
				if (!Effect)
					continue;

				Effect->ApplyEffect(EffectRequestContext);
			}
		}

		if (DataDrivenService->bPricingEnabled)
		{
			float Price;

			if (DataDrivenService->GetServicePrice(InContext, &Price))
			{
				UFortWorldItemDefinition* ResourceCurrency = DataDrivenService->ResourceCurrency;

				if (ResourceCurrency)
				{
					UFortKismetLibrary::K2_RemoveItemFromPlayer(PlayerController, ResourceCurrency, FGuid(), int32(Price), true);
				}
			}
		}
	}
	else if (auto GrantPlayerBounty = TaskNode->Cast<UFortConversationTaskNode_GrantPlayerBounty>())
	{
		auto PlayerController = (AFortPlayerControllerAthena*)InContext.ActiveConversation->Participants.List[1].Actor;
		auto PlayerState = PlayerController ? (AFortPlayerStateAthena*)PlayerController->PlayerState : nullptr;
		
		if (!PlayerController || !PlayerState)
			return Result;

		auto TransientQuestsComponent = PlayerController->TransientQuestsComponent;

		if (!TransientQuestsComponent)
			return Result;

		auto HunterQuest = GrantPlayerBounty->QuestToGrantPtr.Get()->Cast<UFortUrgentQuestItemDefinition>();
		auto TargetQuest = GrantPlayerBounty->TargetQuestToGrantPtr.Get();
		auto TeammateQuest = GrantPlayerBounty->ProtectorQuestToGrantPtr.Get();
		auto PoachedQuest = GrantPlayerBounty->PoachedQuestToGrantPtr.Get();

		TArray<AFortPlayerControllerAthena*> AllPossiblePlayers = UFortKismetLibrary::GetAllFortPlayerControllers(UWorld::GetWorld(), true, false);

		AllPossiblePlayers.Remove(PlayerController);

		if (AllPossiblePlayers.Num() == 0)
			return Result;

		float NumberOfPlayersToSelectFrom = Utils::EvaluateScalableFloat(GrantPlayerBounty->NumberOfPlayersToSelectFrom);

		int32 RandomPlayerIdx = UKismetMathLibrary::RandomIntegerInRange(0, int32(NumberOfPlayersToSelectFrom - 1) <= AllPossiblePlayers.Num() - 1 ? int32(NumberOfPlayersToSelectFrom - 1) : AllPossiblePlayers.Num() - 1);

		Log(L"RandomPlayerIdx: %i", RandomPlayerIdx);
		Log(L"AllPossiblePlayers.Num(): %i", AllPossiblePlayers.Num());
		
		auto TargetPlayerController = AllPossiblePlayers[RandomPlayerIdx]; // todo: find proper way to get the target pc
		auto TargetPlayerState = TargetPlayerController ? (AFortPlayerStateAthena*)TargetPlayerController->PlayerState : nullptr;

		if (!TargetPlayerController || !TargetPlayerState)
			return Result;

		FUrgentQuestData UrgentQuestData = HunterQuest->GetUrgentQuestData();
		UrgentQuestData.EventSubtitleSecondary = FText();
		UrgentQuestData.AcceptingPlayer = PlayerState;
		UrgentQuestData.DisplayPlayer = TargetPlayerState;
		UrgentQuestData.EventStartTime = UKismetMathLibrary::Now();
		UrgentQuestData.SocialAvatarBrushPtr = TargetPlayerState->GetSocialAvatarBrush(true);
		UrgentQuestData.TotalEventTime = 360.f;
		UrgentQuestData.bShowBountyThreatInformation = true;
		UrgentQuestData.bShowBountyPriceAndDistance = true;
		UrgentQuestData.bIsSharedUrgentQuest = true;

		if (PlayerState->PlayerTeam)
		{
			for (const auto& Controller : PlayerState->PlayerTeam->TeamMembers)
			{
				if (!Controller)
					continue;

				auto TeamMemberPlayerController = Controller->Cast<AFortPlayerControllerAthena>(); // rule out playerbots

				if (!TeamMemberPlayerController)
					continue;

				auto TeamMemberTransientQuestsComponent = TeamMemberPlayerController->TransientQuestsComponent;

				if (!TeamMemberTransientQuestsComponent)
					continue;

				auto TeamMemberPlayerState = (AFortPlayerStateAthena*)Controller->PlayerState;

				if (!TeamMemberPlayerState || !TeamMemberPlayerState->AbilitySystemComponent)
					continue;

				bool bIsHuntingPlayer = Controller == PlayerController;

				TeamMemberTransientQuestsComponent->TrackedBountyHunters.Add(TeamMemberPlayerState);

				TeamMemberTransientQuestsComponent->TrackedPrimaryHunter = PlayerState;
				TeamMemberTransientQuestsComponent->TrackedHunterBountyTarget = TargetPlayerState;

				TeamMemberTransientQuestsComponent->ProtectorQuestToGrant = TeammateQuest;
				TeamMemberTransientQuestsComponent->TargetQuestToGrant = TargetQuest;

				TeamMemberTransientQuestsComponent->TrackedHunterBountyTargetDistance = (int32) TargetPlayerController->Pawn->GetDistanceTo(PlayerController->Pawn);
				TeamMemberTransientQuestsComponent->OnRep_TrackedHunterBountyTargetDistance();

				TeamMemberTransientQuestsComponent->TrackedHunterBountyTargetPrice = (int32) Utils::EvaluateScalableFloat(GrantPlayerBounty->Price);

				TeamMemberTransientQuestsComponent->ActiveUrgentQuests.Add(UrgentQuestData);

				if (HunterQuest->bGrantTransientQuestToSquad || bIsHuntingPlayer)
				{
					TeamMemberTransientQuestsComponent->GrantTransientQuest(HunterQuest, PoachedQuest /* maybe just nullptr? */);
					TeamMemberTransientQuestsComponent->ClientGrantTransientQuest(HunterQuest, PoachedQuest /* maybe just nullptr? */);
				}

				TeamMemberTransientQuestsComponent->ClientSetBountyHunterNPCIcon(PlayerState->GetSocialAvatarBrush(true));
				TeamMemberTransientQuestsComponent->ClientSetBountyTargetNPCIcon(UrgentQuestData.SocialAvatarBrushPtr);

				auto GivenSpecs = Abilities::GiveAbilitySet(TeamMemberPlayerState->AbilitySystemComponent, HunterQuest->QuestAbilitySet.Get());

				for (const auto& GivenSpec : GivenSpecs)
				{
					TeamMemberPlayerState->AbilitySystemComponent->ServerTryActivateAbility(GivenSpec.Handle, GivenSpec.InputPressed, GivenSpec.ActivationInfo.PredictionKeyWhenActivated);

					UFortPlayerBountyGameplayAbility* BountyAbility = GivenSpec.ReplicatedInstances[0]->Cast<UFortPlayerBountyGameplayAbility>();

					if (!BountyAbility)
						continue;

					BountyAbility->PoachedBountyQuestPtr = TSoftObjectPtr<UFortQuestItemDefinition_Athena>(GrantPlayerBounty->PoachedQuestToGrantPtr.Get());
					BountyAbility->StartUrgentQuestEvent(UrgentQuestData);

					TeamMemberTransientQuestsComponent->ClientBroadcastOnUrgentQuestStarted(UrgentQuestData, 360.f);
				}
			}
		}

		if (TargetPlayerState->PlayerTeam)
		{
			for (const auto& TargetTeamController : TargetPlayerState->PlayerTeam->TeamMembers)
			{
				if (!TargetTeamController)
					continue;

				auto TargetTeamMemberPlayerController = TargetTeamController->Cast<AFortPlayerControllerAthena>();

				if (!TargetTeamMemberPlayerController)
					continue;

				auto TargetTeamMemberTransientQuestsComponent = TargetTeamMemberPlayerController->TransientQuestsComponent;

				if (!TargetTeamMemberTransientQuestsComponent)
					continue;

				auto TargetTeamMemberPlayerState = (AFortPlayerStateAthena*)TargetTeamController->PlayerState;

				if (!TargetTeamMemberPlayerState || !TargetTeamMemberPlayerState->AbilitySystemComponent)
					continue;

				bool bIsTargetedPlayer = TargetTeamController == TargetPlayerController;

				TargetTeamMemberTransientQuestsComponent->TrackedPrimaryHunter = PlayerState;
				TargetTeamMemberTransientQuestsComponent->TrackedHunterBountyTarget = TargetPlayerState;

				TargetTeamMemberTransientQuestsComponent->ProtectorQuestToGrant = TeammateQuest;
				TargetTeamMemberTransientQuestsComponent->TargetQuestToGrant = TargetQuest;

				TargetTeamMemberTransientQuestsComponent->TrackedHunterBountyTargetDistance = (int32) TargetPlayerController->Pawn->GetDistanceTo(PlayerController->Pawn);
				TargetTeamMemberTransientQuestsComponent->OnRep_TrackedHunterBountyTargetDistance();

				TargetTeamMemberTransientQuestsComponent->TrackedHunterBountyTargetPrice = (int32) Utils::EvaluateScalableFloat(GrantPlayerBounty->Price);
				
				TargetTeamMemberTransientQuestsComponent->ActiveUrgentQuests.Add(UrgentQuestData);

				if ((bIsTargetedPlayer ? TargetQuest->bGrantTransientQuestToSquad : TeammateQuest->bGrantTransientQuestToSquad) || bIsTargetedPlayer)
				{
					TargetTeamMemberTransientQuestsComponent->GrantTransientQuest(bIsTargetedPlayer ? TargetQuest : TeammateQuest, PoachedQuest /* maybe just nullptr? */);
					TargetTeamMemberTransientQuestsComponent->ClientGrantTransientQuest(bIsTargetedPlayer ? TargetQuest : TeammateQuest, PoachedQuest /* maybe just nullptr? */);
				}

				TargetTeamMemberTransientQuestsComponent->ClientSetBountyHunterNPCIcon(PlayerState->GetSocialAvatarBrush(true));
				TargetTeamMemberTransientQuestsComponent->ClientSetBountyTargetNPCIcon(UrgentQuestData.SocialAvatarBrushPtr);

				auto GivenSpecs = Abilities::GiveAbilitySet(TargetTeamMemberPlayerState->AbilitySystemComponent, bIsTargetedPlayer ? TargetQuest->QuestAbilitySet.Get() : TeammateQuest->QuestAbilitySet.Get());

				for (const auto& GivenSpec : GivenSpecs)
				{
					TargetTeamMemberPlayerState->AbilitySystemComponent->ServerTryActivateAbility(GivenSpec.Handle, GivenSpec.InputPressed, GivenSpec.ActivationInfo.PredictionKeyWhenActivated);

					UFortPlayerBountyGameplayAbility* BountyAbility = GivenSpec.ReplicatedInstances[0]->Cast<UFortPlayerBountyGameplayAbility>();

					if (!BountyAbility)
						continue;

					BountyAbility->PoachedBountyQuestPtr = TSoftObjectPtr<UFortQuestItemDefinition_Athena>(GrantPlayerBounty->PoachedQuestToGrantPtr.Get());
					BountyAbility->StartUrgentQuestEvent(UrgentQuestData);

					TargetTeamMemberTransientQuestsComponent->ClientBroadcastOnUrgentQuestStarted(UrgentQuestData, 360.f);
				}
			}
		}

		ABuildingGameplayActorTransientQuest* BountyBoard = InContext.ActiveConversation->Participants.List[0].Actor->Cast<ABuildingGameplayActorTransientQuest>();

		if (BountyBoard)
		{
			BountyBoard->NumberOfPlayersToSelectFrom = GrantPlayerBounty->NumberOfPlayersToSelectFrom;
			
			for (const auto& TrackedBountyHunter : TransientQuestsComponent->TrackedBountyHunters)
			{
				BountyBoard->PlayersGrantedBounty.Add(TrackedBountyHunter);
			}

			BountyBoard->OnBountyStarted(BountyBoard->PlayersGrantedBounty);
		}
	}

	return Result;
}

FConversationTaskResult Conversation::ExecuteTaskNodeWithSideEffects(UConversationTaskNode* Node, FConversationContext& InContext)
{
	TGuardValue<UObject*> Swapper(Node->EvalWorldContextObj, UWorld::GetWorld());

	auto Result = ExecuteTaskNode(Node, InContext);

	// After executing the task we need to determine if we should run side effects on the server and client.
	if (Result.Type != EConversationTaskResultType::Invalid && Result.Type != EConversationTaskResultType::AbortConversation)
	{
		for (UConversationNode* SubNode : Node->SubNodes)
		{
			if (UConversationSideEffectNode* SideEffectNode = SubNode->Cast<UConversationSideEffectNode>())
			{
				SideEffectNode->ServerCauseSideEffect(InContext);
			}
		}

		FConversationParticipants Participants = InContext.ActiveConversation->Participants;
		for (const FConversationParticipantEntry& ParticipantEntry : Participants.List)
		{
			if (UConversationParticipantComponent* Component = GetParticipantComponent(ParticipantEntry))
			{
				// Notify each client in the conversation
				if (Component->GetOwner()->GetRemoteRole() == ENetRole::ROLE_AutonomousProxy)
				{
					if (Component->GetOwner()->GetLocalRole() == ENetRole::ROLE_Authority)
					{
						Component->ClientExecuteTaskAndSideEffects(InContext.ActiveConversation->CurrentBranchPoint.ClientChoice.ChoiceReference.NodeReference);
					}
				}
			}
		}
	}

	return Result;
}

void Conversation::ModifyCurrentConversationNode(UConversationInstance* Instance, FConversationBranchPoint& NewBranchPoint)
{
	Instance->CurrentBranchPoint = NewBranchPoint;

	for (auto& Handle : NewBranchPoint.ReturnScopeStack)
	{
		FConversationChoiceReference Ref;
		Ref.NodeReference = Handle;
		Instance->ScopeStack.Add(Ref);
	}

	OnCurrentConversationNodeModified(Instance);
}


void Conversation::ModifyCurrentConversationNode(UConversationInstance* Instance, const FConversationChoiceReference& NewChoice)
{
	FConversationBranchPoint BranchPoint;
	BranchPoint.ClientChoice.ChoiceReference = NewChoice;

	ModifyCurrentConversationNode(Instance, BranchPoint);
}

void Conversation::PauseConversationAndSendClientChoices(UConversationInstance* Instance, FConversationContext& Context, FClientConversationMessage& ClientMessage)
{
	FClientConversationMessagePayload LastMessage = FClientConversationMessagePayload();
	LastMessage.Message = ClientMessage;
	LastMessage.Options = Instance->CurrentUserChoices;
	LastMessage.CurrentNode = Context.ActiveConversation->CurrentBranchPoint.ClientChoice.ChoiceReference.NodeReference;
	LastMessage.Participants = Instance->Participants;

	Instance->ClientBranchPoints.Add(FCheckpoint(Instance->CurrentBranchPoint, Instance->ScopeStack));

	for (FConversationParticipantEntry& KVP : LastMessage.Participants.List)
	{
		if (UConversationParticipantComponent* ParticipantComponent = GetParticipantComponent(KVP))
		{
			ParticipantComponent->LastMessage = LastMessage;

			if (ParticipantComponent->GetOwner()->GetRemoteRole() == ENetRole::ROLE_AutonomousProxy)
				ParticipantComponent->ClientUpdateConversation(LastMessage);
		}
	}
}

void Conversation::ReturnToLastClientChoice(UConversationInstance* Instance, FConversationContext& Context)
{
	if (Instance->ClientBranchPoints.Num() > 1)
	{
		Instance->ClientBranchPoints.Remove(Instance->ClientBranchPoints.Num() - 1);

		FCheckpoint& Checkpoint = Instance->ClientBranchPoints[Instance->ClientBranchPoints.Num() - 1];
		Instance->ScopeStack = Checkpoint.ScopeStack;
		ModifyCurrentConversationNode(Instance, Checkpoint.ClientBranchPoint);
	}
}

void Conversation::ReturnToCurrentClientChoice(UConversationInstance* Instance, FConversationContext& Context)
{
	if (Instance->ClientBranchPoints.Num() > 0)
	{
		FCheckpoint Checkpoint = Instance->ClientBranchPoints[Instance->ClientBranchPoints.Num() - 1];

		// Pop after get the last checkpoint since we're about to repeat and push the same one onto the stack.
		Instance->ClientBranchPoints.Remove(Instance->ClientBranchPoints.Num() - 1);

		Instance->ScopeStack = Checkpoint.ScopeStack;
		ModifyCurrentConversationNode(Instance, Checkpoint.ClientBranchPoint);
	}
}

void Conversation::ReturnToStart(UConversationInstance* Instance, FConversationContext& Context)
{
	FGameplayTag EntryStartPointGameplayTagCache = Instance->StartingEntryGameplayTag;
	FConversationBranchPoint StartingBranchPointCache = Instance->StartingBranchPoint;

	ResetConversationProgress(Instance);

	Instance->StartingEntryGameplayTag = EntryStartPointGameplayTagCache;
	Instance->StartingBranchPoint = StartingBranchPointCache;

	ModifyCurrentConversationNode(Instance, Instance->StartingBranchPoint);
}

void Conversation::ServerAdvanceConversation(UConversationInstance* Instance, FAdvanceConversationRequest InChoicePicked)
{
	if (!Instance)
		return;

	EConversationRequirementResult (*CheckRequirements)(UConversationTaskNode*, FConversationContext&) = decltype(CheckRequirements)(Sarah::Offsets::CheckRequirements);
	
	if (Instance->bConversationStarted && UKismetGuidLibrary::IsValid_Guid(Instance->CurrentBranchPoint.ClientChoice.ChoiceReference.NodeReference.NodeGUID))
	{
		TArray<FConversationBranchPoint> CandidateDestinations;

		if (UKismetGuidLibrary::IsValid_Guid(InChoicePicked.Choice.NodeReference.NodeGUID))
		{
			FConversationBranchPoint* BranchPoint = Instance->CurrentBranchPoints.Search([&](FConversationBranchPoint& Point) {
				return Point.ClientChoice.ChoiceType == EConversationChoiceType::ServerOnly ? false : Point.ClientChoice.ChoiceReference.NodeReference.NodeGUID == InChoicePicked.Choice.NodeReference.NodeGUID;
			});

			if (BranchPoint)
			{
				CandidateDestinations.ResetNum();
				CandidateDestinations.Add(*BranchPoint);

				FConversationContext Context = CreateServerContext(Instance, nullptr);
				if (const UConversationTaskNode* TaskNode = TryToResolve(Context, BranchPoint->ClientChoice.ChoiceReference.NodeReference.NodeGUID)->Cast<UConversationTaskNode>())
				{
					for (const UConversationNode* SubNode : TaskNode->SubNodes)
					{
						if (const UConversationChoiceNode* ChoiceNode = SubNode->Cast<UConversationChoiceNode>())
						{
							//ChoiceNode->NotifyChoicePickedByUser(Context, BranchPoint->ClientChoice);
							break;
						}
					}
				}
			}
			else
			{
				ServerAbortConversation(Instance);
				return;
			}
		}
		else
		{
			if (Instance->CurrentBranchPoints.Num() == 0 && Instance->ScopeStack.Num() > 0)
			{
				ModifyCurrentConversationNode(Instance, Instance->ScopeStack[Instance->ScopeStack.Num() - 1]);
				return;
			}

			for (const FConversationBranchPoint& BranchPoint : Instance->CurrentBranchPoints)
			{
				if (BranchPoint.ClientChoice.ChoiceType != EConversationChoiceType::UserChoiceUnavailable)
				{
					CandidateDestinations.Add(BranchPoint);
				}
			}
		}

		// Double check the choices are still valid, things may have changed since the user picked the choices.
		TArray<FConversationBranchPoint> ValidDestinations;
		{
			FConversationContext Context = CreateServerContext(Instance, nullptr);
			for (FConversationBranchPoint& BranchPoint : CandidateDestinations)
			{
				if (UConversationTaskNode* TaskNode = TryToResolve(Context, BranchPoint.ClientChoice.ChoiceReference.NodeReference.NodeGUID)->Cast<UConversationTaskNode>())
				{
					EConversationRequirementResult Result = TaskNode->bIgnoreRequirementsWhileAdvancingConversations ? EConversationRequirementResult::Passed : CheckRequirements(TaskNode, Context);
					
					if (Result == EConversationRequirementResult::Passed)
					{
						ValidDestinations.Add(BranchPoint);
					}
				}
				else
				{
					ValidDestinations.Add(BranchPoint);
				}
			}
		}

		if (ValidDestinations.Num() == 0)
		{
			ServerAbortConversation(Instance);
			return;
		}
		else
		{
			const int32 StartingIndex = UKismetMathLibrary::RandomIntegerInRange(0, ValidDestinations.Num() - 1);
			FConversationBranchPoint& TargetChoice = ValidDestinations[StartingIndex];

			ModifyCurrentConversationNode(Instance, TargetChoice);
		}
	}
}

void Conversation::OnCurrentConversationNodeModified(UConversationInstance* Instance)
{
	FConversationContext AnonContext = CreateServerContext(Instance, nullptr);
	const UConversationNode* CurrentNode = TryToResolve(AnonContext, Instance->CurrentBranchPoint.ClientChoice.ChoiceReference.NodeReference.NodeGUID);
	if (UConversationTaskNode* TaskNode = CurrentNode->Cast<UConversationTaskNode>())
	{
		FConversationContext Context = AnonContext;
		Context.TaskBeingConsidered = TaskNode;

		FConversationTaskResult TaskResult = ExecuteTaskNodeWithSideEffects(TaskNode, Context);

		if (Instance->ScopeStack.Num() > 0 && Instance->ScopeStack[Instance->ScopeStack.Num() - 1].NodeReference.NodeGUID == TaskNode->Compiled_NodeGUID)
		{
			// Now that we've finally executed the Subgraph / scope modifying node, we can pop it from
			// the scope stack.
			Instance->ScopeStack.Remove(Instance->ScopeStack.Num() - 1);
		}

		// Update the next choices now that we've executed the task
		UpdateNextChoices(Instance, Context);

		if (TaskResult.Type == EConversationTaskResultType::AbortConversation)
		{
			ServerAbortConversation(Instance);
		}
		else if (TaskResult.Type == EConversationTaskResultType::AdvanceConversation)
		{
			ServerAdvanceConversation(Instance, FAdvanceConversationRequest());
		}
		else if (TaskResult.Type == EConversationTaskResultType::AdvanceConversationWithChoice)
		{
			//@TODO: CONVERSATION: We are only using the Choice here part of the request, but we need to complete
			// support UserParameters, just so we don't have unexpected differences in the system.
			ModifyCurrentConversationNode(Instance, TaskResult.AdvanceToChoice.Choice);
		}
		else if (TaskResult.Type == EConversationTaskResultType::PauseConversationAndSendClientChoices)
		{
			PauseConversationAndSendClientChoices(Instance, Context, TaskResult.Message);
		}
		else if (TaskResult.Type == EConversationTaskResultType::ReturnToLastClientChoice)
		{
			ReturnToLastClientChoice(Instance, Context);
		}
		else if (TaskResult.Type == EConversationTaskResultType::ReturnToCurrentClientChoice)
		{
			ReturnToCurrentClientChoice(Instance, Context);
		}
		else if (TaskResult.Type == EConversationTaskResultType::ReturnToConversationStart)
		{
			ReturnToStart(Instance, Context);
		}
	}
	else
	{
		ServerAbortConversation(Instance);
	}
}

void Conversation::ServerStartConversation(UConversationInstance* Instance, FGameplayTag EntryPoint)
{
	UConversationRegistry* (*GetConversationRegistryFromWorld)(UWorld*) = decltype(GetConversationRegistryFromWorld)(Sarah::Offsets::GetConversationRegistryFromWorld);
	ResetConversationProgress(Instance);
	Instance->StartingEntryGameplayTag = EntryPoint;

	UConversationRegistry* ConversationRegistry = GetConversationRegistryFromWorld(UWorld::GetWorld());

	TArray<FGuid> PotentialStartingPoints = GetOutputLinkGUIDs(ConversationRegistry, EntryPoint);
	if (PotentialStartingPoints.Num() == 0)
	{
		ServerAbortConversation(Instance);
		return;
	}
	else
	{
		TArray<FGuid> LegalStartingPoints = DetermineBranches(Instance, PotentialStartingPoints, EConversationRequirementResult::FailedButVisible);

		if (LegalStartingPoints.Num() == 0)
		{
			ServerAbortConversation(Instance);
			return;
		}
		else
		{
			//const int32 StartingIndex = Instance->ConversationRNG.RandRange(0, LegalStartingPoints.Num() - 1);
			const int32 StartingIndex = UKismetMathLibrary::RandomIntegerInRange(0, LegalStartingPoints.Num() - 1);

			FConversationBranchPoint StartingPoint;
			StartingPoint.ClientChoice.ChoiceReference.NodeReference.NodeGUID = LegalStartingPoints[StartingIndex];

			Instance->StartingBranchPoint = StartingPoint;
			Instance->CurrentBranchPoint = StartingPoint;
		}
	}

	for (FConversationParticipantEntry& ParticipantEntry : Instance->Participants.List)
	{
		if (UConversationParticipantComponent* ParticipantComponent = GetParticipantComponent(ParticipantEntry))
		{
			ServerNotifyConversationStarted(ParticipantComponent, Instance, ParticipantEntry.ParticipantID);
		}
	}

	Instance->bConversationStarted = true;

	OnCurrentConversationNodeModified(Instance);
}

UConversationInstance* Conversation::StartConversation(UObject* Context, FFrame& Stack, UConversationInstance** Ret)
{
	Log(L"a");
	FGameplayTag ConversationEntryTag;
	AActor* Instigator;
	FGameplayTag InstigatorTag;
	AActor* Target;
	FGameplayTag TargetTag;
	Stack.StepCompiledIn(&ConversationEntryTag);
	Stack.StepCompiledIn(&Instigator);
	Stack.StepCompiledIn(&InstigatorTag);
	Stack.StepCompiledIn(&Target);
	Stack.StepCompiledIn(&TargetTag);
	Stack.IncrementCode();

	if (Instigator == nullptr || Target == nullptr)
	{
		return nullptr;
	}

	if (UWorld* World = UWorld::GetWorld())
	{
		UClass* InstanceClass = UConversationSettings::GetDefaultObj()->ConversationInstanceClass;
		if (InstanceClass == nullptr)
		{
			InstanceClass = UConversationInstance::StaticClass();
		}

		UConversationInstance* ConversationInstance = (UConversationInstance*) UGameplayStatics::SpawnObject(InstanceClass, World);
		if (ConversationInstance)
		{
			FConversationContext ConversationContext = CreateServerContext(ConversationInstance, nullptr);

			UConversationContextHelpers::MakeConversationParticipant(ConversationContext, Target, TargetTag);
			UConversationContextHelpers::MakeConversationParticipant(ConversationContext, Instigator, InstigatorTag);

			ServerStartConversation(ConversationInstance, ConversationEntryTag);
		}

		return ConversationInstance;
	}

	return nullptr;
}

void Conversation::RequestServerAbortConversation(UObject* Context, FFrame& Stack)
{
	Stack.IncrementCode();
	ServerAbortConversation(((UFortPlayerConversationComponent*)Context)->Auth_CurrentConversation);
}

void Conversation::ServerAdvanceConversationHook(UObject* Context, FFrame& Stack)
{
	FAdvanceConversationRequest InChoicePicked;
	Stack.StepCompiledIn(&InChoicePicked);
	Stack.IncrementCode();
	ServerAdvanceConversation(((UConversationParticipantComponent*)Context)->Auth_CurrentConversation, InChoicePicked);
}

EConversationRequirementResult Conversation::IsRequirementSatisfied(UObject* Context, FFrame& Stack, EConversationRequirementResult* Ret)
{
	Log(L"IsRequirementSatisfied Node->Class: %s, VFTable: 0x%p", Context->Class->GetFullWName().c_str(), Context->VTable);

	auto& ConversationContext = Stack.StepCompiledInRef<FConversationContext>();
	Stack.IncrementCode();

	auto SpeakerComp =
		(UFortNonPlayerConversationParticipantComponent*)
		ConversationContext.ActiveConversation->Participants.List[0].Actor->GetComponentByClass(UFortNonPlayerConversationParticipantComponent::StaticClass());

	auto ListenerComp =
		(UFortPlayerConversationComponent*)
		ConversationContext.ActiveConversation->Participants.List[1].Actor->GetComponentByClass(UFortPlayerConversationComponent::StaticClass());

	if (SpeakerComp && ListenerComp)
	{
		if (auto UpgradeItem = Context->Cast<UFortConversationTaskNode_UpgradeItem>())
		{
			auto PlayerController = (AFortPlayerController*)ConversationContext.ActiveConversation->Participants.List[1].Actor;
			auto ItemDef = PlayerController->MyFortPawn->CurrentWeapon->WeaponData;

			auto UpgradedWeaponDef = UFortKismetLibrary::GetUpgradedWeaponItemVerticalToRarity(ItemDef, EFortRarity(uint8(ItemDef->Rarity) + 1) /* crazy ass cast */);

			if (UpgradedWeaponDef && UpgradedWeaponDef != ItemDef) // if there is an upgraded version, we pass
			{
				return *Ret = EConversationRequirementResult::Passed;
			}

			return *Ret = EConversationRequirementResult::FailedButVisible;
		}
		else if (auto GrantPlayerBounty = Context->Cast<UFortConversationTaskNode_GrantPlayerBounty>())
		{
			auto PlayerController = (AFortPlayerController*)ConversationContext.ActiveConversation->Participants.List[1].Actor;

			if (PlayerController)
			{
				TArray<AFortPlayerController*> AllPossiblePlayers = UFortKismetLibrary::GetAllFortPlayerControllers(UWorld::GetWorld(), true, false);

				AllPossiblePlayers.Remove(PlayerController);

				if (AllPossiblePlayers.Num() > 0)
				{
					return *Ret = EConversationRequirementResult::Passed;
				}
			}

			return *Ret = EConversationRequirementResult::FailedButVisible;
		}
		else if (auto HasService = Context->Cast<UFortConversationRequirement_HasService>())
		{
			Log(L"t: %s", HasService->ServiceTag.TagName.ToWString().c_str());
			for (auto& GameplayTag : SpeakerComp->SupportedServices.GameplayTags)
			{
				Log(L"s: %s", GameplayTag.TagName.ToWString().c_str());
				if (GameplayTag.TagName == HasService->ServiceTag.TagName)
				{
					return *Ret = EConversationRequirementResult::Passed;
				}
			}
			return *Ret = EConversationRequirementResult::FailedAndHidden;
		}
		else if (auto HasNoActiveQuests = Context->Cast<UFortConversationRequirement_HasNoActiveQuests>())
		{
			auto PlayerController = (AFortPlayerControllerAthena*)ConversationContext.ActiveConversation->Participants.List[1].Actor;
			
			if (PlayerController)
			{
				if (auto TransientQuestsComponent = PlayerController->TransientQuestsComponent)
				{
					if (TransientQuestsComponent->ActiveUrgentQuests.Num() == 0)
					{
						return *Ret = EConversationRequirementResult::Passed;
					}
				}
			}

			return *Ret = EConversationRequirementResult::FailedAndHidden;
		}
		else if (auto AllSlottedQuestPrerequisitesCompleted = Context->Cast<UFortConversationRequirement_AllSlottedQuestPrerequisitesCompleted>())
		{
			auto PlayerController = (AFortPlayerControllerAthena*)ConversationContext.ActiveConversation->Participants.List[1].Actor;

			if (PlayerController)
			{
				if (auto TransientQuestsComponent = PlayerController->TransientQuestsComponent)
				{
					if (TransientQuestsComponent->ActiveUrgentQuests.IsValidIndex(AllSlottedQuestPrerequisitesCompleted->QuestSlot))
					{

					}
				}
			}

			return *Ret = EConversationRequirementResult::FailedAndHidden;
		}
		else if (auto SlottedQuestNotCompletedThisMatch = Context->Cast<UFortConversationRequirement_SlottedQuestNotCompletedThisMatch>())
		{
			Log(L"unimplemented: SlottedQuestNotCompletedThisMatch");
		}
	}

	int VTableIdx = 0x0;

	if (Context->IsA<UConversationTaskNode>())
	{
		VTableIdx = 0x2E8;
	}
	else if (Context->IsA<UConversationRequirementNode>())
	{
		VTableIdx = 0x2D0;
	}

	if (VTableIdx)
	{
		static EConversationRequirementResult (*IsRequirementSatisfiedOG)(UObject*, const FConversationContext& Context) =
			decltype(IsRequirementSatisfiedOG)(Context->VTable[VTableIdx]);

		auto Out = IsRequirementSatisfiedOG(Context, ConversationContext);
		Log(L"%d", Out);
		return *Ret = Out;
	}
	else
	{
		Log(L"IsRequirementSatisfied: %s has no vft idx!", Context->Class->GetWName().c_str());
		return *Ret = EConversationRequirementResult::FailedAndHidden;
	}
}

static FConversationTaskResult ExecuteTaskNodeWithSideEffectsHook(UConversationTaskNode* TaskNode, FConversationTaskResult* Ret, FConversationContext& InContext)
{
	if (Ret)
	{
		return *Ret = Conversation::ExecuteTaskNodeWithSideEffects(TaskNode, InContext);
	}

	return Conversation::ExecuteTaskNodeWithSideEffects(TaskNode, InContext);
}

/*

void Conversation::ResolveMessageForContext(UObject* Context, FFrame& Stack, bool* Ret)
{
	UFortConversationTaskNode_Speech* SpeechTaskNode = (UFortConversationTaskNode_Speech*)Context;

	FGameplayTag SpeakerParticipantTag;
	FConversationContext ConversationContext;
	FContextualMessageQueryResult* OutResult;

	Stack.StepCompiledIn(&SpeakerParticipantTag);
	Stack.StepCompiledIn(&ConversationContext);
	OutResult = Stack.StepCompiledInRef<FContextualMessageQueryResult*>(&OutResult);
	Stack.IncrementCode();

	*/

bool Conversation::ResolveMessageForContext(UFortConversationTaskNode_Speech* SpeechTaskNode, FGameplayTag SpeakerParticipantTag, FConversationContext ConversationContext, FContextualMessageQueryResult* OutResult)
{
	auto ParticipantComponent = (UFortNonPlayerConversationParticipantComponent*)GetParticipantComponent(ConversationContext.ActiveConversation->Participants.List[0]);

	if (ParticipantComponent)
	{
		FGameplayTag& ParticipantID = ConversationContext.ActiveConversation->Participants.List[1].ParticipantID; // maybe List[1] ???

		TArray<FContextualMessageCandidate> Candidates;

		for (FContextualMessageCandidate& ContextualMessage : SpeechTaskNode->GeneralConfig.ContextualMessages)
		{
			if (ContextualMessage.ContextRequirements.Num() == 0)
			{
				Candidates.Add(ContextualMessage);
			}
			else if (ContextualMessage.RequirementMatchPolicy == EContextRequirementMatchPolicy::RequireAny)
			{
				for (FFortConversationContextRequirement& ContextRequirement : ContextualMessage.ContextRequirements)
				{
					if (ContextRequirement.ParticipantID == ParticipantID)
					{
						Candidates.Add(ContextualMessage);
						break;
					}
				}
			}
			else if (ContextualMessage.RequirementMatchPolicy == EContextRequirementMatchPolicy::RequireAll)
			{
				for (FFortConversationContextRequirement& ContextRequirement : ContextualMessage.ContextRequirements)
				{
					if (ContextRequirement.ParticipantID != ParticipantID)
					{
						break;
					}

					Candidates.Add(ContextualMessage);
				}
			}
		}

		FContextualMessageConfig* ConfigSearched = SpeechTaskNode->SpeakerEntryTagToConfig.Search([&](FGameplayTag& Tag, FContextualMessageConfig&)
			{ return Tag == ParticipantComponent->ServiceProviderIDTag; } // shouldn't we be using SpeakerParticipantTag
		);

		if (!ConfigSearched)
		{
			UContextualMessageConfigCollection* ExternalContextualMessageCollection = SpeechTaskNode->ExternalContextualMessageCollection;

			if (ExternalContextualMessageCollection)
			{
				ConfigSearched = ExternalContextualMessageCollection->SpeakerEntryTagToConfig.Search([&](FGameplayTag& Tag, FContextualMessageConfig&)
					{ return Tag == ParticipantComponent->ServiceProviderIDTag; } // shouldn't we be using SpeakerParticipantTag
				);
			}
		}

		if (ConfigSearched)
		{
			for (FContextualMessageCandidate& ContextualMessage : ConfigSearched->ContextualMessages)
			{
				if (ContextualMessage.ContextRequirements.Num() == 0)
				{
					Candidates.Add(ContextualMessage);
				}
				else if (ContextualMessage.RequirementMatchPolicy == EContextRequirementMatchPolicy::RequireAny)
				{
					for (FFortConversationContextRequirement& ContextRequirement : ContextualMessage.ContextRequirements)
					{
						if (ContextRequirement.ParticipantID == ParticipantID)
						{
							Candidates.Add(ContextualMessage);
							break;
						}
					}
				}
				else if (ContextualMessage.RequirementMatchPolicy == EContextRequirementMatchPolicy::RequireAll)
				{
					for (FFortConversationContextRequirement& ContextRequirement : ContextualMessage.ContextRequirements)
					{
						if (ContextRequirement.ParticipantID != ParticipantID)
						{
							break;
						}

						Candidates.Add(ContextualMessage);
					}
				}
			}
		}

		if (Candidates.Num() > 0)
		{
			FContextualMessageCandidate Candidate = PickWeighted(Candidates, [](float Total) { return ((float)rand() / 32767.f) * Total; });

			OutResult->Message = Candidate.Message;
			OutResult->MetadataParameters.Append(Candidate.MetadataParameters);
		}
		else
		{
			OutResult->Message = SpeechTaskNode->GeneralConfig.DefaultMessage;
			OutResult->MetadataParameters.Append(SpeechTaskNode->GeneralConfig.DefaultMetadataParameters);
		}

		Log(L"UFortConversationTaskNode_Speech::ResolveMessageForContext OutResult.Message: %s", OutResult->Message.ToWString().c_str());

		// *Ret = true;
		return true;
	}

	// *Ret = false;
	return false;
}

void Conversation::StartConversationHook(UObject* Context, FFrame& Stack)
{
	Log(L"UFortNonPlayerConversationParticipantComponent::StartConversation");

	UFortNonPlayerConversationParticipantComponent* ParticipantComponent = (UFortNonPlayerConversationParticipantComponent*)Context;

	FGameplayTag InConversationEntryTag;
	AActor* Instigator;
	AActor* Target;

	Stack.StepCompiledIn(&InConversationEntryTag);
	Stack.StepCompiledIn(&Instigator);
	Stack.StepCompiledIn(&Target);

	Stack.IncrementCode();

	UConversationLibrary::StartConversation(InConversationEntryTag, Instigator, ParticipantComponent->InteractorParticipantTag, Target, ParticipantComponent->SelfParticipantTag);
}

void skidda()
{
	Log(L"SelectItemsToGive");
}

void skidda2()
{
	Log(L"TossItems");
}
void skidda3()
{
	Log(L"GiveItemsToPlayer");
}

void Conversation::Hook()
{
	Utils::ExecHook(L"/Script/CommonConversationRuntime.ConversationLibrary.StartConversation", StartConversation);
	Utils::ExecHook(L"/Script/FortniteConversationRuntime.FortPlayerConversationComponent.RequestServerAbortConversation", RequestServerAbortConversation);
	Utils::ExecHook(L"/Script/CommonConversationRuntime.ConversationParticipantComponent.ServerAdvanceConversation", ServerAdvanceConversationHook);
	Utils::ExecHook(L"/Script/CommonConversationRuntime.ConversationRequirementNode.IsRequirementSatisfied", IsRequirementSatisfied);
	Utils::ExecHook(L"/Script/CommonConversationRuntime.ConversationTaskNode.IsRequirementSatisfied", IsRequirementSatisfied);

	Utils::ExecHook(L"/Script/CommonConversationRuntime.ConversationContextHelpers.MakeConversationParticipant", Helpers::MakeConversationParticipant);
	Utils::ExecHook(L"/Script/CommonConversationRuntime.ConversationContextHelpers.AdvanceConversation", Helpers::AdvanceConversation);
	Utils::ExecHook(L"/Script/CommonConversationRuntime.ConversationContextHelpers.AdvanceConversationWithChoice", Helpers::AdvanceConversationWithChoice);
	Utils::ExecHook(L"/Script/CommonConversationRuntime.ConversationContextHelpers.PauseConversationAndSendClientChoices", Helpers::PauseConversationAndSendClientChoices);
	Utils::ExecHook(L"/Script/CommonConversationRuntime.ConversationContextHelpers.ReturnToLastClientChoice", Helpers::ReturnToLastClientChoice);
	Utils::ExecHook(L"/Script/CommonConversationRuntime.ConversationContextHelpers.ReturnToCurrentClientChoice", Helpers::ReturnToCurrentClientChoice);
	Utils::ExecHook(L"/Script/CommonConversationRuntime.ConversationContextHelpers.ReturnToConversationStart", Helpers::ReturnToConversationStart);
	Utils::ExecHook(L"/Script/CommonConversationRuntime.ConversationContextHelpers.AbortConversation", Helpers::AbortConversation);

	Utils::ExecHook(L"/Script/FortniteGame.FortNonPlayerConversationParticipantComponent.StartConversation", StartConversationHook);
	Utils::ExecHook(L"/Script/FortniteGame.FortNonPlayerConversationParticipantComponent.SelectItemsToGive", skidda);
	Utils::ExecHook(L"/Script/FortniteGame.FortNonPlayerConversationParticipantComponent.TossItems", skidda2);
	Utils::ExecHook(L"/Script/FortniteGame.FortNonPlayerConversationParticipantComponent.SelectItemsToGive", skidda3);

	// Utils::ExecHook(L"/Script/FortniteGame.FortConversationTaskNode_Speech.ResolveMessageForContext", ResolveMessageForContext);

	Utils::Hook(Sarah::Offsets::ImageBase + 0x5AE57C4, ExecuteTaskNodeWithSideEffectsHook); // i think this is unneeded
	Utils::ExecHook(L"/Script/FortniteGame.FortControllerEffect.ApplyEffectInternal", ApplyEffect);
}