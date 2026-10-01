#ifndef DEF_TRANSMOG_H
#define DEF_TRANSMOG_H

#include "Player.h"
#include "ItemTemplate.h"
#include "Config.h"
#include "ScriptMgr.h"
#include "ObjectGuid.h"
#include "UpdateFields.h"
#include "ObjectMgr.h"
#include "Item.h"
#include "WorldSession.h"
#include "DatabaseEnv.h"
#include "GameEventMgr.h"
#include <array>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <sstream>
#include <shared_mutex>
#include <mutex>

// Reserved entry used to hide an armor-slot appearance without an item template.
constexpr uint32 HIDDEN_ITEM_ID = 999999;
// Maximum appearance entries shown in one gossip page.
constexpr uint8 MAX_ITEMS_PER_PAGE = 20;

// Shared result codes keep gossip and addon failures consistent.
enum class TransmogApplyResult : uint8
{
    Success,
    AlreadyApplied,
    InvalidSlot,
    EmptySlot,
    InvalidAppearance,
    NotEnoughMoney
};

enum MenuId : uint32
{
    MENU_MAIN = 1000,
    MENU_SELECT_SLOT,
    MENU_HIDE_SLOT,
    MENU_REMOVE_SLOT,
    MENU_REMOVE_ALL,
    MENU_APPLY,
    MENU_PAGE
};

enum TransmogString : uint32
{
    LANG_TRANSMOG_OK = 1,
    LANG_TRANSMOG_INVALID_SRC,
    LANG_TRANSMOG_ALL_REMOVED,
    LANG_TRANSMOG_HIDDEN,
    LANG_TRANSMOG_SLOT_REMOVED,
    LANG_TRANSMOG_NO_MONEY,
    LANG_TRANSMOG_REMOVE_ALL,
    LANG_TRANSMOG_REMOVE_ALL_ASK,
    LANG_TRANSMOG_BACK,
    LANG_TRANSMOG_HIDE_SLOT,
    LANG_TRANSMOG_REMOVE_SLOT,
    LANG_TRANSMOG_NEXT_PAGE,
    LANG_TRANSMOG_PREVIOUS_PAGE,
    LANG_TRANSMOG_NO_APPEARANCES,
    LANG_TRANSMOG_APPEARANCE_ADDED,
    LANG_TRANSMOG_FREE,
    LANG_TRANSMOG_EMPTY_SLOT
};

inline std::string const& Tstr(WorldSession* session, uint32 id)
{
    return *session->GetModuleString("mod-transmog-plus", id);
}

class Transmog
{
public:
    static Transmog* instance();

    void LoadConfig();

    bool Enable;
    uint32 PriceCopper;
    std::set<uint32> Allowed;
    std::set<uint32> NotAllowed;

    bool AllowPoor;
    bool AllowCommon;
    bool AllowUncommon;
    bool AllowRare;
    bool AllowEpic;
    bool AllowLegendary;
    bool AllowArtifact;
    bool AllowHeirloom;

    bool AllowMixedArmorTypes;
    bool AllowMixedOffhandArmorTypes;
    bool AllowLowerTiers;
    bool AllowMixedWeaponHandedness;
    bool AllowFishingPoles;
    // 0 = strict, 1 = modern grouped weapons, 2 = unrestricted.
    uint8 AllowMixedWeaponTypes;

    bool IgnoreReqRace;
    bool IgnoreReqClass;
    bool IgnoreReqSkill;
    bool IgnoreReqSpell;
    bool IgnoreReqEvent;
    bool IgnoreReqStats;

    // Obtainable appearances up to this item level count as collected on every account; 0 disables it.
    uint32 UnlockItemLevel;

    // Unlocked items that look identical, ordered from the lowest item level.
    struct UnlockedLook
    {
        uint64 key;
        std::vector<uint32> items;
    };

    // Unlock data is built once at startup and only read afterwards, so it needs no lock.
    std::unordered_set<uint32> unlockedAppearances;
    std::vector<UnlockedLook> unlockedLooks;
    // Display IDs mapped to the first display ID with the same models, textures and effects.
    std::unordered_map<uint32, uint32> visualGroups;

