#include "Transmog.h"

// Keep one shared collection cache per account while logged-in players reference it.
// Load one account cache and reuse it for all characters from that account.
void Transmog::LoadCollectionForAccount(uint32 accountId)
{
    {
        // Readers can inspect the shared cache concurrently.
        std::shared_lock<std::shared_mutex> lock(collectionMutex);
        if (collectionCache.contains(accountId))
            return;
    }

    std::unordered_set<uint32> items;
    QueryResult result = CharacterDatabase.Query("SELECT item_template_id FROM mod_transmog_plus_appearances WHERE account_id = {}", accountId);
    if (result)
    {
        do
        {
            items.insert((*result)[0].Get<uint32>());
        } while (result->NextRow());
    }

    std::unique_lock<std::shared_mutex> lock(collectionMutex);
    if (!collectionCache.contains(accountId))
        collectionCache.emplace(accountId, std::move(items));
}

// Release the account cache only after the last character stops using it.
void Transmog::UnrefCollectionForAccount(uint32 accountId)
{
    std::unique_lock<std::shared_mutex> lock(collectionMutex);
    auto refIt = collectionRefCounts.find(accountId);
    if (refIt == collectionRefCounts.end())
        return;

    if (--refIt->second == 0)
    {
        collectionRefCounts.erase(refIt);
        collectionCache.erase(accountId);
    }
}

// Collection updates use an exclusive lock so readers never observe a partial insert.
bool Transmog::AddCollectedAppearance(uint32 accountId, uint32 itemId)
{
    std::unique_lock<std::shared_mutex> lock(collectionMutex);
    auto accountIt = collectionCache.find(accountId);
    if (accountIt == collectionCache.end())
    {
        collectionCache.emplace(accountId, std::unordered_set<uint32>{ itemId });
        return true;
    }

    auto result = accountIt->second.insert(itemId);
    return result.second;
}

// Unlocked appearances belong to every account, so only other items need the account cache.
bool Transmog::HasCollectedAppearance(uint32 accountId, uint32 itemId) const
{
    if (IsUnlockedAppearance(itemId))
        return true;

    std::shared_lock<std::shared_mutex> lock(collectionMutex);
    auto accountIt = collectionCache.find(accountId);
    return accountIt != collectionCache.end() && accountIt->second.contains(itemId);
}
