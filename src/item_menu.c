#include "global.h"
#include "gflib.h"
#include "battle.h"
#include "battle_controllers.h"
#include "battle_main.h"
#include "berry_pouch.h"
#include "decompress.h"
#include "event_scripts.h"
#include "event_object_movement.h"
#include "field_player_avatar.h"
#include "graphics.h"
#include "help_system.h"
#include "international_string_util.h"
#include "item.h"
#include "item_icon.h"
#include "item_menu.h"
#include "item_menu_icons.h"
#include "item_use.h"
#include "link.h"
#include "list_menu.h"
#include "load_save.h"
#include "mail.h"
#include "map_name_popup.h"
#include "menu.h"
#include "money.h"
#include "overworld.h"
#include "party_menu.h"
#include "pokemon_storage_system.h"
#include "scanline_effect.h"
#include "script.h"
#include "shop.h"
#include "strings.h"
#include "teachy_tv.h"
#include "text_window.h"
#include "tm_case.h"
#include "constants/items.h"
#include "constants/quest_log.h"
#include "constants/songs.h"

#define TAG_POCKET_SCROLL_ARROW 110
#define TAG_BAG_SCROLL_ARROW    111

// The buffer for the bag item list needs to be large enough to hold the maximum
// number of item slots that could fit in a single pocket, + 1 for Cancel.
// This constant picks the max of the existing pocket sizes.
// By default, the largest pocket is BAG_TMHM_COUNT at 64.
#define MAX_POCKET_ITEMS  ((max(BAG_TMHM_COUNT,              \
                            max(BAG_BERRIES_COUNT,           \
                            max(BAG_ITEMS_COUNT,             \
                            max(BAG_KEYITEMS_COUNT,          \
                                BAG_POKEBALLS_COUNT))))) + 1)

// Up to 6 item slots can be visible at a time
#define MAX_ITEMS_SHOWN 6

enum {
    SWITCH_POCKET_NONE,
    SWITCH_POCKET_LEFT,
    SWITCH_POCKET_RIGHT
};

enum PocketSwitchMode
{
    SWITCHMODE_NORMAL,
    SWITCHMODE_PC,
    SWITCHMODE_OLDMAN
};

enum {
    ACTION_USE,
    ACTION_TOSS,
    ACTION_REGISTER,
    ACTION_GIVE,
    ACTION_CANCEL,
    ACTION_BATTLE_USE,
    ACTION_CHECK,
    ACTION_OPEN,
    ACTION_OPEN_BERRIES,
    ACTION_WALK,
    ACTION_DESELECT,
    ACTION_DUMMY,
};

enum {
    WIN_ITEM_LIST,
    WIN_DESCRIPTION,
    WIN_POCKET_NAME
};

// Item list ID for toSwapPos to indicate an item is not currently being swapped
#define NOT_SWAPPING 0xFF

struct ListBuffer1 {
    struct ListMenuItem subBuffers[MAX_POCKET_ITEMS];
};

struct ListBuffer2 {
    u8 name[MAX_POCKET_ITEMS][max(ITEM_NAME_LENGTH, MOVE_NAME_LENGTH) + 15];
};

struct BagBackup
{
    struct ItemSlot bagPocket_Items[BAG_ITEMS_COUNT];
    struct ItemSlot bagPocket_KeyItems[BAG_KEYITEMS_COUNT];
    struct ItemSlot bagPocket_PokeBalls[BAG_POKEBALLS_COUNT];
    u16 cursorPosition[POCKETS_COUNT_NO_CASES];
    u16 scrollPosition[POCKETS_COUNT_NO_CASES];
    u16 registeredItem;
    u16 pocket;
};

static void CB2_Bag(void);
static bool8 SetupBagMenu(void);
static void BagMenu_InitBGs(void);
static bool8 LoadBagMenu_Graphics(void);
static void LoadBagMenuTextWindows(void);
static void AllocateBagItemListBuffers(void);
static void LoadBagItemListBuffers(u8 pocket);
static void CreatePocketScrollArrowPair(void);
static void CreatePocketSwitchArrowPair(void);
static void DestroyPocketSwitchArrowPair(void);
static void Task_BagMenu_HandleInput(u8 taskId);
static void PrintItemDescription(s32);
static void BagMenu_PrintCursorAtPos(u8 y, u8 colorIdx);
static void BagMenu_Print(u8 windowId, u8 fontId, const u8 * str, u8 x, u8 y, u8 letterSpacing, u8 lineSpacing, u8 speed, u8 colorIdx);
static void Task_CloseBagMenu(u8 taskId);
static u8 AddItemMessageWindow(u8);
static void RemoveItemMessageWindow(u8);
static void ReturnToItemList(u8);
static void PrintItemQuantity(u8, s16);
static u8 BagMenu_AddWindow(u8 windowType);
static u8 GetSwitchBagPocketDirection(u8 pocketId);
static void SwitchBagPocket(u8 taskId, s16 direction, bool16 skipEraseList);
static void StartItemSwap(u8 taskId);
static void Task_SwitchBagPocket(u8 taskId);
static void Task_HandleSwappingItemsInput(u8 taskId);
static void DoItemSwap(u8 taskId);
static void CancelItemSwap(u8 taskId);
static void PrintContextMenuItems(u8);
static void Task_ItemContext_SingleRow(u8 taskId);
static void BagMenu_RemoveWindow(u8);
static void PrintThereIsNoPokemon(u8 taskId);
static void Task_ChooseHowManyToToss(u8 taskId);
static void AskTossItems(u8 taskId);
static void Task_RemoveItemFromBag(u8 taskId);
static void ItemMenu_Cancel(u8 taskId);
static void ItemMenu_UseInBattle(u8 taskId);
static void HandleErrorMessage(u8 taskId);
static void PrintItemCantBeHeld(u8 taskId);
static void DisplayCurrentMoneyWindow(void);
static void DisplaySellItemPriceAndConfirm(u8 taskId);
static void InitSellHowManyInput(u8 taskId);
static void AskSellItems(u8 taskId);
static void Task_ChooseHowManyToSell(u8 taskId);
static void SellItem(u8 taskId);
static void WaitAfterItemSell(u8 taskId);
static void TryDepositItem(u8 taskId);
static void Task_ChooseHowManyToDeposit(u8 taskId);
static void UpdatePocketItemLists(void);
static void InitPocketListPositions(void);
static void InitPocketScrollPositions(void);
static u8 CreateBagInputHandlerTask(u8 location);
static void DrawItemListBgRow(u8 row);
static void BagMenu_MoveCursorCallback(s32 itemIndex, bool8 onInit, struct ListMenu *list);
static void BagMenu_ItemPrintCallback(u8 windowId, u32 itemId, u8 y);
static void ItemMenu_UseOutOfBattle(u8 taskId);
static void ItemMenu_Toss(u8 taskId);
static void ItemMenu_Register(u8 taskId);
static void ItemMenu_Give(u8 taskId);
static void Task_ItemContext_Normal(u8 taskId);
static void Task_ItemContext_GiveToParty(u8 taskId);
static void Task_ItemContext_Sell(u8 taskId);
static void Task_ItemContext_Deposit(u8 taskId);
static void Task_ItemContext_GiveToPC(u8 taskId);
static void ConfirmToss(u8 taskId);
static void CancelToss(u8 taskId);
static void ConfirmSell(u8 taskId);
static void CancelSell(u8 taskId);

// pokefirered functions
static void BagListMenuGetItemNameColored(u8 *dest, u16 itemId);
static void PrintPocketName(void);
static void GoToTMCase_Give(void);
static void GoToBerryPouch_Give(void);
static void ReturnToBagMenuFromSubmenu_Give(void);
static void GoToTMCase_PCBox(void);
static void GoToBerryPouch_PCBox(void);
static void ReturnToBagMenuFromSubmenu_PCBox(void);
static void GoToTMCase_Sell(void);
static void GoToBerryPouch_Sell(void);
static void ReturnToBagMenuFromSubmenu_Sell(void);
static bool32 IsTutorialBag(void);
static void Task_Bag_OldManTutorial(u8 taskId);
static void Task_Pokedude_FadeFromBag(u8 taskId);
static void Task_Pokedude_WaitFadeAndExitBag(u8 taskId);
static void Task_Bag_TeachyTvRegister(u8 taskId);
static void Task_Bag_TeachyTvCatching(u8 taskId);
static void Task_Bag_TeachyTvStatus(u8 taskId);
static void Task_Bag_TeachyTvTMs(u8 taskId);
static void BagDrawDepositItemTextBox(void);
static void BagDrawTextBoxOnWindow(u8 windowId);

static const u8 sText_DepositHowManyVar1[] = _("Deposit how many\n{STR_VAR_1}(s)?");
static const u8 sText_DepositedVar2Var1s[] = _("Deposited {STR_VAR_2}\n{STR_VAR_1}(s).");
static const u8 sText_NoRoomForItems[] = _("There's no room to\nstore items.");
static const u8 sText_CantStoreImportantItems[] = _("Important items\ncan't be stored in\nthe PC!");

static const struct BgTemplate sBgTemplates_ItemMenu[2] = {
    {
        .bg = 0,
        .charBaseIndex = 0,
        .mapBaseIndex = 31,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 0,
        .baseTile = 0x000
    }, {
        .bg = 1,
        .charBaseIndex = 3,
        .mapBaseIndex = 30,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 1,
        .baseTile = 0x000
    }
};

static const struct ListMenuTemplate sItemListMenu =
{
    .items = NULL,
    .moveCursorFunc = BagMenu_MoveCursorCallback,
    .itemPrintFunc = BagMenu_ItemPrintCallback,
    .totalItems = 0,
    .maxShowed = 0,
    .windowId = 0,
    .header_X = 0,
    .item_X = 9,
    .cursor_X = 1,
    .upText_Y = 2,
    .cursorPal = 2,
    .fillValue = 0,
    .cursorShadowPal = 3,
    .lettersSpacing = 0,
    .itemVerticalPadding = 2,
    .scrollMultiple = LIST_NO_MULTIPLE_SCROLL,
    .fontId = FONT_NORMAL,
    .cursorKind = CURSOR_BLACK_ARROW,
};

static const struct MenuAction sItemMenuActions[] = {
    [ACTION_USE] = {gOtherText_Use, {.void_u8 = ItemMenu_UseOutOfBattle}},
    [ACTION_TOSS] = {gOtherText_Toss, {.void_u8 = ItemMenu_Toss}},
    [ACTION_REGISTER] = {gOtherText_Register, {.void_u8 = ItemMenu_Register}},
    [ACTION_GIVE] = {gOtherText_Give, {.void_u8 = ItemMenu_Give}},
    [ACTION_CANCEL] = {gFameCheckerText_Cancel, {.void_u8 = ItemMenu_Cancel}},
    [ACTION_BATTLE_USE] = {gOtherText_Use, {.void_u8 = ItemMenu_UseInBattle}},
    [ACTION_CHECK] = {gOtherText_Check, {.void_u8 = ItemMenu_UseOutOfBattle}},
    [ACTION_OPEN] = {gOtherText_Open, {.void_u8 = ItemMenu_UseOutOfBattle}},
    [ACTION_OPEN_BERRIES] = {gOtherText_Open, {.void_u8 = ItemMenu_UseInBattle}},
    [ACTION_WALK] = {gOtherText_Walk, {.void_u8 = ItemMenu_UseOutOfBattle}},
    [ACTION_DESELECT] = {gOtherText_Deselect, {.void_u8 = ItemMenu_Register}},
    [ACTION_DUMMY] = {gString_Dummy, {.void_u8 = NULL}}
};

static const u8 sContextMenuItems_Field[][4] = {
    [POCKET_ITEMS] =
    {
        ACTION_USE,
        ACTION_GIVE,
        ACTION_TOSS,
        ACTION_CANCEL
    },
    [POCKET_KEY_ITEMS] =
    {
        ACTION_USE,
        ACTION_REGISTER,
        ACTION_CANCEL,
        ACTION_DUMMY
    },
    [POCKET_POKE_BALLS] =
    {
        ACTION_GIVE,
        ACTION_TOSS,
        ACTION_CANCEL,
        ACTION_DUMMY
    }
};

static const u8 sContextMenuItems_CheckGiveTossCancel[] = {
    ACTION_CHECK,
    ACTION_GIVE,
    ACTION_TOSS,
    ACTION_CANCEL
};

static const u8 sContextMenuItems_GiveIfNotKeyItemPocket[][2] = {
    [POCKET_ITEMS] =
    {
        ACTION_GIVE,
        ACTION_CANCEL
    },
    [POCKET_KEY_ITEMS] =
    {
        ACTION_CANCEL,
        ACTION_DUMMY
    },
    [POCKET_POKE_BALLS] =
    {
        ACTION_GIVE,
        ACTION_CANCEL
    }
};

static const u8 sContextMenuItems_Open[] = {
    ACTION_OPEN,
    ACTION_CANCEL
};

static const u8 sContextMenuItems_BattleUse[] = {
    ACTION_BATTLE_USE,
    ACTION_CANCEL
};

static const u8 sContextMenuItems_Cancel[] = {
    ACTION_CANCEL,
    ACTION_DUMMY
};

static const TaskFunc sContextMenuFuncs[] = {
    [ITEMMENULOCATION_FIELD]            = Task_ItemContext_Normal,
    [ITEMMENULOCATION_PARTY]            = Task_ItemContext_GiveToParty,
    [ITEMMENULOCATION_SHOP]             = Task_ItemContext_Sell,
    [ITEMMENULOCATION_ITEMPC]           = Task_ItemContext_Deposit,
    [ITEMMENULOCATION_PCBOX]            = Task_ItemContext_GiveToPC,
    [ITEMMENULOCATION_BATTLE]           = Task_ItemContext_Normal,
    [ITEMMENULOCATION_OLD_MAN]          = NULL,
    [ITEMMENULOCATION_TTVSCR_STATUS]    = NULL,
    [ITEMMENULOCATION_TTVSCR_CATCHING]  = NULL,
    [ITEMMENULOCATION_TTVSCR_REGISTER]  = NULL,
    [ITEMMENULOCATION_TTVSCR_TMS]       = NULL,
    [ITEMMENULOCATION_BERRY_TREE_MULCH] = Task_ItemContext_Normal,
};

