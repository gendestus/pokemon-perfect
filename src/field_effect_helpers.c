#include "global.h"
#include "gflib.h"
#include "event_object_movement.h"
#include "field_camera.h"
#include "field_effect.h"
#include "field_effect_helpers.h"
#include "field_weather.h"
#include "fieldmap.h"
#include "metatile_behavior.h"
#include "rtc.h"
#include "constants/field_effects.h"
#include "constants/event_objects.h"
#include "constants/songs.h"

#define OBJ_EVENT_PAL_TAG_NONE 0x11FF // duplicate of define in event_object_movement.c
#define PAL_TAG_REFLECTION_OFFSET 0x2000 // reflection tag value is paletteTag + 0x2000
#define PAL_RAW_REFLECTION_OFFSET 0x4000 // raw reflection tag is paletteNum + 0x4000
#define HIGH_BRIDGE_PAL_TAG 0x4010
// Build a unique tag for reflection's palette based on based tag, or paletteNum
#define REFLECTION_PAL_TAG(tag, num) ((tag) == TAG_NONE ? (num) + PAL_RAW_REFLECTION_OFFSET : (tag) + PAL_TAG_REFLECTION_OFFSET)

static void UpdateObjectReflectionSprite(struct Sprite *sprite);
static void LoadObjectReflectionPalette(struct ObjectEvent * objectEvent, struct Sprite *sprite);
static void LoadObjectHighBridgeReflectionPalette(struct ObjectEvent *, struct Sprite *sprite);
static void LoadObjectRegularReflectionPalette(struct ObjectEvent *, struct Sprite *sprite);
static void UpdateGrassFieldEffectSubpriority(struct Sprite *sprite, u8 z, u8 offset);
static void FadeFootprintsTireTracks_Step0(struct Sprite *sprite);
static void FadeFootprintsTireTracks_Step1(struct Sprite *sprite);
static void UpdateFeetInFlowingWaterFieldEffect(struct Sprite *sprite);
static void UpdateAshFieldEffect_Step0(struct Sprite *sprite);
static void UpdateAshFieldEffect_Step1(struct Sprite *sprite);
static void UpdateAshFieldEffect_Step2(struct Sprite *sprite);
static void SynchroniseSurfAnim(struct ObjectEvent * objectEvent, struct Sprite *sprite);
static void SynchroniseSurfPosition(struct ObjectEvent * objectEvent, struct Sprite *sprite);
static void CreateBobbingEffect(struct ObjectEvent * objectEvent, struct Sprite *linkedSprite, struct Sprite *sprite);
static void SpriteCB_UnderwaterSurfBlob(struct Sprite *sprite);
static u32 ShowDisguiseFieldEffect(u32 fldEff, const struct SpriteTemplate* template, u32 paletteNum);

// Data used by all the field effects that share UpdateJumpImpactEffect
#define sJumpElevation  data[0]
#define sJumpFldEff     data[1]

// Data used by all the field effects that share WaitFieldEffectSpriteAnim
#define sWaitFldEff  data[0]

#define sReflectionObjEventId       data[0]
#define sReflectionObjEventLocalId  data[1]
#define sReflectionVerticalOffset   data[2]
#define sIsStillReflection          data[7]

void SetUpReflection(struct ObjectEvent * objectEvent, struct Sprite *sprite, bool8 stillReflection)
{
    struct Sprite *reflectionSprite;

    reflectionSprite = &gSprites[CreateCopySpriteAt(sprite, sprite->x, sprite->y, 152)];
    reflectionSprite->callback = UpdateObjectReflectionSprite;
    reflectionSprite->oam.priority = 3;
    reflectionSprite->usingSheet = TRUE;
    reflectionSprite->anims = gDummySpriteAnimTable;
    StartSpriteAnim(reflectionSprite, 0);
    reflectionSprite->affineAnims = gDummySpriteAffineAnimTable;
    reflectionSprite->affineAnimBeginning = TRUE;
    reflectionSprite->subspriteMode = SUBSPRITES_IGNORE_PRIORITY;
    reflectionSprite->subspriteTableNum = 0;
    reflectionSprite->sReflectionObjEventId = sprite->sReflectionObjEventId;
    reflectionSprite->sReflectionObjEventLocalId = objectEvent->localId;
    reflectionSprite->sIsStillReflection = stillReflection;
    LoadObjectReflectionPalette(objectEvent, reflectionSprite);

    if (!stillReflection)
        reflectionSprite->oam.affineMode = ST_OAM_AFFINE_NORMAL;
}

static s16 GetReflectionVerticalOffset(struct ObjectEvent * objectEvent)
{
    return GetObjectEventGraphicsInfo(objectEvent->graphicsId)->height - 2;
}

static void LoadObjectReflectionPalette(struct ObjectEvent * objectEvent, struct Sprite *reflectionSprite)
{
    u8 bridgeType;
    u16 bridgeReflectionVerticalOffsets[] = {
        [0] = 12,
        [1] = 28,
        [2] = 44
    };
    reflectionSprite->sReflectionVerticalOffset = 0;
    if ((bridgeType = MetatileBehavior_GetBridgeType(objectEvent->previousMetatileBehavior))
        || (bridgeType = MetatileBehavior_GetBridgeType(objectEvent->currentMetatileBehavior)))
    {
        reflectionSprite->sReflectionVerticalOffset = bridgeReflectionVerticalOffsets[bridgeType - 1];
        LoadObjectHighBridgeReflectionPalette(objectEvent, reflectionSprite);
    }
    else
    {
        LoadObjectRegularReflectionPalette(objectEvent, reflectionSprite);
    }
}

// Apply a blue tint effect to a palette
static void ApplyPondFilter(u8 paletteNum, u16 *dest)
{
    u32 i, r, g, b;
    // CpuCopy16(gPlttBufferUnfaded + 0x100 + paletteNum * 16, dest, 32);
    u16 *src = gPlttBufferUnfaded + OBJ_PLTT_ID(paletteNum);
    *dest++ = *src++; // copy transparency
    for (i = 0; i < 16 - 1; i++)
    {
        r = GET_R(src[i]);
        g = GET_G(src[i]);
        b = GET_B(src[i]);
        b += 10;
        if (b > 31)
            b = 31;
        *dest++ = RGB2(r, g, b);
    }
}

// Apply a ice tint effect to a palette
static void ApplyIceFilter(u8 paletteNum, u16 *dest)
{
    u32 i, r, g, b;
    // CpuCopy16(gPlttBufferUnfaded + 0x100 + paletteNum * 16, dest, 32);
    u16 *src = gPlttBufferUnfaded + OBJ_PLTT_ID(paletteNum);
    *dest++ = *src++; // copy transparency
    for (i = 0; i < 16 - 1; i++)
    {
        r = GET_R(src[i]);
        r -= 5;
        if (r > 31)
            r = 0;
        g = GET_G(src[i]);
        g += 3;
        if (g > 31)
            g = 31;
        b = GET_B(src[i]);
        b += 16;
        if (b > 31)
            b = 31;
        *dest++ = RGB2(r, g, b);
    }
}

static void LoadObjectRegularReflectionPalette(struct ObjectEvent *objectEvent, struct Sprite *sprite)
{
    const struct Sprite *mainSprite = &gSprites[objectEvent->spriteId];
    u16 baseTag = GetSpritePaletteTagByPaletteNum(mainSprite->oam.paletteNum);
    u16 paletteTag = REFLECTION_PAL_TAG(baseTag, mainSprite->oam.paletteNum);
    u8 paletteNum = IndexOfSpritePaletteTag(paletteTag);
    if (paletteNum <= 16)
    {
        // Load filtered palette
        u16 filteredData[16];
        struct SpritePalette filteredPal = {.tag = paletteTag, .data = filteredData};
        if (sprite->sIsStillReflection == FALSE)
            ApplyPondFilter(mainSprite->oam.paletteNum, filteredData);
        else
            ApplyIceFilter(mainSprite->oam.paletteNum, filteredData);
        paletteNum = LoadSpritePalette(&filteredPal);
        ApplyGlobalFieldPaletteTint(paletteNum);
        UpdateSpritePaletteWithWeather(paletteNum, TRUE);
    }
    sprite->oam.paletteNum = paletteNum;
    sprite->oam.objMode = ST_OAM_OBJ_BLEND;
}

// When walking on a bridge high above water (Route 120), the reflection is a solid dark blue color.
// This is so the sprite blends in with the dark water metatile underneath the bridge.
static void LoadObjectHighBridgeReflectionPalette(struct ObjectEvent *objectEvent, struct Sprite *sprite)
{
    u16 blueData[16];
    struct SpritePalette bluePalette = {.tag = HIGH_BRIDGE_PAL_TAG, .data = blueData};
    CpuFill16(0x55C9, blueData, PLTT_SIZE_4BPP);
    sprite->oam.paletteNum = LoadSpritePalette(&bluePalette);
    ApplyGlobalFieldPaletteTint(sprite->oam.paletteNum);
    UpdateSpritePaletteWithWeather(sprite->oam.paletteNum, TRUE);
}

