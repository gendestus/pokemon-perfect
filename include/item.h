#ifndef GUARD_ITEM_H
#define GUARD_ITEM_H

#include "constants/item.h"
#include "constants/items.h"
#include "constants/tms_hms.h"

typedef void (*ItemUseFunc)(u8);

struct Item
{
    u32 price;
    u16 secondaryId;
    ItemUseFunc fieldUseFunc;
    const u8 *description;
    const u8 *effect;
    u8 name[ITEM_NAME_LENGTH];
    u8 pluralName[ITEM_NAME_PLURAL_LENGTH];
    u8 holdEffect;
    u8 holdEffectParam;
    u8 importance:2;
    u8 notConsumed:1;
    u8 padding:5;
    u8 pocket;
    u8 type;
    u8 battleUsage;
    u8 flingPower;
    const u32 *iconPic;
    const u16 *iconPalette;
};

struct BagPocket
{
    struct ItemSlot *itemSlots;
    u8 capacity;
};

extern const struct Item gItemsInfo[];
extern struct BagPocket gBagPockets[];

u16 GetBagItemQuantity(enum Pocket pocketId, u32 pocketPos);
void GetBerryCountString(u8 *dst, const u8 *berryName, u32 quantity);
u8 *CopyItemName(u16 itemId, u8 *string);
u8 *CopyItemNameHandlePlural(u16 itemId, u8 *string, u32 quantity);
bool8 IsBagPocketNonEmpty(u8 pocket);
bool8 CheckBagHasItem(u16 itemId, u16 count);
bool8 CheckBagHasSpace(u16 itemId, u16 count);
u32 GetFreeSpaceForItemInBag(u16 itemId);
bool8 RemoveBagItem(u16 itemId, u16 count);
u8 GetPocketByItemId(u16 itemId);
u8 CountUsedPCItemSlots(void);
bool8 CheckPCHasItem(u16 itemId, u16 count);
bool8 AddPCItem(u16 itemId, u16 count);
void SwapRegisteredBike(void);
const u8 *GetItemName(u16 itemId);
u32 GetItemPrice(u16 itemId);
u8 GetItemHoldEffect(u16 itemId);
u8 GetItemHoldEffectParam(u16 itemId);
const u8 *GetItemDescription(u16 itemId);
u8 GetItemImportance(u16 itemId);
u8 GetItemConsumability(u16 itemId);
enum Pocket GetItemPocket(u16 itemId);
u8 GetItemType(u16 itemId);
ItemUseFunc GetItemFieldFunc(u16 itemId);
u8 GetItemBattleUsage(u16 itemId);
u16 GetItemSecondaryId(u16 itemId);
u32 GetItemFlingPower(u32 itemId);
void MoveItemSlotInPocket(enum Pocket pocketId, u32 from, u32 to);
void MoveItemSlotInPC(struct ItemSlot *itemSlots_, u32 from, u32 to_);
void ClearBag(void);
void ClearPCItemSlots(void);
void TrySetObtainedItemQuestLogEvent(u16 itemId);
bool8 AddBagItem(u16 itemId, u16 amount);

u16 GetBagItemId(enum Pocket pocketId, u32 pocketPos);
u16 CountTotalItemQuantityInBag(u16 item);
u8 GetItemImportance(u16 itemId);
u16 GetPCItemQuantity(u16 *);
void SetBagItemsPointers(void);

void ItemPcCompaction(void);
void RemovePCItem(u16 itemId, u16 quantity);
void CompactItemsInBagPocket(enum Pocket pocketId);
void SortBerriesOrTMHMs(enum Pocket pocketId);
u8 CountItemsInPC(void);
bool8 HasAtLeastOneBerry(void);
bool8 HasAtLeastOnePokeBall(void);

bool8 IsItemTM(u16 itemId);
bool8 IsItemHM(u16 itemId);
bool8 IsItemTMHM(u16 itemId);

bool8 IsItemBall(u16 itemId);

const u8 *GetItemEffect(u32 itemId);
u32 GetItemStatus1Mask(u16 itemId);
u32 GetItemStatus2Mask(u16 itemId);
u32 GetItemSellPrice(u32 itemId);

/* Expands to:
 * enum
 * {
 *   ITEM_TM_FOCUS_PUNCH,
 *   ...
 *   ITEM_HM_CUT,
 *   ...
 * }; */
#define ENUM_TM(id) CAT(ITEM_TM_, id),
#define ENUM_HM(id) CAT(ITEM_HM_, id),
enum
{
    ENUM_TM_START_ = ITEM_TM01 - 1,
    FOREACH_TM(ENUM_TM)

    ENUM_HM_START_ = ITEM_HM01 - 1,
    FOREACH_HM(ENUM_HM)
};
#undef ENUM_TM
#undef ENUM_HM

#endif // GUARD_ITEM_H
