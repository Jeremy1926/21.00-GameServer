#pragma once
#include "pch.h"
#include "Utils.h"

struct FNetworkObjectInfo
{
	class SDK::AActor* Actor;

	TWeakObjectPtr<class SDK::AActor> WeakActor;

	double NextUpdateTime;

	double LastNetReplicateTime;

	float OptimalNetUpdateDelta;

	double LastNetUpdateTimestamp;

	TSet<TWeakObjectPtr<class SDK::UNetConnection>> DormantConnections;

	TSet<TWeakObjectPtr<class SDK::UNetConnection>> RecentlyDormantConnections;

	uint8 bPendingNetUpdate : 1;

	uint8 bDirtyForReplay : 1;

	uint8 bSwapRolesOnReplicate : 1;

	uint32 ForceRelevantFrame = 0;
};

class FNetworkObjectList
{
public:
	using FNetworkObjectSet = TSet<class SDK::TSharedPtr<FNetworkObjectInfo>>;

	FNetworkObjectSet AllNetworkObjects;
	FNetworkObjectSet ActiveNetworkObjects;
	FNetworkObjectSet ObjectsDormantOnAllConnections;

	TMap<TWeakObjectPtr<UNetConnection>, int32> NumDormantObjectsPerConnection;
};


struct FClientFrameInfo
{
	int32 LastRecvInputFrame = -1;	// The latest inputcmd that the server acknowledged receiving, but not yet processed (this is our frame number that we gave them)
	int32 LastProcessedInputFrame = -1; // The latest InputCmd that the server actually processed (this is our frame number that we gave them)
	int32 LastRecvServerFrame = -1; // the latest ServerFrame number that the processing of LastRecvInputFrame happened on (Server's local frame number)

	int8 QuantizedTimeDilation = 1; // Server sent this to this client, telling them to dilate local time either catch up or slow down
	float TargetNumBufferedCmds = 0.f;
};

struct FServerFrameInfo
{
	int32 LastProcessedInputFrame = -1;
	int32 LastLocalFrame = -1;
	int32 LastSentLocalFrame = -1;

	float TargetTimeDilation = 1.f;
	int8 QuantizedTimeDilation = 1;
	float TargetNumBufferedCmds = 1.f;
	bool bFault = true;
};

class FNetworkGUID
{
public:

	uint32 Value;

public:

	FNetworkGUID()
		: Value(0)
	{
	}

	FNetworkGUID(uint32 V)
		: Value(V)
	{
	}

public:

	friend bool operator==(const FNetworkGUID& X, const FNetworkGUID& Y)
	{
		return (X.Value == Y.Value);
	}

	friend bool operator!=(const FNetworkGUID& X, const FNetworkGUID& Y)
	{
		return (X.Value != Y.Value);
	}
};


struct FActorDestructionInfo
{
public:
	FActorDestructionInfo()
		: Reason(0)
		, bIgnoreDistanceCulling(false)
	{
	}

	TWeakObjectPtr<class SDK::ULevel> Level;
	TWeakObjectPtr<class SDK::UObject> ObjOuter;
	struct SDK::FVector DestroyedPosition;
	class FNetworkGUID NetGUID;
	class SDK::FString PathName;
	class SDK::FName StreamingLevelName;
	uint8_t Reason;

	bool bIgnoreDistanceCulling;
};

template< class ObjectType>
class TUniquePtr
{
public:
	ObjectType* Ptr;

	FORCEINLINE ObjectType* Get()
	{
		return Ptr;
	}
	FORCEINLINE ObjectType* Get() const
	{
		return Ptr;
	}
	FORCEINLINE ObjectType& operator*()
	{
		return *Ptr;
	}
	FORCEINLINE const ObjectType& operator*() const
	{
		return *Ptr;
	}
	FORCEINLINE ObjectType* operator->()
	{
		return Ptr;
	}
	FORCEINLINE ObjectType* operator->() const
	{
		return Ptr;
	}
};