static void UpdateObjectReflectionSprite(struct Sprite *reflectionSprite)
{
    struct ObjectEvent *objectEvent = &gObjectEvents[reflectionSprite->sReflectionObjEventId];
    struct Sprite *mainSprite = &gSprites[objectEvent->spriteId];

    if (!objectEvent->active || !objectEvent->hasReflection || objectEvent->localId != reflectionSprite->sReflectionObjEventLocalId)
    {
        reflectionSprite->inUse = FALSE;
        FieldEffectFreePaletteIfUnused(reflectionSprite->oam.paletteNum);
        return;
    }

    // Only filter palette if not using the high bridge blue palette
    // This is basically a copy of LoadObjectRegularReflectionPalette
    if (IndexOfSpritePaletteTag(HIGH_BRIDGE_PAL_TAG) != reflectionSprite->oam.paletteNum)
    {
        u16 baseTag = GetSpritePaletteTagByPaletteNum(mainSprite->oam.paletteNum);
        u16 paletteTag = REFLECTION_PAL_TAG(baseTag, mainSprite->oam.paletteNum);
        u8 paletteNum = IndexOfSpritePaletteTag(paletteTag);
        if (paletteNum >= 16)
        {
            // Build filtered palette
            u16 filteredData[16];
            struct SpritePalette filteredPal = {.tag = paletteTag, .data = filteredData};
            // Free palette if unused
            reflectionSprite->inUse = FALSE;
            FieldEffectFreePaletteIfUnused(reflectionSprite->oam.paletteNum);
            reflectionSprite->inUse = TRUE;
            if (reflectionSprite->sIsStillReflection == FALSE)
                ApplyPondFilter(mainSprite->oam.paletteNum, filteredData);
            else
                ApplyIceFilter(mainSprite->oam.paletteNum, filteredData);
            paletteNum = LoadSpritePalette(&filteredPal);
            ApplyGlobalFieldPaletteTint(paletteNum);
            UpdateSpritePaletteWithWeather(paletteNum, TRUE);
        }
        reflectionSprite->oam.paletteNum = paletteNum;
    }
    reflectionSprite->oam.shape = mainSprite->oam.shape;
    reflectionSprite->oam.size = mainSprite->oam.size;
    reflectionSprite->oam.matrixNum = mainSprite->oam.matrixNum | ST_OAM_VFLIP;
    reflectionSprite->oam.tileNum = mainSprite->oam.tileNum;
    reflectionSprite->subspriteTables = mainSprite->subspriteTables;
    reflectionSprite->invisible = mainSprite->invisible;
    reflectionSprite->x = mainSprite->x;
    // sReflectionVerticalOffset is only set for high bridges
    reflectionSprite->y = mainSprite->y + GetReflectionVerticalOffset(objectEvent) + reflectionSprite->sReflectionVerticalOffset;
    reflectionSprite->centerToCornerVecX = mainSprite->centerToCornerVecX;
    reflectionSprite->centerToCornerVecY = mainSprite->centerToCornerVecY;
    reflectionSprite->x2 = mainSprite->x2;
    reflectionSprite->y2 = -mainSprite->y2;
    reflectionSprite->coordOffsetEnabled = mainSprite->coordOffsetEnabled;

    if (objectEvent->hideReflection == TRUE)
        reflectionSprite->invisible = TRUE;

    // Support "virtual" sprites which can't be rotated via affines
    if (reflectionSprite->subspriteTables[0].subsprites)
    {
        reflectionSprite->oam.affineMode = ST_OAM_AFFINE_OFF;
        return;
    }
    if (reflectionSprite->sIsStillReflection == FALSE)
    {
        // Sets the reflection sprite's rot/scale matrix to the correct
        // water reflection matrix based on the main sprite's facing direction.
        // If the sprite is facing east, then it's flipped, and its matrixNum is 1.
        reflectionSprite->oam.matrixNum = (mainSprite->oam.matrixNum & ST_OAM_HFLIP) ? 1 : 0;
    }
}

#undef sReflectionObjEventId
#undef sReflectionObjEventLocalId
#undef sReflectionVerticalOffset
#undef sIsStillReflection

u8 CreateWarpArrowSprite(void)
{
    u8 spriteId;
    struct Sprite *sprite;

    spriteId = CreateSpriteAtEnd(&gFieldEffectObjectTemplate_Arrow, 0, 0, 0x52);
    if (spriteId != MAX_SPRITES)
    {
        sprite = &gSprites[spriteId];
        sprite->oam.paletteNum = LoadPlayerObjectEventPalette(gSaveBlock2Ptr->playerGender);
        sprite->oam.priority = 1;
        sprite->coordOffsetEnabled = TRUE;
        sprite->invisible = TRUE;
    }
    return spriteId;
}

void SetSpriteInvisible(u8 spriteId)
{
    gSprites[spriteId].invisible = TRUE;
}

void ShowWarpArrowSprite(u8 spriteId, u8 direction, s16 x, s16 y)
{
    s16 x2;
    s16 y2;
    struct Sprite *sprite;

    sprite = &gSprites[spriteId];
    if (sprite->invisible || sprite->data[0] != x || sprite->data[1] != y)
    {
        SetSpritePosToMapCoords(x, y, &x2, &y2);
        sprite = &gSprites[spriteId];
        sprite->x = x2 + 8;
        sprite->y = y2 + 8;
        sprite->invisible = FALSE;
        sprite->data[0] = x;
        sprite->data[1] = y;
        StartSpriteAnim(sprite, direction - 1);
    }
}

const struct SpriteTemplate* const gShadowEffectTemplates[] = {
    [SHADOW_SIZE_S]  = &gFieldEffectObjectTemplate_ShadowSmall,
    [SHADOW_SIZE_M]  = &gFieldEffectObjectTemplate_ShadowMedium,
    [SHADOW_SIZE_L]  = &gFieldEffectObjectTemplate_ShadowLarge,
    [SHADOW_SIZE_XL] = &gFieldEffectObjectTemplate_ShadowExtraLarge,
};

const u16 gShadowVerticalOffsets[] = {
    [SHADOW_SIZE_S]  =  4,
    [SHADOW_SIZE_M]  =  4,
    [SHADOW_SIZE_L]  =  4,
    [SHADOW_SIZE_XL] = 16
};

// Sprite data for FLDEFF_SHADOW
#define sLocalId  data[0]
#define sMapNum   data[1]
#define sMapGroup data[2]
#define sYOffset  data[3]

u32 FldEff_Shadow(void)
{
    u8 objectEventId;
    const struct ObjectEventGraphicsInfo *graphicsInfo;
    u8 spriteId;
    u8 i;
    for (i = 0; i < MAX_SPRITES; i++)
    {
        // Return early if a shadow sprite already exists
        if (gSprites[i].data[0] == gFieldEffectArguments[0] && gSprites[i].callback == UpdateShadowFieldEffect)
            return 0;
    }
    objectEventId = GetObjectEventIdByLocalIdAndMap(gFieldEffectArguments[0], gFieldEffectArguments[1], gFieldEffectArguments[2]);
    graphicsInfo = GetObjectEventGraphicsInfo(gObjectEvents[objectEventId].graphicsId);
    if (graphicsInfo->shadowSize == SHADOW_SIZE_XL) // don't create a shadow at all
        return 0;
    spriteId = CreateSpriteAtEnd(gShadowEffectTemplates[graphicsInfo->shadowSize], 0, 0, 0x94);
    if (spriteId != MAX_SPRITES)
    {
        // SetGpuReg(REG_OFFSET_BLDALPHA, BLDALPHA_BLEND(8, 12));
        gSprites[spriteId].oam.objMode = 1; // BLEND
        gSprites[spriteId].coordOffsetEnabled = TRUE;
        gSprites[spriteId].sLocalId = gFieldEffectArguments[0];
        gSprites[spriteId].sMapNum = gFieldEffectArguments[1];
        gSprites[spriteId].sMapGroup = gFieldEffectArguments[2];
        #if OW_LARGE_OW_SUPPORT
        gSprites[spriteId].sYOffset = gShadowVerticalOffsets[graphicsInfo->shadowSize];
        #else
        gSprites[spriteId].sYOffset = (graphicsInfo->height >> 1) - gShadowVerticalOffsets[graphicsInfo->shadowSize];
        #endif
    }
    return 0;
}

void UpdateShadowFieldEffect(struct Sprite *sprite)
{
    u8 objectEventId;

    if (TryGetObjectEventIdByLocalIdAndMap(sprite->sLocalId, sprite->sMapNum, sprite->sMapGroup, &objectEventId))
    {
        FieldEffectStop(sprite, FLDEFF_SHADOW);
    }
    else
    {
        struct ObjectEvent *objectEvent = &gObjectEvents[objectEventId];
        struct Sprite *linkedSprite = &gSprites[objectEvent->spriteId];
        sprite->oam.priority = linkedSprite->oam.priority;
        sprite->x = linkedSprite->x;
        #if OW_LARGE_OW_SUPPORT
        // Read 'live' size from linked sprite
        sprite->y = linkedSprite->y - linkedSprite->centerToCornerVecY - sprite->sYOffset;
        #else
        sprite->y = linkedSprite->y + sprite->sYOffset;
        #endif
        sprite->invisible = linkedSprite->invisible;
        if (!objectEvent->active || !objectEvent->hasShadow
         || MetatileBehavior_IsPokeGrass(objectEvent->currentMetatileBehavior)
         || MetatileBehavior_IsSurfable(objectEvent->currentMetatileBehavior)
         || MetatileBehavior_IsSurfable(objectEvent->previousMetatileBehavior)
         || MetatileBehavior_IsReflective(objectEvent->currentMetatileBehavior)
         || MetatileBehavior_IsReflective(objectEvent->previousMetatileBehavior))
        {
            FieldEffectStop(sprite, FLDEFF_SHADOW);
        }
    }
}

#undef sLocalId
#undef sMapNum
#undef sMapGroup
#undef sYOffset

