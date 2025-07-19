#include "global.h"
#include "gflib.h"
#include "berry.h"
#include "event_data.h"
#include "graphics.h"
#include "item.h"
#include "item_use.h"
#include "item_menu.h"
#include "load_save.h"
#include "party_menu.h"
#include "pokeball.h"
#include "quest_log.h"
#include "strings.h"
#include "constants/hold_effects.h"
#include "constants/item.h"
#include "constants/item_effects.h"
#include "constants/items.h"
#include "constants/maps.h"

EWRAM_DATA struct BagPocket gBagPockets[POCKETS_COUNT] = {};

static const u8 *ItemId_GetPluralName(u16);
static bool32 DoesItemHavePluralName(u16);

// Item descriptions and data
#include "constants/moves.h"
#include "pokemon_summary_screen.h"
#include "data/pokemon/item_effects.h"
#include "data/items.h"

static inline u16 GetBagItemIdPocket(struct BagPocket *pocket, u32 pocketPos)
{
    return pocket->itemSlots[pocketPos].itemId;
}

static inline u16 GetBagItemQuantityPocket(struct BagPocket *pocket, u32 pocketPos)
{
    return pocket->itemSlots[pocketPos].quantity;
}

static inline void SetBagItemIdPocket(struct BagPocket *pocket, u32 pocketPos, u16 itemId)
{
    pocket->itemSlots[pocketPos].itemId = itemId;
}

static inline void SetBagItemQuantityPocket(struct BagPocket *pocket, u32 pocketPos, u16 newValue)
{
    pocket->itemSlots[pocketPos].quantity = newValue;
}

u16 GetBagItemId(enum Pocket pocketId, u32 pocketPos)
{
    return GetBagItemIdPocket(&gBagPockets[pocketId], pocketPos);
}

u16 GetBagItemQuantity(enum Pocket pocketId, u32 pocketPos)
{
    return GetBagItemQuantityPocket(&gBagPockets[pocketId], pocketPos);
}

// static void SetBagItemId(enum Pocket pocketId, u32 pocketPos, u16 itemId)
// {
//     SetBagItemIdPocket(&gBagPockets[pocketId], pocketPos, itemId);
// }

void SetBagItemQuantity(enum Pocket pocketId, u32 pocketPos, u16 newValue)
{
    SetBagItemQuantityPocket(&gBagPockets[pocketId], pocketPos, newValue);
}

u16 GetPCItemQuantity(u16 *quantity)
{
    return *quantity;
}

void SetPCItemQuantity(u16 *quantity, u16 newValue)
{
    *quantity = newValue;
}

void SetBagItemsPointers(void)
{
    gBagPockets[POCKET_ITEMS].itemSlots = gSaveBlock1Ptr->bag.items;
    gBagPockets[POCKET_ITEMS].capacity = BAG_ITEMS_COUNT;

    gBagPockets[POCKET_KEY_ITEMS].itemSlots = gSaveBlock1Ptr->bag.keyItems;
    gBagPockets[POCKET_KEY_ITEMS].capacity = BAG_KEYITEMS_COUNT;

    gBagPockets[POCKET_POKE_BALLS].itemSlots = gSaveBlock1Ptr->bag.pokeBalls;
    gBagPockets[POCKET_POKE_BALLS].capacity = BAG_POKEBALLS_COUNT;

    gBagPockets[POCKET_TM_HM].itemSlots = gSaveBlock1Ptr->bag.TMsHMs;
    gBagPockets[POCKET_TM_HM].capacity = BAG_TMHM_COUNT;

    gBagPockets[POCKET_BERRIES].itemSlots = gSaveBlock1Ptr->bag.berries;
    gBagPockets[POCKET_BERRIES].capacity = BAG_BERRIES_COUNT;
}

u8 *CopyItemName(u16 itemId, u8 * dest)
{
    return StringCopy(dest, GetItemName(itemId));
}

