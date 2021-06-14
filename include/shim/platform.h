#ifndef __SHIM_PLATFORM_H__
#define __SHIM_PLATFORM_H__

#include <stddef.h>
#include <stdint.h>

typedef void (*gbsa_int_handler)(void);

#define BG_ID_BG 0
#define BG_ID_WINDOW 1

void gbsa_init(void);
void gbsa_exit(void);

uint16_t gbsa_joypad_get(void);
void gbsa_intr_add_handler(uint8_t intr_id, gbsa_int_handler handler);
void gbsa_intr_set_mask(uint8_t mask);
void gbsa_intr_set_enabled(uint8_t value);
void gbsa_intr_wait_vbl(void);

void gbsa_palette_set_bkg(int idx, const uint16_t *data);
void gbsa_palette_set_sprite(int idx, const uint16_t *data);
void gbsa_palette_set_bkg_dmg(uint8_t v);
void gbsa_palette_set_sprite_dmg(uint8_t idx, uint8_t v);

void gbsa_fx_fade(uint8_t to_black, uint8_t level /* 5 - 0; 5 - least, 0 - most */, uint8_t frame, uint8_t frame_ctr, uint8_t going_down);

void gbsa_tile_set_data(uint8_t id, const uint8_t *data);
void gbsa_map_set_bg_tile(uint8_t bg_id, uint8_t x, uint8_t y, uint16_t id);
void gbsa_map_set_bg_attr(uint8_t bg_id, uint8_t x, uint8_t y, uint16_t id);
void gbsa_map_set_bg_scroll(uint8_t x, uint8_t y);

void gbsa_update_viewport_size(int width, int height);
void gbsa_window_set_pos(uint8_t x, uint8_t y);

void gbsa_sprite_set_enabled(uint8_t enabled);
void gbsa_sprite_move(int id, int x, int y);
void gbsa_sprite_set_data(uint8_t id, const uint8_t *data);
void gbsa_sprite_set_tile(int id, uint16_t idx);
void gbsa_sprite_set_props(int id, uint16_t props);

void gbsa_sram_enable(void);
void gbsa_sram_disable(void);
uint8_t *gbsa_sram_get_ptr(uint32_t offset);

// GBA sound hack

#ifdef __GBA__
typedef volatile unsigned char vu8;

#define NR10_REG *(vu8*)(0x04000060)
#define NR11_REG *(vu8*)(0x04000062)
#define NR12_REG *(vu8*)(0x04000063)
#define NR13_REG *(vu8*)(0x04000064)
#define NR14_REG *(vu8*)(0x04000065)
#define NR21_REG *(vu8*)(0x04000068)
#define NR22_REG *(vu8*)(0x04000069)
#define NR23_REG *(vu8*)(0x0400006C)
#define NR24_REG *(vu8*)(0x0400006D)
#define NR30_REG *(vu8*)(0x04000070)
#define NR31_REG *(vu8*)(0x04000072)
#define NR32_REG *(vu8*)(0x04000073)
#define NR33_REG *(vu8*)(0x04000074)
#define NR34_REG *(vu8*)(0x04000075)
#define NR41_REG *(vu8*)(0x04000078)
#define NR42_REG *(vu8*)(0x04000079)
#define NR43_REG *(vu8*)(0x0400007C)
#define NR44_REG *(vu8*)(0x0400007D)
#define NR50_REG *(vu8*)(0x04000080)
#define NR51_REG *(vu8*)(0x04000081)
#define NR52_REG *(vu8*)(0x04000084)
#endif

// Debug code

#ifdef __GBA__
enum debug_log_level {
	LOG_FATAL = 0,
	LOG_ERROR = 1,
	LOG_WARN = 2,
	LOG_INFO = 3,
	LOG_DEBUG = 4
};
#else
enum debug_log_level {
	LOG_FATAL = 0,
	LOG_ERROR = 1,
	LOG_WARN = 2,
	LOG_INFO = 3,
	LOG_DEBUG = 4
};
#endif

#ifdef DEBUG
#ifdef __GBA__
#include "../source/shim/posprintf.h"

#define REG_DEBUG_ENABLE *(uint16_t*)(0x4FFF780)
#define REG_DEBUG_FLAGS *(uint16_t*)(0x4FFF700)
#define REG_DEBUG_STRING (char*)(0x4FFF600)

#define debug_printf(level, format, ...) \
	{ \
		posprintf(REG_DEBUG_STRING, (format), ##__VA_ARGS__); \
		REG_DEBUG_FLAGS = 0x100 | (level); \
	}
#else
#define debug_printf(level, format, ...)
#endif
#else
#define debug_printf(level, format, ...)
#endif

#endif /* __SHIM_PLATFORM_H__ */