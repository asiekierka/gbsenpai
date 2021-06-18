#ifndef __SHIM_PLATFORM_H__
#define __SHIM_PLATFORM_H__

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

typedef void (*gbsa_int_handler)(void);

#define BG_ID_BG 0
#define BG_ID_WINDOW 1

#ifdef DEBUG_PROFILE
void gbsa_profile_push(const char *name);
void gbsa_profile_pop(void);
#else
static inline void gbsa_profile_push(const char *name) { }
static inline void gbsa_profile_pop(void) { }
#endif

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
void gbsa_map_set_bg_scroll(int x, int y);

void gbsa_update_viewport_size(int width, int height);
void gbsa_window_set_pos(int x, int y);

void gbsa_sprite_set_enabled(uint8_t enabled);
void gbsa_sprite_move(int id, int x, int y);
void gbsa_sprite_set_data(uint8_t id, const uint8_t *data);
void gbsa_sprite_set_tile(int id, uint16_t idx);
void gbsa_sprite_set_props(int id, uint16_t props);

void gbsa_sram_enable(void);
void gbsa_sram_disable(void);
uint8_t *gbsa_sram_get_ptr(uint32_t offset);

// Sound

enum sound_update_mode {
	SOUND_MODE_DISABLE,
	SOUND_MODE_UPDATE,
	SOUND_MODE_TRIGGER,
	SOUND_MODE_CH4_BEEP
};

void gbsa_sound_start(uint8_t reinit);
void gbsa_sound_pause(uint8_t paused);
void gbsa_sound_stop(void);
void gbsa_sound_channel1_update(uint8_t mode, uint8_t instr, uint8_t vol, uint16_t freq);
void gbsa_sound_channel2_update(uint8_t mode, uint8_t instr, uint8_t vol, uint16_t freq);
void gbsa_sound_channel3_load_instrument(const uint8_t *instr);
void gbsa_sound_channel3_update(uint8_t mode, uint8_t vol, uint16_t freq);
void gbsa_sound_channel4_update(uint8_t mode, uint8_t instr, uint8_t vol);
uint8_t gbsa_sound_pan_get(void);
void gbsa_sound_pan_update(uint8_t pan);

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
#define debug_printf(level, format, ...) \
	{ \
		iprintf((format), ##__VA_ARGS__); \
		iprintf("\n"); \
	}
#endif
#else
#define debug_printf(level, format, ...)
#endif

#endif /* __SHIM_PLATFORM_H__ */