const u8 sText_s[] =_("s");

u8 *CopyItemNameHandlePlural(u16 itemId, u8 *dst, u32 quantity)
{
    if (quantity == 1)
    {
        return StringCopy(dst, GetItemName(itemId));
    }
    else if (DoesItemHavePluralName(itemId))
    {
        return StringCopy(dst, ItemId_GetPluralName(itemId));
    }
    else
    {
        u8 *end = StringCopy(dst, GetItemName(itemId));
        return StringCopy(end, sText_s);
    }
}

s8 BagPocketGetFirstEmptySlot(u8 pocketId)
{
    u16 i;

    for (i = 0; i < gBagPockets[pocketId].capacity; i++)
    {
        if (gBagPockets[pocketId].itemSlots[i].itemId == ITEM_NONE)
            return i;
    }

    return -1;
}

bool8 IsBagPocketNonEmpty(u8 pocketId)
{
    u8 i;

    for (i = 0; i < gBagPockets[pocketId].capacity; i++)
    {
        if (gBagPockets[pocketId].itemSlots[i].itemId != ITEM_NONE)
            return TRUE;
    }

    return FALSE;
}

bool8 CheckBagHasItem(u16 itemId, u16 count)
{
    u8 i;
    enum Pocket pocket = GetItemPocket(itemId);

    if (pocket >= POCKETS_COUNT)
        return FALSE;

    // Check for item slots that contain the item
    for (i = 0; i < gBagPockets[pocket].capacity; i++)
    {
        if (gBagPockets[pocket].itemSlots[i].itemId == itemId)
        {
            u16 quantity;
            // Does this item slot contain enough of the item?
            quantity = GetBagItemQuantity(pocket, i);
            if (quantity >= count)
                return TRUE;
                // RS and Emerald check whether there is enough of the
                // item across all stacks.
                // For whatever reason, FR/LG assume there's only one
                // stack of the item.
            else
                return FALSE;
        }
    }
    return FALSE;
}

bool8 HasAtLeastOneBerry(void)
{
    u16 itemId;
    bool8 exists;

    exists = CheckBagHasItem(ITEM_BERRY_POUCH, 1);
    if (!exists)
    {
        gSpecialVar_Result = FALSE;
        return FALSE;
    }
    for (itemId = FIRST_BERRY_INDEX; itemId <= LAST_BERRY_INDEX; itemId++)
    {
        exists = CheckBagHasItem(itemId, 1);
        if (exists)
        {
            gSpecialVar_Result = TRUE;
            return TRUE;
        }
    }

    gSpecialVar_Result = FALSE;
    return FALSE;
}

bool8 HasAtLeastOnePokeBall(void)
{
    u16 ballId;

    for (ballId = BALL_STRANGE; ballId < POKEBALL_COUNT; ballId++)
    {
        if (CheckBagHasItem(ballId, 1) == TRUE)
            return TRUE;
    }
    return FALSE;
}

bool8 CheckBagHasSpace(u16 itemId, u16 count)
{
    if (GetItemPocket(itemId) == POCKET_NONE)
        return FALSE;

    return GetFreeSpaceForItemInBag(itemId) >= count;
}

u32 GetFreeSpaceForItemInBag(u16 itemId)
{
    u8 i;
    u8 pocket = GetItemPocket(itemId);
    u16 ownedCount;
    u32 spaceForItem = 0;

    if (pocket >= POCKETS_COUNT)
        return 0;

    // Check space in any existing item slots that already contain this item
    for (i = 0; i < gBagPockets[pocket].capacity; i++)
    {
        if (gBagPockets[pocket].itemSlots[i].itemId == itemId)
        {
            ownedCount = GetBagItemQuantity(pocket, i);
            spaceForItem += max(0, MAX_BAG_ITEM_CAPACITY - ownedCount);
        }
        else if (gBagPockets[pocket].itemSlots[i].itemId == ITEM_NONE)
        {
            spaceForItem += MAX_BAG_ITEM_CAPACITY;
        }
    }
    return spaceForItem;
}

