#include "pch.h"
#include "Replication.h"
#include "Misc.h"
#include "Options.h"
#include "Player.h"

ULevel* GetLevel(AActor* Actor) {
	auto Outer = Actor->Outer;

	while (Outer)
	{
		if (Outer->Class == ULevel::StaticClass())
			return (ULevel*)Outer;
		else
			Outer = Outer->Outer;
	}
	return nullptr;
}

UWorld* GetWorld(AActor* Actor) {
	if (!Actor->IsDefaultObject() && Actor->Outer && !(Actor->Outer->Flags & EObjectFlags::BeginDestroyed) && !(Actor->Outer->Flags & EObjectFlags(0x10000000)))
	{
		if (auto Level = GetLevel(Actor))
			return Level->OwningWorld;
	}
	return nullptr;
}

TArray<TPair<UNetConnection*, TArray<FNetViewer>>> ViewerMap;
void Replication::BuildViewerMap(UNetDriver* Driver)
{
	//Log(L"PC");
	for (auto& Conn : Driver->ClientConnections)
	{
		auto Owner = Conn->OwningActor;
		if (Owner && GetState(Conn) == USOCK_Open && GetElapsedTime(Conn->Driver) - Conn->LastReceiveTime < 1.5)
		{
			auto OutViewTarget = Owner;
			if (auto Controller = Conn->PlayerController)
				if (auto ViewTarget = GetViewTarget(Controller))
					if (GetWorld(Controller))
						OutViewTarget = ViewTarget;

			Conn->ViewTarget = OutViewTarget;

			TArray<FNetViewer> Viewers;
			Viewers.Reserve(1 + Conn->Children.Num());
			Viewers.Add(ConstructNetViewer(Conn));

			for (auto& Child : Conn->Children)
			{
				if (auto Controller = Child->PlayerController)
				{
					Child->ViewTarget = GetViewTarget(Controller);
					Viewers.Add(ConstructNetViewer(Child));
				}
				else
					Child->ViewTarget = nullptr;
			}
			ViewerMap.Add({ Conn, Viewers });
		}
		else
		{
			Conn->ViewTarget = nullptr;
			for (auto& Child : Conn->Children)
				Child->ViewTarget = nullptr;
		}
	}
}

float FRand()
{
	/*random_device rd;
	mt19937 gen(rd());
	uniform_real_distribution<> dis(0, 1);
	float random_number = (float) dis(gen);

	return random_number;*/
	constexpr int32 RandMax = 0x00ffffff < RAND_MAX ? 0x00ffffff : RAND_MAX;
	return (rand() & RandMax) / (float)RandMax;
}

float fastLerp(float a, float b, float t)
{
	//return (a * (1.f - t)) + (b * t);
	return a - (a + b) * t;
}

