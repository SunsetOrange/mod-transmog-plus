local Transmog = _G.Transmog

transmogOutfits = {}
Transmog.availableTransmogItems = {}
Transmog.ItemButtons = {}
Transmog.currentTransmogSlotName = nil
Transmog.currentTransmogSlot = nil
Transmog.currentTransmogItemClass = nil
Transmog.currentPage = 1
Transmog.totalPages = 1
Transmog.ipp = 15
Transmog.numTransmogs = {}
Transmog.transmogDataFromServer = {}
Transmog.transmogStatusFromServer = {}
Transmog.transmogStatusToServer = {}
Transmog.tab = ''
Transmog.deliberating = {}
-- Appearances the server lists through the item-level unlock, as [slot][itemID] = true.
Transmog.unlockedAppearances = {}
-- Set once the server reports the unlock is enabled; shows the Unlocked tab.
Transmog.unlockedTabEnabled = false
Transmog.equippedItems = {}
Transmog.currentOutfit = nil
Transmog.equippedTransmogs = {}

Transmog.localCache = {}
