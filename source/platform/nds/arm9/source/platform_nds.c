#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <nds.h>
#include "nds/arm9/background.h"
#include "nds/arm9/sprite.h"
#include "nds/arm9/video.h"
#include "nds/interrupts.h"
#include "nds/system.h"
#include "shim/gb_shim.h"
#include "shim/platform.h"

#ifdef FEAT_BORDER
#include "border.h"
#endif

// #define LAYER_PRIORITY_EMULATION

// GBSA logic

static uint16_t shadow_hofs, shadow_vofs;
static uint16_t bg_x_offset, bg_y_offset, viewport_width, viewport_height;
static int win_x_pos = 0, win_y_pos = MAXWNDPOSY + 1;
static uint32_t lut_expand_8_32[256] = {
#include "lut_expand_8_32.inc"
};

static uint16_t dmg_palette[4] = {
    0x7BDE | 0x8000,
    0x5294 | 0x8000,
    0x294A | 0x8000,
    0x0000 | 0x8000
};

/**
 * VRAM layout: 
 * 00000 - 07FFF: tile area
 * 08000 - 08FFF: bgmap 0 (background tiles)
 * 09000 - 09FFF: bgmap 2 (foreground tiles)
 * 0A000 - 0A7FF: bgmap 1 (window tiles)
 * 0A800 - 0AFFF: bgmap 3 (border)
 *
 * tile 0x303, color 15 = blank area
 */
#ifdef FEAT_WIDESCREEN
#define EMU_BGMAP_SIZE BG_64x32
#else
#define EMU_BGMAP_SIZE BG_32x32
#endif

#ifdef DEBUG_PROFILE
#define MAX_PROFILE_STACK 8
#define PROFILE_RAW (REG_TM2CNT_L | (REG_TM3CNT_L << 16))
static const char *profile_names[MAX_PROFILE_STACK];
static uint32_t profile_times[MAX_PROFILE_STACK];
static int32_t profile_pos = 0;

void gbsa_profile_push(const char *name) {
    if (profile_pos >= 0 && profile_pos < MAX_PROFILE_STACK) {
        profile_names[profile_pos] = name;
        profile_times[profile_pos] = PROFILE_RAW;
    }
    profile_pos++;
}

void gbsa_profile_pop(void) {
    profile_pos--;
    if (profile_pos >= 0 && profile_pos < MAX_PROFILE_STACK) {
        int32_t time = (int32_t) (PROFILE_RAW - profile_times[profile_pos]);
        iprintf("%s: %d cycles\n", profile_names[profile_pos], time);
    }
}
#endif

#define MEM_VRAM 0x06200000
#define MEM_OBJ_VRAM 0x06600000

typedef struct {
	u16 attr0, attr1, attr2, attr3;
} OAMRawEntry;