    // Account appearance data is shared while logged-in characters reference it.
    std::unordered_map<uint32, std::unordered_set<uint32>> collectionCache;
    std::unordered_map<uint32, uint32> collectionRefCounts;
    // Slot state is protected separately because it changes during equipment hooks.
    std::unordered_map<ObjectGuid, std::array<uint32, EQUIPMENT_SLOT_END>> slotMap;
    std::unordered_map<ObjectGuid, uint8> selectionCache;
    mutable std::shared_mutex slotMapMutex;
    mutable std::shared_mutex collectionMutex;

    // Player login and logout hooks own the collection cache lifecycle.
    void LoadCollectionForAccount(uint32 accountId);
    void UnrefCollectionForAccount(uint32 accountId);
    bool AddCollectedAppearance(uint32 accountId, uint32 itemId);
    // Unlocked appearances count as collected without per-account rows.
    bool HasCollectedAppearance(uint32 accountId, uint32 itemId) const;

    void LoadUnlockedAppearances();
    bool IsUnlockedAppearance(uint32 itemId) const;
    // Items with the same key look identical in the same kind of slot.
    uint64 GetLookKey(ItemTemplate const* proto) const;

    uint32 GetAppearanceCost(uint32 fakeEntry) const;
    // Gossip and addon adapters use one server-side mutation path.
    TransmogApplyResult ApplyAppearance(Player* player, uint8 slot, uint32 fakeEntry);

    void LoadPlayerSlots(ObjectGuid guid);
    void UnloadPlayerSlots(ObjectGuid guid);
    void SetSlotAppearance(Player* player, uint8 slot, uint32 fakeEntry);
    uint32 GetSlotAppearance(ObjectGuid guid, uint8 slot) const;
    void ApplySlot(Player* player, uint8 slot, Item* item);
    // Refresh methods synchronize stored state with client-visible item fields.
    void RefreshSlot(Player* player, uint8 slot);
    void RefreshAllSlots(Player* player);
    void ClearAllSlots(Player* player);
    uint32 GetVisibleEntryForSlot(Player const* player, uint8 slot, Item const* item) const;

    uint8 GetSelectedSlot(ObjectGuid guid) const;
    void SetSelectedSlot(ObjectGuid guid, uint8 slot);
    void ClearSelection(ObjectGuid guid);

    static std::string GetSlotName(uint8 slot);
    static std::string GetSlotIcon(uint8 slot, uint32 width, uint32 height, int x, int y);
    static std::string GetItemIcon(uint32 entry, uint32 width, uint32 height, int x, int y);
    static std::string GetItemLink(uint32 entry, WorldSession* session);
    static std::string GetSlotGossipIcon(Player* player, uint8 slot);
    static uint16 GetVisibleItemIndex(uint8 slot);

    // Gossip and addon list responses share the same filtered appearance set.
    // unlockedOnly receives the listed items that come from the unlock rather than the account collection.
    static std::vector<ItemTemplate const*> GetValidAppearances(Player* player, ItemTemplate const* targetTemplate,
        std::unordered_set<uint32>* unlockedOnly = nullptr);
};

#define sTransmog Transmog::instance()

bool TransmogRules_IsArmorSlot(uint8 slot);
bool TransmogRules_IsArmorProficiencySpell(uint32 spellId);
bool TransmogRules_IsAllowed(uint32 entry);
bool TransmogRules_IsValidOffhandArmor(uint32 subClass, uint32 invType);
bool TransmogRules_IsTieredArmorSubclass(uint32 subClass);
bool TransmogRules_TierAvailable(Player const* player, uint32 tier);
bool TransmogRules_IsRangedWeapon(uint32 itemClass, uint32 subClass);
bool TransmogRules_CanNeverTransmog(ItemTemplate const* proto);
bool TransmogRules_IsCollectible(ItemTemplate const* proto);
bool TransmogRules_IsItemTransmogrifiable(Player const* player, ItemTemplate const* proto);
bool TransmogRules_SuitableForTransmogrification(Player const* player, ItemTemplate const* proto);
bool TransmogRules_CanTransmogrifyItemWithItem(Player const* player, ItemTemplate const* target, ItemTemplate const* source);

#endif
