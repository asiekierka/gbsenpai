#include "Core_Main.h"

#include "shim/gb_shim.h"
#include <string.h>
#include <stdlib.h> // GBSA - rand.h

#ifdef __GBA__
#include <tonc_memmap.h>
#endif

#include "Actor.h"
#include "BankManager.h"
#include "Camera.h"
#include "DataManager.h"
#include "FadeManager.h"
#include "GameTime.h"
#include "Input.h"
#include "MusicManager.h"
#include "Palette.h"
#include "Projectiles.h"
#include "ScriptRunner.h"
#include "Scroll.h"
#include "Sprite.h"
#include "UI.h"
#include "gbt_player.h"
#include "data_ptrs.h"
#include "main.h"
#include "shim/platform.h"

UBYTE game_time = 0;
UBYTE seedRand = 2;
UINT16 next_state;
UINT8 delta_time;
UINT16 current_state;
UINT8 state_running = 0;
UINT8 vbl_count = 0;
INT16 old_scroll_x, old_scroll_y;
UINT8 music_mute_frames = 0;

extern SCENE_STATE scene_stack;

void SetScene(UINT16 state) {
  state_running = 0;
  next_state = state;
}

void vbl_update() {
  vbl_count++;

  // Update background scroll in vbl
  // interupt to prevent tearing
  gbsa_map_set_bg_scroll(draw_scroll_x, draw_scroll_y);
  
#ifdef CGB
  if (palette_dirty) {
    set_bkg_palette(0, 8, BkgPaletteBuffer);
    set_sprite_palette(0, 8, SprPaletteBuffer);
    palette_dirty = FALSE;
  }
#endif

  if (music_mute_frames != 0) {
    music_mute_frames--;
    if (music_mute_frames == 0) {
      gbt_enable_channels(0xF);
    }
  }

  if (!hide_sprites) {
    gbsa_sprite_set_enabled(1);
  }
}

void lcd_update() {
    // TODO GBSA
  /* if (LYC_REG == 0x0) {
    if(WY_REG == 0x0) {
      HIDE_SPRITES;
    }
    
    // If UI is open cause lcd interupt
    // to fire again when first line of
    // window is being drawn
    if (WY_REG != MENU_CLOSED_Y) {
      LYC_REG = WY_REG;
    }
  } else if (WX_REG == WIN_LEFT_X) {
    // If window is covering entire scan line
    // can just hide all sprites until next frame
    HIDE_SPRITES;
    LYC_REG = 0x0;
  } */
}

