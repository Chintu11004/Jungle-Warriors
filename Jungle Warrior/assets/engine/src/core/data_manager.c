#pragma bank 255

#include <string.h>

#include "system.h"
#include "vm.h"
#include "data_manager.h"
#include "linked_list.h"
#include "actor.h"
#include "projectiles.h"
#include "scroll.h"
#include "trigger.h"
#include "camera.h"
#include "ui.h"
#include "palette.h"
#include "data/spritesheet_none.h"
#include "data/data_bootstrap.h"
#include "bankdata.h"
#include "macro.h"

#define ALLOC_BKG_TILES_TOWARDS_SPR

#define EMOTE_SPRITE_SIZE       4
#define SCENE_SPRITE_UNLOADED   0xFF
#define MAX_DEFERRED_ACTOR_SLOTS 15
#define DEFERRED_SLOT_FREE      0xFF

far_ptr_t current_scene;

UBYTE image_bank;
unsigned char* image_ptr;

UBYTE image_attr_bank;
unsigned char* image_attr_ptr;

UBYTE collision_bank;
unsigned char* collision_ptr;

UBYTE image_tile_width;
UBYTE image_tile_height;
UINT16 image_width;
UINT16 image_height;
UINT16 image_width_subpx;
UINT16 image_height_subpx;
UBYTE sprites_len;
UBYTE actors_len;
UBYTE projectiles_len;
UBYTE player_sprite_len;
scene_type_e scene_type;
LCD_isr_e scene_LCD_type;

const far_ptr_t spritesheet_none_far = TO_FAR_PTR_T(spritesheet_none);

scene_stack_item_t scene_stack[SCENE_STACK_SIZE];
scene_stack_item_t * scene_stack_ptr;

UBYTE scene_sprites_base_tiles[MAX_SCENE_SPRITES];

typedef struct {
    UBYTE ref_count;       /* Number of actors using this tile region; 0 = slot free */
    UBYTE base_tile;
    UBYTE tile_count;
    far_ptr_t sprite;      /* Spritesheet identity for same-sprite sharing */
} deferred_slot_t;
static deferred_slot_t deferred_slots[MAX_DEFERRED_ACTOR_SLOTS];
static UWORD deferred_allocated_delta;  /* Tiles allocated for deferred since level start */

static void deferred_slots_reset(void) {
    UBYTE i;
    for (i = 0; i < MAX_DEFERRED_ACTOR_SLOTS; i++) {
        deferred_slots[i].ref_count = 0;
        deferred_slots[i].base_tile = DEFERRED_SLOT_FREE;
        deferred_slots[i].tile_count = DEFERRED_SLOT_FREE;
    }
}

/** Resets deferred slots and reclaims VRAM for level transition.
 *  Iterates all actors and unloads any deferred actors still loaded, then resets slots and reclaims VRAM.
 *  Call once on level transition—no need to manually unload each actor first. */
void deload_prev_level(void) BANKED {
    UBYTE i;
    actor_t *actor;
    for (i = 1; i < actors_len; i++) {
        actor = actors + i;
        if (!actor->reserve_tiles && actor->base_tile != SCENE_SPRITE_UNLOADED) {
            unload_actor_sprite(actor);
        }
    }
    deferred_slots_reset();
    allocated_sprite_tiles -= (UBYTE)deferred_allocated_delta;
    deferred_allocated_delta = 0;
}

static UBYTE actor_list_remove(actor_t **head, actor_t *actor) {
    actor_t *current;
    UBYTE i;
    current = *head;
    for (i = 0; current && i < MAX_ACTORS; i++) {
        if (current == actor) {
            DL_REMOVE_ITEM((*head), actor);
            actor->next = 0;
            actor->prev = 0;
            return TRUE;
        }
        current = current->next;
    }
    return FALSE;
}

