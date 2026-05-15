#include "pch.h"
#include "Misc.h"
#include "Replication.h"
#include "Options.h"

int Misc::GetNetMode() 
{
	return 1;
}

float Misc::GetMaxTickRate(UEngine* Engine, float DeltaTime, bool bAllowFrameRateSmoothing) 
{
	// improper, DS is supposed to do hitching differently
	return std::clamp(1.f / DeltaTime, 1.f, MaxTickRate);
}

bool Misc::RetTrue() 
{ 
	return true; 
}

void Misc::TickFlush(UNetDriver* Driver, float DeltaTime)
{
	if (Driver->ClientConnections.Num() > 0) 
		Replication::ServerReplicateActors(Driver, DeltaTime);

	return TickFlushOG(Driver, DeltaTime);
}

void* Misc::DispatchRequest(void* Arg1, void* MCPData, int)
{
	return DispatchRequestOG(Arg1, MCPData, 3);
}

void Misc::Listen()
{
	auto World = UWorld::GetWorld();
	auto Engine = UEngine::GetEngine();

	FWorldContext* Context = ((FWorldContext * (*)(UEngine*, UWorld*)) (Sarah::Offsets::ImageBase + 0xebd6a8))(Engine, World);
	auto NetDriverName = FName(L"GameNetDriver");
	auto NetDriver = World->NetDriver = ((UNetDriver * (*)(UEngine*, FWorldContext*, FName))Sarah::Offsets::CreateNetDriver)(Engine, Context, NetDriverName);

	NetDriver->NetDriverName = NetDriverName;
	NetDriver->World = World;

	for (auto& Collection : World->LevelCollections) 
		Collection.NetDriver = NetDriver;

	FString Err;
	if (((bool (*)(UNetDriver*, UWorld*, FURL&, bool, FString&))Sarah::Offsets::InitListen)(NetDriver, World, World->PersistentLevel->URL, false, Err)) 
	{
		
		((void (*)(UNetDriver*, UWorld*))Sarah::Offsets::SetWorld)(NetDriver, World);
	}
	else
	{
		Log(L"Failed to listen");
	}
}

void Misc::SetDynamicFoundationEnabled(UObject* Context, FFrame& Stack)
{
	auto Foundation = (ABuildingFoundation*)Context;
	bool bEnabled;
	Stack.StepCompiledIn(&bEnabled);
	Stack.IncrementCode();
	Foundation->DynamicFoundationRepData.EnabledState = bEnabled ? EDynamicFoundationEnabledState::Enabled : EDynamicFoundationEnabledState::Disabled;
	Foundation->OnRep_DynamicFoundationRepData();
	Foundation->FoundationEnabledState = bEnabled ? EDynamicFoundationEnabledState::Enabled : EDynamicFoundationEnabledState::Disabled;
}

void Misc::SetDynamicFoundationTransform(UObject* Context, FFrame& Stack)
{
	auto Foundation = (ABuildingFoundation*)Context;
	auto& Transform = Stack.StepCompiledInRef<FTransform>();
	Stack.IncrementCode();
	Foundation->DynamicFoundationTransform = Transform;
	Foundation->DynamicFoundationRepData.Rotation = Transform.Rotation.Rotator();
	Foundation->DynamicFoundationRepData.Translation = Transform.Translation;
	Foundation->StreamingData.FoundationLocation = Transform.Translation;
	Foundation->StreamingData.BoundingBox = Foundation->StreamingBoundingBox;
	Foundation->OnRep_DynamicFoundationRepData();
}

bool Misc::RetFalse()
{
	return false;
}

