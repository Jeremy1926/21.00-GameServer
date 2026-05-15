#pragma once
#include "Utils.h"
#include "pch.h"

class Quests
{
private:
	static void SendStatEventTags(UFortQuestManager*, __int64, FGameplayTagContainer, FGameplayTagContainer);
	static void SendCustomStatEvent(UObject*, FFrame&);
	static void SendCustomStatEventDirect(UObject*, FFrame&);

	InitHooks;
};