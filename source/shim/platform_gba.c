#include <tonc.h>
#include "shim/gb_shim.h"
#include "shim/platform.h"
#include "tonc_bios.h"
#include "tonc_irq.h"
#include "tonc_memdef.h"
#include "tonc_memmap.h"
#include "tonc_oam.h"

#ifdef FEAT_BORDER
#include "border.h"
#endif

// #define LAYER_PRIORITY_EMULATION
// #define GBA_COLOR_CORRECTION

// GBSA logic

static uint16_t bg_x_offset, bg_y_offset, viewport_width, viewport_height;
static int win_x_pos = 0, win_y_pos = MAXWNDPOSY + 1;
static uint32_t lut_expand_8_32[256] = {
#include "lut_expand_8_32.inc"
};

static uint16_t dmg_palette[4] = {
    0x7BDE,
    0x5294,
    0x294A,
    0x0000
};

#ifdef LAYER_PRIORITY_EMULATION
static uint32_t cgb_layer[32] = { 0 };
#endif

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
#ifdef FEAT_WIDESCREEN_240
#define EMU_BGMAP_SIZE BG_SIZE1
#else
#define EMU_BGMAP_SIZE BG_SIZE0
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
        debug_printf(LOG_INFO, "%s: %d cycles", profile_names[profile_pos], time);
    }
}
#endif

void gbsa_init(void) {
    REG_DISPCNT = DCNT_BLANK;

    irq_init(isr_master);

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
    OAM_CLEAR();
    for (int i = 0; i < 128; i++) {
        obj_mem[i].attr0 = 0x8200;
        obj_mem[i].attr2 = 0x0C00;
        pal_bg_mem[i] = 0;
        pal_bg_mem[i | 128] = 0;
        pal_obj_mem[i] = 0;
        pal_obj_mem[i | 128] = 0;
    }

#ifdef FEAT_BORDER
    memcpy32((uint32_t*) (MEM_VRAM + 0x0C000), borderTiles, borderTilesLen >> 2);
    memcpy32((uint32_t*) (MEM_VRAM + 0x0A800), borderMap, 0x800 >> 2);
    memcpy32((uint32_t*) (pal_bg_mem + 240), borderPal, borderPalLen >> 2);
    REG_BG3CNT = BG_PRIO(0) | BG_CBB(3) | BG_SBB(21) | BG_4BPP | BG_SIZE0;
#else
    REG_BG3CNT = BG_PRIO(0) | BG_CBB(0) | BG_SBB(21) | BG_4BPP | BG_SIZE0;
#endif

#ifdef DEBUG
    REG_DEBUG_ENABLE = 0xC0DE;
    debug_printf(LOG_WARN, "Debugging enabled.");

#ifdef DEBUG_PROFILE
    REG_TM2CNT_L = 0;
    REG_TM2CNT_H = TM_FREQ_1 | TM_ENABLE;
    REG_TM3CNT_L = 0;
    REG_TM3CNT_H = TM_FREQ_1 | TM_ENABLE | TM_CASCADE;
#endif
#endif

    gbsa_update_viewport_size(SCREENWIDTH, SCREENHEIGHT);

    REG_WININ = WIN_BUILD((WIN_BG1 | WIN_BLD), (WIN_BG0 | WIN_OBJ | WIN_BG2 | WIN_BLD));
    REG_WINOUT = WIN_BUILD((WIN_BG3), (0));

    REG_SOUNDCNT_H = SDS_DMG100;
    REG_SOUNDBIAS = 0xC200; // 262.144kHz, optimal for DMG

    REG_BG0CNT = BG_PRIO(3) | BG_CBB(0) | BG_SBB(16) | BG_4BPP | EMU_BGMAP_SIZE;
    REG_BG1CNT = BG_PRIO(2) | BG_CBB(0) | BG_SBB(20) | BG_4BPP | BG_SIZE0;
    REG_BG2CNT = BG_PRIO(1) | BG_CBB(0) | BG_SBB(18) | BG_4BPP | EMU_BGMAP_SIZE;
    REG_DISPCNT = DCNT_MODE0 | DCNT_BG0 | DCNT_BG1 | DCNT_BG3 | DCNT_OBJ_1D | DCNT_WIN1;
#ifdef LAYER_PRIORITY_EMULATION
    REG_DISPCNT |= DCNT_BG2;
#endif
}