static const struct YesNoFuncTable sYesNoTossFunctions = {
    ConfirmToss,
    CancelToss
};

static const struct YesNoFuncTable sYesNoSellItemFunctions = {
    ConfirmSell,
    CancelSell
};

static const struct ScrollArrowsTemplate sBagScrollArrowsTemplate = {
    .firstArrowType = SCROLL_ARROW_LEFT,
    .firstX = 8,
    .firstY = 72,
    .secondArrowType = SCROLL_ARROW_RIGHT,
    .secondX = 72,
    .secondY = 72,
    .fullyUpThreshold = 0,
    .fullyDownThreshold = 2,
    .tileTag = TAG_BAG_SCROLL_ARROW,
    .palTag = TAG_BAG_SCROLL_ARROW,
    .palNum = 0,
};

static const u8 sRegisteredSelect_Gfx[] = INCBIN_U8("graphics/interface/select_button.4bpp");

enum {
    COLORID_NORMAL,
    COLORID_BLACK_CURSOR,
    COLORID_GRAY_CURSOR,
    COLORID_UNUSED,
    COLORID_NONE = 0xFF
};

static const u8 sFontColorTable[][3] = {
    [COLORID_NORMAL]        = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_WHITE, TEXT_COLOR_DARK_GRAY},
    [COLORID_BLACK_CURSOR]  = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_DARK_GRAY, TEXT_COLOR_LIGHT_GRAY},
    [COLORID_GRAY_CURSOR]   = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_LIGHT_GRAY, TEXT_COLOR_DARK_GRAY},
    [COLORID_UNUSED]        = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_BLUE, TEXT_COLOR_LIGHT_BLUE}
};

static const struct WindowTemplate sDefaultBagWindows[] = {
    [WIN_ITEM_LIST] =
    {
        .bg = 0,
        .tilemapLeft = 11,
        .tilemapTop = 1,
        .width = 18,
        .height = 12,
        .paletteNum = 15,
        .baseBlock = 0x008a
    },
    [WIN_DESCRIPTION] =
    {
        .bg = 0,
        .tilemapLeft = 5,
        .tilemapTop = 14,
        .width = 25,
        .height = 6,
        .paletteNum = 15,
        .baseBlock = 0x0162
    },
    [WIN_POCKET_NAME] =
    {
        .bg = 0,
        .tilemapLeft = 1,
        .tilemapTop = 1,
        .width = 9,
        .height = 2,
        .paletteNum = 15,
        .baseBlock = 0x01f8
    },
    DUMMY_WIN_TEMPLATE
};

static const struct WindowTemplate sDefaultBagWindowsDeposit[] = {
    [WIN_ITEM_LIST] =
    {
        .bg = 0,
        .tilemapLeft = 11,
        .tilemapTop = 1,
        .width = 18,
        .height = 12,
        .paletteNum = 15,
        .baseBlock = 0x008a
    },
    [WIN_DESCRIPTION] =
    {
        .bg = 0,
        .tilemapLeft = 5,
        .tilemapTop = 14,
        .width = 25,
        .height = 6,
        .paletteNum = 15,
        .baseBlock = 0x0162
    },
    [WIN_POCKET_NAME] =
    {
        .bg = 0,
        .tilemapLeft = 1,
        .tilemapTop = 1,
        .width = 8,
        .height = 2,
        .paletteNum = 12,
        .baseBlock = 0x01f8
    },
    DUMMY_WIN_TEMPLATE
};

static const struct WindowTemplate sContextMenuWindowTemplates[] = {
    [ITEMWIN_QUANTITY] =
    {
        .bg = 0,
        .tilemapLeft = 24,
        .tilemapTop = 15,
        .width = 5,
        .height = 4,
        .paletteNum = 15,
        .baseBlock = 0x242
    },
    [ITEMWIN_QUANTITY_WIDE] =
    {
        .bg = 0,
        .tilemapLeft = 17,
        .tilemapTop = 9,
        .width = 12,
        .height = 4,
        .paletteNum = 15,
        .baseBlock = 0x242
    },
    [ITEMWIN_MONEY] =
    {
        .bg = 0,
        .tilemapLeft = 1,
        .tilemapTop = 1,
        .width = 8,
        .height = 3,
        .paletteNum = 12,
        .baseBlock = 0x272
    },
    [ITEMWIN_YESNO_LOW] =
    {
        .bg = 0,
        .tilemapLeft = 23,
        .tilemapTop = 15,
        .width = 6,
        .height = 4,
        .paletteNum = 15,
        .baseBlock = 0x28a
    },
    [ITEMWIN_YESNO_HIGH] =
    {
        .bg = 0,
        .tilemapLeft = 21,
        .tilemapTop = 9,
        .width = 6,
        .height = 4,
        .paletteNum = 15,
        .baseBlock = 0x28a
    },
    [ITEMWIN_MESSAGE] =
    {
        .bg = 0,
        .tilemapLeft = 2,
        .tilemapTop = 15,
        .width = 26,
        .height = 4,
        .paletteNum = 15,
        .baseBlock = 0x2a2
    },
    [ITEMWIN_SELECTIONTEXT] =
    {
        .bg = 0,
        .tilemapLeft = 6,
        .tilemapTop = 15,
        .width = 14,
        .height = 4,
        .paletteNum = 12,
        .baseBlock = 0x2a2
    },
    [ITEMWIN_ASKTOSS] =
    {
        .bg = 0,
        .tilemapLeft = 6,
        .tilemapTop = 15,
        .width = 15,
        .height = 4,
        .paletteNum = 12,
        .baseBlock = 0x2da
    },
    [ITEMWIN_ASKTOSS_QUANTITY] =
    {
        .bg = 0,
        .tilemapLeft = 6,
        .tilemapTop = 15,
        .width = 16,
        .height = 4,
        .paletteNum = 12,
        .baseBlock = 0x316
    },
    [ITEMWIN_TOSSED] =
    {
        .bg = 0,
        .tilemapLeft = 6,
        .tilemapTop = 15,
        .width = 23,
        .height = 4,
        .paletteNum = 12,
        .baseBlock = 0x356
    },
    [ITEMWIN_1x1] =
    {
        .bg = 0,
        .tilemapLeft = 22,
        .tilemapTop = 17,
        .width = 7,
        .height = 2,
        .paletteNum = 15,
        .baseBlock = 0x20a
    },
    [ITEMWIN_1x2] =
    {
        .bg = 0,
        .tilemapLeft = 22,
        .tilemapTop = 15,
        .width = 7,
        .height = 4,
        .paletteNum = 15,
        .baseBlock = 0x20a
    },
    [ITEMWIN_1x3] =
    {
        .bg = 0,
        .tilemapLeft = 22,
        .tilemapTop = 13,
        .width = 7,
        .height = 6,
        .paletteNum = 15,
        .baseBlock = 0x20a
    },
    [ITEMWIN_1x4] =
    {
        .bg = 0,
        .tilemapLeft = 22,
        .tilemapTop = 11,
        .width = 7,
        .height = 8,
        .paletteNum = 15,
        .baseBlock = 0x20a
    }
};

// pokefirered specific
#define LIST_TILES_WIDTH  18
#define LIST_TILES_HEIGHT 12

static const u16 sItemListTilemap[LIST_TILES_WIDTH * LIST_TILES_HEIGHT] = INCBIN_U16("graphics/item_menu/list.bin");
static const u8 sListItemTextColor_RegularItem[] = _("{COLOR_HIGHLIGHT_SHADOW DARK_GRAY TRANSPARENT LIGHT_GRAY}");
static const u8 sListItemTextColor_TmCase_BerryPouch[] = _("{COLOR_HIGHLIGHT_SHADOW BLUE TRANSPARENT LIGHT_BLUE}");
static const u16 sBagWindowPalF[] = INCBIN_U16("graphics/item_menu/bag_window_pal.gbapal");
//

EWRAM_DATA struct BagMenu *gBagMenu = NULL;
EWRAM_DATA struct BagPosition gBagPosition = {0};
static EWRAM_DATA struct ListBuffer1 *sListBuffer1 = 0;
static EWRAM_DATA struct ListBuffer2 *sListBuffer2 = 0;
EWRAM_DATA u16 gSpecialVar_ItemId = ITEM_NONE;
static EWRAM_DATA struct BagBackup *sBackupPlayerBag = NULL;

void ResetBagScrollPositions(void)
{
    gBagPosition.pocket = POCKET_ITEMS;
    memset(gBagPosition.cursorPosition, 0, sizeof(gBagPosition.cursorPosition));
    memset(gBagPosition.scrollPosition, 0, sizeof(gBagPosition.scrollPosition));
}

void CB2_BagMenuFromStartMenu(void)
{
    GoToBagMenu(ITEMMENULOCATION_FIELD, POCKETS_COUNT_NO_CASES, CB2_ReturnToFieldWithOpenMenu);
}

void CB2_BagMenuFromBattle(void)
{
    GoToBagMenu(ITEMMENULOCATION_BATTLE, POCKETS_COUNT_NO_CASES, CB2_SetUpReshowBattleScreenAfterMenu2);
}

static void ReturnToBagMenuFromSubmenu_PCBox(void)
{
    GoToBagMenu(ITEMMENULOCATION_PCBOX, POCKETS_COUNT_NO_CASES, CB2_ReturnToPokeStorage);
}

static void ReturnToBagMenuFromSubmenu_Sell(void)
{
    GoToBagMenu(ITEMMENULOCATION_SHOP, POCKETS_COUNT_NO_CASES, CB2_ReturnToField);
}

// Choosing mulch to use
void CB2_ChooseMulch(void)
{
    GoToBagMenu(ITEMMENULOCATION_BERRY_TREE_MULCH, POCKET_ITEMS, CB2_ReturnToFieldContinueScript);
}

static void GoToTMCase_Give(void)
{
    InitTMCase(TMCASE_GIVE_PARTY, ReturnToBagMenuFromSubmenu_Give, FALSE);
}

static void GoToTMCase_PCBox(void)
{
    InitTMCase(TMCASE_GIVE_PC, ReturnToBagMenuFromSubmenu_PCBox, FALSE);
}

static void GoToTMCase_Sell(void)
{
    InitTMCase(TMCASE_SELL, ReturnToBagMenuFromSubmenu_Sell, FALSE);
}

// Choosing berry to plant
void CB2_ChooseBerry(void)
{
    InitBerryPouch(BERRYPOUCH_FROMBERRYTREE, CB2_ReturnToFieldContinueScript, FALSE);
}

static void GoToBerryPouch_Give(void)
{
    InitBerryPouch(BERRYPOUCH_FROMPARTYGIVE, ReturnToBagMenuFromSubmenu_Give, FALSE);
}

static void GoToBerryPouch_PCBox(void)
{
    InitBerryPouch(BERRYPOUCH_FROMPOKEMONSTORAGEPC, ReturnToBagMenuFromSubmenu_PCBox, FALSE);
}

static void GoToBerryPouch_Sell(void)
{
    InitBerryPouch(BERRYPOUCH_FROMMARTSELL, ReturnToBagMenuFromSubmenu_Sell, FALSE);
}

void GoToBagMenu(u8 location, u8 pocket, MainCallback exitCallback)
{
    gBagMenu = AllocZeroed(sizeof(*gBagMenu));
    if (gBagMenu == NULL)
    {
        SetMainCallback2(exitCallback);
    }
    else
    {
        if (location != ITEMMENULOCATION_LAST)
            gBagPosition.location = location;
        if (exitCallback != NULL)
            gBagPosition.exitCallback = exitCallback;
        if (pocket == POCKET_ITEMS || pocket == POCKET_KEY_ITEMS || pocket == POCKET_POKE_BALLS)
            gBagPosition.pocket = pocket;
        if (location == ITEMMENULOCATION_ITEMPC)
            gBagMenu->pocketSwitchMode = SWITCHMODE_PC;
        else if (location == ITEMMENULOCATION_OLD_MAN)
            gBagMenu->pocketSwitchMode = SWITCHMODE_OLDMAN;
        else
            gBagMenu->pocketSwitchMode = SWITCHMODE_NORMAL;
        gBagMenu->newScreenCallback = NULL;
        gBagMenu->toSwapPos = NOT_SWAPPING;
        gBagMenu->pocketScrollArrowsTask = TASK_NONE;
        gBagMenu->pocketSwitchArrowsTask = TASK_NONE;
        memset(gBagMenu->spriteIds, SPRITE_NONE, sizeof(gBagMenu->spriteIds));
        memset(gBagMenu->windowIds, WINDOW_NONE, sizeof(gBagMenu->windowIds));
        SetMainCallback2(CB2_Bag);
        
        gTextFlags.autoScroll = FALSE;
        gSpecialVar_ItemId = ITEM_NONE;
    }
}

static void CB2_BagMenuRun(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    DoScheduledBgTilemapCopiesToVram();
    UpdatePaletteFade();
}