void gbsa_init(void) {
    REG_DISPCNT = DISPLAY_SCREEN_OFF;
    REG_DISPCNT_SUB = DISPLAY_SCREEN_OFF;

	irqInit();

	lcdMainOnBottom();

	vramSetBankC(VRAM_C_SUB_BG);
	vramSetBankI(VRAM_I_SUB_SPRITE);

    // clear VRAM
    for (int i = 0; i < (0x1000 >> 2); i++) {
        *((uint32_t*) (MEM_VRAM + 0x8000 + (i * 4))) = 0x03FF03FF;
        *((uint32_t*) (MEM_VRAM + 0x9000 + (i * 4))) = 0x03FE03FE;
#if defined(LAYER_PRIORITY_EMULATION) || !defined(FEAT_BORDER)
        *((uint32_t*) (MEM_VRAM + 0xA000 + (i * 4))) = 0x03FF03FF;
#endif
    }
    for (int i = 0; i < 8; i++) {
        *((uint32_t*) (MEM_VRAM + (0x3FE * 32) + (i * 4))) = 0x00000000;
        *((uint32_t*) (MEM_VRAM + (0x3FF * 32) + (i * 4))) = 0xFFFFFFFF;
    }
	OAMRawEntry *obj_mem = (OAMRawEntry*) OAM_SUB;
    for (int i = 0; i < 128; i++) {
        obj_mem[i].attr0 = 0x8200;
        obj_mem[i].attr2 = 0x0C00;
        BG_PALETTE_SUB[i] = 0;
        BG_PALETTE_SUB[i | 128] = 0;
        SPRITE_PALETTE_SUB[i] = 0;
        SPRITE_PALETTE_SUB[i | 128] = 0;
    }

#ifdef FEAT_BORDER
    memcpy((uint32_t*) (MEM_VRAM + 0x0C000), borderTiles, borderTilesLen);
    memcpy((uint32_t*) (MEM_VRAM + 0x0A800), borderMap, 0x800);
    memcpy((uint32_t*) (BG_PALETTE_SUB + 240), borderPal, borderPalLen);
    REG_BG3CNT_SUB = BG_PRIORITY(0) | BG_TILE_BASE(3) | BG_MAP_BASE(21) | BG_COLOR_16 | BG_32x32;
#else
    REG_BG3CNT_SUB = BG_PRIORITY(0) | BG_TILE_BASE(0) | BG_MAP_BASE(21) | BG_COLOR_16 | BG_32x32;
#endif

#ifdef DEBUG_PROFILE
    REG_TM2CNT_L = 0;
    REG_TM2CNT_H = TM_FREQ_1 | TM_ENABLE;
    REG_TM3CNT_L = 0;
    REG_TM3CNT_H = TM_FREQ_1 | TM_ENABLE | TM_CASCADE;
#endif

    gbsa_update_viewport_size(SCREENWIDTH, SCREENHEIGHT);

    SUB_WIN_IN = ((0b00110101) << 8) | 0b00100010;
    SUB_WIN_OUT = 0b00001000;

    REG_BG0CNT_SUB = BG_PRIORITY(3) | BG_TILE_BASE(0) | BG_MAP_BASE(16) | BG_COLOR_16 | EMU_BGMAP_SIZE;
    REG_BG1CNT_SUB = BG_PRIORITY(2) | BG_TILE_BASE(0) | BG_MAP_BASE(20) | BG_COLOR_16 | BG_32x32;
    REG_BG2CNT_SUB = BG_PRIORITY(1) | BG_TILE_BASE(0) | BG_MAP_BASE(18) | BG_COLOR_16 | EMU_BGMAP_SIZE;
	REG_DISPCNT_SUB = MODE_0_2D | DISPLAY_BG0_ACTIVE | DISPLAY_BG1_ACTIVE | DISPLAY_BG3_ACTIVE | DISPLAY_SPR_1D | DISPLAY_WIN1_ON;
#ifdef LAYER_PRIORITY_EMULATION
    REG_DISPCNT_SUB |= DISPLAY_BG2_ACTIVE;
#endif
}

void gbsa_update_viewport_size(int width, int height) {
    int hofs = shadow_hofs + bg_x_offset;
    int vofs = shadow_vofs + bg_y_offset;

    if (width > SCREENWIDTH) width = SCREENWIDTH;
    if (height > SCREENHEIGHT) height = SCREENHEIGHT;

    bg_x_offset = (256 - width) >> 1;
    bg_y_offset = (192 - height) >> 1;
    viewport_width = width;
    viewport_height = height;

	windowSetBoundsSub(WINDOW_1, bg_x_offset, bg_y_offset, bg_x_offset + width - 1, bg_y_offset + height - 1);

    shadow_hofs = hofs - bg_x_offset;
    shadow_vofs = vofs - bg_y_offset;
	REG_BG0HOFS_SUB = shadow_hofs;
	REG_BG0VOFS_SUB = shadow_vofs;

    gbsa_window_set_pos(win_x_pos, win_y_pos);
}

void gbsa_exit(void) {

}

static inline uint16_t gbsa_gbc_to_nds_color(uint16_t v) {
    return v | 0x8000;
}