void Misc::StartNewSafeZonePhase(AFortGameModeAthena* GameMode, int a2)
{
	auto GameState = (AFortGameStateAthena *) GameMode->GameState;

	FFortSafeZoneDefinition* SafeZoneDefinition = &GameState->MapInfo->SafeZoneDefinition;
	TArray<float>& Durations = *(TArray<float>*)(__int64(SafeZoneDefinition) + 0x248);
	TArray<float>& HoldDurations = *(TArray<float>*)(__int64(SafeZoneDefinition) + 0x238);

	constexpr static std::array<float, 8> LateGameDurations{
		0.f,
		65.f,
		60.f,
		50.f,
		45.f,
		35.f,
		30.f,
		40.f,
	};

	constexpr static std::array<float, 8> LateGameHoldDurations{
		0.f,
		60.f,
		55.f,
		50.f,
		45.f,
		30.f,
		0.f,
		0.f,
	};


	if (bLateGame && GameMode->SafeZonePhase < 3)
	{
		GameMode->SafeZoneIndicator->SafeZoneStartShrinkTime = UGameplayStatics::GetTimeSeconds(UWorld::GetWorld());
		GameMode->SafeZoneIndicator->SafeZoneFinishShrinkTime = GameMode->SafeZoneIndicator->SafeZoneStartShrinkTime + 0.05f;
	}
	else if (bLateGame && GameMode->SafeZonePhase == 3)
	{
		auto Duration = bLateGame ? LateGameDurations[GameMode->SafeZonePhase - 1] : Durations[GameMode->SafeZonePhase + 1];

		GameMode->SafeZoneIndicator->SafeZoneStartShrinkTime = UGameplayStatics::GetTimeSeconds(UWorld::GetWorld()) + 30.f;
		GameMode->SafeZoneIndicator->SafeZoneFinishShrinkTime = GameMode->SafeZoneIndicator->SafeZoneStartShrinkTime + Duration;
	}
	else
	{
		auto Duration = bLateGame ? LateGameDurations[GameMode->SafeZonePhase - 1] : Durations[GameMode->SafeZonePhase + 1];
		auto HoldDuration = bLateGame ? LateGameHoldDurations[GameMode->SafeZonePhase - 1] : HoldDurations[GameMode->SafeZonePhase + 1];

		GameMode->SafeZoneIndicator->SafeZoneStartShrinkTime = UGameplayStatics::GetTimeSeconds(UWorld::GetWorld()) + HoldDuration;
		GameMode->SafeZoneIndicator->SafeZoneFinishShrinkTime = GameMode->SafeZoneIndicator->SafeZoneStartShrinkTime + Duration;
	}

	StartNewSafeZonePhaseOG(GameMode, a2);
}

uint32 Misc::CheckCheckpointHeartBeat() 
{
	return -1;
}

void Misc::ExecuteTick(__int64 a1, __int64 a2, unsigned int a3) 
{
	// why bro
	auto v3 = *(__int64**)(a1 + 0x28);
	if (!v3)
		return;

	return ExecuteTickOG(a1, a2, a3);
}


bool Misc::StartAircraftPhase(AFortGameModeAthena* GameMode, char a2)
{
	auto Ret = StartAircraftPhaseOG(GameMode, a2);

	if (bLateGame)
	{
		auto GameState = (AFortGameStateAthena*)GameMode->GameState;

		auto Aircraft = GameState->Aircrafts[0];
		Aircraft->FlightInfo.FlightSpeed = 0.f;
		FVector Loc{ 0, 0, 0 };
		while (Loc.X == 0 && Loc.Y == 0 && Loc.Z == 0)
			Loc = GameMode->SafeZoneLocations[rand() % (GameMode->SafeZoneLocations.Num() - 1)];
		Loc.Z = 17500.f;

		Aircraft->FlightInfo.FlightStartLocation = Loc.Quantize100();

		Aircraft->FlightInfo.TimeTillFlightEnd = 7.f;
		Aircraft->FlightInfo.TimeTillDropEnd = 0.f;
		Aircraft->FlightInfo.TimeTillDropStart = 0.f;
		Aircraft->FlightStartTime = UGameplayStatics::GetTimeSeconds(UWorld::GetWorld());
		Aircraft->FlightEndTime = UGameplayStatics::GetTimeSeconds(UWorld::GetWorld()) + 7.f;
		//GameState->bAircraftIsLocked = false;
		GameState->SafeZonesStartTime = UGameplayStatics::GetTimeSeconds(UWorld::GetWorld()) + 7.f;
	}

	return Ret;
}

const wchar_t* Misc::GetCommandLet() 
{
	static auto cmdLine = UEAllocatedWString(GetCommandLetOG()) + L" -AllowAllPlaylistsInShipping";
	return cmdLine.c_str();
}

FServicePermissionsMcp* Misc::MatchMakingServicePerms(int64, int64) 
{
	FServicePermissionsMcp* Perms = new FServicePermissionsMcp();
	Perms->Id = L"ec684b8c687f479fadea3cb2ad83f5c6";
	return Perms;
}

static inline __int64 (*OnAFortPlayerPawn_GetCharacterPartFailedOG)(
	__int64 WhatIsThis,
	int PartTypeNumber_1,
	unsigned __int8 a3,
	char a4);

__int64 __fastcall OnAFortPlayerPawn_GetCharacterPartFailed(
	__int64 WhatIsThis,
	int PartTypeNumber_1,
	unsigned __int8 a3,
	char a4)
{
	Log(L"OnAFortPlayerPawn_GetCharacterPartFailed ReturnAddress: 0x%p", __int64(_ReturnAddress()) - Sarah::Offsets::ImageBase);
	return OnAFortPlayerPawn_GetCharacterPartFailedOG(WhatIsThis, PartTypeNumber_1, a3, a4);
}

