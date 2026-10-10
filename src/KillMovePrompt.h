#pragma once

#include "SkyPrompt/API.hpp"
#include "kaputtAPI.h"

// Shows a SkyPrompt key prompt anchored on a valid enemy; pressing the key
// asks Kaputt to play a killmove on that enemy.
class KillMovePrompt final :
    public SkyPromptAPI::PromptSink,
    public RE::BSTEventSink<SKSE::CrosshairRefEvent>
{
public:
    static KillMovePrompt* Get()
    {
        static KillMovePrompt instance;
        return &instance;
    }

    void Init();      // call on kDataLoaded
    void Reset();     // call on load game / new game

    // SkyPromptAPI::PromptSink
    void                                   ProcessEvent(SkyPromptAPI::PromptEvent a_event) const override;
    std::span<const SkyPromptAPI::Prompt>  GetPrompts() const override;

    // Crosshair tracking
    RE::BSEventNotifyControl ProcessEvent(const SKSE::CrosshairRefEvent*                  a_event,
                                          RE::BSTEventSource<SKSE::CrosshairRefEvent>*    a_source) override;

private:
    KillMovePrompt() = default;

    void        StartScanner();
    void        Tick();                                  // main thread
    RE::Actor*  FindTarget(RE::PlayerCharacter* a_player);
    bool        IsValidTarget(RE::PlayerCharacter* a_player, RE::Actor* a_target) const;
    void        Show(RE::Actor* a_target);
    void        Hide();
    void        Execute(RE::ActorHandle a_target);       // main thread

    static float HealthPercent(RE::Actor* a_actor);

    SkyPromptAPI::ClientID clientID_{ 0 };
    kaputt::KaputtAPI*     kaputt_{ nullptr };

    std::array<std::pair<RE::INPUT_DEVICE, SkyPromptAPI::ButtonID>, 2> buttons_{};
    std::array<SkyPromptAPI::Prompt, 1>                                 prompts_;  // no {} — Prompt has an explicit ctor
    std::string                                                         promptText_;

    RE::ActorHandle  shownTarget_{};
    RE::ObjectRefHandle crosshairRef_{};
    std::mutex       crosshairLock_;

    std::atomic_bool tickQueued_{ false };
    std::atomic_bool needResend_{ false };
    std::atomic<std::chrono::steady_clock::time_point::rep> cooldownUntil_{ 0 };
    std::atomic_bool running_{ false };
};