void gbsa_palette_set_bkg(int idx, const uint16_t *data) {
#ifdef CGB
    BG_PALETTE_SUB[(idx << 4) | 0] = gbsa_gbc_to_nds_color(data[0]);
    BG_PALETTE_SUB[(idx << 4) | 1] = gbsa_gbc_to_nds_color(data[1]);
    BG_PALETTE_SUB[(idx << 4) | 2] = gbsa_gbc_to_nds_color(data[2]);
    BG_PALETTE_SUB[(idx << 4) | 3] = gbsa_gbc_to_nds_color(data[3]);

    // For windows
	*((u32*) (BG_PALETTE_SUB + (idx << 4) + 8)) = *((u32*) (BG_PALETTE_SUB + (idx << 4)));
	*((u32*) (BG_PALETTE_SUB + (idx << 4) + 10)) = *((u32*) (BG_PALETTE_SUB + (idx << 4) + 2));
#endif
}

void gbsa_palette_set_sprite(int idx, const uint16_t *data) {
#ifdef CGB
    SPRITE_PALETTE_SUB[(idx << 4) | 0] = gbsa_gbc_to_nds_color(data[0]);
    SPRITE_PALETTE_SUB[(idx << 4) | 1] = gbsa_gbc_to_nds_color(data[1]);
    SPRITE_PALETTE_SUB[(idx << 4) | 2] = gbsa_gbc_to_nds_color(data[2]);
    SPRITE_PALETTE_SUB[(idx << 4) | 3] = gbsa_gbc_to_nds_color(data[3]);
#endif
}

void gbsa_palette_set_bkg_dmg(uint8_t v) {
#ifndef CGB
    BG_PALETTE_SUB[0] = dmg_palette[(v >> 0) & 0x03];
    BG_PALETTE_SUB[1] = dmg_palette[(v >> 2) & 0x03];
    BG_PALETTE_SUB[2] = dmg_palette[(v >> 4) & 0x03];
    BG_PALETTE_SUB[3] = dmg_palette[(v >> 6) & 0x03];

	*((u32*) (BG_PALETTE_SUB + 8)) = *((u32*) (BG_PALETTE_SUB));
	*((u32*) (BG_PALETTE_SUB + 10)) = *((u32*) (BG_PALETTE_SUB + 2));
#endif
}

void gbsa_palette_set_sprite_dmg(uint8_t idx, uint8_t v) {
#ifndef CGB
    idx <<= 4;

    SPRITE_PALETTE_SUB[idx | 0] = dmg_palette[(v >> 0) & 0x03];
    SPRITE_PALETTE_SUB[idx | 1] = dmg_palette[(v >> 2) & 0x03];
    SPRITE_PALETTE_SUB[idx | 2] = dmg_palette[(v >> 4) & 0x03];
    SPRITE_PALETTE_SUB[idx | 3] = dmg_palette[(v >> 6) & 0x03];
#endif
}

void gbsa_window_set_pos(int x, int y) {
    win_x_pos = x;
    win_y_pos = y;
    if (y > MAXWNDPOSY) {
        REG_DISPCNT_SUB &= ~DISPLAY_WIN0_ON;
    } else {
        int win_x_offset = bg_x_offset + ((viewport_width - WINDOWWIDTH) >> 1);
        int win_y_offset = bg_y_offset + (viewport_height - WINDOWHEIGHT);

		windowSetBoundsSub(WINDOW_0,
        	win_x_offset + x - 7,
			win_y_offset + y,
        	win_x_offset + WINDOWWIDTH - 1,
        	win_y_offset + WINDOWHEIGHT - 1);

        REG_BG1HOFS_SUB = -(x - 7 + win_x_offset);
        REG_BG1VOFS_SUB = -(y + win_y_offset);

        REG_DISPCNT_SUB |= DISPLAY_WIN0_ON;
    }
}

static const uint8_t fade_levels[6] = {16, 12, 9, 6, 3, 0};

void gbsa_fx_fade(uint8_t to_white, uint8_t level /* 5 - 0; 5 - least, 0 - most */, uint8_t frame, uint8_t frame_ctr, uint8_t going_down) {
#ifdef FEAT_SMOOTH_FADES
    int max_level = 4 * frame_ctr;
    int curr_level = going_down ? (level * frame_ctr - frame) : ((level - 1) * frame_ctr + frame);
    if (curr_level < 0) curr_level = 0;
    else if (curr_level > max_level) curr_level = max_level;
    REG_BLDY_SUB = 16 - ((curr_level << 4) / max_level);
    debug_printf(LOG_DEBUG, "smoothfade: new level %d/%d (%d; %d, %d/%d, %d)", curr_level, max_level, REG_BLDY, level, frame, frame_ctr, going_down);
#else
    REG_BLDY_SUB = fade_levels[level > 5 ? 5 : level];
#endif
    if (level == 5) {
        REG_BLDCNT_SUB = 0;
    } else {
        REG_BLDCNT_SUB = 0x3F | (to_white ? 0x80 : 0xC0);
    }
}