void gbsa_update_viewport_size(int width, int height) {
    int hofs = REG_BG0HOFS + bg_x_offset;
    int vofs = REG_BG0VOFS + bg_y_offset;

    if (width > SCREENWIDTH) width = SCREENWIDTH;
    if (height > SCREENHEIGHT) height = SCREENHEIGHT;

    bg_x_offset = (240 - width) >> 1;
    bg_y_offset = (160 - height) >> 1;
    viewport_width = width;
    viewport_height = height;

    REG_WIN1L = bg_x_offset;
    REG_WIN1R = bg_x_offset + width;
    REG_WIN1T = bg_y_offset;
    REG_WIN1B = bg_y_offset + height;

    REG_BG0HOFS = hofs - bg_x_offset;
    REG_BG0VOFS = vofs - bg_y_offset;

    gbsa_window_set_pos(win_x_pos, win_y_pos);
}

void gbsa_exit(void) {

}

static inline uint16_t gbsa_gbc_to_gba_color(uint16_t v) {
#ifdef GBA_COLOR_CORRECTION
    uint16_t r = (v & 0x1F);
    uint16_t g = ((v >> 5) & 0x1F);
    uint16_t b = ((v >> 10) & 0x1F);

    r = ((r * 3) >> 2) + 8;
    g = ((g * 3) >> 2) + 8;
    b = ((b * 3) >> 2) + 8;

    return ((b << 10) | (g << 5) | r);
#else
    return v;
#endif
}

void gbsa_palette_set_bkg(int idx, const uint16_t *data) {
#ifdef CGB
    pal_bg_mem[(idx << 4) | 0] = gbsa_gbc_to_gba_color(data[0]);
    pal_bg_mem[(idx << 4) | 1] = gbsa_gbc_to_gba_color(data[1]);
    pal_bg_mem[(idx << 4) | 2] = gbsa_gbc_to_gba_color(data[2]);
    pal_bg_mem[(idx << 4) | 3] = gbsa_gbc_to_gba_color(data[3]);

    // For windows
    memcpy32(pal_bg_mem + (idx << 4) + 8, pal_bg_mem + (idx << 4), 2);
#endif
}

void gbsa_palette_set_sprite(int idx, const uint16_t *data) {
#ifdef CGB
    pal_obj_mem[(idx << 4) | 0] = gbsa_gbc_to_gba_color(data[0]);
    pal_obj_mem[(idx << 4) | 1] = gbsa_gbc_to_gba_color(data[1]);
    pal_obj_mem[(idx << 4) | 2] = gbsa_gbc_to_gba_color(data[2]);
    pal_obj_mem[(idx << 4) | 3] = gbsa_gbc_to_gba_color(data[3]);
#endif
}

void gbsa_palette_set_bkg_dmg(uint8_t v) {
#ifndef CGB
    pal_bg_mem[0] = dmg_palette[(v >> 0) & 0x03];
    pal_bg_mem[1] = dmg_palette[(v >> 2) & 0x03];
    pal_bg_mem[2] = dmg_palette[(v >> 4) & 0x03];
    pal_bg_mem[3] = dmg_palette[(v >> 6) & 0x03];

    // For windows
    memcpy32(pal_bg_mem + 8, pal_bg_mem, 2);
#endif
}

void gbsa_palette_set_sprite_dmg(uint8_t idx, uint8_t v) {
#ifndef CGB
    idx <<= 4;

    pal_obj_mem[idx | 0] = dmg_palette[(v >> 0) & 0x03];
    pal_obj_mem[idx | 1] = dmg_palette[(v >> 2) & 0x03];
    pal_obj_mem[idx | 2] = dmg_palette[(v >> 4) & 0x03];
    pal_obj_mem[idx | 3] = dmg_palette[(v >> 6) & 0x03];
#endif
}