static void VBlankCB_BagMenuRun(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

#define tListTaskId        data[0]
#define tListPosition      data[1]
#define tQuantity          data[2]
#define tNeverRead         data[3]
#define tItemCount         data[8]
#define tMsgWindowId       data[10]
#define tPocketSwitchDir   data[11]
#define tPocketSwitchTimer data[12]
#define tPocketSwitchState data[13]

static void CB2_Bag(void)
{
    while (IsActiveOverworldLinkBusy() != TRUE && SetupBagMenu() != TRUE && MenuHelpers_IsLinkActive() != TRUE)
        {};
}

static bool8 SetupBagMenu(void)
{
    u8 taskId;

    switch (gMain.state)
    {
    case 0:
        SetVBlankHBlankCallbacksToNull();
        ClearScheduledBgCopiesToVram();
        gMain.state++;
        break;
    case 1:
        ScanlineEffect_Stop();
        gMain.state++;
        break;
    case 2:
        FreeAllSpritePalettes();
        gMain.state++;
        break;
    case 3:
        ResetPaletteFade();
        gPaletteFade.bufferTransferDisabled = TRUE;
        gMain.state++;
        break;
    case 4:
        ResetSpriteData();
        gMain.state++;
        break;
    case 5:
        gMain.state++;
        break;
    case 6:
        if (!MenuHelpers_IsLinkActive())
            ResetTasks();
        gMain.state++;
        break;
    case 7:
        BagMenu_InitBGs();
        gBagMenu->graphicsLoadState = 0;
        gMain.state++;
        break;
    case 8:
        if (!LoadBagMenu_Graphics())
            break;
        gMain.state++;
        break;
    case 9:
        LoadBagMenuTextWindows();
        gMain.state++;
        break;
    case 10:
        UpdatePocketItemLists();
        InitPocketListPositions();
        InitPocketScrollPositions();
        gMain.state++;
        break;
    case 11:
        AllocateBagItemListBuffers();
        gMain.state++;
        break;
    case 12:
        LoadBagItemListBuffers(gBagPosition.pocket);
        gMain.state++;
        break;
    case 13:
        if (gBagPosition.location != ITEMMENULOCATION_ITEMPC)
            PrintPocketName();
        else
            BagDrawDepositItemTextBox();
        gMain.state++;
        break;
    case 14:
        taskId = CreateBagInputHandlerTask(gBagPosition.location);
        gTasks[taskId].tListTaskId = ListMenuInit(&gMultiuseListMenuTemplate, gBagPosition.scrollPosition[gBagPosition.pocket], gBagPosition.cursorPosition[gBagPosition.pocket]);
        gTasks[taskId].tNeverRead = 0;
        gTasks[taskId].tItemCount = 0;
        gMain.state++;
        break;
    case 15:
        AddBagVisualSprite(gBagPosition.pocket);
        gMain.state++;
        break;
    case 16:
        CreateItemMenuSwapLine();
        gMain.state++;
        break;
    case 17:
        CreatePocketScrollArrowPair();
        CreatePocketSwitchArrowPair();
        gMain.state++;
        break;
    case 18:
        gMain.state++;
        break;
    case 19:
        BlendPalettes(PALETTES_ALL, 16, 0);
        gMain.state++;
        break;
    case 20:
        if (gBagPosition.location == ITEMMENULOCATION_ITEMPC)
            SetHelpContext(HELPCONTEXT_PLAYERS_PC_ITEMS);
        else
            SetHelpContext(HELPCONTEXT_BAG);
        BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
        gPaletteFade.bufferTransferDisabled = FALSE;
        gMain.state++;
        break;
    default:
        SetVBlankCallback(VBlankCB_BagMenuRun);
        SetMainCallback2(CB2_BagMenuRun);
        return TRUE;
    }
    return FALSE;
}

static void BagMenu_InitBGs(void)
{
    ResetVramOamAndBgCntRegs();
    memset(gBagMenu->tilemapBuffer, 0, sizeof(gBagMenu->tilemapBuffer));
    ResetBgsAndClearDma3BusyFlags(FALSE);
    InitBgsFromTemplates(0, sBgTemplates_ItemMenu, NELEMS(sBgTemplates_ItemMenu));
    SetBgTilemapBuffer(1, gBagMenu->tilemapBuffer);
    ResetAllBgsCoordinates();
    ScheduleBgCopyTilemapToVram(1);
    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP);
    ShowBg(0);
    ShowBg(1);
    SetGpuReg(REG_OFFSET_BLDCNT, 0);
}

static bool8 LoadBagMenu_Graphics(void)
{
    switch (gBagMenu->graphicsLoadState)
    {
    case 0:
        ResetTempTileDataBuffers();
        DecompressAndCopyTileDataToVram(1, gBagBg_Gfx, 0, 0, 0);
        gBagMenu->graphicsLoadState++;
        break;
    case 1:
        if (FreeTempTileDataBuffersIfPossible() != TRUE)
        {
            if (gBagPosition.location != ITEMMENULOCATION_ITEMPC)
                DecompressDataWithHeaderWram(gBagBg_Tilemap, gBagMenu->tilemapBuffer);
            else
                DecompressDataWithHeaderWram(gBagBg_ItemPC_Tilemap, gBagMenu->tilemapBuffer);
            gBagMenu->graphicsLoadState++;
        }
        break;
    case 2:
        LoadPalette(gBagBgPalette, BG_PLTT_ID(0), 3 * PLTT_SIZE_4BPP);
        if (!IsTutorialBag() && gSaveBlock2Ptr->playerGender == FEMALE)
            LoadPalette(gBagBgPalette_FemaleOverride, BG_PLTT_ID(0), PLTT_SIZE_4BPP);
        gBagMenu->graphicsLoadState++;
        break;
    case 3:
        if (IsTutorialBag() == TRUE || gSaveBlock2Ptr->playerGender == MALE)
            LoadCompressedSpriteSheet(&gSpriteSheet_BagMale);
        else
            LoadCompressedSpriteSheet(&gSpriteSheet_BagFemale);
        gBagMenu->graphicsLoadState++;
        break;
    case 4:
        LoadSpritePalette(&gSpritePalette_Bag);
        gBagMenu->graphicsLoadState++;
        break;
    default:
        LoadListMenuSwapLineGfx();
        gBagMenu->graphicsLoadState = 0;
        return TRUE;
    }

    return FALSE;
}

static u8 CreateBagInputHandlerTask(u8 location)
{
    switch (location)
    {
    case ITEMMENULOCATION_OLD_MAN:
        return CreateTask(Task_Bag_OldManTutorial, 0);
    case ITEMMENULOCATION_TTVSCR_REGISTER:
        return CreateTask(Task_Bag_TeachyTvRegister, 0);
    case ITEMMENULOCATION_TTVSCR_TMS:
        return CreateTask(Task_Bag_TeachyTvTMs, 0);
    case ITEMMENULOCATION_TTVSCR_STATUS:
        return CreateTask(Task_Bag_TeachyTvStatus, 0);
    case ITEMMENULOCATION_TTVSCR_CATCHING:
        return CreateTask(Task_Bag_TeachyTvCatching, 0);
    default:
        return CreateTask(Task_BagMenu_HandleInput, 0);
    }
}

static void AllocateBagItemListBuffers(void)
{
    sListBuffer1 = Alloc(sizeof(*sListBuffer1));
    sListBuffer2 = Alloc(sizeof(*sListBuffer2));
}

static void LoadBagItemListBuffers(u8 pocketId)
{
    u16 i;
    struct BagPocket *pocket = &gBagPockets[pocketId];
    struct ListMenuItem *subBuffer;

    if (!gBagMenu->hideCloseBagText)
    {
        for (i = 0; i < gBagMenu->numItemStacks[pocketId] - 1; i++)
        {
            BagListMenuGetItemNameColored(sListBuffer2->name[i], pocket->itemSlots[i].itemId);
            subBuffer = sListBuffer1->subBuffers;
            subBuffer[i].name = sListBuffer2->name[i];
            subBuffer[i].id = i;
        }
        StringCopy(sListBuffer2->name[i], sListItemTextColor_RegularItem);
        StringAppend(sListBuffer2->name[i], gFameCheckerText_Cancel);
        subBuffer = sListBuffer1->subBuffers;
        subBuffer[i].name = sListBuffer2->name[i];
        subBuffer[i].id = LIST_CANCEL;
    }
    else
    {
        for (i = 0; i < gBagMenu->numItemStacks[pocketId]; i++)
        {
            BagListMenuGetItemNameColored(sListBuffer2->name[i], pocket->itemSlots[i].itemId);
            subBuffer = sListBuffer1->subBuffers;
            subBuffer[i].name = sListBuffer2->name[i];
            subBuffer[i].id = i;
        }
    }

    gMultiuseListMenuTemplate = sItemListMenu;
    gMultiuseListMenuTemplate.items = sListBuffer1->subBuffers;
    gMultiuseListMenuTemplate.totalItems = gBagMenu->numItemStacks[pocketId];
    gMultiuseListMenuTemplate.maxShowed = gBagMenu->numShownItems[pocketId];
}

static void BagListMenuGetItemNameColored(u8 *dest, u16 itemId)
{
    u8* end;
    if (itemId == ITEM_TM_CASE || itemId == ITEM_BERRY_POUCH)
        StringCopy(dest, sListItemTextColor_TmCase_BerryPouch);
    else
        StringCopy(dest, sListItemTextColor_RegularItem);
    
    end = StringAppend(dest, GetItemName(itemId));
    PrependFontIdToFit(dest, end, FONT_NARROW, 61);
}

static void BagMenu_MoveCursorCallback(s32 itemIndex, bool8 onInit, struct ListMenu *list)
{
    if (onInit != TRUE)
    {
        PlaySE(SE_BAG_CURSOR);
        ShakeBagSprite();
    }
    if (gBagMenu->toSwapPos == NOT_SWAPPING)
    {
        RemoveBagItemIconSprite(gBagMenu->itemIconSlot ^ 1);
        if (itemIndex != LIST_CANCEL)
            AddBagItemIconSprite(GetBagItemId(gBagPosition.pocket, itemIndex), gBagMenu->itemIconSlot);
        else
            AddBagItemIconSprite(ITEMS_COUNT, gBagMenu->itemIconSlot);
        gBagMenu->itemIconSlot ^= 1;
        if (!gBagMenu->inhibitItemDescriptionPrint)
            PrintItemDescription(itemIndex);
    }
}

static void BagMenu_ItemPrintCallback(u8 windowId, u32 itemIndex, u8 y)
{
    u16 itemId;
    u16 itemQuantity;


    if (itemIndex == LIST_CANCEL)
        return;

    if (gBagMenu->toSwapPos != NOT_SWAPPING)
    {
        if (gBagMenu->toSwapPos == (u8)itemIndex)
            BagMenu_PrintCursorAtPos(y, COLORID_GRAY_CURSOR);
        else
            BagMenu_PrintCursorAtPos(y, COLORID_NONE);
    }

    itemId = GetBagItemId(gBagPosition.pocket, itemIndex);
    itemQuantity = GetBagItemQuantity(gBagPosition.pocket, itemIndex);

    if (gBagPosition.pocket != POCKET_KEY_ITEMS && GetItemImportance(itemId) == FALSE)
    {
        ConvertIntToDecimalStringN(gStringVar1, itemQuantity, STR_CONV_MODE_RIGHT_ALIGN, MAX_ITEM_DIGITS);
        StringExpandPlaceholders(gStringVar4, gText_xVar1);
        BagMenu_Print(windowId, FONT_SMALL, gStringVar4, 110, y, 0, 0, TEXT_SKIP_DRAW, 1);
    }
    else
    {
        if (gSaveBlock1Ptr->registeredItem != ITEM_NONE && gSaveBlock1Ptr->registeredItem == itemId)
            BlitBitmapToWindow(windowId, sRegisteredSelect_Gfx, 112, y, 24, 16);
    }
}

static void PrintItemDescription(s32 itemIndex)
{
    const u8 *str;
    
    if (itemIndex != LIST_CANCEL)
        str = GetItemDescription(GetBagItemId(gBagPosition.pocket, itemIndex));
    else
        str = gText_CloseBag;

    FillWindowPixelBuffer(WIN_DESCRIPTION, PIXEL_FILL(0));
    BagMenu_Print(WIN_DESCRIPTION, FONT_NORMAL, str, 0, 3, 2, 0, 0, COLORID_NORMAL);
}

static void BagMenu_PrintCursor(u8 listTaskId, u8 colorIndex)
{
    BagMenu_PrintCursorAtPos(ListMenuGetYCoordForPrintingArrowCursor(listTaskId), colorIndex);
}

static void BagMenu_PrintCursorAtPos(u8 y, u8 colorIndex)
{
    if (colorIndex == COLORID_NONE)
        FillWindowPixelRect(WIN_ITEM_LIST, PIXEL_FILL(0), 1, y, GetMenuCursorDimensionByFont(FONT_NORMAL, 0), GetMenuCursorDimensionByFont(FONT_NORMAL, 1));
    else
        BagMenu_Print(WIN_ITEM_LIST, FONT_NORMAL, gText_SelectorArrow2, 1, y, 0, 0, 0, colorIndex);
}

static void CreatePocketScrollArrowPair_SellQuantity(void)
{
    gBagMenu->contextMenuSelectedItem = 1;
    gBagMenu->pocketScrollArrowsTask = AddScrollIndicatorArrowPairParameterized(
        SCROLL_ARROW_UP,
        152,
        72,
        104,
        2,
        TAG_POCKET_SCROLL_ARROW,
        TAG_POCKET_SCROLL_ARROW,
        &gBagMenu->contextMenuSelectedItem
    );
}

static void CreatePocketScrollArrowPair_QuantitySelect(void)
{
    gBagMenu->contextMenuSelectedItem = 1;
    gBagMenu->pocketScrollArrowsTask = AddScrollIndicatorArrowPairParameterized(
        SCROLL_ARROW_UP,
        212,
        120,
        152,
        2,
        TAG_POCKET_SCROLL_ARROW,
        TAG_POCKET_SCROLL_ARROW,
        &gBagMenu->contextMenuSelectedItem
    );
}

static void CreatePocketScrollArrowPair(void)
{
    if (gBagMenu->pocketScrollArrowsTask == TASK_NONE)
        gBagMenu->pocketScrollArrowsTask = AddScrollIndicatorArrowPairParameterized(
            SCROLL_ARROW_UP,
            160,
            8,
            104,
            gBagMenu->numItemStacks[gBagPosition.pocket] - gBagMenu->numShownItems[gBagPosition.pocket],
            TAG_POCKET_SCROLL_ARROW,
            TAG_POCKET_SCROLL_ARROW,
            &gBagPosition.scrollPosition[gBagPosition.pocket]);
}

