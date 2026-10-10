#pragma once

struct Settings
{
    // [Input]
    std::uint32_t key        = 0x22;    // DirectInput scan code, 0x22 = G
    std::uint32_t gamepadKey = 0x1000;  // 0x1000 = A
    bool          holdToExecute = false;
    float         holdDuration  = 0.6f;
    std::string   promptText    = "Execute";
    std::uint32_t promptColor   = 0xFF5555FF;  // AABBGGRR (red)

    // [Target]
    float maxDistance      = 250.0f;  // game units (~3.5 m)
    float maxFacingAngle   = 70.0f;   // degrees either side of where the player faces
    float maxHeightDiff    = 80.0f;   // game units; 0 = no limit
    bool  preferCrosshair  = true;    // use the actor under the crosshair if it qualifies
    bool  requireWeaponDrawn = true;
    bool  requireHostile   = true;
    bool  excludeEssential = true;
    bool  excludeFollowers = true;

    // [Conditions]  -- prompt appears if ANY of these is true
    bool  alwaysShow        = false;
    float healthThreshold   = 0.25f;  // 0..1 of max health; 0 disables this check
    bool  allowStaggered    = true;
    bool  allowBleedout     = true;

    // [Kaputt]
    bool                                       useKaputtPrecondition = true;
    std::set<std::string, std::less<>>         requiredTags;
    std::set<std::string, std::less<>>         bannedTags;

    // [General]
    float scanInterval   = 0.1f;   // seconds between target scans
    float cooldown       = 1.5f;   // seconds after a killmove before the prompt can show again
    bool  sendModEvent   = true;   // fires "KillMovePrompt_Execute" for Papyrus mods

    void Load();

    static Settings& Get()
    {
        static Settings s;
        return s;
    }
};
