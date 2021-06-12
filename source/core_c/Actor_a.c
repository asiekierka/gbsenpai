#include "Actor.h"
#include "GameTime.h"
#include "ScriptRunner.h"
#include "Scroll.h"

void UpdateActors() {
  UBYTE i, k, a, flip, frame;
  UBYTE fo = 0;
  UINT16 screen_x;
  UINT16 screen_y;
  Actor* actor;
  k = 0;
  actors_active_delete_count = 0;
  for (i = 0; i != actors_active_size; i++) {
    a = actors_active[i];
    actor = &actors[a];
    k = actors[a].sprite_index;
    flip = FALSE;
    fo = 0;
    if (!actor->enabled) {
      move_sprite(k, 0, 0);
      move_sprite(k + 1, 0, 0);
      continue;
    }
    if (actor->pinned) {
      screen_x = 8u + actor->pos.x;
      screen_y = 8u + actor->pos.y;
    } else {
      screen_x = 8u + actor->pos.x - scroll_x;
      screen_y = 8u + actor->pos.y - scroll_y;
    }
    // Update animation frames
    if (IS_FRAME_8 &&
        (((actors[a].moving) && actors[a].sprite_type != SPRITE_STATIC) || actors[a].animate)) {
      if (actors[a].anim_speed == 4 || (actors[a].anim_speed == 3 && IS_FRAME_16) ||
          (actors[a].anim_speed == 2 && IS_FRAME_32) ||
          (actors[a].anim_speed == 1 && IS_FRAME_64) ||
          (actors[a].anim_speed == 0 && IS_FRAME_128)) {
        actors[a].frame++;
      }
      if (actors[a].frame == actors[a].frames_len) {
        actors[a].frame = 0;
      }
      actor->rerender = TRUE;
    }
    // Rerender actors
    if (actor->rerender) {
      if (actor->sprite_type != SPRITE_STATIC) {
        // Increase frame based on facing direction
        if (IS_NEG(actor->dir.y)) {
          fo = 1 + (actor->sprite_type == SPRITE_ACTOR_ANIMATED);
        } else if (actor->dir.y == 0 && actor->dir.x != 0) {
          fo = 2 + MUL_2(actor->sprite_type == SPRITE_ACTOR_ANIMATED);
        }
        // Facing left so flip sprite
        if (IS_NEG(actor->dir.x)) {
          flip = TRUE;
        }
      }
      frame = MUL_4(actor->sprite + actor->frame + fo);
      if (flip) {
#ifdef CGB
        set_sprite_prop(k, actor->palette_index | S_FLIPX);
        set_sprite_prop(k + 1, actor->palette_index | S_FLIPX);
#else
        set_sprite_prop(k, S_FLIPX);
        set_sprite_prop(k + 1, S_FLIPX);
#endif
        set_sprite_tile(k, frame + 2);
        set_sprite_tile(k + 1, frame);
      } else {
#ifdef CGB
        set_sprite_prop(k, actor->palette_index);
        set_sprite_prop(k + 1, actor->palette_index);
#else
        set_sprite_prop(k, 0);
        set_sprite_prop(k + 1, 0);
#endif
        set_sprite_tile(k, frame);
        set_sprite_tile(k + 1, frame + 2);
      }
      actor->rerender = FALSE;
    }
    // Hide sprites that are under menus
    // Actors occluded by text boxes are handled by lcd_update instead
	// TODO GBSA
    /* if ((WX_REG != WIN_LEFT_X) && screen_x > WX_REG && screen_y - 8 > WY_REG) {
      move_sprite(k, 0, 0);
      move_sprite(k + 1, 0, 0);
    } else */ {
      // Display sprite at screen x/y coordinate
      move_sprite(k, screen_x, screen_y);
      move_sprite(k + 1, screen_x + 8, screen_y);
    }
    // Check if actor is off screen
    if (IS_FRAME_4 && (a != script_ctxs[0].script_actor)) {
      if (((UINT16)(screen_x + 32u) >= SCREENWIDTH_PLUS_64) ||
          ((UINT16)(screen_y + 32u) >= SCREENHEIGHT_PLUS_64)) {
        // Mark off screen actor for removal
        actors_active_delete[actors_active_delete_count] = i;
        actors_active_delete_count++;
      }
    }
  }
  // Remove all offscreen actors
  for (i = 0; i != actors_active_delete_count; i++) {
    a = actors_active_delete[i];
    DeactivateActiveActor(a);
  }
}