static const struct SpritePalette* GetGeneralFieldPalette1()
{
    if (OW_SEASONS)
    {
        switch (gLoadedSeason)
        {
            case SEASON_SPRING:
            default:
                return &gSpritePalette_GeneralFieldEffect1;
            case SEASON_SUMMER:
                return &gSpritePalette_GeneralFieldEffect1Summer;
            case SEASON_AUTUMN:
                return &gSpritePalette_GeneralFieldEffect1Autumn;
            case SEASON_WINTER:
                return &gSpritePalette_GeneralFieldEffect1Winter;

        }
    }
    return &gSpritePalette_GeneralFieldEffect1;
}


u32 FldEff_TallGrass(void)
{
    s16 x;
    s16 y;
    u8 spriteId;
    struct Sprite *sprite;
    const struct SpriteTemplate* spriteTemplate;
    const struct SpritePalette* spritePalette;

    if (OW_SEASONS)
    {
        switch (gLoadedSeason)
        {
            case SEASON_SPRING:
            default:
                spriteTemplate = &gFieldEffectObjectTemplate_TallGrass;
                break;
            case SEASON_SUMMER:
                spriteTemplate = &gFieldEffectObjectTemplate_TallGrassSummer;
                break;
            case SEASON_AUTUMN:
                spriteTemplate = &gFieldEffectObjectTemplate_TallGrassAutumn;
                break;
            case SEASON_WINTER:
                spriteTemplate = &gFieldEffectObjectTemplate_TallGrassWinter;
                break;

        }
    }
    else
    {
        spriteTemplate = &gFieldEffectObjectTemplate_TallGrass;
    }
    spritePalette = GetGeneralFieldPalette1();
    
    FieldEffectScript_LoadFadedPal(spritePalette);
    x = gFieldEffectArguments[0];
    y = gFieldEffectArguments[1];
    SetSpritePosToOffsetMapCoords(&x, &y, 8, 8);
    spriteId = CreateSpriteAtEnd(spriteTemplate, x, y, 0);

    if (spriteId != MAX_SPRITES)
    {
        sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gFieldEffectArguments[3];
        sprite->data[0] = gFieldEffectArguments[2];
        sprite->data[1] = gFieldEffectArguments[0];
        sprite->data[2] = gFieldEffectArguments[1];
        sprite->data[3] = gFieldEffectArguments[4];
        sprite->data[4] = gFieldEffectArguments[5];
        sprite->data[5] = gFieldEffectArguments[6];
        if (gFieldEffectArguments[7])
        {
            SeekSpriteAnim(sprite, 4);
        }
    }
    return 0;
}

void UpdateTallGrassFieldEffect(struct Sprite *sprite)
{
    u8 mapNum;
    u8 mapGroup;
    u8 metatileBehavior;
    u8 localId;
    u8 objectEventId;
    struct ObjectEvent * objectEvent;

    mapNum = sprite->data[5] >> 8;
    mapGroup = sprite->data[5];
    if (gCamera.active && (gSaveBlock1Ptr->location.mapNum != mapNum || gSaveBlock1Ptr->location.mapGroup != mapGroup))
    {
        sprite->data[1] -= gCamera.x;
        sprite->data[2] -= gCamera.y;
        sprite->data[5] = ((u8)gSaveBlock1Ptr->location.mapNum << 8) | (u8)gSaveBlock1Ptr->location.mapGroup;
    }
    localId = sprite->data[3] >> 8;
    mapNum = sprite->data[3];
    mapGroup = sprite->data[4];
    metatileBehavior = MapGridGetMetatileBehaviorAt(sprite->data[1], sprite->data[2]);
    if (TryGetObjectEventIdByLocalIdAndMap(localId, mapNum, mapGroup, &objectEventId) || !MetatileBehavior_IsTallGrass(metatileBehavior) || (sprite->data[7] && sprite->animEnded))
    {
        FieldEffectStop(sprite, FLDEFF_TALL_GRASS);
    }
    else
    {
        objectEvent = &gObjectEvents[objectEventId];
        if ((objectEvent->currentCoords.x != sprite->data[1] || objectEvent->currentCoords.y != sprite->data[2]) && (objectEvent->previousCoords.x != sprite->data[1] || objectEvent->previousCoords.y != sprite->data[2]))
            sprite->data[7] = TRUE;

        // This variable is misused.
        metatileBehavior = 0;
        if (sprite->animCmdIndex == 0)
            metatileBehavior = 4;

        UpdateObjectEventSpriteInvisibility(sprite, FALSE);
        UpdateGrassFieldEffectSubpriority(sprite, sprite->data[0], metatileBehavior);
    }
}

u32 FldEff_JumpTallGrass(void)
{
    u8 spriteId;
    struct Sprite *sprite;
    const struct SpriteTemplate* spriteTemplate;
    const struct SpritePalette* spritePalette;

    if (OW_SEASONS)
    {
        switch (gLoadedSeason)
        {
            // TODO: seasonal templates
            case SEASON_SPRING:
            default:
                spriteTemplate = &gFieldEffectObjectTemplate_JumpTallGrass;
                break;
            case SEASON_SUMMER:
                spriteTemplate = &gFieldEffectObjectTemplate_JumpTallGrass;
                break;
            case SEASON_AUTUMN:
                spriteTemplate = &gFieldEffectObjectTemplate_JumpTallGrass;
                break;
            case SEASON_WINTER:
                spriteTemplate = &gFieldEffectObjectTemplate_JumpTallGrass;
                break;

        }
    }
    else
    {
        spriteTemplate = &gFieldEffectObjectTemplate_JumpTallGrass;
    }
    spritePalette = GetGeneralFieldPalette1();

    FieldEffectScript_LoadFadedPal(spritePalette);
    SetSpritePosToOffsetMapCoords((s16 *)&gFieldEffectArguments[0], (s16 *)&gFieldEffectArguments[1], 8, 12);
    spriteId = CreateSpriteAtEnd(spriteTemplate, gFieldEffectArguments[0], gFieldEffectArguments[1], 0);
    if (spriteId != MAX_SPRITES)
    {
        sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gFieldEffectArguments[3];
        sprite->data[0] = gFieldEffectArguments[2];
        sprite->data[1] = 12;
    }
    return 0;
}

u8 FindTallGrassFieldEffectSpriteId(u8 localId, u8 mapNum, u8 mapGroup, s16 x, s16 y)
{
    struct Sprite *sprite;
    u8 i;

    for (i = 0; i < MAX_SPRITES; i++)
    {
        if (gSprites[i].inUse)
        {
            sprite = &gSprites[i];
            if (sprite->callback == UpdateTallGrassFieldEffect && (x == sprite->data[1] && y == sprite->data[2]) && (localId == (sprite->data[3] >> 8) && mapNum == (sprite->data[3] & 0xFF) && mapGroup == sprite->data[4]))
                return i;
        }
    }

    return MAX_SPRITES;
}

u32 FldEff_LongGrass(void)
{
    s16 x;
    s16 y;
    u8 spriteId;
    struct Sprite *sprite;
    const struct SpritePalette *spritePalette = GetGeneralFieldPalette1();

    FieldEffectScript_LoadFadedPal(spritePalette);
    x = gFieldEffectArguments[0];
    y = gFieldEffectArguments[1];
    SetSpritePosToOffsetMapCoords(&x, &y, 8, 8);
    spriteId = CreateSpriteAtEnd(&gFieldEffectObjectTemplate_LongGrass, x, y, 0);
    if (spriteId != MAX_SPRITES)
    {
        sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = ElevationToPriority(gFieldEffectArguments[2]);
        sprite->data[0] = gFieldEffectArguments[2];
        sprite->data[1] = gFieldEffectArguments[0];
        sprite->data[2] = gFieldEffectArguments[1];
        sprite->data[3] = gFieldEffectArguments[4];
        sprite->data[4] = gFieldEffectArguments[5];
        sprite->data[5] = gFieldEffectArguments[6];
        if (gFieldEffectArguments[7])
        {
            SeekSpriteAnim(sprite, 6);
        }
    }
    return 0;
}

void UpdateLongGrassFieldEffect(struct Sprite *sprite)
{
    u8 mapNum;
    u8 mapGroup;
    u8 metatileBehavior;
    u8 localId;
    u8 objectEventId;
    struct ObjectEvent * objectEvent;

    mapNum = sprite->data[5] >> 8;
    mapGroup = sprite->data[5];
    if (gCamera.active && (gSaveBlock1Ptr->location.mapNum != mapNum || gSaveBlock1Ptr->location.mapGroup != mapGroup))
    {
        sprite->data[1] -= gCamera.x;
        sprite->data[2] -= gCamera.y;
        sprite->data[5] = ((u8)gSaveBlock1Ptr->location.mapNum << 8) | (u8)gSaveBlock1Ptr->location.mapGroup;
    }
    localId = sprite->data[3] >> 8;
    mapNum = sprite->data[3];
    mapGroup = sprite->data[4];
    metatileBehavior = MapGridGetMetatileBehaviorAt(sprite->data[1], sprite->data[2]);
    if (TryGetObjectEventIdByLocalIdAndMap(localId, mapNum, mapGroup, &objectEventId) || !MetatileBehavior_IsLongGrass(metatileBehavior) || (sprite->data[7] && sprite->animEnded))
    {
        FieldEffectStop(sprite, FLDEFF_LONG_GRASS);
    }
    else
    {
        objectEvent = &gObjectEvents[objectEventId];
        if ((objectEvent->currentCoords.x != sprite->data[1] || objectEvent->currentCoords.y != sprite->data[2]) && (objectEvent->previousCoords.x != sprite->data[1] || objectEvent->previousCoords.y != sprite->data[2]))
        {
            sprite->data[7] = TRUE;
        }
        UpdateObjectEventSpriteInvisibility(sprite, FALSE);
        UpdateGrassFieldEffectSubpriority(sprite, sprite->data[0], 0);
    }
}

