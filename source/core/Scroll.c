#include "Scroll.h"

#include "Actor.h"
#include "BankManager.h"
#include "Core_Main.h"
#include "DataManager.h"
#include "GameTime.h"
#include "FadeManager.h"
#include "Palette.h"
#include "data_ptrs.h"
#include "shim/platform.h"

INT16 scroll_x = 0;
INT16 scroll_y = 0;
INT16 draw_scroll_x = 0;
INT16 draw_scroll_y = 0;
UINT16 scroll_x_max = 0;
UINT16 scroll_y_max = 0;

INT16 scroll_offset_x = 0;
INT16 scroll_offset_y = 0;

INT16 pending_h_x, pending_h_y;
UINT8 pending_h_i;
unsigned char* pending_h_map = 0;
unsigned char* pending_w_map = 0;
#ifdef CGB
unsigned char* pending_h_cmap = 0;
unsigned char* pending_w_cmap = 0;
#endif
INT16 pending_w_x, pending_w_y;
UINT8 pending_w_i;
Pos* scroll_target = 0;

#ifdef FEAT_FAST_LOADS
#define MAX_ROWS 32
#else
#define MAX_ROWS 5
#endif

void ScrollUpdateRow(INT16 x, INT16 y);
void RefreshScroll_b() __banked;

/* Update pending (up to 5) rows */
void ScrollUpdateRowR() {
  UINT8 i = 0u;
  UINT16 id;
  UBYTE y_offset;

  y_offset = SCREENMAPHEIGHTCLIP(pending_w_y);

  PUSH_BANK(image_bank);

#ifdef CGB
  if (_cpu == CGB_TYPE) {  // Color Row Load
    for (i = 0u; i != MAX_ROWS && pending_w_i != 0; ++i, --pending_w_i) {
      gbsa_map_set_bg_attr(BG_ID_BG, SCREENMAPWIDTHCLIP(pending_w_x), y_offset, *(pending_w_cmap++));
      gbsa_map_set_bg_tile(BG_ID_BG, SCREENMAPWIDTHCLIP(pending_w_x++), y_offset, *(pending_w_map++));
    }
  } else
#endif
  {  // DMG Row Load
    for (i = 0u; i != MAX_ROWS && pending_w_i != 0; ++i, --pending_w_i) {
      gbsa_map_set_bg_tile(BG_ID_BG, SCREENMAPWIDTHCLIP(pending_w_x++), pending_w_y, *(pending_w_map++));
    }
  }

  POP_BANK;
}

void ScrollUpdateRowWithDelay(INT16 x, INT16 y) {
  UBYTE i;

  while (pending_w_i) {
    ScrollUpdateRowR();
  }

  pending_w_x = x;
  pending_w_y = y;

  pending_w_i = SCREEN_TILE_REFRES_W;
  pending_w_map = image_ptr + image_tile_width * y + x;

  // Activate Actors in Row
  for (i = 1; i != actors_len; i++) {
    if (actors[i].pos.y >> 3 == y) {
      INT16 tx = actors[i].pos.x >> 3;
      if (U_LESS_THAN(x, tx) && U_LESS_THAN(tx, x + SCREEN_TILE_REFRES_W)) {
        ActivateActor(i);
      }
    }
  }

#ifdef CGB
  pending_w_cmap = image_attr_ptr + image_tile_width * y + x;
#endif
}

void ScrollUpdateRow(INT16 x, INT16 y) {
  UINT8 i = 0u;
  UINT16 id;
  UBYTE screen_x, screen_y;
  unsigned char* map = image_ptr + image_tile_width * y + x;
#ifdef CGB
  unsigned char* cmap = image_attr_ptr + image_tile_width * y + x;
#endif

  PUSH_BANK(image_bank);

  screen_x = x;
  screen_y = SCREENMAPHEIGHTCLIP(y);

  for (i = 0; i != SCREEN_TILE_REFRES_W; i++) {
#ifdef CGB
    gbsa_map_set_bg_attr(BG_ID_BG, SCREENMAPWIDTHCLIP(screen_x), screen_y, *(cmap++));
#endif
    gbsa_map_set_bg_tile(BG_ID_BG, SCREENMAPWIDTHCLIP(screen_x++), screen_y, *(map++));
  }

  // Activate Actors in Row
  for (i = 1; i != actors_len; i++) {
    if (actors[i].pos.y >> 3 == y) {
      INT16 tx = actors[i].pos.x >> 3;
      if (U_LESS_THAN(x, tx + 1) && U_LESS_THAN(tx, x + SCREEN_TILE_REFRES_W + 1)) {
        ActivateActor(i);
      }
    }
  }

  POP_BANK;
}