/** Read spritesheet tile count without loading. Returns max of DMG and CGB counts. */
static UBYTE spritesheet_get_tile_count(const spritesheet_t *sprite, UBYTE bank) {
    far_ptr_t data;
    UWORD n_dmg = 0, n_cgb = 0;
    ReadBankedFarPtr(&data, (void *)&sprite->tileset, bank);
    if (data.ptr) n_dmg = ReadBankedUWORD((const unsigned char *)&((const tileset_t *)data.ptr)->n_tiles, data.bank);
#ifdef CGB
    if (_is_CGB) {
        ReadBankedFarPtr(&data, (void *)&sprite->cgb_tileset, bank);
        if (data.ptr) n_cgb = ReadBankedUWORD((const unsigned char *)&((const tileset_t *)data.ptr)->n_tiles, data.bank);
    }
#endif
    return (UBYTE)((n_cgb > n_dmg) ? n_cgb : n_dmg);
}

void load_init(void) BANKED {
    actors_len = 0;
    player_sprite_len = 0;
    scene_stack_ptr = scene_stack;
}

void load_bkg_tileset(const tileset_t* tiles, UBYTE bank) BANKED {
    if ((!bank) && (!tiles)) return;

    UWORD n_tiles = ReadBankedUWORD(&(tiles->n_tiles), bank);

    // load first background chunk, align to zero tile
    UBYTE * data = tiles->tiles;
    if (n_tiles < 128) {
        if ((UBYTE)n_tiles) SetBankedBkgData(0, n_tiles, data, bank);
        return;
    }
    SetBankedBkgData(0, 128, data, bank);
    n_tiles -= 128; data += 128 * 16;

    // load second background chunk
    if (n_tiles < 128) {
        if (n_tiles < 65) {
            #ifdef ALLOC_BKG_TILES_TOWARDS_SPR
                // new allocation style, align to 192-th tile
                if ((UBYTE)n_tiles) SetBankedBkgData(192 - n_tiles, n_tiles, data, bank);
            #else
                // old allocation style, align to 128-th tile
                if ((UBYTE)n_tiles) SetBankedBkgData(128, n_tiles, data, bank);
            #endif
        } else {
            // if greater than 64 allow overflow into UI, align to 128-th tile
            if ((UBYTE)n_tiles) SetBankedBkgData(128, n_tiles, data, bank);
        }
        return;
    }
    SetBankedBkgData(128, 128, data, bank);
    n_tiles -= 128; data += 128 * 16;

    // if more than 256 - then it's a 360-tile logo, load rest to sprite area
    if ((UBYTE)n_tiles) SetBankedSpriteData(0, n_tiles, data, bank);
}

void load_background(const background_t* background, UBYTE bank) BANKED {
    background_t bkg;
    MemcpyBanked(&bkg, background, sizeof(bkg), bank);

    image_bank = bkg.tilemap.bank;
    image_ptr = bkg.tilemap.ptr;

    image_attr_bank = bkg.cgb_tilemap_attr.bank;
    image_attr_ptr = bkg.cgb_tilemap_attr.ptr;

    image_tile_width = bkg.width;
    image_tile_height = bkg.height;
    image_width = TILE_TO_PX(image_tile_width);
    image_width_subpx = PX_TO_SUBPX(image_width);
    image_height = TILE_TO_PX(image_tile_height);
    image_height_subpx = PX_TO_SUBPX(image_height);

    load_bkg_tileset(bkg.tileset.ptr, bkg.tileset.bank);
#ifdef CGB
    if ((_is_CGB) && (bkg.cgb_tileset.ptr)) {
        VBK_REG = 1;
        load_bkg_tileset(bkg.cgb_tileset.ptr, bkg.cgb_tileset.bank);
        VBK_REG = 0;
    }
#endif
}

inline UBYTE load_sprite_tileset(UBYTE base_tile, const tileset_t * tileset, UBYTE bank) {
    if ((!bank) && (!tileset)) return 0;
    UBYTE n_tiles = ReadBankedUBYTE(&(tileset->n_tiles), bank);
    if (n_tiles) SetBankedSpriteData(base_tile, n_tiles, tileset->tiles, bank);
    return n_tiles;
}

