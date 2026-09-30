#include "Transmog.h"

namespace
{
    std::set<uint32> ParseEntryList(std::string const& value)
    {
    // Configuration entry lists are whitespace-separated numeric item IDs.
    std::set<uint32> entries;
        std::istringstream stream(value);
        for (uint32 entry; stream >> entry;)
            entries.insert(entry);
        return entries;
    }
}

// Load all policy switches before any player or appearance request is handled.
void Transmog::LoadConfig()
{
    Enable = sConfigMgr->GetOption<bool>("Transmog.Enable", true);
    PriceCopper = sConfigMgr->GetOption<uint32>("Transmog.PriceCopper", 1000);

    Allowed = ParseEntryList(sConfigMgr->GetOption<std::string>("Transmog.Allowed", ""));
    NotAllowed = ParseEntryList(sConfigMgr->GetOption<std::string>("Transmog.NotAllowed", ""));
    UnlockItemLevel = sConfigMgr->GetOption<uint32>("Transmog.UnlockItemLevel", 0);

    AllowPoor = sConfigMgr->GetOption<bool>("Transmog.AllowPoor", true);
    AllowCommon = sConfigMgr->GetOption<bool>("Transmog.AllowCommon", true);
    AllowUncommon = sConfigMgr->GetOption<bool>("Transmog.AllowUncommon", true);
    AllowRare = sConfigMgr->GetOption<bool>("Transmog.AllowRare", true);
    AllowEpic = sConfigMgr->GetOption<bool>("Transmog.AllowEpic", true);
    AllowLegendary = sConfigMgr->GetOption<bool>("Transmog.AllowLegendary", true);
    AllowArtifact = sConfigMgr->GetOption<bool>("Transmog.AllowArtifact", true);
    AllowHeirloom = sConfigMgr->GetOption<bool>("Transmog.AllowHeirloom", true);

    AllowMixedArmorTypes = sConfigMgr->GetOption<bool>("Transmog.AllowMixedArmorTypes", false);
    AllowMixedOffhandArmorTypes = sConfigMgr->GetOption<bool>("Transmog.AllowMixedOffhandArmorTypes", false);
    AllowLowerTiers = sConfigMgr->GetOption<bool>("Transmog.AllowLowerTiers", false);
    AllowMixedWeaponHandedness = sConfigMgr->GetOption<bool>("Transmog.AllowMixedWeaponHandedness", true);
    AllowFishingPoles = sConfigMgr->GetOption<bool>("Transmog.AllowFishingPoles", false);
    AllowMixedWeaponTypes = sConfigMgr->GetOption<uint8>("Transmog.AllowMixedWeaponTypes", 1);

    IgnoreReqRace = sConfigMgr->GetOption<bool>("Transmog.IgnoreReqRace", false);
    IgnoreReqClass = sConfigMgr->GetOption<bool>("Transmog.IgnoreReqClass", false);
    IgnoreReqSkill = sConfigMgr->GetOption<bool>("Transmog.IgnoreReqSkill", false);
    IgnoreReqSpell = sConfigMgr->GetOption<bool>("Transmog.IgnoreReqSpell", false);
    IgnoreReqEvent = sConfigMgr->GetOption<bool>("Transmog.IgnoreReqEvent", false);
    IgnoreReqStats = sConfigMgr->GetOption<bool>("Transmog.IgnoreReqStats", false);
}