TArray<TPair<UNetConnection*, TArray<FActorPriority>>> PriorityLists;
void Replication::BuildPriorityLists(UNetDriver* Driver, float DeltaTime)
{
	//Log(L"BCL");
	void (*&CallPreReplication)(AActor*, UNetDriver*) = decltype(CallPreReplication)(ReplicationOffsets::CallPreReplication);
	void (*&RemoveNetworkActor)(UNetDriver*, AActor*) = decltype(RemoveNetworkActor)(ReplicationOffsets::RemoveNetworkActor);
	UActorChannel* (*&FindActorChannelRef)(UNetConnection*, const TWeakObjectPtr<AActor>&) = decltype(FindActorChannelRef)(ReplicationOffsets::FindActorChannelRef);
	void (*&CloseActorChannel)(UActorChannel*, uint8) = decltype(CloseActorChannel)(ReplicationOffsets::CloseActorChannel);
	void (*&StartBecomingDormant)(UActorChannel*) = decltype(StartBecomingDormant)(ReplicationOffsets::StartBecomingDormant);
	bool (*&SupportsObject)(void*, const UObject*, const TWeakObjectPtr<UObject>*) = decltype(SupportsObject)(ReplicationOffsets::SupportsObject);
	UObject* (*&GetArchetype)(UObject*) = decltype(GetArchetype)(ReplicationOffsets::GetArchetype);


	auto& ActiveNetworkObjects = GetNetworkObjectList(Driver).ActiveNetworkObjects;
	auto Time = GetTimeSeconds(Driver->World);

	PriorityLists.ResetNum();
	PriorityLists.Reserve(ViewerMap.Num());

	for (auto& ViewerPair : ViewerMap)
	{
		auto& Conn = ViewerPair.First;
		auto& Viewers = ViewerPair.Second;

		TArray<FActorPriority> List;
		List.Reserve(ActiveNetworkObjects.Num() + GetDestroyedStartupOrDormantActors(Driver).Num());

		for (auto& NetworkGUID : GetDestroyedStartupOrDormantActorGUIDs(Conn))
		{
			auto Equals = [](const FNetworkGUID& LeftKey, const FNetworkGUID& RightKey) -> bool
				{
					return LeftKey == RightKey;
				};

			auto DestructionInfoPtr = GetDestroyedStartupOrDormantActors(Driver).Search([&](FNetworkGUID& GUID, TUniquePtr<FActorDestructionInfo>& InfoUPtr)
				{
					return GUID == NetworkGUID && (InfoUPtr->StreamingLevelName == FName(0) || GetClientVisibleLevelNames(Conn).Contains(InfoUPtr->StreamingLevelName));
				});

			if (DestructionInfoPtr)
			{
				auto PriorityActor = FActorPriority(Conn, (*DestructionInfoPtr).Get(), Viewers);
				int InsertIdx = 0;
				for (; InsertIdx < List.Num(); InsertIdx++)
				{
					auto& PrioActor = List[InsertIdx];

					if (PrioActor.Priority <= PriorityActor.Priority)
						break;
				}
				List.Add(PriorityActor, InsertIdx);
			}
		}

		PriorityLists.Add({ Conn, List });
	}

	for (auto& ActorInfo : ActiveNetworkObjects)
	{
		if (!ActorInfo->bPendingNetUpdate && Time <= ActorInfo->NextUpdateTime)
			continue;

		auto Actor = ActorInfo->Actor;

		if (!Actor || !Actor->bActorInitialized || Actor->NetDriverName != Driver->NetDriverName)
			continue;

		auto Outer = Actor->Outer;
		if (Actor->bActorIsBeingDestroyed || Actor->Flags & (EObjectFlags::PendingKill | EObjectFlags::Garbage) || Actor->RemoteRole == ENetRole::ROLE_None || (Actor->bNetStartup && Actor->NetDormancy == ENetDormancy::DORM_Initial))
		{
			RemoveNetworkActor(Driver, Actor);
			continue;
		}


		ULevel* Level = GetLevel(Actor);
		if (!Level || Level == Level->OwningWorld->CurrentLevelPendingVisibility || Level == Level->OwningWorld->CurrentLevelPendingInvisibility || Level->bIsAssociatingLevel)
			continue;

		bool bAnyRelevant = false;
		for (auto& ViewerPair : ViewerMap)
		{
			auto& Conn = ViewerPair.First;

			auto Channel = FindActorChannelRef(Conn, ActorInfo->WeakActor);

			auto& Viewers = ViewerPair.Second;


			bool bLevelInitializedForActor = IsLevelInitializedForActor(Driver, Actor, Conn);
			bool bRelevantToConn = IsActorRelevantToConnection(Actor, Viewers);
			if (!Channel && (!bRelevantToConn || !bLevelInitializedForActor))
				continue;

			auto PriorityConn = Conn;

			if (Actor->bOnlyRelevantToOwner)
			{
				bool bHasNullViewTarget = false;

				if (!(PriorityConn = IsActorOwnedByAndRelevantToConnection(Actor, Viewers, bHasNullViewTarget)))
				{
					if (!bHasNullViewTarget && Channel != NULL && GetElapsedTime(Driver) - GetRelevantTime(Channel) >= Driver->RelevantTimeout)
						CloseActorChannel(Channel, 3);

					continue;
				}
			}
			else
			{
				if (ActorInfo->DormantConnections.Contains(Conn))
					continue;

				if (ShouldActorGoDormant(Actor, Viewers, Channel, (float)GetElapsedTime(Driver), false))
					StartBecomingDormant(Channel);
			}

			if (Conn->SentTemporaries.Contains(Actor))
				continue;

			if (!Channel && (!SupportsObject(GetGuidCache(Driver).Get(), Actor->Class, nullptr) || !SupportsObject(GetGuidCache(Driver).Get(), Actor->bNetStartup ? Actor : GetArchetype(Actor), nullptr)))
				continue;

			bool bRelevant = bLevelInitializedForActor && bRelevantToConn && !Actor->bTearOff && (!Channel || GetElapsedTime(Driver) - GetRelevantTime(Channel) > 1.0);
			bool bRecentlyRelevant = bRelevant || (Channel && GetElapsedTime(Driver) - GetRelevantTime(Channel) < Driver->RelevantTimeout) || (ActorInfo->ForceRelevantFrame >= GetLastProcessedFrame(Conn));

			if (bRecentlyRelevant)
			{

				bAnyRelevant = true;
				auto PriorityActor = FActorPriority(PriorityConn, Channel, ActorInfo.Get(), bRelevant, Viewers);
				auto PriorityListPair = PriorityLists.Search([&](TPair<UNetConnection*, TArray<FActorPriority>>& Pair)
					{
						return Pair.First == Conn;
					});
				int InsertIdx = 0;
				for (; InsertIdx < PriorityListPair->Second.Num(); InsertIdx++)
				{
					auto& PrioActor = PriorityListPair->Second[InsertIdx];

					if (PrioActor.Priority <= PriorityActor.Priority)
						break;
				}
				PriorityListPair->Second.Add(PriorityActor, InsertIdx);

				continue;
			}

			if (Channel && (!bRecentlyRelevant || Actor->bTearOff) && (!bLevelInitializedForActor || !Actor->bNetStartup))
				CloseActorChannel(Channel, Actor->bTearOff ? 4 : 3);
		}

		/*if (ActorInfo->LastNetReplicateTime == 0)
		{
			ActorInfo->LastNetReplicateTime = Time;
			ActorInfo->OptimalNetUpdateDelta = 1.f / Actor->NetUpdateFrequency;
		}

		const float LastReplicateDelta = float(Time - ActorInfo->LastNetReplicateTime);

		if (LastReplicateDelta > 2.f)
		{
			if (Actor->MinNetUpdateFrequency == 0.f)
			{
				Actor->MinNetUpdateFrequency = 2.f;
			}

			const float MinOptimalDelta = 1.f / Actor->NetUpdateFrequency;
			const float MaxOptimalDelta = max(1.f / Actor->MinNetUpdateFrequency, MinOptimalDelta);

			const float Alpha = clamp((LastReplicateDelta - 2.f) / 5.f, 0.f, 1.f);
			ActorInfo->OptimalNetUpdateDelta = fastLerp(MinOptimalDelta, MaxOptimalDelta, Alpha);
		}*/

		if (!ActorInfo->bPendingNetUpdate)
		{
			//constexpr bool bUseAdapativeNetFrequency = false;
			const float NextUpdateDelta = /*bUseAdapativeNetFrequency ? ActorInfo->OptimalNetUpdateDelta : */1.f / Actor->NetUpdateFrequency;

			float ServerTickTime = 1.f / std::clamp(1.f / DeltaTime, 1.f, MaxTickRate);
			ActorInfo->NextUpdateTime = Time + FRand() * ServerTickTime + NextUpdateDelta;
			ActorInfo->LastNetUpdateTimestamp = (float)GetElapsedTime(Driver);
		}
		else
		{
			ActorInfo->bPendingNetUpdate = false;
		}


		if (bAnyRelevant)
			CallPreReplication(Actor, Driver);
	}
}

