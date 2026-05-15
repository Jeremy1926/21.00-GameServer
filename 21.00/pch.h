#pragma once

#include "MinHook.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <thread>
#include <iostream>
#include <algorithm>
#include <numeric>
#include <map>
#include <intrin.h>
#include <sstream>
#include <array>
#include <tlhelp32.h>
#include <future>
#include <iomanip>  
#include <set>
#include <random>
#include <execution>

#include "UnrealContainers.hpp"
#include "SDK/Basic.hpp"
#include "SDK/CoreUObject_structs.hpp"
#include "SDK/CoreUObject_classes.hpp"
#include "SDK/Engine_structs.hpp"
#include "SDK/Engine_classes.hpp"
#include "SDK/FortniteGame_structs.hpp"
#include "SDK/FortniteGame_classes.hpp"
#include "SDK/GameplayAbilities_structs.hpp"
#include "SDK/GameplayAbilities_classes.hpp"
#include "SDK/GAB_Emote_Generic_classes.hpp"
#include "SDK/GAB_Spray_Generic_classes.hpp"
#include "SDK/TacticalSprintRuntime_structs.hpp"
#include "SDK/TacticalSprintRuntime_classes.hpp"
#include "SDK/EnergyRuntime_classes.hpp"
#include "SDK/EnergyRuntime_structs.hpp"
#include "SDK/FortniteGame_parameters.hpp"
#include "SDK/B_Athena_Zipline_Ascender_classes.hpp"
#include "SDK/FortniteConversationRuntime_classes.hpp"
#include "SDK/CommonConversationRuntime_parameters.hpp"
#include "SDK/Speech_classes.hpp"
#include "SDK/Back_classes.hpp"
#include "SDK/Continue_classes.hpp"
#include "SDK/Repeat_classes.hpp"
#include "SDK/LagerRuntime_classes.hpp"
#include "SDK/LagerRuntime_structs.hpp"
#include "SDK/GAB_AthenaDBNO_classes.hpp"
#include "SDK/CampsiteRuntime_classes.hpp"
#include "SDK/CampsiteRuntime_structs.hpp"
#include "SDK/FortniteConversationUI_classes.hpp"

using namespace SDK;