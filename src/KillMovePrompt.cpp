#include "KillMovePrompt.h"
#include "Settings.h"

namespace
{
    using Clock = std::chrono::steady_clock;

    Clock::time_point::rep Now() { return Clock::now().time_since_epoch().count(); }

    Clock::time_point::rep After(float a_seconds)
    {
        return (Clock::now() + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<float>(a_seconds)))
            .time_since_epoch()
            .count();
    }
}

void KillMovePrompt::Init()
{
    auto& s = Settings::Get();

    clientID_ = SkyPromptAPI::RequestClientID();
    if (clientID_ == 0) {
        SKSE::log::error("SkyPrompt not found (or wrong version). KillMovePrompt is disabled.");
        return;
    }
    SKSE::log::info("SkyPrompt client id {}", clientID_);

    auto api = kaputt::RequestKaputtAPI();
    if (std::holds_alternative<kaputt::KaputtAPI*>(api)) {
        kaputt_ = std::get<kaputt::KaputtAPI*>(api);
        SKSE::log::info("Kaputt API acquired");
    } else {
        SKSE::log::warn("Kaputt API unavailable: {}. Only the mod event will fire on key press.",
            std::get<std::string>(api));
    }

    buttons_ = { {
        { RE::INPUT_DEVICE::kKeyboard, static_cast<SkyPromptAPI::ButtonID>(s.key) },
        { RE::INPUT_DEVICE::kGamepad, static_cast<SkyPromptAPI::ButtonID>(s.gamepadKey) },
    } };
    promptText_ = s.promptText;

    if (auto* src = SKSE::GetCrosshairRefEventSource()) {
        src->AddEventSink(this);
    }

    StartScanner();
}

void KillMovePrompt::Reset()
{
    Hide();
    {
        std::scoped_lock l(crosshairLock_);
        crosshairRef_.reset();
    }
    cooldownUntil_ = 0;
}

// ---------------------------------------------------------------- scanning

void KillMovePrompt::StartScanner()
{
    if (running_.exchange(true)) return;

    std::thread([this] {
        while (running_) {
            std::this_thread::sleep_for(std::chrono::duration<float>(Settings::Get().scanInterval));
            // Only one queued tick at a time so nothing piles up while the game is paused/loading.
            if (!tickQueued_.exchange(true)) {
                SKSE::GetTaskInterface()->AddTask([this] {
                    Tick();
                    tickQueued_ = false;
                });
            }
        }
    }).detach();
}

void KillMovePrompt::Tick()
{
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* ui     = RE::UI::GetSingleton();

    RE::Actor* target = nullptr;
    if (player && player->Is3DLoaded() && ui && !ui->GameIsPaused() && Now() >= cooldownUntil_) {
        target = FindTarget(player);
    }

    const RE::ActorHandle newHandle = target ? target->GetHandle() : RE::ActorHandle{};
    const bool changed = newHandle != shownTarget_;

    if (changed || needResend_.exchange(false)) {
        if (target) Show(target);
        else        Hide();
    }
}

RE::Actor* KillMovePrompt::FindTarget(RE::PlayerCharacter* a_player)
{
    auto& s = Settings::Get();

    if (a_player->IsDead() || a_player->IsInKillMove() || a_player->IsOnMount()) return nullptr;
    if (s.requireWeaponDrawn && !a_player->AsActorState()->IsWeaponDrawn()) return nullptr;

    // 1) The actor under the crosshair, if it qualifies.
    if (s.preferCrosshair) {
        RE::NiPointer<RE::TESObjectREFR> ref;
        {
            std::scoped_lock l(crosshairLock_);
            ref = crosshairRef_.get();
        }
        if (ref) {
            if (auto* actor = ref->As<RE::Actor>(); actor && IsValidTarget(a_player, actor)) {
                return actor;
            }
        }
    }

    // 2) Otherwise the closest qualifying actor in front of the player (works in melee
    //    where the crosshair often sits off the enemy).
    RE::Actor* best     = nullptr;
    float      bestDist = std::numeric_limits<float>::max();
    const auto ppos     = a_player->GetPosition();

    if (auto* lists = RE::ProcessLists::GetSingleton()) {
        for (auto& handle : lists->highActorHandles) {
            auto actorPtr = handle.get();
            auto* actor   = actorPtr.get();
            if (!actor || !IsValidTarget(a_player, actor)) continue;

            const float d = ppos.GetDistance(actor->GetPosition());
            if (d < bestDist) {
                bestDist = d;
                best     = actor;
            }
        }
    }
    return best;
}