UBYTE load_sprite(UBYTE sprite_offset, const spritesheet_t * sprite, UBYTE bank) BANKED {
    far_ptr_t data;
    ReadBankedFarPtr(&data, (void *)&sprite->tileset, bank);
    UBYTE n_tiles = load_sprite_tileset(sprite_offset, data.ptr, data.bank);
#ifdef CGB
    if (_is_CGB) {
        ReadBankedFarPtr(&data, (void *)&sprite->cgb_tileset, bank);
        if (data.ptr) {
            VBK_REG = 1;
            UBYTE n_cgb_tiles = load_sprite_tileset(sprite_offset, data.ptr, data.bank);
            VBK_REG = 0;
            if (n_cgb_tiles > n_tiles) return n_cgb_tiles;
        }
    }
#endif
    return n_tiles;
}

/**
Loads the sprite for an actor. If the sprite is already loaded, it returns the base tile.
If the sprite is not loaded, it loads the sprite and returns the base tile.
Deferred actors (reserve_tiles==0) use the slot table for private allocation.
*/
UBYTE load_actor_sprite(actor_t * actor) BANKED {
    far_ptr_t scene_sprites;
    UBYTE idx = sprites_len;
    UBYTE i;
    if (!actor) return 0;

    /* Deferred actors: use slot table with ref-counting for sprite sharing.
     * ref_count == 0 means slot is free.
     * base_tile == DEFERRED_SLOT_FREE means never-used; else previously used (reuse if tile_count >= n_tiles).
     */
    if (!actor->reserve_tiles) {
        UBYTE n_tiles = spritesheet_get_tile_count(actor->sprite.ptr, actor->sprite.bank);
        if (!n_tiles) return 0;

        /* Check if sprite already loaded (shared with reserved actor or scene) */
        if (sprites_len) {
            ReadBankedFarPtr(&scene_sprites, (const unsigned char *)&((scene_t *)current_scene.ptr)->sprites, current_scene.bank);
            idx = IndexOfFarPtr(scene_sprites.ptr, scene_sprites.bank, sprites_len, &actor->sprite);
            if (idx < sprites_len && scene_sprites_base_tiles[idx] != SCENE_SPRITE_UNLOADED) {
                actor->base_tile = scene_sprites_base_tiles[idx];
                /* If tiles came from a deferred slot, increment ref_count to match unload's decrement */
                for (i = 0; i < MAX_DEFERRED_ACTOR_SLOTS; i++) {
                    if (deferred_slots[i].ref_count != 0 &&
                        deferred_slots[i].base_tile == actor->base_tile) {
                        deferred_slots[i].ref_count++;
                        break;
                    }
                }
                CLR_FLAG(actor->flags, ACTOR_FLAG_HIDDEN | ACTOR_FLAG_DISABLED);
                return 1;
            }
        }

        /* Loop 1: same sprite already loaded in a deferred slot? Share it (ref-count) */
        for (i = 0; i < MAX_DEFERRED_ACTOR_SLOTS; i++) {
            if (deferred_slots[i].ref_count == 0) continue;
            if (deferred_slots[i].sprite.bank == actor->sprite.bank &&
                deferred_slots[i].sprite.ptr == actor->sprite.ptr) {
                deferred_slots[i].ref_count++;
                actor->base_tile = deferred_slots[i].base_tile;
                CLR_FLAG(actor->flags, ACTOR_FLAG_HIDDEN | ACTOR_FLAG_DISABLED);
                return deferred_slots[i].tile_count;
            }
        }

        /* Loop 2: find free slot (ref_count == 0), then new alloc or reuse */
        for (i = 0; i < MAX_DEFERRED_ACTOR_SLOTS; i++) {
            if (deferred_slots[i].ref_count != 0) continue;

            if (deferred_slots[i].base_tile == DEFERRED_SLOT_FREE) {
                /* Never-used: load at allocated_sprite_tiles */
                UBYTE n_loaded = load_sprite(allocated_sprite_tiles, actor->sprite.ptr, actor->sprite.bank);
                if (!n_loaded) return 0;
                deferred_slots[i].ref_count = 1;
                deferred_slots[i].sprite = actor->sprite;
                deferred_slots[i].base_tile = allocated_sprite_tiles;
                deferred_slots[i].tile_count = n_loaded;
                actor->base_tile = allocated_sprite_tiles;
                if (sprites_len && idx < sprites_len) scene_sprites_base_tiles[idx] = allocated_sprite_tiles;
                allocated_sprite_tiles += n_loaded;
                deferred_allocated_delta += n_loaded;
                CLR_FLAG(actor->flags, ACTOR_FLAG_HIDDEN | ACTOR_FLAG_DISABLED);
                return n_loaded;
            }
            if (deferred_slots[i].tile_count >= n_tiles) {
                /* Previously used, enough space: reuse at base_tile */
                UBYTE n_loaded = load_sprite(deferred_slots[i].base_tile, actor->sprite.ptr, actor->sprite.bank);
                if (!n_loaded) return 0;
                deferred_slots[i].ref_count = 1;
                deferred_slots[i].sprite = actor->sprite;
                actor->base_tile = deferred_slots[i].base_tile;
                if (sprites_len && idx < sprites_len) scene_sprites_base_tiles[idx] = deferred_slots[i].base_tile;
                CLR_FLAG(actor->flags, ACTOR_FLAG_HIDDEN | ACTOR_FLAG_DISABLED);
                return n_loaded;
            }
            /* Freed slot but tile_count < n_tiles: skip, try next */
        }
        return 0; /* No free slot */
    }

    /* Reserved actors: use shared scene_sprites cache */
    if (sprites_len) {
        ReadBankedFarPtr(&scene_sprites, (const unsigned char *)&((scene_t *)current_scene.ptr)->sprites, current_scene.bank);
        idx = IndexOfFarPtr(scene_sprites.ptr, scene_sprites.bank, sprites_len, &actor->sprite);
    }
    if (idx < sprites_len && scene_sprites_base_tiles[idx] != SCENE_SPRITE_UNLOADED) {
        actor->base_tile = scene_sprites_base_tiles[idx];
        return 1;
    }
    UBYTE n_loaded = load_sprite(allocated_sprite_tiles, actor->sprite.ptr, actor->sprite.bank);
    if (!n_loaded) return 0;
    actor->base_tile = allocated_sprite_tiles;
    if (idx < sprites_len) scene_sprites_base_tiles[idx] = allocated_sprite_tiles;
    allocated_sprite_tiles += n_loaded;
    return n_loaded;
}