int core_start() {
  // TODO GBSA

  // display_off();

  // Init LCD
  // LCDC_REG = 0x67;

  // Set interupt handlers
  add_VBL(vbl_update);
  add_TIM(MusicUpdate);
  // add_LCD(lcd_update);

#ifdef CGB
  // TMA_REG = _cpu == CGB_TYPE ? 120U : 0xBCU;
#else
  // TMA_REG = 0xBCU;
#endif
  // TAC_REG = 0x04U;

  // LYC_REG = 0x0;  // LCD interupt pos

  set_interrupts(VBL_IFLAG | TIM_IFLAG | LCD_IFLAG);
  enable_interrupts();

  // STAT_REG = 0x45;

  // Set palettes
  gbsa_palette_set_bkg_dmg(0xE4);
  gbsa_palette_set_sprite_dmg(0, 0xE4);
  gbsa_palette_set_sprite_dmg(1, 0xD2);

  gbsa_map_set_bg_scroll(0, 0);

  // Position Window Layer
  gbsa_window_set_pos(7, MAXWNDPOSY + 1);

  // Initialize structures
  memset(&script_variables, 0, sizeof(script_variables));

  memset(&input_script_ptrs, 0, sizeof(input_script_ptrs));

  memset(&scene_stack, 0, sizeof(scene_stack));
  memset(&script_cmd_args, 0, sizeof(script_cmd_args));
  memset(&script_stack, 0, sizeof(script_stack));
  memset(&script_bank_stack, 0, sizeof(script_bank_stack));
  memset(&script_start_stack, 0, sizeof(script_start_stack));

  memset(&actors, 0, sizeof(actors));
  memset(&active_script_ctx, 0, sizeof(active_script_ctx));
  memset(&script_ctxs, 0, sizeof(script_ctxs));

  memset(&SprPalette, 0, sizeof(SprPalette));
  memset(&BkgPalette, 0, sizeof(BkgPalette));
  memset(&SprPaletteBuffer, 0, sizeof(SprPaletteBuffer));
  memset(&BkgPaletteBuffer, 0, sizeof(BkgPaletteBuffer));

  // Initialise Player
  player.sprite = 0;
  player.moving = TRUE;
  player.frame = 0;
  player.frames_len = 2;
  map_next_pos.x = start_scene_x;
  map_next_pos.y = start_scene_y;
  map_next_dir.x = player.dir.x = start_scene_dir_x;
  map_next_dir.y = player.dir.y = start_scene_dir_y;
  map_next_sprite = start_player_sprite;
  player.enabled = TRUE;
  player.move_speed = start_player_move_speed;
  player.anim_speed = start_player_anim_speed;

  state_running = 0;
  next_state = start_scene_index;
  game_time = 0;
  scene_type = 0;

  UIInit();
  FadeInit();
  ScriptRunnerInit();
  ActorsInit();

  while (1) {
    while (state_running) {
    /* Game Core Loop Start *********************************/

    if (!vbl_count) {
      wait_vbl_done();
    }

    gbsa_profile_push("vblank");

    delta_time = vbl_count == 1u ? 0u : 1u;
    vbl_count = 0;

    last_joy = joy;
    joy = joypad();
    if ((joy & INPUT_DPAD) != (last_joy & INPUT_DPAD)) {
      recent_joy = joy & ~last_joy;
    }

    if (seedRand) {
      if(seedRand == 2){
        // Seed on first button press
        if (joy) {
          seedRand--;
#if defined(__GBA__)
          srand(((REG_TM1CNT_L << 8) & 0x1FF00)+game_time);
#else
          srand(game_time);
#endif
        }
      } else {
        // Seed on first button release
          if (!joy) {
          seedRand = FALSE;
#if defined(__GBA__)
          srand(rand()+((REG_TM1CNT_L << 8) & 0x1FF00)+game_time);
#else
          srand(rand()+game_time);
#endif
        }
      }
    }

    PUSH_BANK(1);

    UpdateCamera();
    RefreshScroll_b();
    UpdateActors();
    UpdateProjectiles_b();
    UIOnInteract_b();
    UIUpdate_b();

    if (!script_ctxs[0].script_ptr_bank && !ui_block) {
      PUSH_BANK(stateBanks[scene_type]);
      updateFuncs[scene_type]();
      POP_BANK;
      HandleInputScripts();
    }

    ScriptTimerUpdate();

    ScriptRestoreCtx(0);

    if (!ui_block) {
      // Run background scripts
      ScriptRestoreCtx(1);
      ScriptRestoreCtx(2);
      ScriptRestoreCtx(3);
      ScriptRestoreCtx(4);
      ScriptRestoreCtx(5);
      ScriptRestoreCtx(6);
      ScriptRestoreCtx(7);
      ScriptRestoreCtx(8);
      ScriptRestoreCtx(9);
      ScriptRestoreCtx(10);
      ScriptRestoreCtx(11);

      // Reposition actors and check for collisions
      ActorRunCollisionScripts();
    }

    game_time++;

    POP_BANK;

    gbsa_profile_pop();

    /* Game Core Loop End ***********************************/
  }


    //  Switch Scene

    // Fade out current scene
    FadeOut();
    while (fade_running) {
      wait_vbl_done();
      FadeUpdate();
    }

    if (!fade_style)
    {
      // DISPLAY_OFF
    }

    state_running = 1;
    current_state = next_state;

    // Reset scroll target and camera
    scroll_target = 0;
    scroll_target = &camera_pos;
    camera_settings = CAMERA_LOCK_FLAG;

    // Disable timer script
    timer_script_duration = 0;

    // Configured in platform
    gbsa_palette_set_bkg_dmg(PAL_DEF(0U, 1U, 2U, 3U));
    gbsa_palette_set_sprite_dmg(0, PAL_DEF(0U, 0U, 1U, 3U));
    gbsa_palette_set_sprite_dmg(1, PAL_DEF(0U, 0U, 1U, 3U));

    // Force Clear Emote
    move_sprite(0, 0, 0);
    move_sprite(1, 0, 0);
    // Force Clear invoke stack
    script_stack_ptr = 0;
    // Force all palettes to update on switch
    #ifdef CGB
      palette_update_mask = 0x3F;
    #endif

    UIInit();
    LoadScene(current_state);

    // Run scene type init function
    PUSH_BANK(stateBanks[scene_type]);
    startFuncs[scene_type]();
    POP_BANK;

    game_time = 0;
    old_scroll_x = scroll_x;
    old_scroll_y = scroll_y;

    // Fade in new scene
    // DISPLAY_ON;
    FadeIn();

    // Run scene init script
    ScriptStart(&scene_events_start_ptr);
    ScriptRestoreCtx(0);

    UpdateCamera();
    RefreshScroll();
    UpdateActors();
    UIUpdate();

    // Wait for fade in to complete
    while (fade_running) {
      wait_vbl_done();
      FadeUpdate();
    }
  }
}