bool8 AddBagItem(u16 itemId, u16 count)
{
    u8 i;
    enum Pocket pocket = GetItemPocket(itemId);
    s8 idx;

    if (count == 0)
        return FALSE;

    if (pocket >= POCKETS_COUNT)
        return FALSE;

    for (i = 0; i < gBagPockets[pocket].capacity; i++)
    {
        if (gBagPockets[pocket].itemSlots[i].itemId == itemId)
        {
            u16 quantity;
            // Does this stack have room for more??
            quantity = GetBagItemQuantity(pocket, i);
            if (quantity + count <= MAX_BAG_ITEM_CAPACITY)
            {
                quantity += count;
                SetBagItemQuantity(pocket, i, quantity);
                return TRUE;
            }
            // RS and Emerald check whether there is enough of the
            // item across all stacks.
            // For whatever reason, FR/LG assume there's only one
            // stack of the item.
            else
                return FALSE;
        }
    }

    if (pocket == POCKET_TM_HM && !CheckBagHasItem(ITEM_TM_CASE, 1))
    {
        idx = BagPocketGetFirstEmptySlot(POCKET_KEY_ITEMS);
        if (idx == -1)
            return FALSE;
        gBagPockets[POCKET_KEY_ITEMS].itemSlots[idx].itemId = ITEM_TM_CASE;
        SetBagItemQuantity(POCKET_KEY_ITEMS, idx, 1);
    }

    if (pocket == POCKET_BERRIES && !CheckBagHasItem(ITEM_BERRY_POUCH, 1))
    {
        idx = BagPocketGetFirstEmptySlot(POCKET_KEY_ITEMS);
        if (idx == -1)
            return FALSE;
        gBagPockets[POCKET_KEY_ITEMS].itemSlots[idx].itemId = ITEM_BERRY_POUCH;
        SetBagItemQuantity(POCKET_KEY_ITEMS, idx, 1);
        FlagSet(FLAG_SYS_GOT_BERRY_POUCH);
    }

    if (itemId == ITEM_BERRY_POUCH)
        FlagSet(FLAG_SYS_GOT_BERRY_POUCH);

    idx = BagPocketGetFirstEmptySlot(pocket);
    if (idx == -1)
        return FALSE;

    gBagPockets[pocket].itemSlots[idx].itemId = itemId;
    SetBagItemQuantity(pocket, idx, count);
    return TRUE;
}

bool8 RemoveBagItem(u16 itemId, u16 count)
{
    u8 i;
    enum Pocket pocket = GetItemPocket(itemId);

    if (pocket >= POCKETS_COUNT)
        return FALSE;

    if (itemId == ITEM_NONE)
        return FALSE;

    // Check for item slots that contain the item
    for (i = 0; i < gBagPockets[pocket].capacity; i++)
    {
        if (gBagPockets[pocket].itemSlots[i].itemId == itemId)
        {
            u16 quantity;
            // Does this item slot contain enough of the item?
            quantity = GetBagItemQuantity(pocket, i);
            if (quantity >= count)
            {
                quantity -= count;
                SetBagItemQuantity(pocket, i, quantity);
                if (quantity == 0)
                    gBagPockets[pocket].itemSlots[i].itemId = ITEM_NONE;
                return TRUE;
            }
            // RS and Emerald check whether there is enough of the
            // item across all stacks.
            // For whatever reason, FR/LG assume there's only one
            // stack of the item.
            else
                return FALSE;
        }
    }
    return FALSE;
}

u8 GetPocketByItemId(u16 itemId)
{
    return GetItemPocket(itemId); // wow such important
}

void ClearPCItemSlots(void)
{
    u16 i;

    for (i = 0; i < PC_ITEMS_COUNT; i++)
    {
        gSaveBlock1Ptr->pcItems[i].itemId = ITEM_NONE;
        SetPCItemQuantity(&gSaveBlock1Ptr->pcItems[i].quantity, 0);
    }
}

