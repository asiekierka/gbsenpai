#pragma bank 1

#include "FadeManager.h"
#include "shim/gb_shim.h"
#include <string.h>
#include "Palette.h"
#include "Math.h"
#include "data_ptrs.h"

// GBSA: Heavily modified

static UBYTE fade_frame;
static FADE_DIRECTION fade_direction;

void ApplyPaletteChange_b() __banked {
#ifdef CGB
  memcpy(BkgPaletteBuffer, BkgPalette, 64);
  memcpy(SprPaletteBuffer, SprPalette, 64);
  palette_dirty = TRUE;
#endif

#ifdef FEAT_SMOOTH_FADES
  gbsa_fx_fade(fade_style, fade_timer, fade_frame & fade_frames_per_step, fade_frames_per_step + 1, fade_direction == FADE_OUT);
#else
  gbsa_fx_fade(fade_style, fade_timer, 0, 1, 0);
#endif
}

void FadeIn_b() __banked {
  fade_frame = 0;
  fade_direction = FADE_IN;
  fade_running = TRUE;
  fade_timer = 0;
  ApplyPaletteChange_b();
}

void FadeOut_b() __banked {
  fade_frame = 0;
  fade_direction = FADE_OUT;
  fade_running = TRUE;
  fade_timer = 5;
  ApplyPaletteChange_b();
}

void FadeUpdate_b()  __banked {
  if (fade_running) {
    if ((fade_frame & fade_frames_per_step) == 0) {
      if (fade_direction == FADE_IN) {
        fade_timer++;
        if (fade_timer == 5) {
          fade_running = FALSE;
        }
      } else {
        fade_timer--;
        if (fade_timer == 0) {
          fade_running = FALSE;
        }
      }
#ifndef FEAT_SMOOTH_FADES
      ApplyPaletteChange_b();
#endif
	}
#ifdef FEAT_SMOOTH_FADES
    ApplyPaletteChange_b();
#endif
    fade_frame++;
  }
}
