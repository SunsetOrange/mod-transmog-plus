#include "Transmog.h"
#include "DBCFileLoader.h"
#include "DBCStores.h"
#include "DisableMgr.h"
#include "Log.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "Util.h"
#include "World.h"
#include <algorithm>
#include <iterator>
#include <string>
#include <string_view>

namespace
{
    // ItemDisplayInfo.dbc has 25 four-byte columns; the core only keeps the ID and the icon.
    constexpr char ItemDisplayInfoFormat[] = "nssssssiiiiiiiissssssssii";
    constexpr uint32 ItemDisplayInfoColumns = 25;
    static_assert(sizeof(ItemDisplayInfoFormat) - 1 == ItemDisplayInfoColumns);

    // Columns that decide how an item is drawn: models, textures, geosets, flags and effects.
    // Icons (5-6) and the sound index (12) are left out because they do not change the look.
    constexpr uint32 LookTextColumns[] = { 1, 2, 3, 4, 15, 16, 17, 18, 19, 20, 21, 22 };
    constexpr uint32 LookNumberColumns[] = { 7, 8, 9, 10, 11, 13, 14, 23, 24 };

    // World tables that hand items to players; quests, recipes and starting outfits are read from memory.
    constexpr std::string_view ItemSourceQueries[] =
    {
        "SELECT DISTINCT Item FROM creature_loot_template WHERE Reference = 0",
        "SELECT DISTINCT Item FROM gameobject_loot_template WHERE Reference = 0",
        "SELECT DISTINCT Item FROM item_loot_template WHERE Reference = 0",
        "SELECT DISTINCT Item FROM reference_loot_template WHERE Reference = 0",
        "SELECT DISTINCT Item FROM fishing_loot_template WHERE Reference = 0",
        "SELECT DISTINCT Item FROM pickpocketing_loot_template WHERE Reference = 0",
        "SELECT DISTINCT Item FROM mail_loot_template WHERE Reference = 0",
        "SELECT DISTINCT Item FROM spell_loot_template WHERE Reference = 0",
        "SELECT DISTINCT item FROM npc_vendor WHERE item > 0",
        "SELECT DISTINCT item FROM game_event_npc_vendor",
        "SELECT DISTINCT ItemID FROM achievement_reward WHERE ItemID > 0",
        "SELECT DISTINCT itemid FROM playercreateinfo_item"
    };

    // Title prefixes of placeholder quests that were never removed, compared in lower case.
    constexpr std::string_view DeadQuestTitlePrefixes[] =
    {
        "deprecated", "unused", "<unused>", "[unused]", "<nyi>", "[nyi]", "<txt>", "[ph]", "reuse"
    };

    // Players cannot take disabled or placeholder quests, so their rewards are not obtainable.
    bool IsQuestAvailable(Quest const* quest)
    {
        if (sDisableMgr->IsDisabledFor(DISABLE_TYPE_QUEST, quest->GetQuestId(), nullptr))
            return false;

        std::string title = quest->GetTitle();
        std::transform(title.begin(), title.end(), title.begin(), charToLower);
        return std::none_of(std::begin(DeadQuestTitlePrefixes), std::end(DeadQuestTitlePrefixes),
            [&title](std::string_view prefix) { return title.starts_with(prefix); });
    }

    // Blizzard names NPC-only equipment "Monster - ..." or "NPC Equip ...", and marks retired copies.
    bool IsNpcOrRetiredItem(ItemTemplate const* proto)
    {
        std::string name = proto->Name1;
        std::transform(name.begin(), name.end(), name.begin(), charToLower);
        return name.starts_with("monster -") || name.starts_with("npc equip") ||
            name.find("deprecated") != std::string::npos;
    }

    // Group display IDs that draw identically and collect the displays that draw nothing at all.
    void LoadDisplayLooks(std::unordered_map<uint32, uint32>& visualGroups, std::unordered_set<uint32>& emptyDisplays)
    {
        std::string const path = sWorld->GetDataPath() + "dbc/ItemDisplayInfo.dbc";
        DBCFileLoader dbc;
        if (!dbc.Load(path.c_str(), ItemDisplayInfoFormat) || dbc.GetCols() != ItemDisplayInfoColumns)
        {
            LOG_WARN("module", "Transmog: could not read {}, so identical looks are only merged by display ID.", path);
            return;
        }

        std::unordered_map<std::string, uint32> groupByLook;
        for (uint32 row = 0; row < dbc.GetNumRows(); ++row)
        {
            DBCFileLoader::Record record = dbc.getRecord(row);
            uint32 displayId = record.getUInt(0);

            std::string look;
            bool drawsSomething = false;
            for (uint32 column : LookTextColumns)
            {
                // Client file paths are case-insensitive.
                std::string text = record.getString(column);
                std::transform(text.begin(), text.end(), text.begin(), charToLower);
                drawsSomething |= !text.empty();
                look += text;
                look += '|';
            }

            for (uint32 column : LookNumberColumns)
            {
                look += std::to_string(record.getUInt(column));
                look += '|';
            }

            if (!drawsSomething)
                emptyDisplays.insert(displayId);

            auto [groupIt, inserted] = groupByLook.try_emplace(std::move(look), displayId);
            if (!inserted)
                visualGroups[displayId] = groupIt->second;
        }
    }