void MoveItemSlotInPocket(enum Pocket pocketId, u32 from, u32 to)
{
    if (from != to)
    {
        u32 i;
        s8 shift = -1;
        struct BagPocket *pocket = &gBagPockets[pocketId];

        // Record the values at "from"
        u16 fromItemId = GetBagItemIdPocket(pocket, from),
            fromQuantity = GetBagItemQuantityPocket(pocket, from);

        // Shuffle items between "to" and "from"
        if (to > from)
        {
            to--;
            shift = 1;
        }

        for (i = from; i == to - shift; i += shift)
        {
            SetBagItemIdPocket(pocket, i, GetBagItemIdPocket(pocket, i + shift));
            SetBagItemQuantityPocket(pocket, i, GetBagItemQuantityPocket(pocket, i + shift));
        }

        // Move the saved "from" to "to"
        SetBagItemIdPocket(pocket, to, fromItemId);
        SetBagItemQuantityPocket(pocket, to, fromQuantity);
    }
}

void MoveItemSlotInPC(struct ItemSlot *itemSlots_, u32 from, u32 to_)
{
    // dumb assignments needed to match
    struct ItemSlot *itemSlots = itemSlots_;
    u32 to = to_;

    if (from != to)
    {
        s16 i, count;
        struct ItemSlot firstSlot = itemSlots[from];

        if (to > from)
        {
            to--;
            for (i = from, count = to; i < count; i++)
                itemSlots[i] = itemSlots[i + 1];
        }
        else
        {
            for (i = from, count = to; i > count; i--)
                itemSlots[i] = itemSlots[i - 1];
        }
        itemSlots[to] = firstSlot;
    }
}

void ClearBag(void)
{
    CpuFastFill(0, &gSaveBlock1Ptr->bag, sizeof(struct Bag));
}

s8 PCItemsGetFirstEmptySlot(void)
{
    s8 i;

    for (i = 0; i < PC_ITEMS_COUNT; i++)
    {
        if (gSaveBlock1Ptr->pcItems[i].itemId == ITEM_NONE)
            return i;
    }

    return -1;
}

u8 CountItemsInPC(void)
{
    u8 count = 0;
    u8 i;

    for (i = 0; i < PC_ITEMS_COUNT; i++)
    {
        if (gSaveBlock1Ptr->pcItems[i].itemId != ITEM_NONE)
            count++;
    }

    return count;
}

bool8 CheckPCHasItem(u16 itemId, u16 count)
{
    u8 i;
    u16 quantity;

    for (i = 0; i < PC_ITEMS_COUNT; i++)
    {
        if (gSaveBlock1Ptr->pcItems[i].itemId == itemId)
        {
            quantity = GetPCItemQuantity(&gSaveBlock1Ptr->pcItems[i].quantity);
            if (quantity >= count)
                return TRUE;
        }
    }

    return FALSE;
}

bool8 AddPCItem(u16 itemId, u16 count)
{
    u8 i;
    u16 quantity;
    s8 idx;

    for (i = 0; i < PC_ITEMS_COUNT; i++)
    {
        if (gSaveBlock1Ptr->pcItems[i].itemId == itemId)
        {
            quantity = GetPCItemQuantity(&gSaveBlock1Ptr->pcItems[i].quantity);
            if (quantity + count <= MAX_PC_ITEM_CAPACITY)
            {
                quantity += count;
                SetPCItemQuantity(&gSaveBlock1Ptr->pcItems[i].quantity, quantity);
                return TRUE;
            }
            else
                return FALSE;
        }
    }

    idx = PCItemsGetFirstEmptySlot();
    if (idx == -1)
        return FALSE;

    gSaveBlock1Ptr->pcItems[idx].itemId = itemId;
    SetPCItemQuantity(&gSaveBlock1Ptr->pcItems[idx].quantity, count);
    return TRUE;
}