static void BagDestroyPocketScrollArrowPair(void)
{
    if (gBagMenu->pocketScrollArrowsTask != TASK_NONE)
    {
        RemoveScrollIndicatorArrowPair(gBagMenu->pocketScrollArrowsTask);
        gBagMenu->pocketScrollArrowsTask = TASK_NONE;
    }
    DestroyPocketSwitchArrowPair();
}

static void CreatePocketSwitchArrowPair(void)
{
    if (gBagMenu->pocketSwitchMode != SWITCHMODE_PC && gBagMenu->pocketSwitchArrowsTask == TASK_NONE)
        gBagMenu->pocketSwitchArrowsTask = AddScrollIndicatorArrowPair(&sBagScrollArrowsTemplate, &gBagPosition.pocket);
}

static void DestroyPocketSwitchArrowPair(void)
{
    if (gBagMenu->pocketSwitchArrowsTask != TASK_NONE)
    {
        RemoveScrollIndicatorArrowPair(gBagMenu->pocketSwitchArrowsTask);
        gBagMenu->pocketSwitchArrowsTask = TASK_NONE;
    }
}

static void FreeBagMenu(void)
{
    Free(sListBuffer2);
    Free(sListBuffer1);
    FreeAllWindowBuffers();
    Free(gBagMenu);
}

void Task_FadeAndCloseBagMenu(u8 taskId)
{
    BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
    gTasks[taskId].func = Task_CloseBagMenu;
}

static void Task_CloseBagMenu(u8 taskId)
{
    s16 *data = gTasks[taskId].data;
    if (!gPaletteFade.active)
    {
        DestroyListMenuTask(tListTaskId, &gBagPosition.scrollPosition[gBagPosition.pocket], &gBagPosition.cursorPosition[gBagPosition.pocket]);

        if (gBagMenu->newScreenCallback != NULL)
            SetMainCallback2(gBagMenu->newScreenCallback);
        else
            SetMainCallback2(gBagPosition.exitCallback);

        BagDestroyPocketScrollArrowPair();
        ResetSpriteData();
        FreeAllSpritePalettes();
        FreeBagMenu();
        DestroyTask(taskId);
    }
}

void UpdatePocketItemList(enum Pocket pocketId)
{
    u16 i;
    struct BagPocket* pocket = &gBagPockets[pocketId];

    CompactItemsInBagPocket(pocketId);

    gBagMenu->numItemStacks[pocketId] = 0;

    for (i = 0; i < pocket->capacity && pocket->itemSlots[i].itemId; i++)
        gBagMenu->numItemStacks[pocketId]++;

    if (!gBagMenu->hideCloseBagText)
        gBagMenu->numItemStacks[pocketId]++;

    if (gBagMenu->numItemStacks[pocketId] > MAX_ITEMS_SHOWN)
        gBagMenu->numShownItems[pocketId] = MAX_ITEMS_SHOWN;
    else
        gBagMenu->numShownItems[pocketId] = gBagMenu->numItemStacks[pocketId];
}

static void UpdatePocketItemLists(void)
{
    enum Pocket i;
    for (i = 0; i < POCKETS_COUNT_NO_CASES; i++)
        UpdatePocketItemList(i);
}

void UpdatePocketListPosition(enum Pocket pocketId)
{
    SetCursorWithinListBounds(&gBagPosition.scrollPosition[pocketId], &gBagPosition.cursorPosition[pocketId], gBagMenu->numShownItems[pocketId], gBagMenu->numItemStacks[pocketId]);
}

static void InitPocketListPositions(void)
{
    u8 i;
    for (i = 0; i < POCKETS_COUNT_NO_CASES; i++)
        UpdatePocketListPosition(i);
}

static void InitPocketScrollPositions(void)
{
    u8 i;
    for (i = 0; i < POCKETS_COUNT_NO_CASES; i++)
        SetCursorScrollWithinListBounds(&gBagPosition.scrollPosition[i], &gBagPosition.cursorPosition[i], gBagMenu->numShownItems[i], gBagMenu->numItemStacks[i], MAX_ITEMS_SHOWN);
}

void DisplayItemMessage(u8 taskId, u8 fontId, const u8 *str, void (*callback)(u8 taskId))
{
    s16 *data = gTasks[taskId].data;
    tMsgWindowId = AddItemMessageWindow(ITEMWIN_MESSAGE);
    FillWindowPixelBuffer(tMsgWindowId, PIXEL_FILL(1));
    DisplayMessageAndContinueTask(taskId, tMsgWindowId, 0x06D, 13, fontId, GetPlayerTextSpeedDelay(), str, callback);
    ScheduleBgCopyTilemapToVram(sContextMenuWindowTemplates[ITEMWIN_MESSAGE].bg);
}

void CloseItemMessage(u8 taskId)
{
    s16 *data = gTasks[taskId].data;
    u16 *scrollPos = &gBagPosition.scrollPosition[gBagPosition.pocket];
    u16 *cursorPos = &gBagPosition.cursorPosition[gBagPosition.pocket];
    RemoveItemMessageWindow(ITEMWIN_MESSAGE);
    DestroyListMenuTask(tListTaskId, scrollPos, cursorPos);
    UpdatePocketItemList(gBagPosition.pocket);
    UpdatePocketListPosition(gBagPosition.pocket);
    LoadBagItemListBuffers(gBagPosition.pocket);
    tListTaskId = ListMenuInit(&gMultiuseListMenuTemplate, *scrollPos, *cursorPos);
    ScheduleBgCopyTilemapToVram(0);
    BagMenu_PrintCursor(tListTaskId, COLORID_BLACK_CURSOR);
    ReturnToItemList(taskId);
}

static void AddItemQuantityWindow(u8 windowType)
{
    PrintItemQuantity(BagMenu_AddWindow(windowType), 1);
}

static void PrintItemQuantity(u8 windowId, s16 quantity)
{
    ConvertIntToDecimalStringN(gStringVar1, quantity, STR_CONV_MODE_LEADING_ZEROS, MAX_ITEM_DIGITS);
    StringExpandPlaceholders(gStringVar4, gText_xVar1);
    AddTextPrinterParameterized(windowId, FONT_SMALL, gStringVar4, GetStringCenterAlignXOffset(FONT_SMALL, gStringVar4, 0x28), 10, 0, 0);
}

// Prints the quantity of items to be sold and the amount that would be earned
static void PrintItemSoldAmount(int windowId, int numSold, int moneyEarned)
{
    ConvertIntToDecimalStringN(gStringVar1, numSold, STR_CONV_MODE_LEADING_ZEROS, MAX_ITEM_DIGITS);
    StringExpandPlaceholders(gStringVar4, gText_xVar1);
    AddTextPrinterParameterized(windowId, FONT_SMALL, gStringVar4, 4, 10, TEXT_SKIP_DRAW, 0);
    PrintMoneyAmount(windowId, CalculateMoneyTextHorizontalPosition(moneyEarned), 10, moneyEarned, 0);
}

static void Task_BagMenu_HandleInput(u8 taskId)
{
    s16 *data = gTasks[taskId].data;
    u16 *scrollPos = &gBagPosition.scrollPosition[gBagPosition.pocket];
    u16 *cursorPos = &gBagPosition.cursorPosition[gBagPosition.pocket];
    s32 listPosition;

    if (gPaletteFade.active)
        return;
    if (IsActiveOverworldLinkBusy() == TRUE)
        return;

    switch (GetSwitchBagPocketDirection(gBagPosition.pocket))
    {
    case SWITCH_POCKET_LEFT:
        SwitchBagPocket(taskId, MENU_CURSOR_DELTA_LEFT, FALSE);
        return;
    case SWITCH_POCKET_RIGHT:
        SwitchBagPocket(taskId, MENU_CURSOR_DELTA_RIGHT, FALSE);
        return;
    default:
        if (JOY_NEW(SELECT_BUTTON) && gBagPosition.location == ITEMMENULOCATION_FIELD)
        {
            ListMenuGetScrollAndRow(tListTaskId, scrollPos, cursorPos);
            if ((*scrollPos + *cursorPos) != gBagMenu->numItemStacks[gBagPosition.pocket] - 1)
            {
                PlaySE(SE_SELECT);
                StartItemSwap(taskId);
                return;
            }
        }
        break;
    }

    listPosition = ListMenu_ProcessInput(tListTaskId);
    ListMenuGetScrollAndRow(tListTaskId, scrollPos, cursorPos);
    switch (listPosition)
    {
    case LIST_NOTHING_CHOSEN:
        return;
    case LIST_CANCEL:
        PlaySE(SE_SELECT);
        gSpecialVar_ItemId = ITEM_NONE;
        gTasks[taskId].func = Task_FadeAndCloseBagMenu;
        break;
    default:
        PlaySE(SE_SELECT);
        BagDestroyPocketScrollArrowPair();
        BagMenu_PrintCursor(tListTaskId, COLORID_GRAY_CURSOR);
        tListPosition = listPosition;
        tQuantity = GetBagItemQuantity(gBagPosition.pocket, listPosition);
        gSpecialVar_ItemId = GetBagItemId(gBagPosition.pocket, listPosition);
        sContextMenuFuncs[gBagPosition.location](taskId);
        break;
    }
}

static void ReturnToItemList(u8 taskId)
{
    CreatePocketScrollArrowPair();
    CreatePocketSwitchArrowPair();
    gTasks[taskId].func = Task_BagMenu_HandleInput;
}

static u8 GetSwitchBagPocketDirection(u8 pocketId)
{
    u8 lrState;
    if (gBagMenu->pocketSwitchMode != SWITCHMODE_NORMAL)
        return SWITCH_POCKET_NONE;
    lrState = GetLRKeysPressed();
    if (JOY_NEW(DPAD_LEFT) || lrState == MENU_L_PRESSED)
    {
        if (pocketId == POCKET_ITEMS)
            return SWITCH_POCKET_NONE;
        PlaySE(SE_BAG_POCKET);
        return SWITCH_POCKET_LEFT;
    }
    if (JOY_NEW(DPAD_RIGHT) || lrState == MENU_R_PRESSED)
    {
        if (pocketId >= POCKET_POKE_BALLS)
            return SWITCH_POCKET_NONE;
        PlaySE(SE_BAG_POCKET);
        return SWITCH_POCKET_RIGHT;
    }
    return SWITCH_POCKET_NONE;
}

static void SwitchBagPocket(u8 taskId, s16 deltaBagPocketId, bool16 skipEraseList)
{
    s16 *data = gTasks[taskId].data;
    tPocketSwitchState = 0;
    tPocketSwitchTimer = 0;
    tPocketSwitchDir = deltaBagPocketId;
    if (!skipEraseList)
    {
        ClearWindowTilemap(WIN_ITEM_LIST);
        ClearWindowTilemap(WIN_DESCRIPTION);
        ClearWindowTilemap(WIN_POCKET_NAME);
        DestroyListMenuTask(tListTaskId, &gBagPosition.scrollPosition[gBagPosition.pocket], &gBagPosition.cursorPosition[gBagPosition.pocket]);
        ScheduleBgCopyTilemapToVram(0);
        RemoveBagItemIconSprite(gBagMenu->itemIconSlot ^ 1);
        BagDestroyPocketScrollArrowPair();
    }
    FillBgTilemapBufferRect_Palette0(1, 0x02D, 11, 1, 18, 12);
    ScheduleBgCopyTilemapToVram(1);
    SetBagVisualPocketId(gBagPosition.pocket + deltaBagPocketId);
    SetTaskFuncWithFollowupFunc(taskId, Task_SwitchBagPocket, gTasks[taskId].func);
}

static void Task_SwitchBagPocket(u8 taskId)
{
    s16 *data = gTasks[taskId].data;
    if (!MenuHelpers_IsLinkActive() && !IsTutorialBag())
    {
        switch (GetSwitchBagPocketDirection(gBagPosition.pocket + tPocketSwitchDir))
        {
        case SWITCH_POCKET_LEFT:
            gBagPosition.pocket += tPocketSwitchDir;
            SwitchTaskToFollowupFunc(taskId);
            SwitchBagPocket(taskId, MENU_CURSOR_DELTA_LEFT, TRUE);
            return;
        case SWITCH_POCKET_RIGHT:
            gBagPosition.pocket += tPocketSwitchDir;
            SwitchTaskToFollowupFunc(taskId);
            SwitchBagPocket(taskId, MENU_CURSOR_DELTA_RIGHT, TRUE);
            return;
        }
    }
    switch (tPocketSwitchState)
    {
    case 0:
        // Animate the item list being revealed from the bottom row up
        tPocketSwitchTimer++;
        DrawItemListBgRow(tPocketSwitchTimer);
        if (tPocketSwitchTimer == LIST_TILES_HEIGHT)
            tPocketSwitchState++;
        break;
    case 1:
        gBagPosition.pocket += tPocketSwitchDir;
        PrintPocketName();
        LoadBagItemListBuffers(gBagPosition.pocket);
        data[0] = ListMenuInit(&gMultiuseListMenuTemplate, gBagPosition.scrollPosition[gBagPosition.pocket], gBagPosition.cursorPosition[gBagPosition.pocket]);
        PutWindowTilemap(WIN_DESCRIPTION);
        PutWindowTilemap(WIN_POCKET_NAME);
        ScheduleBgCopyTilemapToVram(0);
        CreatePocketScrollArrowPair();
        CreatePocketSwitchArrowPair();
        SwitchTaskToFollowupFunc(taskId);
        break;
    }
}

// row of 0 is the bottom row in the list, up to LIST_TILES_HEIGHT at the top
static void DrawItemListBgRow(u8 row)
{
    CopyToBgTilemapBufferRect(1, &sItemListTilemap[(LIST_TILES_HEIGHT - row) * LIST_TILES_WIDTH], 11, 1 + LIST_TILES_HEIGHT - row, LIST_TILES_WIDTH, 1);
    ScheduleBgCopyTilemapToVram(1);
}