bool KillMovePrompt::IsValidTarget(RE::PlayerCharacter* a_player, RE::Actor* a_target) const
{
    auto& s = Settings::Get();

    if (!a_target || a_target == a_player) return false;
    if (!a_target->Is3DLoaded() || a_target->IsDisabled()) return false;
    if (a_target->IsDead() || a_target->IsInKillMove() || a_target->IsOnMount()) return false;

    // Geometry
    const auto  ppos = a_player->GetPosition();
    const auto  tpos = a_target->GetPosition();
    if (ppos.GetDistance(tpos) > s.maxDistance) return false;
    if (a_player->GetHeadingAngle(tpos, true) > s.maxFacingAngle) return false;
    if (s.maxHeightDiff > 0.0f && std::abs(tpos.z - ppos.z) > s.maxHeightDiff) return false;

    // Who
    if (s.requireHostile && !a_target->IsHostileToActor(a_player)) return false;
    if (s.excludeFollowers && (a_target->IsPlayerTeammate() || a_target->IsCommandedActor())) return false;
    if (s.excludeEssential && (a_target->IsEssential() || a_target->IsProtected())) return false;

    // When (any one is enough)
    bool ok = s.alwaysShow;
    if (!ok && s.healthThreshold > 0.0f && HealthPercent(a_target) <= s.healthThreshold) ok = true;
    if (!ok && s.allowStaggered && a_target->AsActorState()->IsStaggered()) ok = true;
    if (!ok && s.allowBleedout && a_target->AsActorState()->IsBleedingOut()) ok = true;
    if (!ok) return false;

    // Let Kaputt veto targets it has no animation for (race, weapon, etc).
    if (kaputt_ && s.useKaputtPrecondition) {
        if (!kaputt_->isReady() || !kaputt_->precondition(a_player, a_target)) return false;
    }
    return true;
}

float KillMovePrompt::HealthPercent(RE::Actor* a_actor)
{
    auto* av  = a_actor->AsActorValueOwner();
    float cur = av->GetActorValue(RE::ActorValue::kHealth);
    float max = av->GetPermanentActorValue(RE::ActorValue::kHealth) +
                a_actor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth);
    return max > 0.0f ? cur / max : 1.0f;
}

// ---------------------------------------------------------------- prompt

void KillMovePrompt::Show(RE::Actor* a_target)
{
    auto& s = Settings::Get();

    if (shownTarget_) {
        SkyPromptAPI::RemovePrompt(this, clientID_);
    }

    prompts_[0] = SkyPromptAPI::Prompt(
        promptText_,
        1,                                // eventID
        0,                                // actionID
        s.holdToExecute ? SkyPromptAPI::PromptType::kHold : SkyPromptAPI::PromptType::kSinglePress,
        a_target->GetFormID(),            // anchors the prompt on this actor
        buttons_,
        s.promptColor,
        s.holdToExecute ? s.holdDuration : 0.0f);

    shownTarget_ = a_target->GetHandle();
    if (!SkyPromptAPI::SendPrompt(this, clientID_)) {
        SKSE::log::debug("SendPrompt failed; will retry");
        shownTarget_.reset();
    }
}

void KillMovePrompt::Hide()
{
    if (!shownTarget_) return;
    SkyPromptAPI::RemovePrompt(this, clientID_);
    shownTarget_.reset();
}

std::span<const SkyPromptAPI::Prompt> KillMovePrompt::GetPrompts() const
{
    return prompts_;
}

void KillMovePrompt::ProcessEvent(SkyPromptAPI::PromptEvent a_event) const
{
    auto* self = Get();

    switch (a_event.type) {
    case SkyPromptAPI::PromptEventType::kAccepted:
        {
            RE::ActorHandle target = self->shownTarget_;
            SKSE::GetTaskInterface()->AddTask([self, target] { self->Execute(target); });
        }
        break;
    case SkyPromptAPI::PromptEventType::kTimeout:
    case SkyPromptAPI::PromptEventType::kRemovedByMod:
        // SkyPrompt dropped it; show it again on the next tick if still valid.
        self->needResend_ = true;
        break;
    default:
        break;
    }
}

// ---------------------------------------------------------------- execute

void KillMovePrompt::Execute(RE::ActorHandle a_target)
{
    auto& s       = Settings::Get();
    auto* player  = RE::PlayerCharacter::GetSingleton();
    auto  ptr     = a_target.get();
    auto* target  = ptr.get();

    Hide();
    if (!player || !target || target->IsDead() || target->IsInKillMove()) return;

    cooldownUntil_ = After(s.cooldown);

    bool played = false;
    if (kaputt_ && kaputt_->isReady()) {
        kaputt::SubmitInfoStruct info{ s.requiredTags, s.bannedTags };
        played = kaputt_->submit(player, target, info);
        SKSE::log::info("Kaputt submit on {:08X} '{}': {}", target->GetFormID(), target->GetName(),
            played ? "playing" : "no matching animation");
    }

    if (s.sendModEvent) {
        SKSE::ModCallbackEvent ev{ "KillMovePrompt_Execute", played ? "kaputt" : "none",
            played ? 1.0f : 0.0f, target };
        SKSE::GetModCallbackEventSource()->SendEvent(&ev);
    }

    if (!played) {
        cooldownUntil_ = After(0.3f);  // nothing played, allow a quick retry
    }
}

// ---------------------------------------------------------------- crosshair

RE::BSEventNotifyControl KillMovePrompt::ProcessEvent(const SKSE::CrosshairRefEvent* a_event,
    RE::BSTEventSource<SKSE::CrosshairRefEvent>*)
{
    std::scoped_lock l(crosshairLock_);
    if (a_event && a_event->crosshairRef) {
        crosshairRef_ = a_event->crosshairRef->CreateRefHandle();
    } else {
        crosshairRef_.reset();
    }
    return RE::BSEventNotifyControl::kContinue;
}