void RemovePCItem(u16 itemId, u16 count)
{
    u32 i;
    u16 quantity;

    if (itemId == ITEM_NONE)
        return;

    for (i = 0; i < PC_ITEMS_COUNT; i++)
    {
        if (gSaveBlock1Ptr->pcItems[i].itemId == itemId)
            break;
    }

    if (i != PC_ITEMS_COUNT)
    {
        quantity = GetPCItemQuantity(&gSaveBlock1Ptr->pcItems[i].quantity) - count;
        SetPCItemQuantity(&gSaveBlock1Ptr->pcItems[i].quantity, quantity);
        if (quantity == 0)
            gSaveBlock1Ptr->pcItems[i].itemId = ITEM_NONE;
    }
}

void ItemPcCompaction(void)
{
    u16 i, j;
    struct ItemSlot tmp;

    for (i = 0; i < PC_ITEMS_COUNT - 1; i++)
    {
        for (j = i + 1; j < PC_ITEMS_COUNT; j++)
        {
            if (gSaveBlock1Ptr->pcItems[i].itemId == ITEM_NONE)
            {
                tmp = gSaveBlock1Ptr->pcItems[i];
                gSaveBlock1Ptr->pcItems[i] = gSaveBlock1Ptr->pcItems[j];
                gSaveBlock1Ptr->pcItems[j] = tmp;
            }
        }
    }
}

void RegisteredItemHandleBikeSwap(void)
{
    switch (gSaveBlock1Ptr->registeredItem)
    {
    case ITEM_MACH_BIKE:
        gSaveBlock1Ptr->registeredItem = ITEM_ACRO_BIKE;
        break;
    case ITEM_ACRO_BIKE:
        gSaveBlock1Ptr->registeredItem = ITEM_MACH_BIKE;
        break;
    }
}

static void SwapItemSlots(enum Pocket pocketId, u32 pocketPosA, u16 pocketPosB)
{
    struct ItemSlot *itemA = &gBagPockets[pocketId].itemSlots[pocketPosA],
                    *itemB = &gBagPockets[pocketId].itemSlots[pocketPosB],
                    temp;
    SWAP(*itemA, *itemB, temp);
}

void CompactItemsInBagPocket(enum Pocket pocketId)
{
    struct BagPocket *bagPocket = &gBagPockets[pocketId];
    u16 i, j;

    for (i = 0; i < bagPocket->capacity - 1; i++)
    {
        for (j = i + 1; j < bagPocket->capacity; j++)
        {
            if (bagPocket->itemSlots[i].quantity == 0)
                SwapItemSlots(pocketId, i, j);
        }
    }
}

static u32 GetSortIndex(u32 itemId)
{
    if (!IsItemHM(itemId))
        return itemId;

    return (itemId - (NUM_TECHNICAL_MACHINES + NUM_HIDDEN_MACHINES));
}

void SortBerriesOrTMHMs(enum Pocket pocketId)
{
    u16 i, j;

    for (i = 0; i < gBagPockets[pocketId].capacity - 1; i++)
    {
        for (j = i + 1; j < gBagPockets[pocketId].capacity; j++)
        {
            if (GetBagItemQuantity(pocketId, i) != 0)
            {
                if (GetBagItemQuantity(pocketId, j) == 0)
                    continue;
                if (GetSortIndex(GetBagItemId(pocketId, i)) <= GetSortIndex(GetBagItemId(pocketId, j)))
                    continue;
            }
            SwapItemSlots(pocketId, i, j);
        }
    }
}

u16 CountTotalItemQuantityInBag(u16 itemId)
{
    u16 i;
    u16 ownedCount = 0;
    enum Pocket pocketId = GetItemPocket(itemId);

    for (i = 0; i < gBagPockets[pocketId].capacity; i++)
    {
        if (GetBagItemId(pocketId, i) == itemId)
            ownedCount += GetBagItemQuantity(pocketId, i);
    }

    return ownedCount;
}