/**
 * Unloads an actor's sprite: clears base_tile, sets HIDDEN|DISABLED,
 * removes from active/inactive lists, frees slot for deferred actors.
 */
UBYTE unload_actor_sprite(actor_t * actor) BANKED {
    UBYTE i;
    UBYTE was_active;
    if (!actor || actor->reserve_tiles) return 0;
    if (actor->base_tile == SCENE_SPRITE_UNLOADED) return 0;
    was_active = CHK_FLAG(actor->flags, ACTOR_FLAG_ACTIVE);

    /* Decrement deferred slot ref-count if applicable; slot stays "in use" until ref_count hits 0 */
    if (!actor->reserve_tiles) {
        for (i = 0; i < MAX_DEFERRED_ACTOR_SLOTS; i++) {
            if (deferred_slots[i].ref_count != 0 &&
                deferred_slots[i].base_tile == actor->base_tile) {
                deferred_slots[i].ref_count--;
                /* When ref_count==0, slot is free for reuse; base_tile/tile_count kept for VRAM reuse */
                break;
            }
        }
    }

    if (!actor_list_remove(&actors_inactive_head, actor)) {
        actor_list_remove(&actors_active_head, actor);
    }

    if (was_active) {
        if ((actor->hscript_update & SCRIPT_TERMINATED) == 0) {
            script_terminate(actor->hscript_update);
        }
        if ((actor->hscript_hit & SCRIPT_TERMINATED) == 0) {
            script_detach_hthread(actor->hscript_hit);
        }
    }

    CLR_FLAG(actor->flags, ACTOR_FLAG_ACTIVE);
    SET_FLAG(actor->flags, ACTOR_FLAG_HIDDEN | ACTOR_FLAG_DISABLED);
    actor->base_tile = SCENE_SPRITE_UNLOADED;
    actor->next = 0;
    actor->prev = 0;
    return 1;
}

