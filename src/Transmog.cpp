#include "Transmog.h"
#include <algorithm>

// One shared instance owns module configuration and in-memory state.
Transmog* Transmog::instance()
{
    static Transmog instance;
    return &instance;
}

std::string Transmog::GetSlotName(uint8 slot)
{
    switch (slot)
    {
        case EQUIPMENT_SLOT_HEAD: return "Head";
        case EQUIPMENT_SLOT_SHOULDERS: return "Shoulders";
        case EQUIPMENT_SLOT_BODY: return "Shirt";
        case EQUIPMENT_SLOT_CHEST: return "Chest";
        case EQUIPMENT_SLOT_WAIST: return "Waist";
        case EQUIPMENT_SLOT_LEGS: return "Legs";
        case EQUIPMENT_SLOT_FEET: return "Feet";
        case EQUIPMENT_SLOT_WRISTS: return "Wrists";
        case EQUIPMENT_SLOT_HANDS: return "Hands";
        case EQUIPMENT_SLOT_BACK: return "Back";
        case EQUIPMENT_SLOT_MAINHAND: return "Main Hand";
        case EQUIPMENT_SLOT_OFFHAND: return "Off Hand";
        case EQUIPMENT_SLOT_RANGED: return "Ranged";
        case EQUIPMENT_SLOT_TABARD: return "Tabard";
        default: return "";
    }
}

// Gossip icons use client texture tags rather than server-side icon objects.
std::string Transmog::GetSlotIcon(uint8 slot, uint32 width, uint32 height, int x, int y)
{
    std::ostringstream ss;
    ss << "|TInterface/PaperDoll/";
    switch (slot)
    {
        case EQUIPMENT_SLOT_HEAD: ss << "UI-PaperDoll-Slot-Head"; break;
        case EQUIPMENT_SLOT_SHOULDERS: ss << "UI-PaperDoll-Slot-Shoulder"; break;
        case EQUIPMENT_SLOT_BODY: ss << "UI-PaperDoll-Slot-Shirt"; break;
        case EQUIPMENT_SLOT_CHEST: ss << "UI-PaperDoll-Slot-Chest"; break;
        case EQUIPMENT_SLOT_WAIST: ss << "UI-PaperDoll-Slot-Waist"; break;
        case EQUIPMENT_SLOT_LEGS: ss << "UI-PaperDoll-Slot-Legs"; break;
        case EQUIPMENT_SLOT_FEET: ss << "UI-PaperDoll-Slot-Feet"; break;
        case EQUIPMENT_SLOT_WRISTS: ss << "UI-PaperDoll-Slot-Wrists"; break;
        case EQUIPMENT_SLOT_HANDS: ss << "UI-PaperDoll-Slot-Hands"; break;
        case EQUIPMENT_SLOT_BACK: ss << "UI-PaperDoll-Slot-Chest"; break;
        case EQUIPMENT_SLOT_MAINHAND: ss << "UI-PaperDoll-Slot-MainHand"; break;
        case EQUIPMENT_SLOT_OFFHAND: ss << "UI-PaperDoll-Slot-SecondaryHand"; break;
        case EQUIPMENT_SLOT_RANGED: ss << "UI-PaperDoll-Slot-Ranged"; break;
        case EQUIPMENT_SLOT_TABARD: ss << "UI-PaperDoll-Slot-Tabard"; break;
        default: ss << "UI-Backpack-EmptySlot";
    }
    ss << ":" << width << ":" << height << ":" << x << ":" << y << "|t";
    return ss.str();
}

std::string Transmog::GetItemIcon(uint32 entry, uint32 width, uint32 height, int x, int y)
{
    std::ostringstream ss;
    ss << "|TInterface";
    ItemTemplate const* temp = sObjectMgr->GetItemTemplate(entry);
    ItemDisplayInfoEntry const* dispInfo = nullptr;
    if (temp)
    {
        dispInfo = sItemDisplayInfoStore.LookupEntry(temp->DisplayInfoID);
        if (dispInfo)
            ss << "/ICONS/" << dispInfo->inventoryIcon;
    }
    if (!dispInfo)
        ss << "/InventoryItems/WoWUnknownItem01";
    ss << ":" << width << ":" << height << ":" << x << ":" << y << "|t";
    return ss.str();
}

// The hidden sentinel has no item template, so it uses a plain label.
std::string Transmog::GetItemLink(uint32 entry, WorldSession* session)
{
    if (entry == HIDDEN_ITEM_ID)
        return "(Hidden)";

    ItemTemplate const* temp = sObjectMgr->GetItemTemplate(entry);
    if (!temp)
        return "";

    int loc_idx = session->GetSessionDbLocaleIndex();
    std::string name = temp->Name1;
    if (ItemLocale const* il = sObjectMgr->GetItemLocale(entry))
        ObjectMgr::GetLocaleString(il->Name, loc_idx, name);

    std::ostringstream oss;
    oss << "|c" << std::hex << ItemQualityColors[temp->Quality] << std::dec
        << "|Hitem:" << entry << ":0:0:0:0:0:0:0:0:0|h[" << name << "]|h|r";
    return oss.str();
}

// Empty slots still need a stable icon for the slot-selection menu.
std::string Transmog::GetSlotGossipIcon(Player* player, uint8 slot)
{
    Item* equipped = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
    uint32 fakeEntry = sTransmog->GetSlotAppearance(player->GetGUID(), slot);

    if (!equipped)
        return GetSlotIcon(slot, 30, 30, -18, 0);
    if (fakeEntry == HIDDEN_ITEM_ID && TransmogRules_IsArmorSlot(slot))
        return "|TInterface/ICONS/inv_misc_enggizmos_27:30:30:-18:0|t";
    if (fakeEntry)
        return GetItemIcon(fakeEntry, 30, 30, -18, 0);
    return GetItemIcon(equipped->GetEntry(), 30, 30, -18, 0);
}