u32 FldEff_JumpLongGrass(void)
{
    u8 spriteId;
    struct Sprite *sprite;
    const struct SpritePalette *spritePalette = GetGeneralFieldPalette1();

    FieldEffectScript_LoadFadedPal(spritePalette);
    SetSpritePosToOffsetMapCoords((s16 *)&gFieldEffectArguments[0], (s16 *)&gFieldEffectArguments[1], 8, 8);
    spriteId = CreateSpriteAtEnd(&gFieldEffectObjectTemplate_JumpLongGrass, gFieldEffectArguments[0], gFieldEffectArguments[1], 0);
    if (spriteId != MAX_SPRITES)
    {
        sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gFieldEffectArguments[3];
        sprite->data[0] = gFieldEffectArguments[2];
        sprite->data[1] = 18;
    }
    return 0;
}

u32 FldEff_ShortGrass(void)
{
    u8 objectEventId;
    struct ObjectEvent * objectEvent;
    u8 spriteId;
    struct Sprite *sprite;
    const struct SpritePalette *spritePalette = GetGeneralFieldPalette1();

    FieldEffectScript_LoadFadedPal(spritePalette);
    objectEventId = GetObjectEventIdByLocalIdAndMap(gFieldEffectArguments[0], gFieldEffectArguments[1], gFieldEffectArguments[2]);
    objectEvent = &gObjectEvents[objectEventId];
    spriteId = CreateSpriteAtEnd(&gFieldEffectObjectTemplate_ShortGrass, 0, 0, 0);
    if (spriteId != MAX_SPRITES)
    {
        sprite = &(gSprites[spriteId]);
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gSprites[objectEvent->spriteId].oam.priority;
        sprite->data[0] = gFieldEffectArguments[0];
        sprite->data[1] = gFieldEffectArguments[1];
        sprite->data[2] = gFieldEffectArguments[2];
        sprite->data[3] = gSprites[objectEvent->spriteId].x;
        sprite->data[4] = gSprites[objectEvent->spriteId].y;
    }
    return 0;
}

void UpdateShortGrassFieldEffect(struct Sprite *sprite)
{
    u8 objectEventId;
    s16 x;
    s16 y;
    const struct ObjectEventGraphicsInfo * graphicsInfo;
    struct Sprite *linkedSprite;

    if (TryGetObjectEventIdByLocalIdAndMap(sprite->data[0], sprite->data[1], sprite->data[2], &objectEventId) || !gObjectEvents[objectEventId].inShortGrass)
    {
        FieldEffectStop(sprite, FLDEFF_SHORT_GRASS);
    }
    else
    {
        graphicsInfo = GetObjectEventGraphicsInfo(gObjectEvents[objectEventId].graphicsId);
        linkedSprite = &gSprites[gObjectEvents[objectEventId].spriteId];
        y = linkedSprite->y;
        x = linkedSprite->x;
        if (x != sprite->data[3] || y != sprite->data[4])
        {
            sprite->data[3] = x;
            sprite->data[4] = y;
            if (sprite->animEnded)
            {
                StartSpriteAnim(sprite, 0);
            }
        }
        sprite->x = x;
        sprite->y = y;
        sprite->y2 = (graphicsInfo->height >> 1) - 8;
        sprite->subpriority = linkedSprite->subpriority - 1;
        sprite->oam.priority = linkedSprite->oam.priority;
        UpdateObjectEventSpriteInvisibility(sprite, linkedSprite->invisible);
    }
}

u32 FldEff_SandFootprints(void)
{
    u8 spriteId;
    struct Sprite *sprite;

    FieldEffectScript_LoadFadedPal(&gSpritePalette_GeneralFieldEffect0);
    SetSpritePosToOffsetMapCoords((s16 *)&gFieldEffectArguments[0], (s16 *)&gFieldEffectArguments[1], 8, 8);
    spriteId = CreateSpriteAtEnd(&gFieldEffectObjectTemplate_SandFootprints, gFieldEffectArguments[0], gFieldEffectArguments[1], gFieldEffectArguments[2]);
    if (spriteId != MAX_SPRITES)
    {
        sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gFieldEffectArguments[3];
        sprite->data[7] = FLDEFF_SAND_FOOTPRINTS;
        StartSpriteAnim(sprite, gFieldEffectArguments[4]);
    }
    return 0;
}

u32 FldEff_SnowFootprints(void)
{
    u8 spriteId;
    struct Sprite *sprite;

    FieldEffectScript_LoadFadedPal(&gSpritePalette_GeneralFieldEffect2);
    SetSpritePosToOffsetMapCoords((s16 *)&gFieldEffectArguments[0], (s16 *)&gFieldEffectArguments[1], 8, 8);
    spriteId = CreateSpriteAtEnd(&gFieldEffectObjectTemplate_SnowFootprints, gFieldEffectArguments[0], gFieldEffectArguments[1], gFieldEffectArguments[2]);
    if (spriteId != MAX_SPRITES)
    {
        sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gFieldEffectArguments[3];
        sprite->data[7] = FLDEFF_SNOW_FOOTPRINTS;
        StartSpriteAnim(sprite, gFieldEffectArguments[4]);
    }
    return 0;
}

u32 FldEff_DeepSandFootprints(void)
{
    u8 spriteId;
    struct Sprite *sprite;

    FieldEffectScript_LoadFadedPal(&gSpritePalette_GeneralFieldEffect0);
    SetSpritePosToOffsetMapCoords((s16 *)&gFieldEffectArguments[0], (s16 *)&gFieldEffectArguments[1], 8, 8);
    spriteId = CreateSpriteAtEnd(&gFieldEffectObjectTemplate_DeepSandFootprints, gFieldEffectArguments[0], gFieldEffectArguments[1], gFieldEffectArguments[2]);
    if (spriteId != MAX_SPRITES)
    {
        sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gFieldEffectArguments[3];
        sprite->data[7] = FLDEFF_DEEP_SAND_FOOTPRINTS;
        StartSpriteAnim(sprite, gFieldEffectArguments[4]);
    }
    return spriteId;
}

u32 FldEff_TracksBug(void)
{
    u8 spriteId;
    struct Sprite *sprite;

    FieldEffectScript_LoadFadedPal(&gSpritePalette_GeneralFieldEffect0);
    SetSpritePosToOffsetMapCoords((s16 *)&gFieldEffectArguments[0], (s16 *)&gFieldEffectArguments[1], 8, 8);
    spriteId = CreateSpriteAtEnd(&gFieldEffectObjectTemplate_BugTracks, gFieldEffectArguments[0], gFieldEffectArguments[1], gFieldEffectArguments[2]);
    if (spriteId != MAX_SPRITES)
    {
        sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gFieldEffectArguments[3];
        sprite->data[7] = FLDEFF_TRACKS_BUG;
        StartSpriteAnim(sprite, gFieldEffectArguments[4]);
    }
    return 0;
}

u32 FldEff_SnowTracksBug(void)
{
    u8 spriteId;
    struct Sprite *sprite;

    FieldEffectScript_LoadFadedPal(&gSpritePalette_GeneralFieldEffect2);
    SetSpritePosToOffsetMapCoords((s16 *)&gFieldEffectArguments[0], (s16 *)&gFieldEffectArguments[1], 8, 8);
    spriteId = CreateSpriteAtEnd(&gFieldEffectObjectTemplate_SnowBugTracks, gFieldEffectArguments[0], gFieldEffectArguments[1], gFieldEffectArguments[2]);
    if (spriteId != MAX_SPRITES)
    {
        sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gFieldEffectArguments[3];
        sprite->data[7] = FLDEFF_SNOW_TRACKS_BUG;
        StartSpriteAnim(sprite, gFieldEffectArguments[4]);
    }
    return 0;
}

u32 FldEff_TracksSpot(void)
{
    u8 spriteId;
    struct Sprite *sprite;

    FieldEffectScript_LoadFadedPal(&gSpritePalette_GeneralFieldEffect0);
    SetSpritePosToOffsetMapCoords((s16 *)&gFieldEffectArguments[0], (s16 *)&gFieldEffectArguments[1], 8, 8);
    spriteId = CreateSpriteAtEnd(&gFieldEffectObjectTemplate_SpotTracks, gFieldEffectArguments[0], gFieldEffectArguments[1], gFieldEffectArguments[2]);
    if (spriteId != MAX_SPRITES)
    {
        sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gFieldEffectArguments[3];
        sprite->data[7] = FLDEFF_TRACKS_SPOT;
        StartSpriteAnim(sprite, gFieldEffectArguments[4]);
    }
    return 0;
}

u32 FldEff_SnowTracksSpot(void)
{
    u8 spriteId;
    struct Sprite *sprite;

    FieldEffectScript_LoadFadedPal(&gSpritePalette_GeneralFieldEffect2);
    SetSpritePosToOffsetMapCoords((s16 *)&gFieldEffectArguments[0], (s16 *)&gFieldEffectArguments[1], 8, 8);
    spriteId = CreateSpriteAtEnd(&gFieldEffectObjectTemplate_SnowSpotTracks, gFieldEffectArguments[0], gFieldEffectArguments[1], gFieldEffectArguments[2]);
    if (spriteId != MAX_SPRITES)
    {
        sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gFieldEffectArguments[3];
        sprite->data[7] = FLDEFF_SNOW_TRACKS_SPOT;
        StartSpriteAnim(sprite, gFieldEffectArguments[4]);
    }
    return 0;
}