bool Replication::IsActorRelevantToConnection(const AActor* Actor, const TArray<FNetViewer>& ConnectionViewers)
{
	bool (*&IsNetRelevantFor)(const AActor*, const AActor*, const AActor*, const FVector&) = decltype(IsNetRelevantFor)(Actor->VTable[ReplicationOffsets::IsNetRelevantForVft]);

	for (auto& Viewer : ConnectionViewers)
	{
		if (IsNetRelevantFor(Actor, Viewer.InViewer, Viewer.ViewTarget, Viewer.ViewLocation))
		{
			return true;
		}
	}

	return false;
}

FNetViewer Replication::ConstructNetViewer(UNetConnection* NetConnection)
{
	FNetViewer NewNetViewer;
	NewNetViewer.Connection = NetConnection;
	NewNetViewer.InViewer = NetConnection->PlayerController ? NetConnection->PlayerController : NetConnection->OwningActor;
	NewNetViewer.ViewTarget = NetConnection->ViewTarget;

	APlayerController* ViewingController = NetConnection->PlayerController;

	if (ViewingController)
	{
		//FRotator ViewRotation = ViewingController->ControlRotation;
		FRotator ViewRotation;
		Player::GetPlayerViewPointInternal(ViewingController, NewNetViewer.ViewLocation, ViewRotation);
		constexpr auto radian = 0.017453292519943295;
		double cosPitch = cos(ViewRotation.Pitch * radian), sinPitch = sin(ViewRotation.Pitch * radian), cosYaw = cos(ViewRotation.Yaw * radian), sinYaw = sin(ViewRotation.Yaw * radian);
		NewNetViewer.ViewDir = FVector(cosPitch * cosYaw, cosPitch * sinYaw, sinPitch);
	}
	else
	{
		NewNetViewer.ViewLocation = {};
		NewNetViewer.ViewDir = {};
	}

	return NewNetViewer;
}

