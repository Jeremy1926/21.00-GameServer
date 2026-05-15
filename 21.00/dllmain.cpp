// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "Utils.h"
#include "Replication.h"
#include "Options.h"
#include "Lategame.h"

DWORD WINAPI HotKeyThread(LPVOID)
{
    while (true)
    {
        if (GetAsyncKeyState(VK_F7))
        {
            UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), L"STARTAIRCRAFT", nullptr);
        }

        if (GetAsyncKeyState(VK_SHIFT) && false /* apparently it crashes */)
        {
            AFortPlayerControllerAthena* PlayerController = UWorld::GetWorld()->NetDriver ?
                UWorld::GetWorld()->NetDriver->ClientConnections.IsValidIndex(0) ?
                (AFortPlayerControllerAthena*)UWorld::GetWorld()->NetDriver->ClientConnections[0]->PlayerController :
                nullptr :
                nullptr;

            if (PlayerController)
            {
                UFortPlayerControllerComponent_TacticalSprint* SprintComp = (UFortPlayerControllerComponent_TacticalSprint*)PlayerController->GetComponentByClass(UFortPlayerControllerComponent_TacticalSprint::StaticClass());

                if (SprintComp)
                {
                    IFortSprintOverrideComponentInterface* SprintInterface = *(IFortSprintOverrideComponentInterface**)(__int64(SprintComp) + 0xA0);

                    static void (*TryToSprint)(IFortSprintOverrideComponentInterface* SprintInterface) = decltype(TryToSprint)(SprintInterface->VTable[0x5]);
                    static void (*EnableTacticalSprint)(IFortSprintOverrideComponentInterface* SprintInterface, bool bEnabled) = decltype(EnableTacticalSprint)(SprintInterface->VTable[0x7]);

                    EnableTacticalSprint(SprintInterface, true);
                    TryToSprint(SprintInterface);
                }
            }
        }

        Sleep(1000 / 30);
    }
}

void Main()
{
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    SetConsoleTitleA("Sarah 21.00: Setting up");
    Sarah::Offsets::Init();
    ReplicationOffsets::Init();
    auto FrontEndGameMode = (AFortGameMode*) UWorld::GetWorld()->AuthorityGameMode;
    while (FrontEndGameMode->MatchState != L"InProgress");
    Sleep(3000);

    LogCategory = FName(L"LogGameserver");
    //Utils::Patch<uint8_t>(Sarah::Offsets::ImageBase + 0x1205c90, 0xc3);
    MH_Initialize();
    for (auto& HookFunc : _HookFuncs)
        HookFunc();
    MH_EnableHook(MH_ALL_HOOKS);
    *(bool*)Sarah::Offsets::GIsClient = false;
    *(bool*)(Sarah::Offsets::GIsClient + 1) = true;
    srand((uint32_t)time(0));
    Lategame::Init();


    Log(L"ImageBase: %p", (LPVOID)Sarah::Offsets::ImageBase);

    CreateThread(0, 0, HotKeyThread, 0, 0, 0);
    //UWorld::GetWorld()->AddToRoot();
    UWorld::GetWorld()->OwningGameInstance->LocalPlayers.Remove(0);

    UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), bCreative ? L"open Creative_NoApollo_Terrain" : L"open Artemis_Terrain", nullptr);
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        std::thread(Main).detach();
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