/** Ensures a scene sprite is loaded into VRAM. Returns base_tile or 0 on failure.
 *  Used by projectiles and other non-actor consumers of scene sprites. */
UBYTE scene_sprite_ensure_loaded(UBYTE sprite_idx) BANKED {
    if (sprite_idx >= sprites_len) return 0;
    if (scene_sprites_base_tiles[sprite_idx] != SCENE_SPRITE_UNLOADED)
        return scene_sprites_base_tiles[sprite_idx];

    far_ptr_t scene_sprites;
    far_ptr_t tmp_ptr;
    ReadBankedFarPtr(&scene_sprites, (const unsigned char *)&((scene_t *)current_scene.ptr)->sprites, current_scene.bank);
    {
        const far_ptr_t * list_ptr = (const far_ptr_t *)scene_sprites.ptr;
        list_ptr += sprite_idx;
        ReadBankedFarPtr(&tmp_ptr, (void *)list_ptr, scene_sprites.bank);
    }
    UBYTE n_loaded = load_sprite(allocated_sprite_tiles, tmp_ptr.ptr, tmp_ptr.bank);
    if (!n_loaded) return 0;
    scene_sprites_base_tiles[sprite_idx] = allocated_sprite_tiles;
    allocated_sprite_tiles += n_loaded;
    return scene_sprites_base_tiles[sprite_idx];
}

void load_animations(const spritesheet_t *sprite, UBYTE bank, UWORD animation_set, animation_t * res_animations) NONBANKED {
    UBYTE _save = CURRENT_BANK;
    SWITCH_ROM(bank);
    memcpy(res_animations, sprite->animations + sprite->animations_lookup[animation_set], sizeof(animation_t) * 8);
    SWITCH_ROM(_save);
}

void load_bounds(const spritesheet_t *sprite, UBYTE bank, rect16_t * res_bounds) BANKED {
    MemcpyBanked(res_bounds, &sprite->bounds, sizeof(sprite->bounds), bank);
}

UBYTE do_load_palette(palette_entry_t * dest, const palette_t * palette, UBYTE bank) BANKED {
    UBYTE mask = ReadBankedUBYTE(&palette->mask, bank);
    palette_entry_t * sour = palette->cgb_palette;
    for (UBYTE i = mask; (i); i >>= 1, dest++) {
        if ((i & 1) == 0) continue;
        MemcpyBanked(dest, sour, sizeof(palette_entry_t), bank);
        sour++;
    }
    return mask;
}

inline void load_bkg_palette(const palette_t * palette, UBYTE bank) {
    UBYTE mask = do_load_palette(BkgPalette, palette, bank);
    DMG_palette[0] = ReadBankedUBYTE(palette->palette, bank);
#ifdef SGB
    if (_is_SGB) {
        UBYTE sgb_palettes = SGB_PALETTES_NONE;
        if (mask & 0b00110000) sgb_palettes |= SGB_PALETTES_01;
        if (mask & 0b11000000) sgb_palettes |= SGB_PALETTES_23;
        SGBTransferPalettes(sgb_palettes);
    }
#endif
}

inline void load_sprite_palette(const palette_t * palette, UBYTE bank) {
    do_load_palette(SprPalette, palette, bank);
    UWORD data = ReadBankedUWORD(palette->palette, bank);
    DMG_palette[1] = (UBYTE)data;
    DMG_palette[2] = (UBYTE)(data >> 8);
}

