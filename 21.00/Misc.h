#pragma once
#include "pch.h"
#include "Utils.h"

struct FServicePermissionsMcp {
public:
	char Unk_0[0x10];
	class FString Id;
};

struct FCharacterNetworkMoveData
{
public:
	enum class ENetworkMoveType
	{
		NewMove,
		PendingMove,
		OldMove
	};

	void** VTable;

	ENetworkMoveType NetworkMoveType;

	float TimeStamp;
	FVector Acceleration;
	FVector Location;
	FRotator ControlRotation;
	uint8 CompressedMoveFlags;

	class UPrimitiveComponent* MovementBase;
	FName MovementBaseBoneName;
	uint8 MovementMode;
};

class Misc 
{
private:
	DefHookOg(const wchar_t*, GetCommandLet);
	static int GetNetMode();
public:
	DefHookOg(float, GetMaxTickRate, UEngine*, float, bool);
private:
	static FServicePermissionsMcp* MatchMakingServicePerms(int64, int64);
	static bool RetTrue();
	static bool RetFalse();
	static uint32 CheckCheckpointHeartBeat();
	DefHookOg(void, TickFlush, UNetDriver*, float);
	DefHookOg(void*, DispatchRequest, void*, void*, int);
	static void Listen();
	static void SetDynamicFoundationEnabled(UObject*, FFrame&);
	static void SetDynamicFoundationTransform(UObject*, FFrame&);
	DefHookOg(void, StartNewSafeZonePhase, AFortGameModeAthena*, int);
	DefHookOg(void, ExecuteTick, __int64, __int64, uint32_t);
	DefHookOg(bool, StartAircraftPhase, AFortGameModeAthena*, char);
	DefHookOg(void, ServerMove_PerformMovement, UCharacterMovementComponent*, FCharacterNetworkMoveData&);
	DefHookOg(void, PauseBeaconRequests, AOnlineBeaconHost*, bool);

	InitHooks;
};