void gbsa_sprite_move(int id, int x, int y) {
	OAMRawEntry *obj_mem = (OAMRawEntry*) OAM_SUB;
    if (x <= -8 || y <= -16) {
        obj_mem[id].attr0 |= 0x0200;	
    } else {
        obj_mem[id].attr0 &= ~0x0200;	
    }
	obj_mem[id].attr0 = (obj_mem[id].attr0 & 0xFF00) | ((y + bg_y_offset) & 0x0FF);
	obj_mem[id].attr1 = (obj_mem[id].attr1 & 0xFE00) | ((x + bg_x_offset) & 0x1FF);
}

void gbsa_sprite_set_data(uint8_t id, const uint8_t *data) {
    int long_id = id;
    u32 *ptr = (u32*) (MEM_OBJ_VRAM + (long_id << 5));

    for (int i = 0; i < 8; i++, ptr++, data += 2) {
        uint32_t ones = lut_expand_8_32[data[0]];
        uint32_t twos = lut_expand_8_32[data[1]] << 1;
        *ptr = ones | twos;
    }	
}

void gbsa_sprite_set_tile(int id, uint16_t idx) {
	OAMRawEntry *obj_mem = (OAMRawEntry*) OAM_SUB;
    obj_mem[id].attr0 = (obj_mem[id].attr0 & 0x3FF) | 0x8000;
    obj_mem[id].attr2 = (obj_mem[id].attr2 & 0xFC00) | (idx & 0x3FF);
}

void gbsa_sprite_set_props(int id, uint16_t props) {
	OAMRawEntry *obj_mem = (OAMRawEntry*) OAM_SUB;
    obj_mem[id].attr1 = (obj_mem[id].attr1 & 0x03FF)
        | ((props & S_FLIPX) ? 0x1000 : 0)
        | ((props & S_FLIPY) ? 0x2000 : 0);
#ifdef CGB
    obj_mem[id].attr2 = (obj_mem[id].attr2 & 0x0FFF) | ((props & 0x07) << 12);
#else
    obj_mem[id].attr2 = (obj_mem[id].attr2 & 0x0FFF) | ((props & 0x10) << 8);
#endif
}

void gbsa_sprite_set_enabled(uint8_t enabled) {
    if (enabled) {
        REG_DISPCNT_SUB |= DISPLAY_SPR_ACTIVE;
    } else {
        REG_DISPCNT_SUB &= ~DISPLAY_SPR_ACTIVE;
    }
}

void gbsa_intr_wait_vbl(void) {
    swiWaitForVBlank();
}

uint16_t gbsa_joypad_get(void) {
	scanKeys();
    uint16_t input = keysHeld();
    return (
        ((input & KEY_A) ? J_A : 0)
        | ((input & KEY_B) ? J_B : 0)
        | ((input & KEY_SELECT) ? J_SELECT : 0)
        | ((input & KEY_START) ? J_START : 0)
        | ((input & KEY_UP) ? J_UP : 0)
        | ((input & KEY_LEFT) ? J_LEFT : 0)
        | ((input & KEY_RIGHT) ? J_RIGHT : 0)
        | ((input & KEY_DOWN) ? J_DOWN : 0)
    );
}

void gbsa_intr_add_handler(uint8_t intr_id, int_handler handler) {
    if (intr_id == VBL_IFLAG) {
        irqSet(IRQ_VBLANK, handler);
		irqEnable(IRQ_VBLANK);
    } else if (intr_id == TIM_IFLAG) {
		// TODO sound
        /* REG_TM1CNT_L = 65536 - 272;
        REG_TM1CNT_H = TM_FREQ_1024 | TM_IRQ | TM_ENABLE;
		irqSet(IRQ_TIMER1, handler);
        irqEnable(IRQ_TIMER1); */
    }
}

