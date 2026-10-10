#include "KillMovePrompt.h"
#include "Settings.h"

namespace
{
    void OnMessage(SKSE::MessagingInterface::Message* a_msg)
    {
        switch (a_msg->type) {
        case SKSE::MessagingInterface::kDataLoaded:
            KillMovePrompt::Get()->Init();
            break;
        case SKSE::MessagingInterface::kPreLoadGame:
        case SKSE::MessagingInterface::kNewGame:
            KillMovePrompt::Get()->Reset();
            break;
        default:
            break;
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
    SKSE::Init(a_skse);  // also sets up Documents\My Games\Skyrim Special Edition\SKSE\KillMovePrompt.log
    SKSE::log::info("KillMovePrompt loaded");

    Settings::Get().Load();
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
    return true;
}