void gbsa_window_set_pos(int x, int y) {
    win_x_pos = x;
    win_y_pos = y;
    if (y > MAXWNDPOSY) {
        REG_DISPCNT &= ~DCNT_WIN0;
    } else {
        int win_x_offset = bg_x_offset + ((viewport_width - WINDOWWIDTH) >> 1);
        int win_y_offset = bg_y_offset + (viewport_height - WINDOWHEIGHT);

        REG_WIN0L = win_x_offset + x - 7;
        REG_WIN0R = win_x_offset + WINDOWWIDTH;
        REG_WIN0T = win_y_offset + y;
        REG_WIN0B = win_y_offset + WINDOWHEIGHT;

        REG_BG1HOFS = -(x - 7 + win_x_offset);
        REG_BG1VOFS = -(y + win_y_offset);

        REG_DISPCNT |= DCNT_WIN0;
    }
}

static const uint8_t fade_levels[6] = {16, 12, 9, 6, 3, 0};

void gbsa_fx_fade(uint8_t to_white, uint8_t level /* 5 - 0; 5 - least, 0 - most */, uint8_t frame, uint8_t frame_ctr, uint8_t going_down) {
#ifdef FEAT_SMOOTH_FADES
    int max_level = 4 * frame_ctr;
    int curr_level = going_down ? (level * frame_ctr - frame) : ((level - 1) * frame_ctr + frame);
    if (curr_level < 0) curr_level = 0;
    else if (curr_level > max_level) curr_level = max_level;
    REG_BLDY = 16 - ((curr_level << 4) / max_level);
    debug_printf(LOG_DEBUG, "smoothfade: new level %d/%d (%d; %d, %d/%d, %d)", curr_level, max_level, REG_BLDY, level, frame, frame_ctr, going_down);
#else
    REG_BLDY = fade_levels[level > 5 ? 5 : level];
#endif
    if (level == 5) {
        REG_BLDCNT = 0;
    } else {
        REG_BLDCNT = 0x3F | (to_white ? 0x80 : 0xC0);
    }
}

void gbsa_sprite_move(int id, int x, int y) {
    if (x <= -8 || y <= -16) {
        oam_mem[id].attr0 |= 0x0200;	
    } else {
        oam_mem[id].attr0 &= ~0x0200;	
    }
    obj_set_pos(obj_mem + id, x + bg_x_offset, y + bg_y_offset);
}

void gbsa_sprite_set_data(uint8_t id, const uint8_t *data) {
    int long_id = id;
    u32 *ptr = (u32*) (MEM_VRAM + 0x10000 + (long_id << 5));

    for (int i = 0; i < 8; i++, ptr++, data += 2) {
        uint32_t ones = lut_expand_8_32[data[0]];
        uint32_t twos = lut_expand_8_32[data[1]] << 1;
        *ptr = ones | twos;
    }	
}

void gbsa_sprite_set_tile(int id, uint16_t idx) {
    obj_mem[id].attr0 = (obj_mem[id].attr0 & 0x3FF) | 0x8000;
    obj_mem[id].attr2 = (obj_mem[id].attr2 & 0xFC00) | (idx & 0x3FF);
}

void gbsa_sprite_set_props(int id, uint16_t props) {
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
        REG_DISPCNT |= DCNT_OBJ;
    } else {
        REG_DISPCNT &= ~DCNT_OBJ;
    }
}

void gbsa_intr_wait_vbl(void) {
    VBlankIntrWait();
}

uint16_t gbsa_joypad_get(void) {
    uint16_t input = REG_KEYINPUT ^ 0xFFFF;
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
        irq_add(II_VBLANK, handler);
    } else if (intr_id == TIM_IFLAG) {
        irq_add(II_TIMER1, handler);
        REG_TM1CNT_L = 65536 - 272;
        REG_TM1CNT_H = TM_FREQ_1024 | TM_IRQ | TM_ENABLE;
    }
}

void gbsa_intr_set_mask(uint8_t mask) {
    // TODO
}

void gbsa_intr_set_enabled(uint8_t value) {
    // TODO
}

void gbsa_map_set_bg_scroll(int x, int y) {
    REG_BG0HOFS = x - bg_x_offset;
    REG_BG0VOFS = y - bg_y_offset;
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

const char sram_save_type[] = "SRAM_Vnnn\0\0\0";

uint8_t *gbsa_sram_get_ptr(uint32_t offset) { 
    return sram_mem + ((offset - 0xA000) & (SRAM_SIZE - 1));
}
