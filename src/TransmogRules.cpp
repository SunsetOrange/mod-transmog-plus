#include "Transmog.h"

namespace
{
// Ordered from plate to cloth so proficiency determines the highest usable tier.
    constexpr uint32 ArmorSpellIds[4] = { 750, 8737, 9077, 9078 };
    constexpr uint32 ArmorTiers[4] =
    {
        ITEM_SUBCLASS_ARMOR_PLATE,
        ITEM_SUBCLASS_ARMOR_MAIL,
        ITEM_SUBCLASS_ARMOR_LEATHER,
        ITEM_SUBCLASS_ARMOR_CLOTH
    };
}

// Hidden appearances are restricted to equipment slots that display armor.
bool TransmogRules_IsArmorSlot(uint8 slot)
{
    switch (slot)
    {
        case EQUIPMENT_SLOT_HEAD:
        case EQUIPMENT_SLOT_SHOULDERS:
        case EQUIPMENT_SLOT_BODY:
        case EQUIPMENT_SLOT_CHEST:
        case EQUIPMENT_SLOT_WAIST:
        case EQUIPMENT_SLOT_LEGS:
        case EQUIPMENT_SLOT_FEET:
        case EQUIPMENT_SLOT_WRISTS:
        case EQUIPMENT_SLOT_HANDS:
        case EQUIPMENT_SLOT_BACK:
        case EQUIPMENT_SLOT_TABARD:
            return true;
        default:
            return false;
    }
}

bool TransmogRules_IsArmorProficiencySpell(uint32 spellId)
{
    for (uint32 armorSpellId : ArmorSpellIds)
    {
        if (spellId == armorSpellId)
            return true;
    }

    return false;
}

bool TransmogRules_IsRangedWeapon(uint32 itemClass, uint32 subClass)
{
    return itemClass == ITEM_CLASS_WEAPON && (
        subClass == ITEM_SUBCLASS_WEAPON_BOW ||
        subClass == ITEM_SUBCLASS_WEAPON_GUN ||
        subClass == ITEM_SUBCLASS_WEAPON_CROSSBOW);
}

bool TransmogRules_IsValidOffhandArmor(uint32 subClass, uint32 invType)
{
    return subClass == ITEM_SUBCLASS_ARMOR_BUCKLER ||
        (subClass == ITEM_SUBCLASS_ARMOR_MISC && invType == INVTYPE_HOLDABLE) ||
        subClass == ITEM_SUBCLASS_ARMOR_SHIELD;
}

bool TransmogRules_IsTieredArmorSubclass(uint32 subClass)
{
    return subClass == ITEM_SUBCLASS_ARMOR_PLATE ||
        subClass == ITEM_SUBCLASS_ARMOR_MAIL ||
        subClass == ITEM_SUBCLASS_ARMOR_LEATHER ||
        subClass == ITEM_SUBCLASS_ARMOR_CLOTH;
}

bool TransmogRules_GetHighestArmorTier(Player const* player, uint32& highest)
{
    for (int i = 0; i < 4; ++i)
    {
        if (player->HasSpell(ArmorSpellIds[i]))
        {
            highest = ArmorTiers[i];
            return true;
        }
    }
    highest = ITEM_SUBCLASS_ARMOR_CLOTH;
    return false;
}

// A character can use its armor tier and every less restrictive tier.
bool TransmogRules_TierAvailable(Player const* player, uint32 tier)
{
    if (!TransmogRules_IsTieredArmorSubclass(tier))
        return false;

    uint32 highest = ITEM_SUBCLASS_ARMOR_CLOTH;
    TransmogRules_GetHighestArmorTier(player, highest);

    switch (highest)
    {
        case ITEM_SUBCLASS_ARMOR_PLATE: return true;
        case ITEM_SUBCLASS_ARMOR_MAIL: return tier != ITEM_SUBCLASS_ARMOR_PLATE;
        case ITEM_SUBCLASS_ARMOR_LEATHER: return tier == ITEM_SUBCLASS_ARMOR_LEATHER || tier == ITEM_SUBCLASS_ARMOR_CLOTH;
        case ITEM_SUBCLASS_ARMOR_CLOTH: return tier == ITEM_SUBCLASS_ARMOR_CLOTH;
        default: return false;
    }
}

// Explicit allow entries bypass normal source restrictions by design.
bool TransmogRules_IsAllowed(uint32 entry)
{
    return sTransmog->Allowed.contains(entry);
}

// Explicit deny entries remain authoritative even when other rules allow an item.
bool TransmogRules_IsNotAllowed(uint32 entry)
{
    return sTransmog->NotAllowed.contains(entry);
}