static void StartItemSwap(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    ListMenuSetTemplateField(tListTaskId, LISTFIELD_CURSORKIND, 1);
    tListPosition = gBagPosition.scrollPosition[gBagPosition.pocket] + gBagPosition.cursorPosition[gBagPosition.pocket];
    gBagMenu->toSwapPos = tListPosition;
    CopyItemName(GetBagItemId(gBagPosition.pocket, tListPosition), gStringVar1);
    StringExpandPlaceholders(gStringVar4, gText_MoveVar1Where);
    FillWindowPixelBuffer(WIN_DESCRIPTION, PIXEL_FILL(0));
    BagMenu_Print(WIN_DESCRIPTION, FONT_NORMAL, gStringVar4, 0, 3, 2, 0, 0, 0);
    UpdateItemMenuSwapLinePos(ListMenuGetYCoordForPrintingArrowCursor(tListTaskId));
    DestroyPocketSwitchArrowPair();
    BagMenu_PrintCursor(tListTaskId, COLORID_GRAY_CURSOR);
    gTasks[taskId].func = Task_HandleSwappingItemsInput;
}

static void Task_HandleSwappingItemsInput(u8 taskId)
{
    s16 *data = gTasks[taskId].data;
    

    if (IsActiveOverworldLinkBusy() == TRUE)
        return;

    if (JOY_NEW(SELECT_BUTTON))
    {
        PlaySE(SE_SELECT);
        ListMenuGetScrollAndRow(tListTaskId, &gBagPosition.scrollPosition[gBagPosition.pocket], &gBagPosition.cursorPosition[gBagPosition.pocket]);
        DoItemSwap(taskId);
    }
    else
    {
        s32 input = ListMenu_ProcessInput(tListTaskId);
        ListMenuGetScrollAndRow(tListTaskId, &gBagPosition.scrollPosition[gBagPosition.pocket], &gBagPosition.cursorPosition[gBagPosition.pocket]);
        SetItemMenuSwapLineInvisibility(FALSE);
        UpdateItemMenuSwapLinePos(ListMenuGetYCoordForPrintingArrowCursor(tListTaskId));
        switch (input)
        {
        case LIST_NOTHING_CHOSEN:
            return;
        case LIST_CANCEL:
            PlaySE(SE_SELECT);
            if (JOY_NEW(A_BUTTON))
                DoItemSwap(taskId);
            else
                CancelItemSwap(taskId);
            break;
        default:
            PlaySE(SE_SELECT);
            DoItemSwap(taskId);
            break;
        }
    }
}

static void DoItemSwap(u8 taskId)
{
    s16 *data = gTasks[taskId].data;
    u16 *scrollPos = &gBagPosition.scrollPosition[gBagPosition.pocket];
    u16 *cursorPos = &gBagPosition.cursorPosition[gBagPosition.pocket];
    u16 realPos = (*scrollPos + *cursorPos);

    if (tListPosition == realPos || tListPosition == realPos - 1)
    {
        CancelItemSwap(taskId);
    }
    else
    {
        MoveItemSlotInPocket(gBagPosition.pocket, tListPosition, realPos);
        gBagMenu->toSwapPos = NOT_SWAPPING;
        DestroyListMenuTask(tListTaskId, scrollPos, cursorPos);
        if (tListPosition < realPos)
            gBagPosition.cursorPosition[gBagPosition.pocket]--;
        LoadBagItemListBuffers(gBagPosition.pocket);
        tListTaskId = ListMenuInit(&gMultiuseListMenuTemplate, *scrollPos, *cursorPos);
        SetItemMenuSwapLineInvisibility(TRUE);
        CreatePocketSwitchArrowPair();
        gTasks[taskId].func = Task_BagMenu_HandleInput;
    }
}

static void CancelItemSwap(u8 taskId)
{
    s16 *data = gTasks[taskId].data;
    u16 *scrollPos = &gBagPosition.scrollPosition[gBagPosition.pocket];
    u16 *cursorPos = &gBagPosition.cursorPosition[gBagPosition.pocket];

    gBagMenu->toSwapPos = NOT_SWAPPING;
    DestroyListMenuTask(tListTaskId, scrollPos, cursorPos);
    if (tListPosition < *scrollPos + *cursorPos)
        gBagPosition.cursorPosition[gBagPosition.pocket]--;
    LoadBagItemListBuffers(gBagPosition.pocket);
    tListTaskId = ListMenuInit(&gMultiuseListMenuTemplate, *scrollPos, *cursorPos);
    SetItemMenuSwapLineInvisibility(TRUE);
    CreatePocketSwitchArrowPair();
    gTasks[taskId].func = Task_BagMenu_HandleInput;
}

static void OpenContextMenu(u8 taskId)
{
    u8 selectionTextWindowId;
    switch (gBagPosition.location)
    {
    case ITEMMENULOCATION_BATTLE:
    case ITEMMENULOCATION_TTVSCR_STATUS:
        if (gSpecialVar_ItemId == ITEM_BERRY_POUCH)
        {
            gBagMenu->contextMenuItemsBuffer[0] = ACTION_OPEN_BERRIES;
            gBagMenu->contextMenuItemsBuffer[1] = ACTION_CANCEL;
            gBagMenu->contextMenuItemsPtr = gBagMenu->contextMenuItemsBuffer;
            gBagMenu->contextMenuNumItems = 2;
        }
        else if (GetItemBattleUsage(gSpecialVar_ItemId))
        {
            gBagMenu->contextMenuItemsPtr = sContextMenuItems_BattleUse;
            gBagMenu->contextMenuNumItems = ARRAY_COUNT(sContextMenuItems_BattleUse);
        }
        else
        {
            gBagMenu->contextMenuItemsPtr = sContextMenuItems_Cancel;
            gBagMenu->contextMenuNumItems = 1;
        }
        break;
    case ITEMMENULOCATION_OLD_MAN:
    case ITEMMENULOCATION_TTVSCR_CATCHING:
        gBagMenu->contextMenuItemsPtr = sContextMenuItems_BattleUse;
        gBagMenu->contextMenuNumItems = ARRAY_COUNT(sContextMenuItems_BattleUse);
        break;
    default:
        if (MenuHelpers_IsLinkActive() == TRUE || InUnionRoom() == TRUE)
        {
            if (gSpecialVar_ItemId == ITEM_TM_CASE || gSpecialVar_ItemId == ITEM_BERRY_POUCH)
            {
                gBagMenu->contextMenuItemsPtr = sContextMenuItems_Open;
                gBagMenu->contextMenuNumItems = ARRAY_COUNT(sContextMenuItems_Open);
            }
            else
            {
                if (gBagPosition.pocket == POCKET_KEY_ITEMS)
                    gBagMenu->contextMenuNumItems = 1;
                else
                    gBagMenu->contextMenuNumItems = 2;
                gBagMenu->contextMenuItemsPtr = sContextMenuItems_GiveIfNotKeyItemPocket[gBagPosition.pocket];
            }
        }
        else
        {
            switch (gBagPosition.pocket)
            {
            case POCKET_ITEMS:
                gBagMenu->contextMenuNumItems = 4;
                if (ItemIsMail(gSpecialVar_ItemId) == TRUE)
                    gBagMenu->contextMenuItemsPtr = sContextMenuItems_CheckGiveTossCancel;
                else
                    gBagMenu->contextMenuItemsPtr = sContextMenuItems_Field[gBagPosition.pocket];
                break;
            case POCKET_KEY_ITEMS:
                gBagMenu->contextMenuItemsPtr = gBagMenu->contextMenuItemsBuffer;
                gBagMenu->contextMenuNumItems = 3;
                gBagMenu->contextMenuItemsBuffer[2] = ACTION_CANCEL;
                if (gSaveBlock1Ptr->registeredItem == gSpecialVar_ItemId)
                    gBagMenu->contextMenuItemsBuffer[1] = ACTION_DESELECT;
                else
                    gBagMenu->contextMenuItemsBuffer[1] = ACTION_REGISTER;
                if (gSpecialVar_ItemId == ITEM_TM_CASE || gSpecialVar_ItemId == ITEM_BERRY_POUCH)
                    gBagMenu->contextMenuItemsBuffer[0] = ACTION_OPEN;
                else if (gSpecialVar_ItemId == ITEM_BICYCLE && TestPlayerAvatarFlags(PLAYER_AVATAR_FLAG_ACRO_BIKE | PLAYER_AVATAR_FLAG_MACH_BIKE))
                    gBagMenu->contextMenuItemsBuffer[0] = ACTION_WALK;
                else
                    gBagMenu->contextMenuItemsBuffer[0] = ACTION_USE;
                break;
            case POCKET_POKE_BALLS:
                gBagMenu->contextMenuItemsPtr = sContextMenuItems_Field[gBagPosition.pocket];
                gBagMenu->contextMenuNumItems = 3;
                break;
            }
        }
    }
    PrintContextMenuItems(BagMenu_AddWindow(ITEMWIN_1x1 + gBagMenu->contextMenuNumItems - 1));
    selectionTextWindowId = BagMenu_AddWindow(ITEMWIN_SELECTIONTEXT);
    CopyItemName(gSpecialVar_ItemId, gStringVar1);
    StringExpandPlaceholders(gStringVar4, gText_Var1IsSelected);
    BagMenu_Print(selectionTextWindowId, FONT_NORMAL, gStringVar4, 0, 2, 1, 0, 0, 1);
}

static void PrintContextMenuItems(u8 windowId)
{
    u32 left = GetMenuCursorDimensionByFont(FONT_NORMAL, 0);
    u32 letterSpacing = GetFontAttribute(FONT_NORMAL, FONTATTR_LETTER_SPACING);
    u32 lineHeight = GetFontAttribute(FONT_NORMAL, FONTATTR_MAX_LETTER_HEIGHT) + 2;

    PrintMenuActionTexts(windowId, FONT_NORMAL, left, 2, letterSpacing, lineHeight, gBagMenu->contextMenuNumItems, sItemMenuActions, gBagMenu->contextMenuItemsPtr);
    InitMenuNormal(windowId, FONT_NORMAL, 0, 2, lineHeight, gBagMenu->contextMenuNumItems, 0);
}

static void Task_ItemContext_Normal(u8 taskId)
{
    OpenContextMenu(taskId);
    gTasks[taskId].func = Task_ItemContext_SingleRow;
}

static void Task_ItemContext_SingleRow(u8 taskId)
{
    if (IsActiveOverworldLinkBusy() != TRUE)
    {
        s8 selection = Menu_ProcessInputNoWrap();
        switch (selection)
        {
        case MENU_NOTHING_CHOSEN:
            break;
        case MENU_B_PRESSED:
            PlaySE(SE_SELECT);
            sItemMenuActions[ACTION_CANCEL].func.void_u8(taskId);
            break;
        default:
            PlaySE(SE_SELECT);
            sItemMenuActions[gBagMenu->contextMenuItemsPtr[selection]].func.void_u8(taskId);
            break;
        }
    }
}

static void RemoveContextWindow(void)
{
    if (gBagMenu->contextMenuNumItems == 1)
        BagMenu_RemoveWindow(ITEMWIN_1x1);
    else if (gBagMenu->contextMenuNumItems == 2)
        BagMenu_RemoveWindow(ITEMWIN_1x2);
    else if (gBagMenu->contextMenuNumItems == 4)
        BagMenu_RemoveWindow(ITEMWIN_1x4);
    else
        BagMenu_RemoveWindow(ITEMWIN_1x3);
}

static void ItemMenu_UseOutOfBattle(u8 taskId)
{
    if (GetItemFieldFunc(gSpecialVar_ItemId) != NULL)
    {
        RemoveContextWindow();
        BagMenu_RemoveWindow(ITEMWIN_SELECTIONTEXT);
        PutWindowTilemap(WIN_ITEM_LIST);
        PutWindowTilemap(WIN_DESCRIPTION);
        ScheduleBgCopyTilemapToVram(0);
        if (CalculatePlayerPartyCount() == 0 && GetItemType(gSpecialVar_ItemId) == ITEM_USE_PARTY_MENU)
            PrintThereIsNoPokemon(taskId);
        else
            GetItemFieldFunc(gSpecialVar_ItemId)(taskId);
    }
}

static void ItemMenu_Toss(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    RemoveContextWindow();
    BagMenu_RemoveWindow(ITEMWIN_SELECTIONTEXT);
    PutWindowTilemap(WIN_ITEM_LIST);
    tItemCount = 1;
    if (tQuantity == 1)
    {
        AskTossItems(taskId);
    }
    else
    {
        u8 windowId = BagMenu_AddWindow(ITEMWIN_ASKTOSS_QUANTITY);
        CopyItemName(gSpecialVar_ItemId, gStringVar1);
        StringExpandPlaceholders(gStringVar4, gText_TossHowManyVar1s);
        BagMenu_Print(windowId, FONT_NORMAL, gStringVar4, 0, 2, 1, 0, 0, 1);
        AddItemQuantityWindow(ITEMWIN_QUANTITY);
        CreatePocketScrollArrowPair_QuantitySelect();
        gTasks[taskId].func = Task_ChooseHowManyToToss;
    }
}

static void AskTossItems(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    ConvertIntToDecimalStringN(gStringVar2, tItemCount, STR_CONV_MODE_LEFT_ALIGN, 3);
    StringExpandPlaceholders(gStringVar4, gText_ThrowAwayStrVar2OfThisItemQM);
    BagMenu_Print(BagMenu_AddWindow(ITEMWIN_ASKTOSS), FONT_NORMAL, gStringVar4, 0, 2, 1, 0, 0, 1);
    BagMenu_YesNo(taskId, ITEMWIN_YESNO_LOW, &sYesNoTossFunctions);
}

static void CancelToss(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    BagMenu_RemoveWindow(ITEMWIN_ASKTOSS);
    PutWindowTilemap(WIN_DESCRIPTION);
    ScheduleBgCopyTilemapToVram(0);
    BagMenu_PrintCursor(tListTaskId, COLORID_BLACK_CURSOR);
    ReturnToItemList(taskId);
}