UNetConnection* Replication::IsActorOwnedByAndRelevantToConnection(const AActor* Actor, TArray<FNetViewer>& ConnViewers, bool& bOutHasNullViewTarget)
{
	bool (*&IsRelevancyOwnerFor)(const AActor*, const AActor*, const AActor*, const AActor*) = decltype(IsRelevancyOwnerFor)(Actor->VTable[ReplicationOffsets::IsRelevancyOwnerForVft]);
	AActor* (*&GetNetOwner)(const AActor*) = decltype(GetNetOwner)(Actor->VTable[ReplicationOffsets::GetNetOwnerVft]);

	const AActor* ActorOwner = GetNetOwner(Actor);

	bOutHasNullViewTarget = false;

	for (auto& Viewer : ConnViewers)
	{
		auto Conn = Viewer.Connection;

		if (Conn->ViewTarget == nullptr)
		{
			bOutHasNullViewTarget = true;
		}

		if (ActorOwner == Conn->PlayerController ||
			(Conn->PlayerController && ActorOwner == Conn->PlayerController->Pawn) ||
			(Conn->ViewTarget && IsRelevancyOwnerFor(Conn->ViewTarget, Actor, ActorOwner, Conn->OwningActor)))
		{
			return Conn;
		}
	}

	return nullptr;
}

bool Replication::ShouldActorGoDormant(AActor* Actor, const TArray<FNetViewer>& ConnectionViewers, UActorChannel* Channel, const float Time, const bool bLowNetBandwidth)
{
	if (Actor->NetDormancy <= ENetDormancy::DORM_Awake || !Channel || Channel->bPendingDormancy || Channel->Dormant)
	{
		// Either shouldn't go dormant, or is already dormant
		return false;
	}

	if (Actor->NetDormancy == ENetDormancy::DORM_DormantPartial)
	{
		//Log(L"dorm partial??? %s", Actor->GetWName().c_str());
		for (int32 viewerIdx = 0; viewerIdx < ConnectionViewers.Num(); viewerIdx++)
		{
			//if (!GetNetDormancy(Actor, ConnectionViewers[viewerIdx].ViewLocation, ConnectionViewers[viewerIdx].ViewDir, ConnectionViewers[viewerIdx].InViewer, ConnectionViewers[viewerIdx].ViewTarget, Channel, Time, bLowNetBandwidth))
			{
				return false;
			}
		}
	}

	return true;
}

bool Replication::IsLevelInitializedForActor(const UNetDriver* NetDriver, const AActor* InActor, UNetConnection* InConnection)
{
	bool (*&ClientHasInitializedLevelFor)(const UNetConnection*, const AActor*) = decltype(ClientHasInitializedLevelFor)(ReplicationOffsets::ClientHasInitializedLevelFor);

	const bool bCorrectWorld = NetDriver->WorldPackage != nullptr && (GetClientWorldPackageName(InConnection) == NetDriver->WorldPackage->Name) && ClientHasInitializedLevelFor(InConnection, InActor);
	const bool bIsConnectionPC = (InActor == InConnection->PlayerController);
	return bCorrectWorld || bIsConnectionPC;
}

