#include "pch.h"
#include "Quests.h"

void Quests::SendStatEventTags(UFortQuestManager* QuestManager, __int64, FGameplayTagContainer AdditionalSourceTags, FGameplayTagContainer TargetTags)
{
	auto GameState = (AFortGameStateAthena*)UWorld::GetWorld()->GameState;

	FGameplayTagContainer PlayerSourceTags;
	FGameplayTagContainer ContextTags;

	QuestManager->GetSourceAndContextTags(&PlayerSourceTags, &ContextTags);
	ContextTags.AppendTags(GameState->CurrentPlaylistInfo.BasePlaylist->GameplayTagContainer);
	AdditionalSourceTags.AppendTags(PlayerSourceTags);

	Log(L"%s", ((UObject * (*)()) (Sarah::Offsets::ImageBase + 0x1EC74A8))()->GetWName().c_str());
	Log(L"SendStatEventTags for %s", QuestManager->GetPlayerControllerBP()->GetWName().c_str());
	for (auto& Tag : AdditionalSourceTags.GameplayTags)
	{
		Log(L"Source Tag %s", Tag.TagName.ToWString().c_str());
	}
	for (auto& Tag : TargetTags.GameplayTags)
	{
		Log(L"Target Tag %s", Tag.TagName.ToWString().c_str());
	}
	for (auto& Tag : ContextTags.GameplayTags)
	{
		Log(L"Context Tag %s", Tag.TagName.ToWString().c_str());
	}
}

void Quests::SendCustomStatEvent(UObject* Context, FFrame& Stack)
{
	FDataTableRowHandle ObjectiveStat;
	int32 Count;
	Stack.StepCompiledIn(&ObjectiveStat);
	Stack.StepCompiledIn(&Count);
	Stack.IncrementCode();

	auto QuestManager = (UFortQuestManager*)Context;

	Log(L"SendCustomStatEvent for %s, row name %s", QuestManager->GetPlayerControllerBP()->GetWName().c_str(), ObjectiveStat.RowName.ToWString().c_str());
}

void Quests::SendCustomStatEventDirect(UObject* Context, FFrame& Stack)
{
	FName ObjectiveBackendName;
	UFortQuestItem* QuestItem;
	int32 Count;
	Stack.StepCompiledIn(&ObjectiveBackendName);
	Stack.StepCompiledIn(&QuestItem);
	Stack.StepCompiledIn(&Count);
	Stack.IncrementCode();

	auto QuestManager = (UFortQuestManager*)Context;

	Log(L"SendCustomStatEventDirect for %s, row name %s", QuestManager->GetPlayerControllerBP()->GetWName().c_str(), ObjectiveBackendName.ToWString().c_str());
}

void Quests::Hook()
{
	Utils::ExecHook(L"/Script/FortniteGame.FortQuestManager.SendCustomStatEvent", SendCustomStatEvent);
	Utils::ExecHook(L"/Script/FortniteGame.FortQuestManager.SendCustomStatEventDirect", SendCustomStatEventDirect);
	Utils::Hook(Sarah::Offsets::ImageBase + 0x728d4d0, SendStatEventTags);

	UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), L"log LogFortQuest VeryVerbose", nullptr);
}