// Incompatible stored appearances are ignored until the equipped item changes.
uint32 Transmog::GetVisibleEntryForSlot(Player const* player, uint8 slot, Item const* item) const
{
    if (!item || slot >= EQUIPMENT_SLOT_END)
        return 0;

    uint32 fakeEntry = GetSlotAppearance(player->GetGUID(), slot);
    if (fakeEntry == 0)
        return item->GetEntry();
    if (fakeEntry == HIDDEN_ITEM_ID)
        return TransmogRules_IsArmorSlot(slot) ? HIDDEN_ITEM_ID : item->GetEntry();

    ItemTemplate const* sourceTemplate = sObjectMgr->GetItemTemplate(fakeEntry);
    if (sourceTemplate && TransmogRules_CanTransmogrifyItemWithItem(player, item->GetTemplate(), sourceTemplate))
        return fakeEntry;

    return item->GetEntry();
}

// Hidden armor appearances use the sentinel and are always free.
uint32 Transmog::GetAppearanceCost(uint32 fakeEntry) const
{
    return fakeEntry == HIDDEN_ITEM_ID ? 0 : PriceCopper;
}

// This is the single mutation path for validation, payment, persistence, and refresh.
TransmogApplyResult Transmog::ApplyAppearance(Player* player, uint8 slot, uint32 fakeEntry)
{
    if (!player || slot >= EQUIPMENT_SLOT_END)
        return TransmogApplyResult::InvalidSlot;

    if (fakeEntry == 0)
        return TransmogApplyResult::InvalidAppearance;

    if (GetSlotAppearance(player->GetGUID(), slot) == fakeEntry)
        return TransmogApplyResult::AlreadyApplied;

    Item* targetItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
    if (!targetItem)
        return TransmogApplyResult::EmptySlot;

    if (fakeEntry == HIDDEN_ITEM_ID)
    {
        if (!TransmogRules_IsArmorSlot(slot))
            return TransmogApplyResult::InvalidAppearance;
    }
    else
    {
        if (!HasCollectedAppearance(player->GetSession()->GetAccountId(), fakeEntry))
            return TransmogApplyResult::InvalidAppearance;

        ItemTemplate const* sourceTemplate = sObjectMgr->GetItemTemplate(fakeEntry);
        if (!sourceTemplate || !TransmogRules_CanTransmogrifyItemWithItem(player, targetItem->GetTemplate(), sourceTemplate))
            return TransmogApplyResult::InvalidAppearance;
    }

    uint32 cost = GetAppearanceCost(fakeEntry);
    if (cost > 0 && !player->HasEnoughMoney(cost))
        return TransmogApplyResult::NotEnoughMoney;

    if (cost > 0)
        player->ModifyMoney(-static_cast<int32>(cost), false);

    SetSlotAppearance(player, slot, fakeEntry);
    RefreshSlot(player, slot);
    return TransmogApplyResult::Success;
}

// The client stores two visible-item update fields per equipment slot.
uint16 Transmog::GetVisibleItemIndex(uint8 slot)
{
    return PLAYER_VISIBLE_ITEM_1_ENTRYID + (slot * 2);
}

// List construction applies collection, quality, requirement, and compatibility rules.
std::vector<ItemTemplate const*> Transmog::GetValidAppearances(Player* player, ItemTemplate const* targetTemplate)
{
    std::vector<ItemTemplate const*> result;
    if (!targetTemplate)
        return result;

    // Unlocked looks are skipped when the equipped item or a collected item already shows them.
    std::unordered_set<uint64> shownLooks{ sTransmog->GetLookKey(targetTemplate) };
    uint32 accountId = player->GetSession()->GetAccountId();

    {
        std::shared_lock<std::shared_mutex> lock(sTransmog->collectionMutex);
        auto accountIt = sTransmog->collectionCache.find(accountId);
        if (accountIt != sTransmog->collectionCache.end())
        {
            // Every collected item stays listed so saved outfits keep matching the list.
            for (uint32 itemId : accountIt->second)
            {
                ItemTemplate const* sourceTemplate = sObjectMgr->GetItemTemplate(itemId);
                if (!sourceTemplate ||
                    !TransmogRules_CanTransmogrifyItemWithItem(player, targetTemplate, sourceTemplate))
                    continue;

                result.push_back(sourceTemplate);
                shownLooks.insert(sTransmog->GetLookKey(sourceTemplate));
            }
        }
    }

    // Each remaining look lists its lowest-level item that this character can use.
    for (UnlockedLook const& look : sTransmog->unlockedLooks)
    {
        if (shownLooks.contains(look.key))
            continue;

        for (uint32 itemId : look.items)
        {
            ItemTemplate const* sourceTemplate = sObjectMgr->GetItemTemplate(itemId);
            if (sourceTemplate && TransmogRules_CanTransmogrifyItemWithItem(player, targetTemplate, sourceTemplate))
            {
                result.push_back(sourceTemplate);
                break;
            }
        }
    }

    std::sort(result.begin(), result.end(), [](ItemTemplate const* a, ItemTemplate const* b)
    {
        int qa = 7 - a->Quality;
        int qb = 7 - b->Quality;
        if (qa != qb)
            return qa < qb;
        return a->Name1 < b->Name1;
    });

    return result;
}