void Replication::SendClientAdjustment(APlayerController* PlayerController)
{
	auto& ServerFrameInfo = *(FServerFrameInfo*)(__int64(PlayerController) + ReplicationOffsets::ServerFrameInfo);

	if (ServerFrameInfo.LastProcessedInputFrame != -1 && ServerFrameInfo.LastProcessedInputFrame != ServerFrameInfo.LastSentLocalFrame)
	{
		ServerFrameInfo.LastSentLocalFrame = ServerFrameInfo.LastProcessedInputFrame;
		auto& ClientFrameInfo = *(FClientFrameInfo*)(__int64(PlayerController) + ReplicationOffsets::ClientFrameInfo);
		ClientFrameInfo.LastRecvServerFrame = ServerFrameInfo.LastLocalFrame;
		ClientFrameInfo.LastRecvInputFrame = ServerFrameInfo.LastProcessedInputFrame;
		ClientFrameInfo.QuantizedTimeDilation = ServerFrameInfo.QuantizedTimeDilation;
		//PlayerController->ClientRecvServerAckFrame(ServerFrameInfo.LastProcessedInputFrame, ServerFrameInfo.LastLocalFrame, ServerFrameInfo.QuantizedTimeDilation);
	}


	if (PlayerController->AcknowledgedPawn != PlayerController->Pawn && !PlayerController->SpectatorPawn)
		return;

	auto Pawn = (ACharacter*)(PlayerController->Pawn ? PlayerController->Pawn : PlayerController->SpectatorPawn);
	if (Pawn && Pawn->RemoteRole == ENetRole::ROLE_AutonomousProxy)
	{
		auto Interface = Utils::GetInterface<INetworkPredictionInterface>(Pawn->CharacterMovement);

		if (Interface)
		{
			void (*&SendClientAdjustment)(INetworkPredictionInterface*) = decltype(SendClientAdjustment)(ReplicationOffsets::SendClientAdjustment);
			SendClientAdjustment(Interface);
		}
	}
}

bool Replication::IsNetReady(UNetConnection* Connection, bool bSaturate) {
	bool (*IsNetReady)(UNetConnection*, bool) = decltype(IsNetReady)(ReplicationOffsets::IsNetReady);

	return IsNetReady(Connection, bSaturate);
}

bool Replication::IsNetReady(UChannel* Channel, bool bSaturate) {
	if (Channel->NumOutRec >= 255)
		return 0;

	return IsNetReady(Channel->Connection, bSaturate);
}

int Replication::ProcessPrioritizedActors(UNetDriver* Driver, UNetConnection* Conn, TArray<FActorPriority>& PriorityActors) {
	//Log(L"PPA");
	UActorChannel* (*&CreateChannelByName)(UNetConnection*, FName*, EChannelCreateFlags, int32_t) = decltype(CreateChannelByName)(ReplicationOffsets::CreateChannelByName);
	__int64 (*&SetChannelActor)(UActorChannel*, AActor*) = decltype(SetChannelActor)(ReplicationOffsets::SetChannelActor);
	__int64 (*&ReplicateActor)(UActorChannel*) = decltype(ReplicateActor)(ReplicationOffsets::ReplicateActor);
	int64(*&SetChannelActorForDestroy)(UActorChannel*, FActorDestructionInfo*) = decltype(SetChannelActorForDestroy)(ReplicationOffsets::SetChannelActorForDestroy);
	void (*&ForceNetUpdate)(AActor*) = decltype(ForceNetUpdate)(ReplicationOffsets::ForceNetUpdate);

	if (!IsNetReady(Conn, false))
		return 0;

	int i = 0;
	for (auto& PriorityActor : PriorityActors)
	{
		i++;
		auto ActorInfo = PriorityActor.ActorInfo;

		if (!ActorInfo && PriorityActor.DestructionInfo)
		{
			GetRepContextLevel(Conn) = PriorityActor.DestructionInfo->Level.Get();

			UActorChannel* Channel = (UActorChannel*)CreateChannelByName(Conn, &ActorFName, EChannelCreateFlags::OpenedLocally, -1);
			if (Channel)
			{
				SetChannelActorForDestroy(Channel, PriorityActor.DestructionInfo);
			}

			GetRepContextLevel(Conn) = nullptr;
			GetDestroyedStartupOrDormantActorGUIDs(Conn).Remove(PriorityActor.DestructionInfo->NetGUID);
			continue;
		}

		UActorChannel* Channel = PriorityActor.Channel;
		if (!Channel || Channel->Actor)
		{
			auto Actor = ActorInfo->Actor;

			if (ActorInfo->bSwapRolesOnReplicate && Actor->RemoteRole == ENetRole::ROLE_Authority)
			{
				auto Role = Actor->Role;
				Actor->Role = Actor->RemoteRole;
				Actor->RemoteRole = Role;
				if (Channel)
					*(uint32*)(__int64(Channel) + ReplicationOffsets::RelevantTime + 0x14) |= 2;
			}

			if (!Channel)
			{
				Channel = CreateChannelByName(Conn, &ActorFName, EChannelCreateFlags::OpenedLocally, -1);

				if (Channel)
					SetChannelActor(Channel, Actor);
			}

			if (Channel)
			{
				if (PriorityActor.bRelevant)
					GetRelevantTime(Channel) = GetElapsedTime(Driver) + 0.5 * FRand();

				if (IsNetReady(Channel, false))
					if (ReplicateActor(Channel))
					{
						/*auto TimeSeconds = GetTimeSeconds(Driver->World);
						const float MinOptimalDelta = 1.f / Actor->NetUpdateFrequency;
						const float MaxOptimalDelta = max(1.f / Actor->MinNetUpdateFrequency, MinOptimalDelta);
						const float DeltaBetweenReplications = float(TimeSeconds - ActorInfo->LastNetReplicateTime);

						ActorInfo->OptimalNetUpdateDelta = clamp(DeltaBetweenReplications * 0.7f, MinOptimalDelta, MaxOptimalDelta);
						ActorInfo->LastNetReplicateTime = TimeSeconds;*/
					}
					else
						ForceNetUpdate(Actor);

				if (!IsNetReady(Conn, false))
					return i;
			}

			if (ActorInfo->bSwapRolesOnReplicate)
			{
				auto Role = Actor->Role;
				Actor->Role = Actor->RemoteRole;
				Actor->RemoteRole = Role;
				if (Channel)
					*(uint32*)(__int64(Channel) + ReplicationOffsets::RelevantTime + 0x14) |= 2;
			}
		}
	}

	return PriorityActors.Num();
}