    // Gather every item a player can get from loot, vendors, rewards, recipes or starting outfits.
    std::unordered_set<uint32> CollectObtainableItems()
    {
        std::unordered_set<uint32> items;

        for (std::string_view query : ItemSourceQueries)
        {
            QueryResult result = WorldDatabase.Query(query);
            if (!result)
                continue;

            do
            {
                items.insert((*result)[0].Get<uint32>());
            } while (result->NextRow());
        }

        for (auto const& [questId, quest] : sObjectMgr->GetQuestTemplates())
        {
            if (!IsQuestAvailable(quest))
                continue;

            items.insert(std::begin(quest->RewardItemId), std::end(quest->RewardItemId));
            items.insert(std::begin(quest->RewardChoiceItemId), std::end(quest->RewardChoiceItemId));
        }

        // Profession recipes create their item through a create-item spell effect.
        for (uint32 i = 0; i < sSkillLineAbilityStore.GetNumRows(); ++i)
        {
            SkillLineAbilityEntry const* ability = sSkillLineAbilityStore.LookupEntry(i);
            if (!ability || !IsProfessionSkill(ability->SkillLine))
                continue;

            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(ability->Spell);
            if (!spellInfo)
                continue;

            for (SpellEffectInfo const& effect : spellInfo->GetEffects())
                if (effect.IsEffect(SPELL_EFFECT_CREATE_ITEM) || effect.IsEffect(SPELL_EFFECT_CREATE_ITEM_2))
                    items.insert(effect.ItemType);
        }

        for (uint32 i = 0; i < sCharStartOutfitStore.GetNumRows(); ++i)
        {
            CharStartOutfitEntry const* outfit = sCharStartOutfitStore.LookupEntry(i);
            if (!outfit)
                continue;

            for (int32 itemId : outfit->ItemId)
                if (itemId > 0)
                    items.insert(uint32(itemId));
        }

        return items;
    }
}

// Build the appearances every account can use without collecting them first.
void Transmog::LoadUnlockedAppearances()
{
    unlockedAppearances.clear();
    unlockedLooks.clear();
    visualGroups.clear();

    if (!UnlockItemLevel)
        return;

    uint32 oldMSTime = getMSTime();

    std::unordered_set<uint32> emptyDisplays;
    LoadDisplayLooks(visualGroups, emptyDisplays);

    std::unordered_map<uint64, std::vector<uint32>> itemsByLook;
    for (uint32 itemId : CollectObtainableItems())
    {
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
        if (!TransmogRules_IsCollectible(proto) || proto->ItemLevel > UnlockItemLevel)
            continue;

        // Items without a drawable display have no model to show.
        if (!proto->DisplayInfoID || !sItemDisplayInfoStore.LookupEntry(proto->DisplayInfoID) ||
            emptyDisplays.contains(proto->DisplayInfoID))
            continue;

        if (IsNpcOrRetiredItem(proto))
            continue;

        unlockedAppearances.insert(itemId);
        itemsByLook[GetLookKey(proto)].push_back(itemId);
    }

    unlockedLooks.reserve(itemsByLook.size());
    for (auto& [key, items] : itemsByLook)
    {
        // The lowest item level stays the listed item when the threshold changes.
        std::sort(items.begin(), items.end(), [](uint32 left, uint32 right)
        {
            uint32 leftLevel = sObjectMgr->GetItemTemplate(left)->ItemLevel;
            uint32 rightLevel = sObjectMgr->GetItemTemplate(right)->ItemLevel;
            return leftLevel != rightLevel ? leftLevel < rightLevel : left < right;
        });

        unlockedLooks.push_back({ key, std::move(items) });
    }

    LOG_INFO("module", ">> Loaded {} transmog appearances up to item level {} ({} distinct looks) in {} ms",
        unlockedAppearances.size(), UnlockItemLevel, unlockedLooks.size(), GetMSTimeDiffToNow(oldMSTime));
}

bool Transmog::IsUnlockedAppearance(uint32 itemId) const
{
    return unlockedAppearances.contains(itemId);
}

// One-handers draw the same in either hand, while a robe draws differently from a chest.
uint64 Transmog::GetLookKey(ItemTemplate const* proto) const
{
    uint32 displayId = proto->DisplayInfoID;
    auto groupIt = visualGroups.find(displayId);
    if (groupIt != visualGroups.end())
        displayId = groupIt->second;

    uint32 inventoryType = proto->InventoryType;
    if (inventoryType == INVTYPE_WEAPONMAINHAND || inventoryType == INVTYPE_WEAPONOFFHAND)
        inventoryType = INVTYPE_WEAPON;

    return (uint64(displayId) << 32) | inventoryType;
}