#define CLOSEPROXIMITY					500.
#define NEARSIGHTTHRESHOLD				2000.
#define MEDSIGHTTHRESHOLD				3162.
#define FARSIGHTTHRESHOLD				8000.
#define CLOSEPROXIMITYSQUARED			(CLOSEPROXIMITY*CLOSEPROXIMITY)
#define NEARSIGHTTHRESHOLDSQUARED		(NEARSIGHTTHRESHOLD*NEARSIGHTTHRESHOLD)
#define MEDSIGHTTHRESHOLDSQUARED		(MEDSIGHTTHRESHOLD*MEDSIGHTTHRESHOLD)
#define FARSIGHTTHRESHOLDSQUARED		(FARSIGHTTHRESHOLD*FARSIGHTTHRESHOLD)

struct FRelevantConnectionInfo
{
	UActorChannel* Channel;
	bool bRelevant;
};

struct FActorPriority
{
	int32 Priority;

	FNetworkObjectInfo* ActorInfo;
	bool bRelevant;
	UActorChannel* Channel;

	FActorDestructionInfo* DestructionInfo;

	FActorPriority() :
		Priority(0), ActorInfo(NULL), bRelevant(false), Channel(NULL), DestructionInfo(NULL)
	{
	}

	FActorPriority(UNetConnection* InConnection, UActorChannel* InChannel, FNetworkObjectInfo* InActorInfo, bool InRelevant, const TArray<FNetViewer>& Viewers)
		: ActorInfo(InActorInfo), Channel(InChannel), bRelevant(InRelevant), DestructionInfo(NULL)
	{
		float (*GetNetPriority)(AActor*, const FVector&, const FVector&, class AActor*, AActor*, UActorChannel*, float, bool) = decltype(GetNetPriority)(ActorInfo->Actor->VTable[0x85]);

		const auto Time = float(Channel ? (*(double*)(__int64(InConnection->Driver) + 0x218) - *(double*)(__int64(InChannel) + 0x80)) : InConnection->Driver->SpawnPrioritySeconds);
		Priority = 0;
		for (int32 i = 0; i < Viewers.Num(); i++)
		{
			Priority = max(Priority, (int32)round(65536.0f * GetNetPriority(ActorInfo->Actor, Viewers[i].ViewLocation, Viewers[i].ViewDir, Viewers[i].InViewer, Viewers[i].ViewTarget, InChannel, Time, false)));
		}
	}

	FActorPriority(UNetConnection* InConnection, FActorDestructionInfo* DestructInfo, const TArray<FNetViewer>& Viewers)
		: ActorInfo(NULL), bRelevant(false), Channel(NULL), DestructionInfo(DestructInfo)
	{
		Priority = 0;

		for (int32 i = 0; i < Viewers.Num(); i++)
		{
			float Time = InConnection->Driver->SpawnPrioritySeconds;

			FVector Dir = DestructionInfo->DestroyedPosition - Viewers[i].ViewLocation;
			double DistSq = Dir.SizeSquared();

			// adjust priority based on distance and whether actor is in front of viewer
			if ((Viewers[i].ViewDir | Dir) < 0.f)
			{
				if (DistSq > NEARSIGHTTHRESHOLDSQUARED)
					Time *= 0.2f;
				else if (DistSq > CLOSEPROXIMITYSQUARED)
					Time *= 0.4f;
			}
			else if (DistSq > MEDSIGHTTHRESHOLDSQUARED)
				Time *= 0.4f;

			Priority = max(Priority, int32(65536.0f * Time));
		}
	}

	bool operator>(FActorPriority& _Rhs) {
		return Priority > _Rhs.Priority;
	}
};

enum class EChannelCreateFlags : uint32_t
{
	None = (1 << 0),
	OpenedLocally = (1 << 1)
};

enum EConnectionState
{
	USOCK_Invalid = 0, // Connection is invalid, possibly uninitialized.
	USOCK_Closed = 1, // Connection permanently closed.
	USOCK_Pending = 2, // Connection is awaiting connection.
	USOCK_Open = 3, // Connection is open.
};

