#pragma once

#include "pch.h"
#include "Utils.h"

class Campsite
{
private:
	static void ClearStashedItem(UObject*, FFrame&);
	static void ClearStashedItemAndGiveToPlayer(UObject*, FFrame&, bool*);
	static void StashCurrentlyHeldItemAndRemoveFromInventory(UObject*, FFrame&, bool*);
	static void SwapStashedItem(UObject*, FFrame&, bool*);

	static void RegisterPreplacedCampsite(UObject*, FFrame&);

public:
	InitHooks;
};