u32 FldEff_BikeTireTracks(void)
{
    u8 spriteId;
    struct Sprite *sprite;

    FieldEffectScript_LoadFadedPal(&gSpritePalette_GeneralFieldEffect0);
    SetSpritePosToOffsetMapCoords((s16 *)&gFieldEffectArguments[0], (s16 *)&gFieldEffectArguments[1], 8, 8);
    spriteId = CreateSpriteAtEnd(&gFieldEffectObjectTemplate_BikeTireTracks, gFieldEffectArguments[0], gFieldEffectArguments[1], gFieldEffectArguments[2]);
    if (spriteId != MAX_SPRITES)
    {
        sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gFieldEffectArguments[3];
        sprite->data[7] = FLDEFF_BIKE_TIRE_TRACKS;
        StartSpriteAnim(sprite, gFieldEffectArguments[4]);
    }
    return spriteId;
}

u32 FldEff_SnowBikeTireTracks(void)
{
    u8 spriteId;
    struct Sprite *sprite;

    FieldEffectScript_LoadFadedPal(&gSpritePalette_GeneralFieldEffect2);
    SetSpritePosToOffsetMapCoords((s16 *)&gFieldEffectArguments[0], (s16 *)&gFieldEffectArguments[1], 8, 8);
    spriteId = CreateSpriteAtEnd(&gFieldEffectObjectTemplate_SnowBikeTireTracks, gFieldEffectArguments[0], gFieldEffectArguments[1], gFieldEffectArguments[2]);
    if (spriteId != MAX_SPRITES)
    {
        sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gFieldEffectArguments[3];
        sprite->data[7] = FLDEFF_SNOW_BIKE_TIRE_TRACKS;
        StartSpriteAnim(sprite, gFieldEffectArguments[4]);
    }
    return spriteId;
}

u32 FldEff_TracksSlither(void)
{
    u8 spriteId;
    struct Sprite *sprite;

    FieldEffectScript_LoadFadedPal(&gSpritePalette_GeneralFieldEffect0);
    SetSpritePosToOffsetMapCoords((s16 *)&gFieldEffectArguments[0], (s16 *)&gFieldEffectArguments[1], 8, 8);
    spriteId = CreateSpriteAtEnd(&gFieldEffectObjectTemplate_SlitherTracks, gFieldEffectArguments[0], gFieldEffectArguments[1], gFieldEffectArguments[2]);
    if (spriteId != MAX_SPRITES)
    {
        sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gFieldEffectArguments[3];
        sprite->data[7] = FLDEFF_TRACKS_SLITHER;
        StartSpriteAnim(sprite, gFieldEffectArguments[4]);
    }
    return spriteId;
}

u32 FldEff_SnowTracksSlither(void)
{
    u8 spriteId;
    struct Sprite *sprite;

    FieldEffectScript_LoadFadedPal(&gSpritePalette_GeneralFieldEffect2);
    SetSpritePosToOffsetMapCoords((s16 *)&gFieldEffectArguments[0], (s16 *)&gFieldEffectArguments[1], 8, 8);
    spriteId = CreateSpriteAtEnd(&gFieldEffectObjectTemplate_SnowSlitherTracks, gFieldEffectArguments[0], gFieldEffectArguments[1], gFieldEffectArguments[2]);
    if (spriteId != MAX_SPRITES)
    {
        sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gFieldEffectArguments[3];
        sprite->data[7] = FLDEFF_SNOW_TRACKS_SLITHER;
        StartSpriteAnim(sprite, gFieldEffectArguments[4]);
    }
    return spriteId;
}

void (*const gFadeFootprintsTireTracksFuncs[])(struct Sprite *sprite) = {
    FadeFootprintsTireTracks_Step0,
    FadeFootprintsTireTracks_Step1
};

void UpdateFootprintsTireTracksFieldEffect(struct Sprite *sprite)
{
    gFadeFootprintsTireTracksFuncs[sprite->data[0]](sprite);
}

static void FadeFootprintsTireTracks_Step0(struct Sprite *sprite)
{
    // Wait 40 frames before the flickering starts.
    if (++sprite->data[1] > 40)
        sprite->data[0] = 1;

    UpdateObjectEventSpriteInvisibility(sprite, FALSE);
}

static void FadeFootprintsTireTracks_Step1(struct Sprite *sprite)
{
    sprite->invisible ^= 1;
    sprite->data[1]++;
    UpdateObjectEventSpriteInvisibility(sprite, sprite->invisible);
    if (sprite->data[1] > 56)
    {
        FieldEffectStop(sprite, sprite->data[7]);
    }
}

u32 FldEff_Splash(void)
{
    u8 objectEventId;
    struct ObjectEvent * objectEvent;
    u8 spriteId;
    struct Sprite *sprite;
    const struct ObjectEventGraphicsInfo * graphicsInfo;
    struct Sprite *linkedSprite;

    FieldEffectScript_LoadFadedPal(&gSpritePalette_GeneralFieldEffect0);
    objectEventId = GetObjectEventIdByLocalIdAndMap(gFieldEffectArguments[0], gFieldEffectArguments[1], gFieldEffectArguments[2]);
    objectEvent = &gObjectEvents[objectEventId];
    spriteId = CreateSpriteAtEnd(&gFieldEffectObjectTemplate_Splash, 0, 0, 0);
    if (spriteId != MAX_SPRITES)
    {
        graphicsInfo = GetObjectEventGraphicsInfo(objectEvent->graphicsId);
        sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        linkedSprite = &gSprites[objectEvent->spriteId];
        sprite->oam.priority = linkedSprite->oam.priority;
        sprite->data[0] = gFieldEffectArguments[0];
        sprite->data[1] = gFieldEffectArguments[1];
        sprite->data[2] = gFieldEffectArguments[2];
        sprite->y2 = (graphicsInfo->height >> 1) - 4;
        PlaySE(SE_PUDDLE);
    }
    return 0;
}

void UpdateSplashFieldEffect(struct Sprite *sprite)
{
    u8 objectEventId;

    if (sprite->animEnded || TryGetObjectEventIdByLocalIdAndMap(sprite->data[0], sprite->data[1], sprite->data[2], &objectEventId))
    {
        FieldEffectStop(sprite, FLDEFF_SPLASH);
    }
    else
    {
        sprite->x = gSprites[gObjectEvents[objectEventId].spriteId].x;
        sprite->y = gSprites[gObjectEvents[objectEventId].spriteId].y;
        UpdateObjectEventSpriteInvisibility(sprite, FALSE);
    }
}

u32 FldEff_JumpSmallSplash(void)
{
    u8 spriteId;
    struct Sprite *sprite;
    
    FieldEffectScript_LoadFadedPal(&gSpritePalette_GeneralFieldEffect0);
    SetSpritePosToOffsetMapCoords((s16 *)&gFieldEffectArguments[0], (s16 *)&gFieldEffectArguments[1], 8, 12);
    spriteId = CreateSpriteAtEnd(&gFieldEffectObjectTemplate_JumpSmallSplash, gFieldEffectArguments[0], gFieldEffectArguments[1], 0);
    if (spriteId != MAX_SPRITES)
    {
        sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gFieldEffectArguments[3];
        sprite->data[0] = gFieldEffectArguments[2];
        sprite->data[1] = FLDEFF_JUMP_SMALL_SPLASH;
    }
    return 0;
}

u32 FldEff_JumpBigSplash(void)
{
    u8 spriteId;
    struct Sprite *sprite;

    FieldEffectScript_LoadFadedPal(&gSpritePalette_GeneralFieldEffect0);
    SetSpritePosToOffsetMapCoords((s16 *)&gFieldEffectArguments[0], (s16 *)&gFieldEffectArguments[1], 8, 8);
    spriteId = CreateSpriteAtEnd(&gFieldEffectObjectTemplate_JumpBigSplash, gFieldEffectArguments[0], gFieldEffectArguments[1], 0);
    if (spriteId != MAX_SPRITES)
    {
        sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gFieldEffectArguments[3];
        sprite->data[0] = gFieldEffectArguments[2];
        sprite->data[1] = FLDEFF_JUMP_BIG_SPLASH;
    }
    return 0;
}

u32 FldEff_FeetInFlowingWater(void)
{
    u8 objectEventId;
    struct ObjectEvent * objectEvent;
    u8 spriteId;
    struct Sprite *sprite;
    const struct ObjectEventGraphicsInfo * graphicsInfo;

    FieldEffectScript_LoadFadedPal(&gSpritePalette_GeneralFieldEffect0);
    objectEventId = GetObjectEventIdByLocalIdAndMap(gFieldEffectArguments[0], gFieldEffectArguments[1], gFieldEffectArguments[2]);
    objectEvent = &gObjectEvents[objectEventId];
    spriteId = CreateSpriteAtEnd(&gFieldEffectObjectTemplate_Splash, 0, 0, 0);
    if (spriteId != MAX_SPRITES)
    {
        graphicsInfo = GetObjectEventGraphicsInfo(objectEvent->graphicsId);
        sprite = &gSprites[spriteId];
        sprite->callback = UpdateFeetInFlowingWaterFieldEffect;
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gSprites[objectEvent->spriteId].oam.priority;
        sprite->data[0] = gFieldEffectArguments[0];
        sprite->data[1] = gFieldEffectArguments[1];
        sprite->data[2] = gFieldEffectArguments[2];
        sprite->data[3] = -1;
        sprite->data[4] = -1;
        sprite->y2 = (graphicsInfo->height >> 1) - 4;
        StartSpriteAnim(sprite, 1);
    }
    return 0;
}