static void Task_ChooseHowManyToToss(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    if (AdjustQuantityAccordingToDPadInput(&tItemCount, tQuantity) == TRUE)
    {
        PrintItemQuantity(gBagMenu->windowIds[ITEMWIN_QUANTITY], tItemCount);
    }
    else if (JOY_NEW(A_BUTTON))
    {
        PlaySE(SE_SELECT);
        BagMenu_RemoveWindow(ITEMWIN_ASKTOSS_QUANTITY);
        BagMenu_RemoveWindow(ITEMWIN_QUANTITY);
        ScheduleBgCopyTilemapToVram(0);
        BagDestroyPocketScrollArrowPair();
        AskTossItems(taskId);
    }
    else if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        BagMenu_RemoveWindow(ITEMWIN_ASKTOSS_QUANTITY);
        BagMenu_RemoveWindow(ITEMWIN_QUANTITY);
        PutWindowTilemap(WIN_ITEM_LIST);
        PutWindowTilemap(WIN_DESCRIPTION);
        ScheduleBgCopyTilemapToVram(0);
        BagMenu_PrintCursor(tListTaskId, COLORID_BLACK_CURSOR);
        BagDestroyPocketScrollArrowPair();
        ReturnToItemList(taskId);
    }
}

static void ConfirmToss(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    BagMenu_RemoveWindow(ITEMWIN_ASKTOSS);
    CopyItemName(gSpecialVar_ItemId, gStringVar1);
    ConvertIntToDecimalStringN(gStringVar2, tItemCount, STR_CONV_MODE_LEFT_ALIGN, MAX_ITEM_DIGITS);
    StringExpandPlaceholders(gStringVar4, gText_ThrewAwayStrVar2StrVar1s);
    BagMenu_Print(BagMenu_AddWindow(ITEMWIN_TOSSED), FONT_NORMAL, gStringVar4, 0, 2, 1, 0, 0, COLORID_BLACK_CURSOR);
    gTasks[taskId].func = Task_RemoveItemFromBag;
}

static void Task_RemoveItemFromBag(u8 taskId)
{
    s16 *data = gTasks[taskId].data;
    u16 *scrollPos = &gBagPosition.scrollPosition[gBagPosition.pocket];
    u16 *cursorPos = &gBagPosition.cursorPosition[gBagPosition.pocket];

    if (JOY_NEW(A_BUTTON) || JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        RemoveBagItem(gSpecialVar_ItemId, tItemCount);
        BagMenu_RemoveWindow(ITEMWIN_TOSSED);
        DestroyListMenuTask(tListTaskId, scrollPos, cursorPos);
        UpdatePocketItemList(gBagPosition.pocket);
        UpdatePocketListPosition(gBagPosition.pocket);
        LoadBagItemListBuffers(gBagPosition.pocket);
        tListTaskId = ListMenuInit(&gMultiuseListMenuTemplate, *scrollPos, *cursorPos);
        PutWindowTilemap(WIN_DESCRIPTION);
        ScheduleBgCopyTilemapToVram(0);
        BagMenu_PrintCursor(tListTaskId, COLORID_BLACK_CURSOR);
        ReturnToItemList(taskId);
    }
}

static void ItemMenu_Register(u8 taskId)
{
    s16 *data = gTasks[taskId].data;
    u16 *scrollPos = &gBagPosition.scrollPosition[gBagPosition.pocket];
    u16 *cursorPos = &gBagPosition.cursorPosition[gBagPosition.pocket];

    if (gSaveBlock1Ptr->registeredItem == gSpecialVar_ItemId)
        gSaveBlock1Ptr->registeredItem = ITEM_NONE;
    else
        gSaveBlock1Ptr->registeredItem = gSpecialVar_ItemId;

    DestroyListMenuTask(tListTaskId, scrollPos, cursorPos);
    LoadBagItemListBuffers(gBagPosition.pocket);
    tListTaskId = ListMenuInit(&gMultiuseListMenuTemplate, *scrollPos, *cursorPos);
    ScheduleBgCopyTilemapToVram(0);
    ItemMenu_Cancel(taskId);
}

static void ItemMenu_Give(u8 taskId)
{
    RemoveContextWindow();
    BagMenu_RemoveWindow(ITEMWIN_SELECTIONTEXT);
    PutWindowTilemap(WIN_ITEM_LIST);
    PutWindowTilemap(WIN_DESCRIPTION);
    CopyWindowToVram(WIN_ITEM_LIST, COPYWIN_MAP);
    if (!IsWritingMailAllowed(gSpecialVar_ItemId))
    {
        DisplayItemMessage(taskId, FONT_NORMAL, gText_CantWriteMail, HandleErrorMessage);
    }
    else if (!GetItemImportance(gSpecialVar_ItemId))
    {
        if (CalculatePlayerPartyCount() == 0)
        {
            PrintThereIsNoPokemon(taskId);
        }
        else
        {
            gBagMenu->newScreenCallback = CB2_ChooseMonToGiveItem;
            Task_FadeAndCloseBagMenu(taskId);
        }
    }
    else
    {
        PrintItemCantBeHeld(taskId);
    }
}

static void PrintThereIsNoPokemon(u8 taskId)
{
    DisplayItemMessage(taskId, FONT_NORMAL, gText_NoPokemon, HandleErrorMessage);
}

static void PrintItemCantBeHeld(u8 taskId)
{
    CopyItemName(gSpecialVar_ItemId, gStringVar1);
    StringExpandPlaceholders(gStringVar4, gText_Var1CantBeHeld);
    DisplayItemMessage(taskId, FONT_NORMAL, gStringVar4, HandleErrorMessage);
}

static void HandleErrorMessage(u8 taskId)
{
    if (JOY_NEW(A_BUTTON))
    {
        PlaySE(SE_SELECT);
        CloseItemMessage(taskId);
    }
}

static void ItemMenu_Cancel(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    RemoveContextWindow();
    BagMenu_RemoveWindow(ITEMWIN_SELECTIONTEXT);
    PutWindowTilemap(WIN_ITEM_LIST);
    PutWindowTilemap(WIN_DESCRIPTION);
    ScheduleBgCopyTilemapToVram(0);
    BagMenu_PrintCursor(tListTaskId, COLORID_BLACK_CURSOR);
    ReturnToItemList(taskId);
}

static void ItemMenu_UseInBattle(u8 taskId)
{
    // Safety check
    u16 type = GetItemType(gSpecialVar_ItemId);
    if (!GetItemBattleUsage(gSpecialVar_ItemId))
        return;

    RemoveContextWindow();
    BagMenu_RemoveWindow(ITEMWIN_SELECTIONTEXT);
    PutWindowTilemap(WIN_ITEM_LIST);
    PutWindowTilemap(WIN_DESCRIPTION);
    CopyWindowToVram(WIN_ITEM_LIST, COPYWIN_MAP);   

    if (type == ITEM_USE_BAG_MENU) {
        ItemUseInBattle_BagMenu(taskId);
    }
    else if (type == ITEM_USE_PARTY_MENU) {
        ItemUseInBattle_PartyMenu(taskId);
    }
    else if (type == ITEM_USE_PARTY_MENU_MOVES) {
        ItemUseInBattle_PartyMenuChooseMove(taskId);
    }
}

static void Task_ItemContext_GiveToParty(u8 taskId)
{
    if (!IsWritingMailAllowed(gSpecialVar_ItemId))
    {
        DisplayItemMessage(taskId, FONT_NORMAL, gText_CantWriteMail, HandleErrorMessage);
    }
    else if (gSpecialVar_ItemId == ITEM_TM_CASE)
    {
        ItemMenu_SetExitCallback(GoToTMCase_Give);
        Task_FadeAndCloseBagMenu(taskId);
    }
    else if (gSpecialVar_ItemId == ITEM_BERRY_POUCH)
    {
        ItemMenu_SetExitCallback(GoToBerryPouch_Give);
        Task_FadeAndCloseBagMenu(taskId);
    }
    else if (gBagPosition.pocket != POCKET_KEY_ITEMS && GetItemImportance(gSpecialVar_ItemId) == 0)
    {
        gTasks[taskId].func = Task_FadeAndCloseBagMenu;
    }
    else
    {
        PrintItemCantBeHeld(taskId);
    }
}

static void Task_ItemContext_GiveToPC(u8 taskId)
{
    if (ItemIsMail(gSpecialVar_ItemId) == TRUE)
    {
        DisplayItemMessage(taskId, FONT_NORMAL, gText_CantWriteMail, HandleErrorMessage);
    }
    else if (gSpecialVar_ItemId == ITEM_TM_CASE)
    {
        ItemMenu_SetExitCallback(GoToTMCase_PCBox);
        Task_FadeAndCloseBagMenu(taskId);
    }
    else if (gSpecialVar_ItemId == ITEM_BERRY_POUCH)
    {
        ItemMenu_SetExitCallback(GoToBerryPouch_PCBox);
        Task_FadeAndCloseBagMenu(taskId);
    }
    else if (gBagPosition.pocket != POCKET_KEY_ITEMS && GetItemImportance(gSpecialVar_ItemId) == 0)
    {
        gTasks[taskId].func = Task_FadeAndCloseBagMenu;
    }
    else
    {
        PrintItemCantBeHeld(taskId);
    }
}

#define tUsingRegisteredKeyItem data[3]

bool8 UseRegisteredKeyItemOnField(void)
{
    u8 taskId;
    if (InUnionRoom() == TRUE)
        return FALSE;
    HideMapNamePopUpWindow();
    ChangeBgY(0, 0, 0);
    if (gSaveBlock1Ptr->registeredItem != ITEM_NONE)
    {
        if (CheckBagHasItem(gSaveBlock1Ptr->registeredItem, 1) == TRUE)
        {
            LockPlayerFieldControls();
            FreezeObjectEvents();
            PlayerFreeze();
            StopPlayerAvatar();
            gSpecialVar_ItemId = gSaveBlock1Ptr->registeredItem;
            taskId = CreateTask(GetItemFieldFunc(gSaveBlock1Ptr->registeredItem), 8);
            gTasks[taskId].tUsingRegisteredKeyItem = TRUE;
            return TRUE;
        }
        gSaveBlock1Ptr->registeredItem = ITEM_NONE;
    }
    ScriptContext_SetupScript(EventScript_SelectWithoutRegisteredItem);
    return TRUE;
}

#undef tUsingRegisteredKeyItem

static void Task_ItemContext_Sell(u8 taskId)
{
    s16 *data = gTasks[taskId].data;
    if (gSpecialVar_ItemId == ITEM_TM_CASE)
    {
        ItemMenu_SetExitCallback(GoToTMCase_Sell);
        Task_FadeAndCloseBagMenu(taskId);
    }
    else if (gSpecialVar_ItemId == ITEM_BERRY_POUCH)
    {
        ItemMenu_SetExitCallback(GoToBerryPouch_Sell);
        Task_FadeAndCloseBagMenu(taskId);
    }
    else if (GetItemPrice(gSpecialVar_ItemId) == 0 || GetItemImportance(gSpecialVar_ItemId))
    {
        CopyItemName(gSpecialVar_ItemId, gStringVar1);
        StringExpandPlaceholders(gStringVar4, gText_OhNoICantBuyThat);
        DisplayItemMessage(taskId, GetDialogBoxFontId(), gStringVar4, CloseItemMessage);
    }
    else
    {
        tItemCount = 1;
        if (tQuantity == 1)
        {
            DisplayCurrentMoneyWindow();
            DisplaySellItemPriceAndConfirm(taskId);
        }
        else
        {
            u32 maxQuantity = MAX_MONEY / GetItemSellPrice(gSpecialVar_ItemId);

            if (tQuantity > MAX_BAG_ITEM_CAPACITY)
                tQuantity = MAX_BAG_ITEM_CAPACITY;
            
            if (tQuantity > maxQuantity)
                tQuantity = maxQuantity;

            CopyItemName(gSpecialVar_ItemId, gStringVar1);
            StringExpandPlaceholders(gStringVar4, gText_HowManyToSell);
            DisplayItemMessage(taskId, GetDialogBoxFontId(), gStringVar4, InitSellHowManyInput);
        }
    }
}

static void DisplaySellItemPriceAndConfirm(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    ConvertIntToDecimalStringN(gStringVar3, GetItemSellPrice(gSpecialVar_ItemId) * tItemCount, STR_CONV_MODE_LEFT_ALIGN, MAX_MONEY_DIGITS);
    StringExpandPlaceholders(gStringVar4, gText_ICanPayThisMuch_WouldThatBeOkay);
    DisplayItemMessage(taskId, GetDialogBoxFontId(), gStringVar4, AskSellItems);
}

static void AskSellItems(u8 taskId)
{
    BagMenu_YesNo(taskId, ITEMWIN_YESNO_HIGH, &sYesNoSellItemFunctions);
}

static void CancelSell(u8 taskId)
{
    s16 *data = gTasks[taskId].data;
    BagMenu_RemoveWindow(ITEMWIN_MONEY);
    RemoveItemMessageWindow(ITEMWIN_MESSAGE);
    PutWindowTilemap(WIN_POCKET_NAME);
    PutWindowTilemap(WIN_ITEM_LIST);
    PutWindowTilemap(WIN_DESCRIPTION);
    ScheduleBgCopyTilemapToVram(0);
    BagMenu_PrintCursor(tListTaskId, COLORID_BLACK_CURSOR);
    ReturnToItemList(taskId);
}

static void InitSellHowManyInput(u8 taskId)
{
    s16 *data = gTasks[taskId].data;
    u8 windowId = BagMenu_AddWindow(ITEMWIN_QUANTITY_WIDE);
    
    PrintItemSoldAmount(windowId, 1, GetItemSellPrice(gSpecialVar_ItemId) * tItemCount);
    DisplayCurrentMoneyWindow();
    CreatePocketScrollArrowPair_SellQuantity();
    gTasks[taskId].func = Task_ChooseHowManyToSell;
}