void TrySetObtainedItemQuestLogEvent(u16 itemId)
{
    // Only some key items trigger this event
    if (itemId == ITEM_OAKS_PARCEL
     || itemId == ITEM_POKE_FLUTE
     || itemId == ITEM_SECRET_KEY
     || itemId == ITEM_BIKE_VOUCHER
     || itemId == ITEM_GOLD_TEETH
     || itemId == ITEM_OLD_AMBER
     || itemId == ITEM_CARD_KEY
     || itemId == ITEM_LIFT_KEY
     || itemId == ITEM_HELIX_FOSSIL
     || itemId == ITEM_DOME_FOSSIL
     || itemId == ITEM_SILPH_SCOPE
     || itemId == ITEM_BICYCLE
     || itemId == ITEM_TOWN_MAP
     || itemId == ITEM_VS_SEEKER
     || itemId == ITEM_TEACHY_TV
     || itemId == ITEM_RAINBOW_PASS
     || itemId == ITEM_TEA
     || itemId == ITEM_POWDER_JAR
     || itemId == ITEM_RUBY
     || itemId == ITEM_SAPPHIRE)
    {
        if (itemId != ITEM_TOWN_MAP || (gSaveBlock1Ptr->location.mapGroup == MAP_GROUP(MAP_PALLET_TOWN_RIVALS_HOUSE) && gSaveBlock1Ptr->location.mapNum == MAP_NUM(MAP_PALLET_TOWN_RIVALS_HOUSE)))
        {
            struct QuestLogEvent_StoryItem * data = malloc(sizeof(*data));
            data->itemId = itemId;
            data->mapSec = gMapHeader.regionMapSectionId;
            SetQuestLogEvent(QL_EVENT_OBTAINED_STORY_ITEM, (const u16 *)data);
            free(data);
        }
    }
}

u16 SanitizeItemId(u16 itemId)
{
    if (itemId >= ITEMS_COUNT)
        return ITEM_NONE;
    return itemId;
}

const u8 * GetItemName(u16 itemId)
{
    return gItemsInfo[SanitizeItemId(itemId)].name;
}

u32 GetItemPrice(u16 itemId)
{
    return gItemsInfo[SanitizeItemId(itemId)].price;
}

static bool32 DoesItemHavePluralName(u16 itemId)
{
    return (gItemsInfo[SanitizeItemId(itemId)].pluralName[0] != '\0');
}

static const u8 *ItemId_GetPluralName(u16 itemId)
{
    return gItemsInfo[SanitizeItemId(itemId)].pluralName;
}

u8 GetItemHoldEffect(u16 itemId)
{
    return gItemsInfo[SanitizeItemId(itemId)].holdEffect;
}

u8 GetItemHoldEffectParam(u16 itemId)
{
    return gItemsInfo[SanitizeItemId(itemId)].holdEffectParam;
}

const u8 * GetItemDescription(u16 itemId)
{
    return gItemsInfo[SanitizeItemId(itemId)].description;
}

u8 GetItemImportance(u16 itemId)
{
    return gItemsInfo[SanitizeItemId(itemId)].importance;
}

u8 GetItemConsumability(u16 itemId)
{
    return !gItemsInfo[SanitizeItemId(itemId)].notConsumed;
}

enum Pocket GetItemPocket(u16 itemId)
{
    return gItemsInfo[SanitizeItemId(itemId)].pocket;
}

u8 GetItemType(u16 itemId)
{
    return gItemsInfo[SanitizeItemId(itemId)].type;
}

ItemUseFunc GetItemFieldFunc(u16 itemId)
{
    return gItemsInfo[SanitizeItemId(itemId)].fieldUseFunc;
}