static void UpdateFeetInFlowingWaterFieldEffect(struct Sprite *sprite)
{
    u8 objectEventId;
    struct Sprite *linkedSprite;
    struct ObjectEvent * objectEvent;

    if (TryGetObjectEventIdByLocalIdAndMap(sprite->data[0], sprite->data[1], sprite->data[2], &objectEventId) || !gObjectEvents[objectEventId].inShallowFlowingWater)
    {
        FieldEffectStop(sprite, FLDEFF_FEET_IN_FLOWING_WATER);
    }
    else
    {
        objectEvent = &gObjectEvents[objectEventId];
        linkedSprite = &gSprites[objectEvent->spriteId];
        sprite->x = linkedSprite->x;
        sprite->y = linkedSprite->y;
        sprite->subpriority = linkedSprite->subpriority;
        UpdateObjectEventSpriteInvisibility(sprite, FALSE);
        if (objectEvent->currentCoords.x != sprite->data[3] || objectEvent->currentCoords.y != sprite->data[4])
        {
            sprite->data[3] = objectEvent->currentCoords.x;
            sprite->data[4] = objectEvent->currentCoords.y;
            if (!sprite->invisible)
            {
                PlaySE(SE_PUDDLE);
            }
        }
    }
}

u32 FldEff_Ripple(void)
{
    u8 spriteId;
    struct Sprite *sprite;
    const struct SpritePalette *spritePalette = GetGeneralFieldPalette1();

    FieldEffectScript_LoadFadedPal(spritePalette);
    spriteId = CreateSpriteAtEnd(&gFieldEffectObjectTemplate_Ripple, gFieldEffectArguments[0], gFieldEffectArguments[1], gFieldEffectArguments[2]);
    if (spriteId != MAX_SPRITES)
    {
        sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gFieldEffectArguments[3];
        sprite->data[0] = FLDEFF_RIPPLE;
    }
    return 0;
}

u32 FldEff_HotSpringsWater(void)
{
    u8 objectEventId;
    struct ObjectEvent * objectEvent;
    u8 spriteId;
    struct Sprite *sprite;
    const struct SpritePalette *spritePalette = GetGeneralFieldPalette1();

    FieldEffectScript_LoadFadedPal(spritePalette);
    objectEventId = GetObjectEventIdByLocalIdAndMap(gFieldEffectArguments[0], gFieldEffectArguments[1], gFieldEffectArguments[2]);
    objectEvent = &gObjectEvents[objectEventId];
    spriteId = CreateSpriteAtEnd(&gFieldEffectObjectTemplate_HotSpringsWater, 0, 0, 0);
    if (spriteId != MAX_SPRITES)
    {
        sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gSprites[objectEvent->spriteId].oam.priority;
        sprite->data[0] = gFieldEffectArguments[0];
        sprite->data[1] = gFieldEffectArguments[1];
        sprite->data[2] = gFieldEffectArguments[2];
        sprite->data[3] = gSprites[objectEvent->spriteId].x;
        sprite->data[4] = gSprites[objectEvent->spriteId].y;
    }
    return 0;
}

void UpdateHotSpringsWaterFieldEffect(struct Sprite *sprite)
{
    u8 objectEventId;
    const struct ObjectEventGraphicsInfo * graphicsInfo;
    struct Sprite *linkedSprite;

    if (TryGetObjectEventIdByLocalIdAndMap(sprite->data[0], sprite->data[1], sprite->data[2], &objectEventId) || !gObjectEvents[objectEventId].inHotSprings)
    {
        FieldEffectStop(sprite, FLDEFF_HOT_SPRINGS_WATER);
    }
    else
    {
        graphicsInfo = GetObjectEventGraphicsInfo(gObjectEvents[objectEventId].graphicsId);
        linkedSprite = &gSprites[gObjectEvents[objectEventId].spriteId];
        sprite->x = linkedSprite->x;
        sprite->y = (graphicsInfo->height >> 1) + linkedSprite->y - 8;
        sprite->subpriority = linkedSprite->subpriority - 1;
        UpdateObjectEventSpriteInvisibility(sprite, FALSE);
    }
}

u32 FldEff_ShakingGrass(void)
{
    u8 spriteId;
    struct Sprite *sprite;
    const struct SpritePalette *spritePalette = GetGeneralFieldPalette1();

    FieldEffectScript_LoadFadedPal(spritePalette);
    SetSpritePosToOffsetMapCoords((s16 *)&gFieldEffectArguments[0], (s16 *)&gFieldEffectArguments[1], 8, 8);
    spriteId = CreateSpriteAtEnd(&gFieldEffectObjectTemplate_UnusedGrass, gFieldEffectArguments[0], gFieldEffectArguments[1], gFieldEffectArguments[2]);
    if (spriteId != MAX_SPRITES)
    {
        sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gFieldEffectArguments[3];
        sprite->data[0] = FLDEFF_SHAKING_GRASS;
    }
    return spriteId;
}

u32 FldEff_ShakingGrass2(void)
{
    u8 spriteId;
    struct Sprite *sprite;
    const struct SpritePalette *spritePalette = GetGeneralFieldPalette1();

    FieldEffectScript_LoadFadedPal(spritePalette);
    SetSpritePosToOffsetMapCoords((s16 *)&gFieldEffectArguments[0], (s16 *)&gFieldEffectArguments[1], 8, 8);
    spriteId = CreateSpriteAtEnd(&gFieldEffectObjectTemplate_UnusedGrass2, gFieldEffectArguments[0], gFieldEffectArguments[1], gFieldEffectArguments[2]);
    if (spriteId != MAX_SPRITES)
    {
        sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gFieldEffectArguments[3];
        sprite->data[0] = FLDEFF_SHAKING_LONG_GRASS;
    }
    return spriteId;
}

u32 FldEff_UnusedSand(void)
{
    u8 spriteId;
    struct Sprite *sprite;

    FieldEffectScript_LoadFadedPal(&gSpritePalette_GeneralFieldEffect0);
    SetSpritePosToOffsetMapCoords((s16 *)&gFieldEffectArguments[0], (s16 *)&gFieldEffectArguments[1], 8, 8);
    spriteId = CreateSpriteAtEnd(&gFieldEffectObjectTemplate_UnusedSand, gFieldEffectArguments[0], gFieldEffectArguments[1], gFieldEffectArguments[2]);
    if (spriteId != MAX_SPRITES)
    {
        sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gFieldEffectArguments[3];
        sprite->data[0] = FLDEFF_SAND_HOLE;
    }
    return spriteId;
}

u32 FldEff_UnusedWaterSurfacing(void)
{
    u8 spriteId;
    struct Sprite *sprite;

    FieldEffectScript_LoadFadedPal(&gSpritePalette_GeneralFieldEffect0);
    SetSpritePosToOffsetMapCoords((s16 *)&gFieldEffectArguments[0], (s16 *)&gFieldEffectArguments[1], 8, 8);
    spriteId = CreateSpriteAtEnd(&gFieldEffectObjectTemplate_WaterSurfacing, gFieldEffectArguments[0], gFieldEffectArguments[1], gFieldEffectArguments[2]);
    if (spriteId != MAX_SPRITES)
    {
        sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gFieldEffectArguments[3];
        sprite->data[0] = FLDEFF_UNUSED_WATER_SURFACING;
    }
    return spriteId;
}

void StartAshFieldEffect(s16 x, s16 y, u16 metatileId, s16 d)
{
    gFieldEffectArguments[0] = x;
    gFieldEffectArguments[1] = y;
    gFieldEffectArguments[2] = 0x52;
    gFieldEffectArguments[3] = 1;
    gFieldEffectArguments[4] = metatileId;
    gFieldEffectArguments[5] = d;
    FieldEffectStart(FLDEFF_ASH);
}

u32 FldEff_Ash(void)
{
    s16 x;
    s16 y;
    u8 spriteId;
    struct Sprite *sprite;
    const struct SpritePalette *spritePalette = GetGeneralFieldPalette1();
	
    FieldEffectScript_LoadFadedPal(spritePalette);

    x = gFieldEffectArguments[0];
    y = gFieldEffectArguments[1];
    SetSpritePosToOffsetMapCoords(&x, &y, 8, 8);
    spriteId = CreateSpriteAtEnd(&gFieldEffectObjectTemplate_Ash, x, y, gFieldEffectArguments[2]);
    if (spriteId != MAX_SPRITES)
    {
        sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gFieldEffectArguments[3];
        sprite->data[1] = gFieldEffectArguments[0];
        sprite->data[2] = gFieldEffectArguments[1];
        sprite->data[3] = gFieldEffectArguments[4];
        sprite->data[4] = gFieldEffectArguments[5];
    }
    return 0;
}

void (*const gAshFieldEffectFuncs[])(struct Sprite *sprite) = {
    UpdateAshFieldEffect_Step0,
    UpdateAshFieldEffect_Step1,
    UpdateAshFieldEffect_Step2
};

void UpdateAshFieldEffect(struct Sprite *sprite)
{
    gAshFieldEffectFuncs[sprite->data[0]](sprite);
}

static void UpdateAshFieldEffect_Step0(struct Sprite *sprite)
{
    sprite->invisible = TRUE;
    sprite->animPaused = TRUE;
    if (--sprite->data[4] == 0)
        sprite->data[0] = 1;
}