// Map item quality values to the corresponding configuration switch.
bool TransmogRules_IsAllowedQuality(uint32 quality)
{
    switch (quality)
    {
        case ITEM_QUALITY_POOR: return sTransmog->AllowPoor;
        case ITEM_QUALITY_NORMAL: return sTransmog->AllowCommon;
        case ITEM_QUALITY_UNCOMMON: return sTransmog->AllowUncommon;
        case ITEM_QUALITY_RARE: return sTransmog->AllowRare;
        case ITEM_QUALITY_EPIC: return sTransmog->AllowEpic;
        case ITEM_QUALITY_LEGENDARY: return sTransmog->AllowLegendary;
        case ITEM_QUALITY_ARTIFACT: return sTransmog->AllowArtifact;
        case ITEM_QUALITY_HEIRLOOM: return sTransmog->AllowHeirloom;
        default: return false;
    }
}

// Reject item categories that cannot safely provide a transmog appearance.
bool TransmogRules_CanNeverTransmog(ItemTemplate const* proto)
{
    return proto->InventoryType == INVTYPE_BAG ||
        proto->InventoryType == INVTYPE_RELIC ||
        proto->InventoryType == INVTYPE_FINGER ||
        proto->InventoryType == INVTYPE_TRINKET ||
        proto->InventoryType == INVTYPE_AMMO ||
        proto->InventoryType == INVTYPE_QUIVER ||
        proto->InventoryType == INVTYPE_NECK;
}

// Only armor and weapons that can show an appearance enter a collection.
bool TransmogRules_IsCollectible(ItemTemplate const* proto)
{
    return proto && (proto->Class == ITEM_CLASS_ARMOR || proto->Class == ITEM_CLASS_WEAPON) &&
        !TransmogRules_CanNeverTransmog(proto);
}

// Apply source-item quality, class, and requirement checks before collection use.
bool TransmogRules_IsItemTransmogrifiable(Player const* player, ItemTemplate const* proto)
{
    if (!proto)
        return false;

    if (TransmogRules_IsNotAllowed(proto->ItemId))
        return false;

    if (!sTransmog->AllowFishingPoles && proto->Class == ITEM_CLASS_WEAPON && proto->SubClass == ITEM_SUBCLASS_WEAPON_FISHING_POLE)
        return false;

    if (!TransmogRules_IsAllowedQuality(proto->Quality))
        return false;

    if (!sTransmog->IgnoreReqEvent && proto->HolidayId && !IsHolidayActive((HolidayIds)proto->HolidayId))
        return false;

    if (!sTransmog->IgnoreReqStats)
    {
        if (!proto->RandomProperty && !proto->RandomSuffix && proto->StatsCount > 0)
        {
            bool found = false;
            for (uint8 i = 0; i < proto->StatsCount; ++i)
            {
                if (proto->ItemStat[i].ItemStatValue != 0)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
                return false;
        }
    }

    return true;
}

// Combine hard exclusions with configurable requirement and type policies.
bool TransmogRules_SuitableForTransmogrification(Player const* player, ItemTemplate const* proto)
{
    if (!player || !proto)
        return false;

    if (proto->Class != ITEM_CLASS_ARMOR && proto->Class != ITEM_CLASS_WEAPON)
        return false;

    if (TransmogRules_IsAllowed(proto->ItemId))
        return true;

    if (!TransmogRules_IsItemTransmogrifiable(player, proto))
        return false;

    uint32 subclassSkill = proto->GetSkill();
    if (proto->SubClass > 0 && subclassSkill && player->GetSkillValue(subclassSkill) == 0)
    {
        if (proto->Class == ITEM_CLASS_ARMOR && !sTransmog->AllowMixedArmorTypes)
            return false;

        if (proto->Class == ITEM_CLASS_WEAPON && sTransmog->AllowMixedWeaponTypes != 2)
            return false;
    }

    if (proto->HasFlag2(ITEM_FLAG2_FACTION_HORDE) && player->GetTeamId() != TEAM_HORDE)
        return false;

    if (proto->HasFlag2(ITEM_FLAG2_FACTION_ALLIANCE) && player->GetTeamId() != TEAM_ALLIANCE)
        return false;

    if (!sTransmog->IgnoreReqClass && (proto->AllowableClass & player->getClassMask()) == 0)
        return false;

    if (!sTransmog->IgnoreReqRace && (proto->AllowableRace & player->getRaceMask()) == 0)
        return false;

    if (!sTransmog->IgnoreReqSkill && proto->RequiredSkill != 0)
    {
        if (player->GetSkillValue(proto->RequiredSkill) == 0)
            return false;
        if (player->GetSkillValue(proto->RequiredSkill) < proto->RequiredSkillRank)
            return false;
    }

    if (!sTransmog->IgnoreReqSpell && proto->RequiredSpell != 0 && !player->HasSpell(proto->RequiredSpell))
        return false;

    return true;
}