class ReplicationOffsets {
public:
	static inline uint32 TimeSeconds = 0x638;
	static inline uint32 ReplicationFrame = 0x3d8;
	static inline uint32 NetworkObjectList = 0x6b8;
	static inline uint32 ElapsedTime = 0x218;
	static inline uint32 RelevantTime = 0x78;
	static inline uint32 NetTag = 0x2e4;
	static inline uint32 DestroyedStartupOrDormantActorGUIDs = 0x1488;
	static inline uint32 DestroyedStartupOrDormantActors = 0x2e8;
	static inline uint32 LastProcessedFrame = 0x200;
	static inline uint32 ClientVisibleLevelNames = 0x1578;
	static inline uint32 ClientWorldPackageName = 0x16b8;
	static inline uint32 RepContextLevel = 0x16d0;
	static inline uint32 GuidCache = 0x150;
	static inline uint32 PendingCloseDueToReplicationFailure = 0x1b56;
	static inline uint32 ClientFrameInfo = 0x790;
	static inline uint32 ServerFrameInfo = 0x7a4;
	static inline uint32 State = 0x134;
	static inline uint32 ComponentLocation = 0x260;

	static inline uint32 IsNetRelevantForVft = 0x9a;
	static inline uint32 IsRelevancyOwnerForVft = 0x9c;
	static inline uint32 GetNetOwnerVft = 0xa0;

	static inline uint64 GetViewTarget;
	static inline uint64 CallPreReplication;
	static inline uint64 SendClientAdjustment;
	static inline uint64 CreateChannelByName;
	static inline uint64 SetChannelActor;
	static inline uint64 ReplicateActor;
	static inline uint64 RemoveNetworkActor;
	static inline uint64 ClientHasInitializedLevelFor;
	static inline uint64 FindActorChannelRef;
	static inline uint64 CloseActorChannel;
	static inline uint64 SetChannelActorForDestroy;
	static inline uint64 StartBecomingDormant;
	static inline uint64 SupportsObject;
	static inline uint64 GetArchetype;
	static inline uint64 IsNetReady;
	static inline uint64 CloseConnection;
	static inline uint64 ForceNetUpdate;

	static void Init() {
		GetViewTarget = Sarah::Offsets::ImageBase + 0x10c40d0;
		CallPreReplication = Sarah::Offsets::ImageBase + 0x85ad614;
		SendClientAdjustment = Sarah::Offsets::ImageBase + 0x864a804;
		CreateChannelByName = Sarah::Offsets::ImageBase + 0x195eae8;
		SetChannelActor = Sarah::Offsets::ImageBase + 0x111d00c;
		ReplicateActor = Sarah::Offsets::ImageBase + 0x869421c;
		RemoveNetworkActor = Sarah::Offsets::ImageBase + 0x111e1a4;
		ClientHasInitializedLevelFor = Sarah::Offsets::ImageBase + 0x877c910;
		FindActorChannelRef = Sarah::Offsets::ImageBase + 0x57f3728;
		CloseActorChannel = Sarah::Offsets::ImageBase + 0x86896d0;
		SetChannelActorForDestroy = Sarah::Offsets::ImageBase + 0x8696a6c;
		StartBecomingDormant = Sarah::Offsets::ImageBase + 0x86978e0;
		SupportsObject = Sarah::Offsets::ImageBase + 0xf1a994;
		GetArchetype = Sarah::Offsets::ImageBase + 0xf2a830;
		IsNetReady = Sarah::Offsets::ImageBase + 0x8781fa0;
		CloseConnection = Sarah::Offsets::ImageBase + 0x1740414;
		ForceNetUpdate = Sarah::Offsets::ImageBase + 0x124bc50;
	}
};

class Replication {
private:
	static inline auto ActorFName = FName(298);