static void UpdateAshFieldEffect_Step1(struct Sprite *sprite)
{
    sprite->invisible = FALSE;
    sprite->animPaused = FALSE;
    MapGridSetMetatileIdAt(sprite->data[1], sprite->data[2], sprite->data[3]);
    CurrentMapDrawMetatileAt(sprite->data[1], sprite->data[2]);
    gObjectEvents[gPlayerAvatar.objectEventId].triggerGroundEffectsOnMove = TRUE;
    sprite->data[0] = 2;
}

static void UpdateAshFieldEffect_Step2(struct Sprite *sprite)
{
    UpdateObjectEventSpriteInvisibility(sprite, FALSE);
    if (sprite->animEnded)
        FieldEffectStop(sprite, FLDEFF_ASH);
}

// Sprite data for surf blob
#define sBitfield       data[0]
#define sPlayerOffset   data[1]
#define sPlayerObjectId data[2]
#define sBobDirection   data[3]
#define sTimer          data[4]

u32 FldEff_SurfBlob(void)
{
    u8 spriteId;
    struct Sprite *sprite;

    SetSpritePosToOffsetMapCoords((s16 *)&gFieldEffectArguments[0], (s16 *)&gFieldEffectArguments[1], 8, 8);
    spriteId = CreateSpriteAtEnd(&gFieldEffectObjectTemplate_SurfBlob, gFieldEffectArguments[0], gFieldEffectArguments[1], 0x96);
    if (spriteId != MAX_SPRITES)
    {
        sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.paletteNum = LoadPlayerObjectEventPalette(gSaveBlock2Ptr->playerGender);
        sprite->sPlayerObjectId = gFieldEffectArguments[2];
        sprite->sBobDirection = 0;
        sprite->data[6] = -1;
        sprite->data[7] = -1;
    }
    FieldEffectActiveListRemove(FLDEFF_SURF_BLOB);
    return spriteId;
}

void SetSurfBlob_BobState(u8 spriteId, u8 bobState)
{
    gSprites[spriteId].sBitfield = (gSprites[spriteId].sBitfield & ~0xF) | (bobState & 0xF);
}

void SetSurfBlob_DontSyncAnim(u8 spriteId, bool8 value)
{
    gSprites[spriteId].sBitfield = (gSprites[spriteId].sBitfield & ~0xF0) | ((value & 0xF) << 4);
}

void SetSurfBlob_PlayerOffset(u8 spriteId, bool8 hasOffset, s16 offset)
{
    gSprites[spriteId].sBitfield = (gSprites[spriteId].sBitfield & ~0xF00) | ((hasOffset & 0xF) << 8);
    gSprites[spriteId].sPlayerOffset = offset;
}

static u8 GetSurfBlob_BobState(struct Sprite *sprite)
{
    return sprite->sBitfield & 0xF;
}

static bool8 GetSurfBlob_DontSyncAnim(struct Sprite *sprite)
{
    return (sprite->sBitfield & 0xF0) >> 4;
}

static bool8 GetSurfBlob_HasPlayerOffset(struct Sprite *sprite)
{
    return (sprite->sBitfield & 0xF00) >> 8;
}

void UpdateSurfBlobFieldEffect(struct Sprite *sprite)
{
    struct ObjectEvent *playerObject;
    struct Sprite *playerSprite;

    playerObject = &gObjectEvents[sprite->sPlayerObjectId];
    playerSprite = &gSprites[playerObject->spriteId];
    SynchroniseSurfAnim(playerObject, sprite);
    SynchroniseSurfPosition(playerObject, sprite);
    CreateBobbingEffect(playerObject, playerSprite, sprite);
    sprite->oam.priority = playerSprite->oam.priority;
}

static void SynchroniseSurfAnim(struct ObjectEvent *objectEvent, struct Sprite *sprite)
{
    u8 surfBlobDirectionAnims[] = {
        [DIR_NONE] = 0,
        [DIR_SOUTH] = 0,
        [DIR_NORTH] = 1,
        [DIR_WEST] = 2,
        [DIR_EAST] = 3
    };

    if (GetSurfBlob_DontSyncAnim(sprite) == FALSE)
        StartSpriteAnimIfDifferent(sprite, surfBlobDirectionAnims[objectEvent->movementDirection]);
}

void SynchroniseSurfPosition(struct ObjectEvent *playerObject, struct Sprite *surfBlobSprite)
{
    u8 i;
    s16 x = playerObject->currentCoords.x;
    s16 y = playerObject->currentCoords.y;
    s32 yOffset = surfBlobSprite->y2;

    if (yOffset == 0 && (x != surfBlobSprite->data[6] || y != surfBlobSprite->data[7]))
    {
        surfBlobSprite->data[5] = yOffset;
        surfBlobSprite->data[6] = x;
        surfBlobSprite->data[7] = y;
        for (i = DIR_SOUTH; i <= DIR_EAST; i++, x = surfBlobSprite->data[6], y = surfBlobSprite->data[7])
        {
            MoveCoords(i, &x, &y);
            if (MapGridGetElevationAt(x, y) == 3)
            {
                surfBlobSprite->data[5]++;
                break;
            }
        }
    }
}

static void CreateBobbingEffect(struct ObjectEvent *objectEvent, struct Sprite *playerSprite, struct Sprite *surfBlobSprite)
{
    u16 intervals[] = {7, 15};
    u8 bobState = GetSurfBlob_BobState(surfBlobSprite);
    if (bobState != BOB_NONE)
    {
        // the surf blob sprite never bobs since sBobDirection will always be 0

        if (((u16)(++surfBlobSprite->sTimer) & intervals[surfBlobSprite->data[5]]) == 0)
            surfBlobSprite->y2 += surfBlobSprite->sBobDirection;

        if ((surfBlobSprite->sTimer & 0x1F) == 0)
            surfBlobSprite->sBobDirection = -surfBlobSprite->sBobDirection;

        if (bobState != BOB_MON_ONLY)
        {
            if (GetSurfBlob_HasPlayerOffset(surfBlobSprite) == FALSE)
                playerSprite->y2 = surfBlobSprite->y2;
            else
                playerSprite->y2 = surfBlobSprite->sPlayerOffset + surfBlobSprite->y2;

            if (surfBlobSprite->animCmdIndex != 0)
                playerSprite->y2++;

            surfBlobSprite->x = playerSprite->x;
            surfBlobSprite->y = playerSprite->y + 8;
        }
    }
}

#undef sBitfield
#undef sPlayerOffset
#undef sPlayerObjectId
#undef sBobDirection
#undef sTimer

u8 StartUnderwaterSurfBlobBobbing(u8 oldSpriteId)
{
    u8 spriteId;
    struct Sprite *sprite;

    spriteId = CreateSpriteAtEnd(&gDummySpriteTemplate, 0, 0, -1);
    sprite = &gSprites[spriteId];
    sprite->callback = SpriteCB_UnderwaterSurfBlob;
    sprite->invisible = TRUE;
    sprite->data[0] = oldSpriteId;
    sprite->data[1] = 1;
    return spriteId;
}

static void SpriteCB_UnderwaterSurfBlob(struct Sprite *sprite)
{
    struct Sprite *oldSprite;

    oldSprite = &gSprites[sprite->data[0]];
    if (((sprite->data[2]++) & 0x03) == 0)
    {
        oldSprite->y2 += sprite->data[1];
    }
    if ((sprite->data[2] & 0x0F) == 0)
    {
        sprite->data[1] = -sprite->data[1];
    }
}

u32 FldEff_Dust(void)
{
    u8 spriteId;
    struct Sprite *sprite;

    FieldEffectScript_LoadFadedPal(&gSpritePalette_GeneralFieldEffect0);

    SetSpritePosToOffsetMapCoords((s16 *)&gFieldEffectArguments[0], (s16 *)&gFieldEffectArguments[1], 8, 12);
    spriteId = CreateSpriteAtEnd(&gFieldEffectObjectTemplate_GroundImpactDust, gFieldEffectArguments[0], gFieldEffectArguments[1], 0);
    if (spriteId != MAX_SPRITES)
    {
        sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gFieldEffectArguments[3];
        sprite->data[0] = gFieldEffectArguments[2];
        sprite->data[1] = 10;
    }
    return 0;
}

u32 FldEff_SandPile(void)
{
    u8 objectEventId;
    struct ObjectEvent * objectEvent;
    u8 spriteId;
    struct Sprite *sprite;
    const struct ObjectEventGraphicsInfo * graphicsInfo;

    FieldEffectScript_LoadFadedPal(&gSpritePalette_GeneralFieldEffect0);
    objectEventId = GetObjectEventIdByLocalIdAndMap(gFieldEffectArguments[0], gFieldEffectArguments[1], gFieldEffectArguments[2]);
    objectEvent = &gObjectEvents[objectEventId];
    spriteId = CreateSpriteAtEnd(&gFieldEffectObjectTemplate_SandPile, 0, 0, 0);
    if (spriteId != MAX_SPRITES)
    {
        graphicsInfo = GetObjectEventGraphicsInfo(objectEvent->graphicsId);
        sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gSprites[objectEvent->spriteId].oam.priority;
        sprite->data[0] = gFieldEffectArguments[0];
        sprite->data[1] = gFieldEffectArguments[1];
        sprite->data[2] = gFieldEffectArguments[2];
        sprite->data[3] = gSprites[objectEvent->spriteId].x;
        sprite->data[4] = gSprites[objectEvent->spriteId].y;
        sprite->y2 = (graphicsInfo->height >> 1) - 2;
        SeekSpriteAnim(sprite, 2);
    }
    return 0;
}