static void Task_ChooseHowManyToSell(u8 taskId)
{
    s16 *data = gTasks[taskId].data;
    if (AdjustQuantityAccordingToDPadInput(&tItemCount, tQuantity) == TRUE)
    {
        PrintItemSoldAmount(gBagMenu->windowIds[ITEMWIN_QUANTITY_WIDE], tItemCount, GetItemSellPrice(gSpecialVar_ItemId) * tItemCount);
    }
    else if (JOY_NEW(A_BUTTON))
    {
        PlaySE(SE_SELECT);
        BagMenu_RemoveWindow(ITEMWIN_QUANTITY_WIDE);
        PutWindowTilemap(WIN_ITEM_LIST);
        ScheduleBgCopyTilemapToVram(0);
        BagDestroyPocketScrollArrowPair();
        DisplaySellItemPriceAndConfirm(taskId);
    }
    else if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        BagMenu_RemoveWindow(ITEMWIN_QUANTITY_WIDE);
        BagMenu_RemoveWindow(ITEMWIN_MONEY);
        RemoveItemMessageWindow(ITEMWIN_MESSAGE);
        PutWindowTilemap(WIN_POCKET_NAME);
        PutWindowTilemap(WIN_ITEM_LIST);
        PutWindowTilemap(WIN_DESCRIPTION);
        ScheduleBgCopyTilemapToVram(0);
        BagDestroyPocketScrollArrowPair();
        BagMenu_PrintCursor(tListTaskId, COLORID_BLACK_CURSOR);
        ReturnToItemList(taskId);
    }
}

static void ConfirmSell(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    PutWindowTilemap(WIN_ITEM_LIST);
    ScheduleBgCopyTilemapToVram(0);
    CopyItemName(gSpecialVar_ItemId, gStringVar2);
    ConvertIntToDecimalStringN(gStringVar1, GetItemSellPrice(gSpecialVar_ItemId) * tItemCount, STR_CONV_MODE_LEFT_ALIGN, MAX_MONEY_DIGITS);
    StringExpandPlaceholders(gStringVar4, gText_TurnedOverVar1ForVar2);
    DisplayItemMessage(taskId, FONT_NORMAL, gStringVar4, SellItem);
}

static void SellItem(u8 taskId)
{
    s16 *data = gTasks[taskId].data;
    u16 *scrollPos = &gBagPosition.scrollPosition[gBagPosition.pocket];
    u16 *cursorPos = &gBagPosition.cursorPosition[gBagPosition.pocket];

    PlaySE(SE_SHOP);
    RemoveBagItem(gSpecialVar_ItemId, tItemCount);
    AddMoney(&gSaveBlock1Ptr->money, GetItemSellPrice(gSpecialVar_ItemId) * tItemCount);
    RecordItemTransaction(gSpecialVar_ItemId, tItemCount, QL_EVENT_SOLD_ITEM - QL_EVENT_USED_POKEMART);
    DestroyListMenuTask(tListTaskId, scrollPos, cursorPos);
    UpdatePocketItemList(gBagPosition.pocket);
    UpdatePocketListPosition(gBagPosition.pocket);
    gBagMenu->inhibitItemDescriptionPrint = TRUE;
    LoadBagItemListBuffers(gBagPosition.pocket);
    tListTaskId = ListMenuInit(&gMultiuseListMenuTemplate, *scrollPos, *cursorPos);
    BagMenu_PrintCursor(tListTaskId, COLORID_GRAY_CURSOR);
    BagDrawTextBoxOnWindow(gBagMenu->windowIds[ITEMWIN_MONEY]);
    PrintMoneyAmountInMoneyBox(gBagMenu->windowIds[ITEMWIN_MONEY], GetMoney(&gSaveBlock1Ptr->money), 0);
    gTasks[taskId].func = WaitAfterItemSell;
}

static void WaitAfterItemSell(u8 taskId)
{
    if (JOY_NEW(A_BUTTON) || JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        BagMenu_RemoveWindow(ITEMWIN_MONEY);
        PutWindowTilemap(WIN_POCKET_NAME);
        gBagMenu->inhibitItemDescriptionPrint = FALSE;
        CloseItemMessage(taskId);
    }
}

static void Task_ItemContext_Deposit(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    tItemCount = 1;
    if (tQuantity == 1)
    {
        TryDepositItem(taskId);
    }
    else
    {
        u8 windowId = BagMenu_AddWindow(ITEMWIN_ASKTOSS_QUANTITY);
        CopyItemName(gSpecialVar_ItemId, gStringVar1);
        StringExpandPlaceholders(gStringVar4, sText_DepositHowManyVar1);
        BagMenu_Print(windowId, FONT_NORMAL, gStringVar4, 0, 2, 1, 0, 0, 1);
        AddItemQuantityWindow(ITEMWIN_QUANTITY);
        CreatePocketScrollArrowPair_QuantitySelect();
        gTasks[taskId].func = Task_ChooseHowManyToDeposit;
    }
}

static void Task_ChooseHowManyToDeposit(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    if (AdjustQuantityAccordingToDPadInput(&tItemCount, tQuantity) == TRUE)
    {
        PrintItemQuantity(gBagMenu->windowIds[ITEMWIN_QUANTITY], tItemCount);
    }
    else if (JOY_NEW(A_BUTTON))
    {
        PlaySE(SE_SELECT);
        BagMenu_RemoveWindow(ITEMWIN_ASKTOSS_QUANTITY);
        BagMenu_RemoveWindow(ITEMWIN_QUANTITY);
        ScheduleBgCopyTilemapToVram(0);
        BagDestroyPocketScrollArrowPair();
        TryDepositItem(taskId);
    }
    else if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        BagMenu_RemoveWindow(ITEMWIN_ASKTOSS_QUANTITY);
        BagMenu_RemoveWindow(ITEMWIN_QUANTITY);
        PutWindowTilemap(WIN_DESCRIPTION);
        ScheduleBgCopyTilemapToVram(0);
        BagMenu_PrintCursor(tListTaskId, COLORID_BLACK_CURSOR);
        BagDestroyPocketScrollArrowPair();
        ReturnToItemList(taskId);
    }
}

static void TryDepositItem(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    if (GetItemImportance(gSpecialVar_ItemId))
    {
        DisplayItemMessage(taskId, FONT_NORMAL, sText_CantStoreImportantItems, HandleErrorMessage);
    }
    else if (AddPCItem(gSpecialVar_ItemId, tItemCount) == TRUE)
    {
        ItemUse_SetQuestLogEvent(QL_EVENT_DEPOSITED_ITEM_PC, 0, gSpecialVar_ItemId, 0xFFFF);
        CopyItemName(gSpecialVar_ItemId, gStringVar1);
        ConvertIntToDecimalStringN(gStringVar2, tItemCount, STR_CONV_MODE_LEFT_ALIGN, 3);
        StringExpandPlaceholders(gStringVar4, sText_DepositedVar2Var1s);
        BagMenu_Print(BagMenu_AddWindow(ITEMWIN_TOSSED), FONT_NORMAL, gStringVar4, 0, 2, 1, 0, 0, 1);
        gTasks[taskId].func = Task_RemoveItemFromBag;
    }
    else
    {
        DisplayItemMessage(taskId, FONT_NORMAL, sText_NoRoomForItems, HandleErrorMessage);
    }
}

static bool32 IsTutorialBag(void)
{
    if (
        gBagPosition.location == ITEMMENULOCATION_OLD_MAN 
     || gBagPosition.location == ITEMMENULOCATION_TTVSCR_CATCHING 
     || gBagPosition.location == ITEMMENULOCATION_TTVSCR_STATUS 
     || gBagPosition.location == ITEMMENULOCATION_TTVSCR_REGISTER 
     || gBagPosition.location == ITEMMENULOCATION_TTVSCR_TMS
    )
        return TRUE;
    return FALSE;
}

static void PrepareBagForTutorial(void)
{
    u32 i;
    sBackupPlayerBag = AllocZeroed(sizeof(*sBackupPlayerBag));
    memcpy(sBackupPlayerBag->bagPocket_Items, gSaveBlock1Ptr->bag.items, sizeof(gSaveBlock1Ptr->bag.items));
    memcpy(sBackupPlayerBag->bagPocket_KeyItems, gSaveBlock1Ptr->bag.keyItems, sizeof(gSaveBlock1Ptr->bag.keyItems));
    memcpy(sBackupPlayerBag->bagPocket_PokeBalls, gSaveBlock1Ptr->bag.pokeBalls, sizeof(gSaveBlock1Ptr->bag.pokeBalls));
    sBackupPlayerBag->registeredItem = gSaveBlock1Ptr->registeredItem;
    sBackupPlayerBag->pocket = gBagPosition.pocket;
    for (i = 0; i < POCKETS_COUNT_NO_CASES; i++)
    {
        sBackupPlayerBag->cursorPosition[i] = gBagPosition.cursorPosition[i];
        sBackupPlayerBag->scrollPosition[i] = gBagPosition.scrollPosition[i];
    }
    memset(gSaveBlock1Ptr->bag.items, 0, sizeof(gSaveBlock1Ptr->bag.items));
    memset(gSaveBlock1Ptr->bag.pokeBalls, 0, sizeof(gSaveBlock1Ptr->bag.pokeBalls));
    memset(gSaveBlock1Ptr->bag.pokeBalls, 0, sizeof(gSaveBlock1Ptr->bag.pokeBalls));
    gSaveBlock1Ptr->registeredItem = ITEM_NONE;
    ResetBagScrollPositions();
}

static void RestorePlayerBag(void)
{
    u32 i;
    memcpy(gSaveBlock1Ptr->bag.items, sBackupPlayerBag->bagPocket_Items, BAG_ITEMS_COUNT * sizeof(struct ItemSlot));
    memcpy(gSaveBlock1Ptr->bag.keyItems, sBackupPlayerBag->bagPocket_KeyItems, BAG_KEYITEMS_COUNT * sizeof(struct ItemSlot));
    memcpy(gSaveBlock1Ptr->bag.pokeBalls, sBackupPlayerBag->bagPocket_PokeBalls, BAG_POKEBALLS_COUNT * sizeof(struct ItemSlot));
    gSaveBlock1Ptr->registeredItem = sBackupPlayerBag->registeredItem;
    gBagPosition.pocket = sBackupPlayerBag->pocket;
    for (i = 0; i < POCKETS_COUNT_NO_CASES; i++)
    {
        gBagPosition.cursorPosition[i] = sBackupPlayerBag->cursorPosition[i];
        gBagPosition.scrollPosition[i] = sBackupPlayerBag->scrollPosition[i];
    }
    Free(sBackupPlayerBag);
}

void InitOldManBag(void)
{
    PrepareBagForTutorial();
    AddBagItem(ITEM_POTION, 1);
    AddBagItem(ITEM_POKE_BALL, 1);
    GoToBagMenu(ITEMMENULOCATION_OLD_MAN, POCKET_ITEMS, CB2_SetUpReshowBattleScreenAfterMenu2);
}

#define tTimer data[8]
#define TUTORIAL_BAG_DELAY 102 // The number of frames between each action Wally takes in the bag

static void Task_Bag_OldManTutorial(u8 taskId)
{
    s16 *data = gTasks[taskId].data;
    if (!gPaletteFade.active)
    {
        switch (tTimer)
        {
        case TUTORIAL_BAG_DELAY * 1:
        case TUTORIAL_BAG_DELAY * 2:
            PlaySE(SE_BAG_POCKET);
            SwitchBagPocket(taskId, 1, FALSE);
            break;
        case TUTORIAL_BAG_DELAY * 3:
            PlaySE(SE_SELECT);
            BagMenu_PrintCursor(tListTaskId, COLORID_GRAY_CURSOR);
            gSpecialVar_ItemId = ITEM_POKE_BALL;
            OpenContextMenu(taskId);
            break;
        case TUTORIAL_BAG_DELAY * 4:
            PlaySE(SE_SELECT);
            RemoveContextWindow();
            BagMenu_RemoveWindow(ITEMWIN_SELECTIONTEXT);
            PutWindowTilemap(WIN_ITEM_LIST);
            PutWindowTilemap(WIN_DESCRIPTION);
            CopyWindowToVram(WIN_ITEM_LIST, COPYWIN_MAP);
            DestroyListMenuTask(tListTaskId, NULL, NULL);
            RestorePlayerBag();
            gTasks[taskId].func = Task_Pokedude_FadeFromBag;
            return;
        }
        tTimer++;
    }
}

static void Task_Pokedude_FadeFromBag(u8 taskId)
{
    BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
    gTasks[taskId].func = Task_Pokedude_WaitFadeAndExitBag;
}

static void Task_Pokedude_WaitFadeAndExitBag(u8 taskId)
{
    if (gPaletteFade.active)
        return;

    if (gBagMenu->newScreenCallback != NULL)
        SetMainCallback2(gBagMenu->newScreenCallback);
    else
        SetMainCallback2(gBagPosition.exitCallback);
    BagDestroyPocketScrollArrowPair();
    FreeBagMenu();
    DestroyTask(taskId);
}

void InitPokedudeBag(u8 a0)
{
    MainCallback cb2;
    u8 location;
    PrepareBagForTutorial();
    AddBagItem(ITEM_POTION, 1);
    AddBagItem(ITEM_ANTIDOTE, 1);
    AddBagItem(ITEM_TEACHY_TV, 1);
    AddBagItem(ITEM_TM_CASE, 1);
    AddBagItem(ITEM_POKE_BALL, 5);
    AddBagItem(ITEM_GREAT_BALL, 1);
    AddBagItem(ITEM_NEST_BALL, 1);
    switch (a0)
    {
    default:
        cb2 = CB2_ReturnToTeachyTV;
        location = a0;
        break;
    case ITEMMENULOCATION_TTVSCR_STATUS:
        cb2 = CB2_SetUpReshowBattleScreenAfterMenu2;
        location = ITEMMENULOCATION_TTVSCR_STATUS;
        break;
    case ITEMMENULOCATION_TTVSCR_CATCHING:
        cb2 = CB2_SetUpReshowBattleScreenAfterMenu2;
        location = ITEMMENULOCATION_TTVSCR_CATCHING;
        break;
    }
    GoToBagMenu(location, POCKET_ITEMS, cb2);
}