bool8 GetItemBattleUsage(u16 itemId)
{
    u16 item = SanitizeItemId(itemId);    
    if (item == ITEM_ENIGMA_BERRY_E_READER)
    {
        switch (GetItemEffectType(gSpecialVar_ItemId))
        {
            case ITEM_EFFECT_X_ITEM:
                return EFFECT_ITEM_INCREASE_STAT;
            case ITEM_EFFECT_HEAL_HP:
                return EFFECT_ITEM_RESTORE_HP;
            case ITEM_EFFECT_CURE_POISON:
            case ITEM_EFFECT_CURE_SLEEP:
            case ITEM_EFFECT_CURE_BURN:
            case ITEM_EFFECT_CURE_FREEZE:
            case ITEM_EFFECT_CURE_PARALYSIS:
            case ITEM_EFFECT_CURE_ALL_STATUS:
            case ITEM_EFFECT_CURE_CONFUSION:
            case ITEM_EFFECT_CURE_INFATUATION:
                return EFFECT_ITEM_CURE_STATUS;
            case ITEM_EFFECT_HEAL_PP:
                return EFFECT_ITEM_RESTORE_PP;
            default:
                return 0;
        }
    }
    else
        return gItemsInfo[item].battleUsage;
}

u16 GetItemSecondaryId(u16 itemId)
{
    return gItemsInfo[SanitizeItemId(itemId)].secondaryId;
}

u32 GetItemFlingPower(u32 itemId)
{
    return gItemsInfo[SanitizeItemId(itemId)].flingPower;
}

const u8 *GetItemEffect(u32 itemId)
{
    if (itemId == ITEM_ENIGMA_BERRY_E_READER)
    #if FREE_ENIGMA_BERRY == FALSE
        return gSaveBlock1Ptr->enigmaBerry.itemEffect;
    #else
        return 0;
    #endif //FREE_ENIGMA_BERRY
    else
        return gItemsInfo[SanitizeItemId(itemId)].effect;
}


u32 GetItemStatus1Mask(u16 itemId)
{
    const u8 *effect = GetItemEffect(itemId);
    switch (effect[3])
    {
        case ITEM3_PARALYSIS:
            return STATUS1_PARALYSIS;
        case ITEM3_FREEZE:
            return STATUS1_FREEZE | STATUS1_FROSTBITE;
        case ITEM3_BURN:
            return STATUS1_BURN;
        case ITEM3_POISON:
            return STATUS1_PSN_ANY | STATUS1_TOXIC_COUNTER;
        case ITEM3_SLEEP:
            return STATUS1_SLEEP;
        case ITEM3_STATUS_ALL:
            return STATUS1_ANY | STATUS1_TOXIC_COUNTER;
    }
    return 0;
}

u32 GetItemStatus2Mask(u16 itemId)
{
    const u8 *effect = GetItemEffect(itemId);
    if (effect[3] & ITEM3_STATUS_ALL)
        return STATUS2_INFATUATION | STATUS2_CONFUSION;
    else if (effect[0] & ITEM0_INFATUATION)
        return STATUS2_INFATUATION;
    else if (effect[3] & ITEM3_CONFUSION)
        return STATUS2_CONFUSION;
    else
        return 0;
}

u32 GetItemSellPrice(u32 itemId)
{
    return GetItemPrice(itemId) / ITEM_SELL_FACTOR;
}

bool8 IsItemTM(u16 itemId)
{
    itemId = SanitizeItemId(itemId);
    return ITEM_TM01 <= itemId && itemId <= ITEM_TM100;
}

bool8 IsItemHM(u16 itemId)
{
    itemId = SanitizeItemId(itemId);
    return ITEM_HM01 <= itemId && itemId <= ITEM_HM08;
}

bool8 IsItemTMHM(u16 itemId)
{
    return IsItemTM(itemId) || IsItemHM(itemId);
}

bool8 IsItemBall(u16 itemId)
{
    itemId = SanitizeItemId(itemId);
    return FIRST_BALL <= itemId && itemId <= LAST_BALL;
}