void UpdateSandPileFieldEffect(struct Sprite *sprite)
{
    u8 objectEventId;
    s16 x;
    s16 y;

    if (TryGetObjectEventIdByLocalIdAndMap(sprite->data[0], sprite->data[1], sprite->data[2], &objectEventId) || !gObjectEvents[objectEventId].inSandPile)
    {
        FieldEffectStop(sprite, FLDEFF_SAND_PILE);
    }
    else
    {
        y = gSprites[gObjectEvents[objectEventId].spriteId].y;
        x = gSprites[gObjectEvents[objectEventId].spriteId].x;
        if (x != sprite->data[3] || y != sprite->data[4])
        {
            sprite->data[3] = x;
            sprite->data[4] = y;
            if (sprite->animEnded)
            {
                StartSpriteAnim(sprite, 0);
            }
        }
        sprite->x = x;
        sprite->y = y;
        sprite->subpriority = gSprites[gObjectEvents[objectEventId].spriteId].subpriority;
        UpdateObjectEventSpriteInvisibility(sprite, FALSE);
    }
}

u32 FldEff_Bubbles(void)
{
    u8 spriteId;
    struct Sprite *sprite;

    FieldEffectScript_LoadFadedPal(&gSpritePalette_GeneralFieldEffect0);
    SetSpritePosToOffsetMapCoords((s16 *)&gFieldEffectArguments[0], (s16 *)&gFieldEffectArguments[1], 8, 0);
    spriteId = CreateSpriteAtEnd(&gFieldEffectObjectTemplate_Bubbles, gFieldEffectArguments[0], gFieldEffectArguments[1], 0x52);
    if (spriteId != MAX_SPRITES)
    {
        sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = 1;
    }
    return 0;
}

void UpdateBubblesFieldEffect(struct Sprite *sprite)
{
    sprite->data[0] += 0x80;
    sprite->data[0] &= 0x100;
    sprite->y -= sprite->data[0] >> 8;
    UpdateObjectEventSpriteInvisibility(sprite, FALSE);
    if (sprite->invisible || sprite->animEnded)
    {
        FieldEffectStop(sprite, FLDEFF_BUBBLES);
    }
}

u32 FldEff_BerryTreeGrowthSparkle(void)
{
    u8 spriteId;

    SetSpritePosToOffsetMapCoords((s16 *)&gFieldEffectArguments[0], (s16 *)&gFieldEffectArguments[1], 8, 4);
    spriteId = CreateSpriteAtEnd(&gFieldEffectObjectTemplate_Sparkle, gFieldEffectArguments[0], gFieldEffectArguments[1], gFieldEffectArguments[2]);
    if (spriteId != MAX_SPRITES)
    {
        struct Sprite *sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gFieldEffectArguments[3];
        UpdateSpritePaletteByTemplate(&gFieldEffectObjectTemplate_Sparkle, sprite);
        sprite->sWaitFldEff = FLDEFF_BERRY_TREE_GROWTH_SPARKLE;
    }
    return spriteId;
}

u32 FldEff_TreeDisguise(void)
{
    return ShowDisguiseFieldEffect(FLDEFF_TREE_DISGUISE, &gFieldEffectObjectTemplate_TreeDisguise, 4);
}

u32 FldEff_MountainDisguise(void)
{
    return ShowDisguiseFieldEffect(FLDEFF_MOUNTAIN_DISGUISE, &gFieldEffectObjectTemplate_MountainDisguise, 3);
}

u32 FldEff_SandDisguise(void)
{
    return ShowDisguiseFieldEffect(FLDEFF_SAND_DISGUISE, &gFieldEffectObjectTemplate_SandDisguisePlaceholder, 2);
}

static u32 ShowDisguiseFieldEffect(u32 fldEff, const struct SpriteTemplate* template, u32 paletteNum)
{
    u8 spriteId;
    struct Sprite *sprite;

    if (TryGetObjectEventIdByLocalIdAndMap(gFieldEffectArguments[0], gFieldEffectArguments[1], gFieldEffectArguments[2], &spriteId))
    {
        FieldEffectActiveListRemove(fldEff);
        return MAX_SPRITES;
    }
    spriteId = CreateSpriteAtEnd(template, 0, 0, 0);
    if (spriteId != MAX_SPRITES)
    {
        sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled++;
        sprite->oam.paletteNum = paletteNum;
        sprite->data[1] = fldEff;
        sprite->data[2] = gFieldEffectArguments[0];
        sprite->data[3] = gFieldEffectArguments[1];
        sprite->data[4] = gFieldEffectArguments[2];
    }
    return spriteId;
}

void UpdateDisguiseFieldEffect(struct Sprite *sprite)
{
    u8 objectEventId;
    const struct ObjectEventGraphicsInfo * graphicsInfo;
    struct Sprite *linkedSprite;

    if (TryGetObjectEventIdByLocalIdAndMap(sprite->data[2], sprite->data[3], sprite->data[4], &objectEventId))
    {
        FieldEffectStop(sprite, sprite->data[1]);
    }

    graphicsInfo = GetObjectEventGraphicsInfo(gObjectEvents[objectEventId].graphicsId);
    linkedSprite = &gSprites[gObjectEvents[objectEventId].spriteId];
    sprite->invisible = linkedSprite->invisible;
    sprite->x = linkedSprite->x;
    sprite->y = (graphicsInfo->height >> 1) + linkedSprite->y - 16;
    sprite->subpriority = linkedSprite->subpriority - 1;
    if (sprite->data[0] == 1)
    {
        sprite->data[0]++;
        StartSpriteAnim(sprite, 1);
    }
    if (sprite->data[0] == 2 && sprite->animEnded)
    {
        sprite->data[7] = 1;
    }
    if (sprite->data[0] == 3)
    {
        FieldEffectStop(sprite, sprite->data[1]);
    }
}

void StartRevealDisguise(struct ObjectEvent * objectEvent)
{
    if (objectEvent->directionSequenceIndex == 1)
    {
        gSprites[objectEvent->fieldEffectSpriteId].data[0]++;
    }
}

bool8 UpdateRevealDisguise(struct ObjectEvent * objectEvent)
{
    struct Sprite *sprite;

    if (objectEvent->directionSequenceIndex == 2)
    {
        return TRUE;
    }
    if (objectEvent->directionSequenceIndex == 0)
    {
        return TRUE;
    }
    sprite = &gSprites[objectEvent->fieldEffectSpriteId];
    if (sprite->data[7])
    {
        objectEvent->directionSequenceIndex = 2;
        sprite->data[0]++;
        return TRUE;
    }
    return FALSE;
}

u32 FldEff_Sparkle(void)
{
    u8 spriteId;

    FieldEffectScript_LoadFadedPal(&gSpritePalette_SmallSparkle);
    gFieldEffectArguments[0] += 7;
    gFieldEffectArguments[1] += 7;
    SetSpritePosToOffsetMapCoords((s16 *)&gFieldEffectArguments[0], (s16 *)&gFieldEffectArguments[1], 8, 8);
    spriteId = CreateSpriteAtEnd(&gFieldEffectObjectTemplate_SmallSparkle, gFieldEffectArguments[0], gFieldEffectArguments[1], 0x52);
    if (spriteId != MAX_SPRITES)
    {
        gSprites[spriteId].oam.priority = gFieldEffectArguments[2];
        gSprites[spriteId].coordOffsetEnabled = TRUE;
    }
    return spriteId;
}

void UpdateSparkleFieldEffect(struct Sprite *sprite)
{
    if (sprite->data[0] == 0)
    {
        if (sprite->animEnded)
        {
            sprite->invisible = TRUE;
            sprite->data[0]++;
        }

        if (sprite->data[0] == 0)
            return;
    }

    if (++sprite->data[1] > 34)
        FieldEffectStop(sprite, FLDEFF_SPARKLE);
}

void UpdateJumpImpactEffect(struct Sprite *sprite)
{
    if (sprite->animEnded)
    {
        FieldEffectStop(sprite, sprite->data[1]);
    }
    else
    {
        UpdateObjectEventSpriteInvisibility(sprite, FALSE);
        SetObjectSubpriorityByElevation(sprite->data[0], sprite, 0);
    }
}

void WaitFieldEffectSpriteAnim(struct Sprite *sprite)
{
    if (sprite->animEnded)
        FieldEffectStop(sprite, sprite->data[0]);
    else
        UpdateObjectEventSpriteInvisibility(sprite, FALSE);
}

static void UpdateGrassFieldEffectSubpriority(struct Sprite *sprite, u8 z, u8 offset)
{
    u8 i;
    s16 var, xhi, lyhi, yhi, ylo;
    struct Sprite *linkedSprite;

    SetObjectSubpriorityByElevation(z, sprite, offset);
    for (i = 0; i < OBJECT_EVENTS_COUNT; i++)
    {
        struct ObjectEvent * objectEvent = &gObjectEvents[i];
        if (objectEvent->active)
        {
            linkedSprite = &gSprites[objectEvent->spriteId];
            xhi = sprite->x + sprite->centerToCornerVecX;
            var = sprite->x - sprite->centerToCornerVecX;
            if (xhi < linkedSprite->x && var > linkedSprite->x)
            {
                lyhi = linkedSprite->y + linkedSprite->centerToCornerVecY;
                var = linkedSprite->y;
                ylo = sprite->y - sprite->centerToCornerVecY;
                yhi = ylo + linkedSprite->centerToCornerVecY;
                if ((lyhi < yhi || lyhi < ylo) && var > yhi && sprite->subpriority <= linkedSprite->subpriority)
                {
                    sprite->subpriority = linkedSprite->subpriority + 2;
                    break;
                }
            }
        }
    }
}