void ScrollUpdateColumnR() {
  UINT8 i = 0u;
  UBYTE a = 0;
  UINT16 id = 0;
  UBYTE x_offset;

  PUSH_BANK(image_bank);

  x_offset = SCREENMAPWIDTHCLIP(pending_h_x);

#ifdef CGB
  if (_cpu == CGB_TYPE) {  // Color Column Load
    for (i = 0u; i != MAX_ROWS && pending_h_i != 0; ++i, pending_h_i--) {
      gbsa_map_set_bg_attr(BG_ID_BG, x_offset, SCREENMAPHEIGHTCLIP(pending_h_y), *pending_h_cmap);
      gbsa_map_set_bg_tile(BG_ID_BG, x_offset, SCREENMAPHEIGHTCLIP(pending_h_y++), *pending_h_map);
      pending_h_map += image_tile_width;
      pending_h_cmap += image_tile_width;
    }
  } else
#endif
  {  // DMG Column Load
    for (i = 0u; i != MAX_ROWS && pending_h_i != 0; ++i, pending_h_i--) {
      gbsa_map_set_bg_tile(BG_ID_BG, x_offset, SCREENMAPHEIGHTCLIP(pending_h_y++), *pending_h_map);
      pending_h_map += image_tile_width;
    }
  }

  POP_BANK;
}

void ScrollUpdateColumnWithDelay(INT16 x, INT16 y) {
  UBYTE i;

  while (pending_h_i) {
    // If previous column wasn't fully rendered
    // render it now before starting next column
    ScrollUpdateColumnR();
  }

  // Activate Actors in Column
  for (i = 1; i != actors_len; i++) {
    if (actors[i].pos.x >> 3 == x) {
      INT16 ty = actors[i].pos.y >> 3;
      if (U_LESS_THAN(y, ty) && U_LESS_THAN(ty, y + SCREEN_TILE_REFRES_H)) {
        ActivateActor(i);
      }
    }
  }

  pending_h_x = x;
  pending_h_y = y;
  pending_h_i = SCREEN_TILE_REFRES_H;
  pending_h_map = image_ptr + image_tile_width * y + x;

#ifdef CGB
  pending_h_cmap = image_attr_ptr + image_tile_width * y + x;
#endif
}

void RefreshScroll() {
  RefreshScroll_b();
}

void InitScroll() {
  pending_w_i = 0;
  pending_h_i = 0;
  scroll_x = 0x7FFF;
  scroll_y = 0x7FFF;
}

void RenderScreen() {
  UINT8 i;
  INT16 y;

  if (!fade_style)
  {
	// TODO GBSA
    // DISPLAY_OFF
  } else if (!fade_timer == 0)
  {
    // Immediately set all palettes black while screen renders.
    #ifdef CGB
    if (_cpu == CGB_TYPE) {
      for (UBYTE c = 0; c != 32; ++c) {
        BkgPaletteBuffer[c] = RGB_BLACK;
      }
      set_bkg_palette(0, 8, BkgPaletteBuffer);
      set_sprite_palette(0, 8, BkgPaletteBuffer);
    } else {
    #endif
		gbsa_palette_set_sprite_dmg(0, 0xFF);
		gbsa_palette_set_bkg_dmg(0xFF);
	#ifdef CGB
	}
	#endif
  }

  // Clear pending rows/ columns
  pending_w_i = 0;
  pending_h_i = 0;

  PUSH_BANK(image_bank);
  y = scroll_y >> 3;
  for (i = 0u; i != (SCREEN_TILE_REFRES_H) && y != image_height; ++i, y++) {
    ScrollUpdateRow((scroll_x >> 3) - SCREEN_PAD_LEFT, y - SCREEN_PAD_TOP);
  }
  POP_BANK;

  game_time = 0;

	// TODO GBSA
  // DISPLAY_ON;
  if (!fade_timer == 0) {
    // Screen palate to nornmal if not fading
    ApplyPaletteChange();
  }
}