void Replication::ServerReplicateActors(UNetDriver* Driver, float DeltaTime)
{
	GetReplicationFrame(Driver)++;

	BuildViewerMap(Driver);
	if (ViewerMap.Num() == 0)
		return;

	BuildPriorityLists(Driver, DeltaTime);

	auto WorldSettings = Driver->World->PersistentLevel->WorldSettings;
	void (*&CloseConnection)(UNetConnection*) = decltype(CloseConnection)(ReplicationOffsets::CloseConnection);


	for (auto& PriorityListPair : PriorityLists)
	{
		auto Conn = PriorityListPair.First;

		if (!Conn->ViewTarget)
			continue;


		auto ViewerPair = ViewerMap.Search([&](TPair<UNetConnection*, TArray<FNetViewer>>& Pair)
			{
				return Pair.First == Conn;
			});
		auto& Viewers = ViewerPair->Second;

		for (auto& Viewer : Viewers)
		{
			if (Viewer.Connection->PlayerController)
				SendClientAdjustment(Viewer.Connection->PlayerController);
		}

		auto& ConnViewers = WorldSettings->ReplicationViewers = Viewers;
		auto& PriorityActors = PriorityListPair.Second;

		auto LastProcessedActor = ProcessPrioritizedActors(Driver, Conn, PriorityActors);
		for (int i = LastProcessedActor; i < PriorityActors.Num(); i++) {
			auto& PriorityActor = PriorityActors[i];
			if (!PriorityActor.ActorInfo)
				continue;

			auto ActorInfo = PriorityActor.ActorInfo;
			auto Actor = ActorInfo->Actor;
			auto Channel = PriorityActor.Channel;

			if (Channel && GetElapsedTime(Driver) - GetRelevantTime(Channel) <= 1.0) {
				ActorInfo->bPendingNetUpdate = true;
			}
			else if (IsActorRelevantToConnection(Actor, ConnViewers)) {
				ActorInfo->bPendingNetUpdate = true;
				if (Channel)
					GetRelevantTime(Channel) = GetElapsedTime(Driver) + 0.5 * FRand();
			}

			if (ActorInfo->ForceRelevantFrame >= GetLastProcessedFrame(Conn))
				ActorInfo->ForceRelevantFrame = GetReplicationFrame(Driver) + 1;
		}

		PriorityActors.Free();

		GetLastProcessedFrame(Conn) = GetReplicationFrame(Driver);

		if (GetPendingCloseDueToReplicationFailure(Conn))
			CloseConnection(Conn);
	}

	for (auto& ViewerPair : ViewerMap)
		ViewerPair.Second.Free();

	ViewerMap.ResetNum();
}