UBYTE load_scene(const scene_t * scene, UBYTE bank, UBYTE init_data) BANKED {
    UBYTE i;
    scene_t scn;

    MemcpyBanked(&scn, scene, sizeof(scn), bank);

    current_scene.bank  = bank;
    current_scene.ptr   = (void *)scene;

    // Load scene
    scene_type      = scn.type;
    actors_len      = MIN(scn.n_actors + 1,     MAX_ACTORS);
    triggers_len    = MIN(scn.n_triggers,       MAX_TRIGGERS);
    projectiles_len = MIN(scn.n_projectiles,    MAX_PROJECTILE_DEFS);
    sprites_len     = MIN(scn.n_sprites,        MAX_SCENE_SPRITES);
    if (init_data) {
        memset(scene_sprites_base_tiles, SCENE_SPRITE_UNLOADED, sizeof(scene_sprites_base_tiles));
        deferred_slots_reset();
        deferred_allocated_delta = 0;
    }

    collision_bank  = scn.collisions.bank;
    collision_ptr   = scn.collisions.ptr;

    // Load UI tiles, they may be overwritten by the following load_background()
    ui_load_tiles();

    // Load background + tiles
    load_background(scn.background.ptr, scn.background.bank);

    load_bkg_palette(scn.palette.ptr, scn.palette.bank);
    load_sprite_palette(scn.sprite_palette.ptr, scn.sprite_palette.bank);

    // Copy parallax settings
    memcpy(&parallax_rows, &scn.parallax_rows, sizeof(parallax_rows));
    if (scn.parallax_rows[0].next_y == 0) {
        scene_LCD_type = (scene_type == SCENE_TYPE_LOGO) ? LCD_fullscreen : LCD_simple;
    } else {
        scene_LCD_type = LCD_parallax;
    }

    scroll_x_min = scn.scroll_bounds.left;
    scroll_x_max = scn.scroll_bounds.right;
    scroll_y_min = scn.scroll_bounds.top;
    scroll_y_max = scn.scroll_bounds.bottom;

    if (scene_type != SCENE_TYPE_LOGO) {
        // Load player
        PLAYER.sprite = scn.player_sprite;
        UBYTE n_loaded = load_sprite(PLAYER.base_tile = 0, scn.player_sprite.ptr, scn.player_sprite.bank);
        allocated_sprite_tiles = (n_loaded > scn.reserve_tiles) ? n_loaded : scn.reserve_tiles;
        load_animations(scn.player_sprite.ptr, scn.player_sprite.bank, ANIM_SET_DEFAULT, PLAYER.animations);
        load_bounds(scn.player_sprite.ptr, scn.player_sprite.bank, &PLAYER.bounds);
    } else {
        // no player on logo, but still some little amount of actors may be present
        PLAYER.base_tile = allocated_sprite_tiles = 0x68;
        PLAYER.sprite = spritesheet_none_far;
        memset(PLAYER.animations, 0, sizeof(PLAYER.animations));
    }

    // Load sprites
    // if (sprites_len != 0) {
    //     far_ptr_t * scene_sprite_ptrs = scn.sprites.ptr;
    //     far_ptr_t tmp_ptr;
    //     for (i = 0; i != sprites_len; i++) {
    //         if (i == MAX_SCENE_SPRITES) break;
    //         ReadBankedFarPtr(&tmp_ptr, (UBYTE *)scene_sprite_ptrs, scn.sprites.bank);
    //         scene_sprites_base_tiles[i] = allocated_sprite_tiles;
    //         allocated_sprite_tiles += load_sprite(allocated_sprite_tiles, tmp_ptr.ptr, tmp_ptr.bank);
    //         scene_sprite_ptrs++;
    //     }
    // }

    if (init_data) {
        camera_reset();
        rope_actor_count = 0;

        // Copy scene player hit scripts to player actor
        memcpy(&PLAYER.script, &scn.script_p_hit1, sizeof(far_ptr_t));

        player_moving = FALSE;

        // Load actors
        actors_active_head = NULL;
        actors_inactive_head = NULL;

        // Add player to inactive, then activate
        CLR_FLAG(PLAYER.flags, ACTOR_FLAG_ACTIVE);
        actors_active_tail = &PLAYER;
        DL_PUSH_HEAD(actors_inactive_head, actors_active_tail);
        activate_actor(&PLAYER);

        // Add other actors, activate pinned
        if (actors_len != 0) {
            actor_t * actor = actors + 1;
            MemcpyBanked(actor, scn.actors.ptr, sizeof(actor_t) * (actors_len - 1), scn.actors.bank);
            for (i = actors_len - 1; i != 0; i--, actor++) {
                actor->next = 0;
                actor->prev = 0;
                if (actor->reserve_tiles) {
                    // exclusive sprites allocated separately to avoid overwriting if modified
                    actor->base_tile = allocated_sprite_tiles;
                    UBYTE n_loaded = load_sprite(allocated_sprite_tiles, actor->sprite.ptr, actor->sprite.bank);
                    allocated_sprite_tiles += (n_loaded > actor->reserve_tiles) ? n_loaded : actor->reserve_tiles;
                } else {
                    actor->base_tile = SCENE_SPRITE_UNLOADED;
                    SET_FLAG(actor->flags, ACTOR_FLAG_HIDDEN | ACTOR_FLAG_DISABLED);
                }
                load_animations((void *)actor->sprite.ptr, actor->sprite.bank, ANIM_SET_DEFAULT, actor->animations);
                CLR_FLAG(actor->flags, ACTOR_FLAG_ACTIVE);
                if (actor->reserve_tiles) {
                    DL_PUSH_HEAD(actors_inactive_head, actor);
                }
            }
        }

    } else {
        // reload sprite data for the unique actors
        if (actors_len != 0) {
            actor_t * actor = actors + 1;
            for (i = actors_len - 1; i != 0; i--, actor++) {
                if (actor->reserve_tiles || actor->base_tile != SCENE_SPRITE_UNLOADED) {
                    load_sprite(actor->base_tile, actor->sprite.ptr, actor->sprite.bank);
                }
            }
        }
        // set actors idle
        actor_t *actor = actors_active_head;
        while (actor) {
            actor_set_anim_idle(actor);
            actor = actor->next;
        }
    }

    // Init and Load projectiles
    projectiles_init();
    if (projectiles_len  != 0) {
        projectile_def_t * projectile_def = projectile_defs;
        MemcpyBanked(projectile_def, scn.projectiles.ptr, sizeof(projectile_def_t) * projectiles_len, scn.projectiles.bank);
        for (i = projectiles_len; i != 0; i--, projectile_def++) {
            // resolve and set base_tile for each projectile (load sprite into VRAM if needed)
            UBYTE idx = IndexOfFarPtr(scn.sprites.ptr, scn.sprites.bank, sprites_len, &projectile_def->sprite);
            projectile_def->base_tile = (idx < sprites_len) ? scene_sprite_ensure_loaded(idx) : 0;
            if (!init_data && projectile_def->base_tile != 0) {
                load_sprite(projectile_def->base_tile, projectile_def->sprite.ptr, projectile_def->sprite.bank);
            }
        }
    }

    // Load triggers
    if (triggers_len != 0) {
        MemcpyBanked(&triggers, scn.triggers.ptr, sizeof(trigger_t) * triggers_len, scn.triggers.bank);
    }

    scroll_reset();
    trigger_reset();

    emote_actor = NULL;

    if ((init_data) && (scn.script_init.ptr != NULL)) {
        return (script_execute(scn.script_init.bank, scn.script_init.ptr, 0, 0) != 0);
    }
    return FALSE;
}

void load_player(void) BANKED {
    PLAYER.pos.x = start_scene_x;
    PLAYER.pos.y = start_scene_y;
    PLAYER.dir = start_scene_dir;
    PLAYER.move_speed = start_player_move_speed;
    PLAYER.anim_tick = start_player_anim_tick;
    PLAYER.frame = 0;
    PLAYER.frame_start = 0;
    PLAYER.frame_end = 2;
    CLR_FLAG(PLAYER.flags, ACTOR_FLAG_PINNED);
    PLAYER.collision_group = COLLISION_GROUP_PLAYER;
    SET_FLAG(PLAYER.flags, ACTOR_FLAG_COLLISION);
}

void load_emote(const unsigned char *tiles, UBYTE bank) BANKED {
    SetBankedSpriteData(allocated_sprite_tiles, EMOTE_SPRITE_SIZE, tiles + 0, bank);
}