	static float& GetTimeSeconds(UWorld* World)
	{
		return *(float*)(__int64(World) + ReplicationOffsets::TimeSeconds);
	}
	static int& GetReplicationFrame(UNetDriver* Driver)
	{
		return *(int*)(__int64(Driver) + ReplicationOffsets::ReplicationFrame);
	}
	static FNetworkObjectList& GetNetworkObjectList(UNetDriver* Driver)
	{
		return *(*(class SDK::TSharedPtr<FNetworkObjectList>*)(__int64(Driver) + ReplicationOffsets::NetworkObjectList));
	}
	static double& GetElapsedTime(UNetDriver* Driver)
	{
		return *(double*)(__int64(Driver) + ReplicationOffsets::ElapsedTime);
	}
	static AActor* GetViewTarget(APlayerController* Controller)
	{
		return (decltype(&GetViewTarget)(ReplicationOffsets::GetViewTarget))(Controller);
	}
	static double& GetRelevantTime(UActorChannel* Channel)
	{
		return *(double*)(__int64(Channel) + ReplicationOffsets::RelevantTime);
	}
	static int32& GetNetTag(UNetDriver* Driver)
	{
		return *(int32*)(__int64(Driver) + ReplicationOffsets::NetTag);
	}
	static TSet<FNetworkGUID>& GetDestroyedStartupOrDormantActorGUIDs(UNetConnection* Conn)
	{
		return *(TSet<FNetworkGUID>*)(__int64(Conn) + ReplicationOffsets::DestroyedStartupOrDormantActorGUIDs);
	}
	static TMap<FNetworkGUID, TUniquePtr<FActorDestructionInfo>>& GetDestroyedStartupOrDormantActors(UNetDriver* Driver)
	{
		return *(TMap<FNetworkGUID, TUniquePtr<FActorDestructionInfo>>*)(__int64(Driver) + ReplicationOffsets::DestroyedStartupOrDormantActors);
	}
	static uint32& GetLastProcessedFrame(UNetConnection* Conn)
	{
		return *(uint32*)(__int64(Conn) + ReplicationOffsets::LastProcessedFrame);
	}
	static TSet<FName>& GetClientVisibleLevelNames(UNetConnection* Conn)
	{
		return *(TSet<FName>*)(__int64(Conn) + ReplicationOffsets::ClientVisibleLevelNames);
	}
	static FName& GetClientWorldPackageName(UNetConnection* Conn)
	{
		return *(FName*)(__int64(Conn) + ReplicationOffsets::ClientWorldPackageName);
	}
	static ULevel*& GetRepContextLevel(UNetConnection* Conn)
	{
		return *(ULevel**)(__int64(Conn) + ReplicationOffsets::RepContextLevel);
	}
	static class SDK::TSharedPtr<void*>& GetGuidCache(UNetDriver* Driver)
	{
		return *(class SDK::TSharedPtr<void*>*)(__int64(Driver) + ReplicationOffsets::GuidCache);
	}
	static bool& GetPendingCloseDueToReplicationFailure(UNetConnection* Conn)
	{
		return *(bool*)(__int64(Conn) + ReplicationOffsets::PendingCloseDueToReplicationFailure);
	}
	static EConnectionState& GetState(UNetConnection* Conn)
	{
		return *(EConnectionState*)(__int64(Conn) + ReplicationOffsets::State);
	}
	static void BuildViewerMap(UNetDriver*);
	static void BuildPriorityLists(UNetDriver*, float);
	static bool IsActorRelevantToConnection(const AActor* Actor, const TArray<FNetViewer>& ConnectionViewers);
	static FNetViewer ConstructNetViewer(UNetConnection* NetConnection);
	static UNetConnection* IsActorOwnedByAndRelevantToConnection(const AActor* Actor, TArray<FNetViewer>&, bool& bOutHasNullViewTarget);
	static bool ShouldActorGoDormant(AActor* Actor, const TArray<FNetViewer>& ConnectionViewers, UActorChannel* Channel, const float Time, const bool bLowNetBandwidth);
	static bool IsLevelInitializedForActor(const UNetDriver* Driver, const AActor* InActor, UNetConnection* InConnection);
	static void SendClientAdjustment(APlayerController*);
	static bool IsNetReady(UNetConnection* Connection, bool bSaturate);
	static bool IsNetReady(UChannel* Channel, bool bSaturate);
	static int ProcessPrioritizedActors(UNetDriver* Driver, UNetConnection* Conn, TArray<FActorPriority>&);
public:
	static void ServerReplicateActors(UNetDriver*, float);
};