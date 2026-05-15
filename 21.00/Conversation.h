#pragma once
#include "pch.h"
#include "Utils.h"


struct FConversationBranchPointBuilder/* : public FNoncopyable*/
{
	TArray<FConversationBranchPoint> BranchPoints;
	inline void AddChoice(FConversationContext& InContext, FClientConversationOptionEntry& InChoice)
	{
		FConversationBranchPoint BranchPoint;
		BranchPoint.ClientChoice = InChoice;
		if (!UKismetGuidLibrary::IsValid_Guid(BranchPoint.ClientChoice.ChoiceReference.NodeReference.NodeGUID))
		{
			BranchPoint.ClientChoice.ChoiceReference.NodeReference.NodeGUID = InContext.TaskBeingConsidered->Compiled_NodeGUID;
		}
		BranchPoint.ReturnScopeStack = InContext.ReturnScopeStack;

		BranchPoints.Add(BranchPoint);
	}
};

class Conversation
{
public:

	class Helpers
	{
	public:
		static void MakeConversationParticipant(UObject*, FFrame&);
		static void AdvanceConversation(UObject*, FFrame&, FConversationTaskResult*);
		static void AdvanceConversationWithChoice(UObject*, FFrame&, FConversationTaskResult*);
		static void PauseConversationAndSendClientChoices(UObject*, FFrame&, FConversationTaskResult*);
		static void ReturnToLastClientChoice(UObject*, FFrame&, FConversationTaskResult*);
		static void ReturnToCurrentClientChoice(UObject*, FFrame&, FConversationTaskResult*);
		static void ReturnToConversationStart(UObject*, FFrame&, FConversationTaskResult*);
		static void AbortConversation(UObject*, FFrame&, FConversationTaskResult*);
	};

	static UConversationParticipantComponent* GetParticipantComponent(const FConversationParticipantEntry&);
	static void ServerNotifyConversationStarted(UConversationParticipantComponent*, UConversationInstance*, FGameplayTag);
	static void ServerNotifyConversationEnded(UConversationParticipantComponent*, UConversationInstance*);
	static void ServerRemoveParticipant(UConversationInstance*, FGameplayTag);
	static void ServerAssignParticipant(UConversationInstance*, FGameplayTag, AActor*);
	static TArray<FGuid> GetEntryPointGUIDs(UConversationRegistry*, FGameplayTag);
	static TArray<FGuid> GetOutputLinkGUIDs(UConversationRegistry*, TArray<FGuid>&);
	static TArray<FGuid> GetOutputLinkGUIDs(UConversationRegistry*, FGameplayTag);
	static void ResetConversationProgress(UConversationInstance*);
	static void ServerAbortConversation(UConversationInstance*);
	static FConversationContext CreateServerContext(UConversationInstance*, UConversationTaskNode*);
	static TArray<FGuid> DetermineBranches(UConversationInstance*, const TArray<FGuid>&, EConversationRequirementResult);
	static UConversationNode* TryToResolve(FConversationContext&, FGuid&);
	static void SetNextChoices(UConversationInstance*, TArray<FConversationBranchPoint>&);
	static void GenerateChoicesForDestinations(FConversationBranchPointBuilder&, FConversationContext&, const TArray<FGuid>&);
	static void UpdateNextChoices(UConversationInstance*, FConversationContext&);
	static void ApplyEffect(UObject*, FFrame&);
	static FConversationTaskResult ExecuteTaskNode(UConversationTaskNode* Node, FConversationContext& InContext);
	static FConversationTaskResult ExecuteTaskNodeWithSideEffects(UConversationTaskNode*, FConversationContext&);
	static void ModifyCurrentConversationNode(UConversationInstance*, FConversationBranchPoint&);
	static void ModifyCurrentConversationNode(UConversationInstance*, const FConversationChoiceReference&);
	static void PauseConversationAndSendClientChoices(UConversationInstance*, FConversationContext&, FClientConversationMessage&);
	static void ReturnToLastClientChoice(UConversationInstance*, FConversationContext&);
	static void ReturnToCurrentClientChoice(UConversationInstance*, FConversationContext&);
	static void ReturnToStart(UConversationInstance*, FConversationContext&);
	static void ServerAdvanceConversation(UConversationInstance*, FAdvanceConversationRequest);
	static void OnCurrentConversationNodeModified(UConversationInstance*);
	static void ServerStartConversation(UConversationInstance*, FGameplayTag);
	static UConversationInstance* StartConversation(UObject*, FFrame&, UConversationInstance**);
	static void RequestServerAbortConversation(UObject*, FFrame&);
	static void ServerAdvanceConversationHook(UObject*, FFrame&);
	// static void ResolveMessageForContext(UObject*, FFrame&, bool*);
	static bool ResolveMessageForContext(UFortConversationTaskNode_Speech* SpeechTaskNode, FGameplayTag SpeakerParticipantTag, FConversationContext ConversationContext, FContextualMessageQueryResult* OutResult);
	static void StartConversationHook(UObject*, FFrame&);

	DefHookOg(EConversationRequirementResult, IsRequirementSatisfied, UObject*, FFrame&, EConversationRequirementResult*);

	template <typename T>
	static T PickWeighted(TArray<T>& Map, float (*RandFunc)(float), bool bCheckZero = true) {
		float TotalWeight = std::accumulate(Map.begin(), Map.end(), 0.0f, [&](float acc, T& p) { return acc + Utils::EvaluateScalableFloat(p.Weight); });
		float RandomNumber = RandFunc(TotalWeight);

		for (auto& Element : Map)
		{
			float Weight = Utils::EvaluateScalableFloat(Element.Weight);
			if (bCheckZero && Weight == 0)
				continue;

			if (RandomNumber <= Weight) return Element;

			RandomNumber -= Weight;
		}

		return T();
	}

	InitHooks;
};