void Misc::ServerMove_PerformMovement(UCharacterMovementComponent* Comp, FCharacterNetworkMoveData& MoveData)
{
	if (MoveData.TimeStamp <= 1.f)
	{
		// running w/o anticheat
		auto PlayerController = Comp->CharacterOwner->Controller->Cast<APlayerController>();
		PlayerController->ClientReturnToMainMenu(L"stop bypassing ac you baboon");
		return;
	}
	else if (MoveData.TimeStamp >= 45387534.634453f && MoveData.TimeStamp < 45387535.634453f)
		MoveData.TimeStamp -= 45387534.634453f;
	ServerMove_PerformMovementOG(Comp, MoveData);
}

void Misc::PauseBeaconRequests(AOnlineBeaconHost* Beacon, bool)
{
	return PauseBeaconRequestsOG(Beacon, false); // skunked but works
}

void Misc::Hook()
{
	Utils::Hook(Sarah::Offsets::WorldNetMode, GetNetMode);
	Utils::Hook(Sarah::Offsets::GetMaxTickRate, GetMaxTickRate, GetMaxTickRateOG);
	Utils::Hook(Sarah::Offsets::TickFlush, TickFlush, TickFlushOG);
	Utils::Hook(Sarah::Offsets::DispatchRequest, DispatchRequest, DispatchRequestOG);
	Utils::ExecHook(L"/Script/FortniteGame.BuildingFoundation.SetDynamicFoundationTransform", SetDynamicFoundationTransform);
	Utils::ExecHook(L"/Script/FortniteGame.BuildingFoundation.SetDynamicFoundationEnabled", SetDynamicFoundationEnabled);
	Utils::Hook(Sarah::Offsets::StartNewSafeZonePhase, StartNewSafeZonePhase, StartNewSafeZonePhaseOG);
	Utils::Patch<uint8_t>(Sarah::Offsets::EncryptionPatch, 0x74);
	Utils::Patch<uint8_t>(Sarah::Offsets::GameSessionPatch, 0x85);
	Utils::Patch<uint8_t>(Sarah::Offsets::ImageBase + 0x715517d, 0xeb);
	Utils::Patch<uint8_t>(Sarah::Offsets::ImageBase + 0x709a754, 0x75);
	//Utils::Patch<uint16_t>(Sarah::Offsets::ImageBase + 0x6A6463A, 0xe990);
	Utils::Hook(Sarah::Offsets::ImageBase + 0x88f5da4, Listen);
	for (auto& NullFunc : Sarah::Offsets::NullFuncs)
		Utils::Patch<uint8_t>(NullFunc, 0xc3);
	for (auto& RetTrueFunc : Sarah::Offsets::RetTrueFuncs) {
		Utils::Patch<uint32_t>(RetTrueFunc, 0xc0ffc031);
		Utils::Patch<uint8_t>(RetTrueFunc + 4, 0xc3);
	}
	Utils::Hook(Sarah::Offsets::ImageBase + 0x4a9d740, CheckCheckpointHeartBeat);
	Utils::Hook(Sarah::Offsets::ImageBase + 0xdb0e9c, ExecuteTick, ExecuteTickOG);
	Utils::Hook(Sarah::Offsets::ImageBase + 0x6a7bf48, StartAircraftPhase, StartAircraftPhaseOG);
	Utils::Hook(Sarah::Offsets::ImageBase + 0xe5c588, GetCommandLet, GetCommandLetOG);
	Utils::Hook(Sarah::Offsets::ImageBase + 0x15601f4, MatchMakingServicePerms);
	if (bAntiCheat) Utils::Hook(Sarah::Offsets::ImageBase + 0x864e1dc, ServerMove_PerformMovement, ServerMove_PerformMovementOG);
	Utils::Hook(Sarah::Offsets::PauseBeaconRequests, PauseBeaconRequests, PauseBeaconRequestsOG);

	//Utils::Hook(Sarah::Offsets::ImageBase + 0x70AB888, brooo, broooo);
	//Utils::Patch<uint16_t>(Sarah::Offsets::ImageBase + 0x120676C, 0xe990);
	// Utils::Hook(Sarah::Offsets::ImageBase + 0x4A02220, OnAFortPlayerPawn_GetCharacterPartFailed, OnAFortPlayerPawn_GetCharacterPartFailedOG);
}
