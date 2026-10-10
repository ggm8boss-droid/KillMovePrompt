#include "Settings.h"

namespace
{
    // GetPrivateProfileString needs an absolute path (a relative one is looked up in C:\\Windows).
    const std::string& IniPath()
    {
        static const std::string path =
            (std::filesystem::current_path() / "Data" / "SKSE" / "Plugins" / "KillMovePrompt.ini").string();
        return path;
    }

    std::string ReadStr(const char* sec, const char* key, const std::string& def)
    {
        char buf[512]{};
        GetPrivateProfileStringA(sec, key, def.c_str(), buf, sizeof(buf), IniPath().c_str());
        std::string s = buf;
        // strip trailing inline comment and whitespace
        if (auto p = s.find(';'); p != std::string::npos) s.erase(p);
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
        return s.empty() ? def : s;
    }

    std::uint32_t ReadHex(const char* sec, const char* key, std::uint32_t def)
    {
        auto s = ReadStr(sec, key, "");
        if (s.empty()) return def;
        try { return static_cast<std::uint32_t>(std::stoul(s, nullptr, 0)); }  // accepts 0x22 or 34
        catch (...) { return def; }
    }

    float ReadFloat(const char* sec, const char* key, float def)
    {
        auto s = ReadStr(sec, key, "");
        if (s.empty()) return def;
        try { return std::stof(s); }
        catch (...) { return def; }
    }

    bool ReadBool(const char* sec, const char* key, bool def)
    {
        auto s = ReadStr(sec, key, def ? "true" : "false");
        for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s == "true" || s == "1" || s == "yes" || s == "on";
    }

    std::set<std::string, std::less<>> ReadList(const char* sec, const char* key)
    {
        std::set<std::string, std::less<>> out;
        auto s = ReadStr(sec, key, "");
        std::string cur;
        auto flush = [&] {
            while (!cur.empty() && std::isspace(static_cast<unsigned char>(cur.back()))) cur.pop_back();
            while (!cur.empty() && std::isspace(static_cast<unsigned char>(cur.front()))) cur.erase(cur.begin());
            if (!cur.empty()) out.insert(cur);
            cur.clear();
        };
        for (char c : s) {
            if (c == ',') flush();
            else cur += c;
        }
        flush();
        return out;
    }
}

void Settings::Load()
{
    key           = ReadHex("Input", "Key", key);
    gamepadKey    = ReadHex("Input", "GamepadKey", gamepadKey);
    holdToExecute = ReadBool("Input", "HoldToExecute", holdToExecute);
    holdDuration  = ReadFloat("Input", "HoldDuration", holdDuration);
    promptText    = ReadStr("Input", "PromptText", promptText);
    promptColor   = ReadHex("Input", "PromptColor", promptColor);

    maxDistance        = ReadFloat("Target", "MaxDistance", maxDistance);
    maxFacingAngle     = ReadFloat("Target", "MaxFacingAngle", maxFacingAngle);
    maxHeightDiff      = ReadFloat("Target", "MaxHeightDiff", maxHeightDiff);
    preferCrosshair    = ReadBool("Target", "PreferCrosshair", preferCrosshair);
    requireWeaponDrawn = ReadBool("Target", "RequireWeaponDrawn", requireWeaponDrawn);
    requireHostile     = ReadBool("Target", "RequireHostile", requireHostile);
    excludeEssential   = ReadBool("Target", "ExcludeEssential", excludeEssential);
    excludeFollowers   = ReadBool("Target", "ExcludeFollowers", excludeFollowers);

    alwaysShow      = ReadBool("Conditions", "AlwaysShow", alwaysShow);
    healthThreshold = ReadFloat("Conditions", "HealthThreshold", healthThreshold);
    allowStaggered  = ReadBool("Conditions", "AllowStaggered", allowStaggered);
    allowBleedout   = ReadBool("Conditions", "AllowBleedout", allowBleedout);

    useKaputtPrecondition = ReadBool("Kaputt", "UseKaputtPrecondition", useKaputtPrecondition);
    requiredTags          = ReadList("Kaputt", "RequiredTags");
    bannedTags            = ReadList("Kaputt", "BannedTags");

    scanInterval = std::max(0.02f, ReadFloat("General", "ScanInterval", scanInterval));
    cooldown     = ReadFloat("General", "Cooldown", cooldown);
    sendModEvent = ReadBool("General", "SendModEvent", sendModEvent);

    SKSE::log::info("Settings: key=0x{:X} pad=0x{:X} hold={} dist={} angle={} hpThreshold={} stagger={} tags+={} tags-={}",
        key, gamepadKey, holdToExecute, maxDistance, maxFacingAngle, healthThreshold, allowStaggered,
        requiredTags.size(), bannedTags.size());
}
