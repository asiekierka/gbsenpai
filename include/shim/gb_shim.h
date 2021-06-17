#ifndef __GB_SHIM_H__
#define __GB_SHIM_H__

#include <stddef.h>
#include <stdint.h>

#include "shim/platform.h"

// More important constants.

#define VBL_IFLAG	0x01
#define LCD_IFLAG	0x02
#define TIM_IFLAG	0x04
#define SIO_IFLAG	0x08
#define JOY_IFLAG	0x10

// GBDK functions.

#ifdef CGB
#define RGB_BLACK 0
#endif

extern uint8_t VBK_REG;

typedef void (*int_handler)(void);

static inline void add_VBL(int_handler handler) { gbsa_intr_add_handler(VBL_IFLAG, handler); }
static inline void add_LCD(int_handler handler) { gbsa_intr_add_handler(LCD_IFLAG, handler); }
static inline void add_TIM(int_handler handler) { gbsa_intr_add_handler(TIM_IFLAG, handler); }
static inline void add_SIO(int_handler handler) { gbsa_intr_add_handler(SIO_IFLAG, handler); }
static inline void add_JOY(int_handler handler) { gbsa_intr_add_handler(JOY_IFLAG, handler); }
static inline void set_interrupts(uint8_t mask) { gbsa_intr_set_mask(mask); }
static inline void enable_interrupts(void) { gbsa_intr_set_enabled(1); }
static inline void disable_interrupts(void) { gbsa_intr_set_enabled(0); }
static inline void wait_vbl_done(void) { gbsa_intr_wait_vbl(); }

static inline uint16_t joypad() { return gbsa_joypad_get(); }

#ifdef CGB
void set_bkg_palette(int first, int count, const uint16_t *data);
void set_sprite_palette(int first, int count, const uint16_t *data);
#endif

void set_bkg_data(int first, int count, const uint8_t *data);
void move_sprite(int id, int x, int y);
void set_sprite_data(int first, int count, const uint8_t *data);
void set_sprite_tile(int id, int idx);
void set_sprite_prop(int id, int props);
void fill_win_rect(int x, int y, int width, int height, int id);
void set_win_tiles(int x, int y, int width, int height, const uint8_t *tiles);

// Hacks.

#ifdef CGB
#define _cpu 1
#else
#define _cpu 0
#endif

#define CGB_TYPE 1

#define SWITCH_ROM_MBC1
#define ENABLE_RAM_MBC5 gbsa_sram_enable()
#define DISABLE_RAM_MBC5 gbsa_sram_disable()

// Constants.

#define S_FLIPX	0x20
#define S_FLIPY	0x40

#define J_RIGHT		(1 << 0)
#define J_LEFT		(1 << 1)
#define J_UP		(1 << 2)
#define J_DOWN		(1 << 3)
#define J_A			(1 << 4)
#define J_B			(1 << 5)
#define J_SELECT	(1 << 6)
#define J_START		(1 << 7)

#ifdef __GBA__
#ifdef FEAT_WIDESCREEN
#ifdef FEAT_WIDESCREEN_240
#define SCREENTILEWIDTH		30
#define SCREENTILEHEIGHT 	20
#define SCREENMAPWIDTHSHIFT 6
#else
#define SCREENTILEWIDTH		28
#define SCREENTILEHEIGHT 	18
#define SCREENMAPWIDTHSHIFT 5
#endif
#else
#define SCREENTILEWIDTH		20
#define SCREENTILEHEIGHT 	18
#define SCREENMAPWIDTHSHIFT 5
#endif
#elif defined(__NDS__)
#ifdef FEAT_WIDESCREEN
#define SCREENTILEWIDTH		32
#define SCREENTILEHEIGHT 	24
#define SCREENMAPWIDTHSHIFT 6
#else
#define SCREENTILEWIDTH		20
#define SCREENTILEHEIGHT 	18
#define SCREENMAPWIDTHSHIFT 5
#endif
#endif
#define SCREENMAPHEIGHTSHIFT 5
#define SCREENMAPWIDTH (1 << SCREENMAPWIDTHSHIFT)
#define SCREENMAPWIDTHMASK (SCREENMAPWIDTH - 1)
#define SCREENMAPWIDTHCLIP(x) ((x) & SCREENMAPWIDTHMASK)
#define SCREENMAPHEIGHT (1 << SCREENMAPHEIGHTSHIFT)
#define SCREENMAPHEIGHTMASK (SCREENMAPHEIGHT - 1)
#define SCREENMAPHEIGHTCLIP(x) ((x) & SCREENMAPHEIGHTMASK)
#define SCREENWIDTH		(SCREENTILEWIDTH << 3)
#define SCREENHEIGHT	(SCREENTILEHEIGHT << 3)
// Making these two larger breaks the logo screen effect.
#define WINDOWTILEWIDTH		20
#define WINDOWTILEHEIGHT	18
#define WINDOWWIDTH		(WINDOWTILEWIDTH << 3)
#define WINDOWHEIGHT	(WINDOWTILEHEIGHT << 3)

#define MINWNDPOSX		7
#define MINWNDPOSY		0
#define MAXWNDPOSX		(WINDOWWIDTH + MINWNDPOSX - 1)
#define MAXWNDPOSY		(WINDOWHEIGHT + MINWNDPOSY - 1)

// Remove attributes unused on modern platforms.

#define __banked
#define __naked
#define __preserves_regs(...)

// Types.

#define FALSE 0
#define TRUE 1

typedef signed char BYTE;
typedef signed short WORD;
typedef unsigned char UBYTE;
typedef unsigned short UWORD;
typedef signed char INT8;
typedef signed short INT16;
typedef unsigned char UINT8;
typedef unsigned short UINT16;

#endif /* __GB_SHIM_H__ */