static bool8 Task_BButtonInterruptTeachyTv(u8 taskId)
{
    if (JOY_NEW(B_BUTTON))
    {
        RestorePlayerBag();
        SetTeachyTvControllerModeToResume();
        gBagMenu->newScreenCallback = CB2_ReturnToTeachyTV;
        gTasks[taskId].func = Task_Pokedude_FadeFromBag;
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

static void Task_Bag_TeachyTvRegister(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    if (!gPaletteFade.active && Task_BButtonInterruptTeachyTv(taskId) != TRUE)
    {
        switch (tTimer)
        {
        case TUTORIAL_BAG_DELAY:
            PlaySE(SE_BAG_POCKET);
            SwitchBagPocket(taskId, 1, FALSE);
            break;
        case TUTORIAL_BAG_DELAY * 2:
            PlaySE(SE_SELECT);
            BagMenu_PrintCursor(tListTaskId, COLORID_GRAY_CURSOR);
            gSpecialVar_ItemId = ITEM_TEACHY_TV;
            OpenContextMenu(taskId);
            break;
        case TUTORIAL_BAG_DELAY * 3:
            PlaySE(SE_SELECT);
            Menu_MoveCursorNoWrapAround(1);
            break;
        case TUTORIAL_BAG_DELAY * 4:
            PlaySE(SE_SELECT);
            gSaveBlock1Ptr->registeredItem = gSpecialVar_ItemId;
            RemoveContextWindow();
            BagMenu_RemoveWindow(ITEMWIN_SELECTIONTEXT);
            PutWindowTilemap(WIN_ITEM_LIST);
            PutWindowTilemap(WIN_DESCRIPTION);
            DestroyListMenuTask(tListTaskId, &gBagPosition.scrollPosition[gBagPosition.pocket], &gBagPosition.cursorPosition[gBagPosition.pocket]);
            LoadBagItemListBuffers(gBagPosition.pocket);
            tListTaskId = ListMenuInit(&gMultiuseListMenuTemplate, gBagPosition.scrollPosition[gBagPosition.pocket], gBagPosition.cursorPosition[gBagPosition.pocket]);
            BagMenu_PrintCursor(tListTaskId, COLORID_BLACK_CURSOR);
            CopyWindowToVram(WIN_ITEM_LIST, COPYWIN_MAP);
            break;
        case TUTORIAL_BAG_DELAY * 5:
        case TUTORIAL_BAG_DELAY * 6:
            gMain.newKeys = 0;
            gMain.newAndRepeatedKeys = DPAD_DOWN;
            ListMenu_ProcessInput(tListTaskId);
            break;
        case TUTORIAL_BAG_DELAY * 7:
            PlaySE(SE_SELECT);
            DestroyListMenuTask(tListTaskId, NULL, NULL);
            RestorePlayerBag();
            gTasks[taskId].func = Task_Pokedude_FadeFromBag;
            return;
        }
        tTimer++;
    }
}

static void Task_Bag_TeachyTvCatching(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    if (!gPaletteFade.active)
    {
        if (Task_BButtonInterruptTeachyTv(taskId) == TRUE)
        {
            FreeRestoreBattleData();
            LoadPlayerParty();
            return;
        }
        switch (tTimer)
        {
        case TUTORIAL_BAG_DELAY * 1:
        case TUTORIAL_BAG_DELAY * 2:
            PlaySE(SE_BAG_POCKET);
            SwitchBagPocket(taskId, 1, FALSE);
            break;
        case TUTORIAL_BAG_DELAY * 3:
        case TUTORIAL_BAG_DELAY * 4:
            gMain.newKeys = 0;
            gMain.newAndRepeatedKeys = DPAD_DOWN;
            ListMenu_ProcessInput(data[0]);
            break;
        case TUTORIAL_BAG_DELAY * 5:
        case TUTORIAL_BAG_DELAY * 6:
            gMain.newKeys = 0;
            gMain.newAndRepeatedKeys = DPAD_UP;
            ListMenu_ProcessInput(data[0]);
            break;
        case TUTORIAL_BAG_DELAY * 7:
            PlaySE(SE_SELECT);
            BagMenu_PrintCursor(tListTaskId, COLORID_GRAY_CURSOR);
            gSpecialVar_ItemId = ITEM_POKE_BALL;
            OpenContextMenu(taskId);
            break;
        case TUTORIAL_BAG_DELAY * 8:
            PlaySE(SE_SELECT);
            RemoveContextWindow();
            BagMenu_RemoveWindow(ITEMWIN_SELECTIONTEXT);
            PutWindowTilemap(WIN_ITEM_LIST);
            PutWindowTilemap(WIN_DESCRIPTION);
            CopyWindowToVram(WIN_ITEM_LIST, COPYWIN_MAP);
            DestroyListMenuTask(tListTaskId, NULL, NULL);
            RestorePlayerBag();
            gTasks[taskId].func = Task_Pokedude_FadeFromBag;
            return;
        }
        tTimer++;
    }
}

static void Task_Bag_TeachyTvStatus(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    if (!gPaletteFade.active)
    {
        if (Task_BButtonInterruptTeachyTv(taskId) == TRUE)
        {
            FreeRestoreBattleData();
            LoadPlayerParty();
            return;
        }
        switch (tTimer)
        {
        case TUTORIAL_BAG_DELAY * 1:
            gMain.newKeys = 0;
            gMain.newAndRepeatedKeys = DPAD_DOWN;
            ListMenu_ProcessInput(data[0]);
            break;
        case TUTORIAL_BAG_DELAY * 2:
            PlaySE(SE_SELECT);
            BagMenu_PrintCursor(tListTaskId, COLORID_GRAY_CURSOR);
            gSpecialVar_ItemId = ITEM_ANTIDOTE;
            OpenContextMenu(taskId);
            break;
        case TUTORIAL_BAG_DELAY * 3:
            PlaySE(SE_SELECT);
            RemoveContextWindow();
            BagMenu_RemoveWindow(ITEMWIN_SELECTIONTEXT);
            PutWindowTilemap(WIN_ITEM_LIST);
            PutWindowTilemap(WIN_DESCRIPTION);
            CopyWindowToVram(WIN_ITEM_LIST, COPYWIN_MAP);
            DestroyListMenuTask(tListTaskId, NULL, NULL);
            RestorePlayerBag();
            gItemUseCB = ItemUseCB_BattleScript;
            ItemMenu_SetExitCallback(Pokedude_ChooseMonForInBattleItem);
            gTasks[taskId].func = Task_Pokedude_FadeFromBag;
            return;
        }
        tTimer++;
    }
}

static void Task_Bag_TeachyTvTMs(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    if (!gPaletteFade.active && Task_BButtonInterruptTeachyTv(taskId) != TRUE)
    {
        switch (tTimer)
        {
        case TUTORIAL_BAG_DELAY * 1:
            PlaySE(SE_BAG_POCKET);
            SwitchBagPocket(taskId, 1, 0);
            break;
        case TUTORIAL_BAG_DELAY * 2:
            gMain.newKeys = 0;
            gMain.newAndRepeatedKeys = DPAD_DOWN;
            ListMenu_ProcessInput(data[0]);
            break;
        case TUTORIAL_BAG_DELAY * 3:
            PlaySE(SE_SELECT);
            BagMenu_PrintCursor(tListTaskId, COLORID_GRAY_CURSOR);
            gSpecialVar_ItemId = ITEM_TM_CASE;
            OpenContextMenu(taskId);
            break;
        case TUTORIAL_BAG_DELAY * 4:
            PlaySE(SE_SELECT);
            RemoveContextWindow();
            BagMenu_RemoveWindow(ITEMWIN_SELECTIONTEXT);
            PutWindowTilemap(WIN_ITEM_LIST);
            PutWindowTilemap(WIN_DESCRIPTION);
            CopyWindowToVram(WIN_ITEM_LIST, COPYWIN_MAP);
            DestroyListMenuTask(tListTaskId, NULL, NULL);
            RestorePlayerBag();
            gBagMenu->newScreenCallback = Pokedude_InitTMCase;
            gTasks[taskId].func = Task_Pokedude_FadeFromBag;
            return;
        }
        tTimer++;
    }
}

#undef TUTORIAL_BAG_DELAY
#undef tTimer

static void PrintPocketName(void)
{
    const u8 *str = gPocketNamesStringsTable[gBagPosition.pocket];
    u32 x = (72 - GetStringWidth(FONT_NORMAL_COPY_1, str, 0)) / 2;

    FillWindowPixelBuffer(WIN_POCKET_NAME, PIXEL_FILL(0));
    AddTextPrinterParameterized3(WIN_POCKET_NAME, FONT_NORMAL_COPY_1, x, 1, sFontColorTable[COLORID_NORMAL], 0, str);
}

static void LoadBagMenuTextWindows(void)
{
    u32 i;

    if (gBagPosition.location != ITEMMENULOCATION_ITEMPC)
        InitWindows(sDefaultBagWindows);
    else
        InitWindows(sDefaultBagWindowsDeposit);

    DeactivateAllTextPrinters();
    LoadUserWindowBorderGfx(0, 0x64, BG_PLTT_ID(14));
    LoadMessageBoxGfx(0, 0x6D, BG_PLTT_ID(13));
    LoadStdWindowGfx(0, 0x81, BG_PLTT_ID(12));
    LoadPalette(sBagWindowPalF, BG_PLTT_ID(15), sizeof(sBagWindowPalF));
    for (i = 0; i <= WIN_POCKET_NAME; i++)
    {
        FillWindowPixelBuffer(i, PIXEL_FILL(0));
        PutWindowTilemap(i);
    }
    ScheduleBgCopyTilemapToVram(0);
}

static void BagMenu_Print(u8 windowId, u8 fontId, const u8 * str, u8 x, u8 y, u8 letterSpacing, u8 lineSpacing, u8 speed, u8 colorIdx)
{
    AddTextPrinterParameterized4(windowId, fontId, x, y, letterSpacing, lineSpacing, sFontColorTable[colorIdx], speed, str);
}

static u8 BagMenu_AddWindow(u8 windowType)
{
    u8 *windowId = &gBagMenu->windowIds[windowType];
    if (*windowId == WINDOW_NONE)
    {
        *windowId = AddWindow(&sContextMenuWindowTemplates[windowType]);
        switch (windowType)
        {
            case ITEMWIN_SELECTIONTEXT:
            case ITEMWIN_ASKTOSS:
            case ITEMWIN_ASKTOSS_QUANTITY:
            case ITEMWIN_TOSSED:
                DrawStdFrameWithCustomTileAndPalette(*windowId, FALSE, 0x081, 12);
                break;
            default:
                DrawStdFrameWithCustomTileAndPalette(*windowId, FALSE, 0x064, 14);
                break;
        }
        ScheduleBgCopyTilemapToVram(0);
    }
    return *windowId;
}

void BagMenu_RemoveWindow(u8 whichWindow)
{
    u8 *windowId = &gBagMenu->windowIds[whichWindow];
    
    if (*windowId != WINDOW_NONE)
    {
        ClearStdWindowAndFrameToTransparent(gBagMenu->windowIds[whichWindow], FALSE);
        ClearWindowTilemap(gBagMenu->windowIds[whichWindow]);
        RemoveWindow(gBagMenu->windowIds[whichWindow]);
        ScheduleBgCopyTilemapToVram(0);
        *windowId = WINDOW_NONE;
    }
}

static u8 AddItemMessageWindow(u8 windowType)
{
    u8 *windowId = &gBagMenu->windowIds[windowType];
    if (*windowId == WINDOW_NONE)
        *windowId = AddWindow(&sContextMenuWindowTemplates[windowType]);
    return *windowId;
}

static void RemoveItemMessageWindow(u8 windowType)
{
    u8 *windowId = &gBagMenu->windowIds[windowType];
    if (*windowId != WINDOW_NONE)
    {
        ClearDialogWindowAndFrameToTransparent(*windowId, FALSE);
        ClearWindowTilemap(*windowId);
        RemoveWindow(*windowId);
        PutWindowTilemap(WIN_DESCRIPTION);
        ScheduleBgCopyTilemapToVram(0);
        *windowId = WINDOW_NONE;
    }
}

void BagMenu_YesNo(u8 taskId, u8 windowType, const struct YesNoFuncTable *funcTable)
{
    CreateYesNoMenuWithCallbacks(taskId, &sContextMenuWindowTemplates[windowType], FONT_NORMAL, 0, 2, 0x064, 14, funcTable);
}

static void DisplayCurrentMoneyWindow(void)
{
    u8 windowId = BagMenu_AddWindow(ITEMWIN_MONEY);
    PrintMoneyAmountInMoneyBoxWithBorder(windowId, 0x081, 0x0C, GetMoney(&gSaveBlock1Ptr->money));
}

static void BagDrawTextBoxOnWindow(u8 windowId)
{
    DrawTextBorderOuter(windowId, 0x064, 14);
}

void ItemMenu_SetExitCallback(MainCallback cb)
{
    gBagMenu->newScreenCallback = cb;
}

static void ReturnToBagMenuFromSubmenu_Give(void)
{
    CB2_SelectBagItemToGive();
}

static void BagDrawDepositItemTextBox(void)
{
    u32 x = 64 - GetStringWidth(FONT_SMALL, gText_DepositItem, 0);

    DrawStdFrameWithCustomTileAndPalette(WIN_POCKET_NAME, FALSE, 0x081, 12);
    AddTextPrinterParameterized(WIN_POCKET_NAME, FONT_SMALL, gText_DepositItem, x / 2, 1, 0, NULL);
}