void gbsa_intr_set_mask(uint8_t mask) {
    // TODO
}

void gbsa_intr_set_enabled(uint8_t value) {
    // TODO
}

void gbsa_map_set_bg_scroll(int x, int y) {
    shadow_hofs = x - bg_x_offset;
    shadow_vofs = y - bg_y_offset;
	REG_BG0HOFS_SUB = shadow_hofs;
	REG_BG0VOFS_SUB = shadow_vofs;
}

void gbsa_tile_set_data(uint8_t id, const uint8_t *data) {
    int long_id = id;
    u32 *ptr = (u32*) (MEM_VRAM + (long_id << 5));
    u32 *wptr = (u32*) (MEM_VRAM + (long_id << 5) + (1 << 13));

    for (int i = 0; i < 8; i++, ptr++, wptr++, data += 2) {
        uint32_t ones = lut_expand_8_32[data[0]];
        uint32_t twos = lut_expand_8_32[data[1]] << 1;
        *ptr = ones | twos;
        *wptr = (ones | twos) + 0x88888888;
    }	
}

void gbsa_map_set_bg_tile(uint8_t bg_id, uint8_t x, uint8_t y, uint16_t id) {
#ifdef LAYER_PRIORITY_EMULATION
    uint32_t curr_layer_offset = ((cgb_layer[y] >> x) & 0x1) << 12;
#else
    uint32_t curr_layer_offset = 0;
#endif

    u16 *ptr;
    if (bg_id) {
        ptr = (u16*) (MEM_VRAM + 0xA000 + curr_layer_offset + (y << 6) + (x << 1));
    } else {
        ptr = (u16*) (MEM_VRAM + 0x8000 + curr_layer_offset + (y << 6) + ((x & 0x1F) << 1) + ((x & 0x20) << 6));
    }
    *ptr = ((*ptr) & 0xFC00) | (id & 0x00FF) | (bg_id ? 0x100 : 0);
}

void gbsa_map_set_bg_attr(uint8_t bg_id, uint8_t x, uint8_t y, uint16_t id) {
#ifdef LAYER_PRIORITY_EMULATION
    uint32_t curr_layer_offset = ((cgb_layer[y] >> x) & 0x1) << 12;
    uint32_t new_layer_offset = (id & 0x80) ? 0x1000 : 0;
#else
    uint32_t curr_layer_offset = 0;
    uint32_t new_layer_offset = 0;
#endif

    u16 *ptr, *dst_ptr;
    if (bg_id) {
        ptr = (u16*) (MEM_VRAM + 0xA000 + curr_layer_offset + (y << 6) + (x << 1));
        dst_ptr = (u16*) (MEM_VRAM + 0xA000 + new_layer_offset + (y << 6) + (x << 1));
    } else {
        ptr = (u16*) (MEM_VRAM + 0x8000 + curr_layer_offset + (y << 6) + ((x & 0x1F) << 1) + ((x & 0x20) << 6));
        dst_ptr = (u16*) (MEM_VRAM + 0x8000 + new_layer_offset + (y << 6) + ((x & 0x1F) << 1) + ((x & 0x20) << 6));
    }

    // write to dst layer
    *dst_ptr = ((*ptr) & 0x03FF) | ((id & 0x07) << 12) | ((id & S_FLIPX) ? 0x0400 : 0) | ((id & S_FLIPY) ? 0x0800 : 0);
#ifdef LAYER_PRIORITY_EMULATION
    if (new_layer_offset) {
        cgb_layer[y] |= (1 << x);
    } else {
        cgb_layer[y] &= ~(1 << x);
        if (curr_layer_offset) {
            *ptr = 0x3FE;
        }
    }
#endif
}

void gbsa_sram_enable(void) {
    // no-op
}

void gbsa_sram_disable(void) {
    // no-op
}

uint8_t *gbsa_sram_get_ptr(uint32_t offset) { 
	// TODO
	return NULL;
    // return sram_mem + ((offset - 0xA000) & (SRAM_SIZE - 1));
}
