#include "MusicManager.h"

#include "BankData.h"
#include "BankManager.h"
#include "data_ptrs.h"
#include "gbt_player.h"
#include "shim/platform.h"

#define MAX_MUSIC 255

UBYTE current_index = MAX_MUSIC;
UBYTE tone_frames = 0;

void MusicPlay(UBYTE index, UBYTE loop, UBYTE return_bank) {
  UBYTE music_bank;

  if (index != current_index) {
    current_index = index;
    music_bank = ReadBankedUBYTE(DATA_PTRS_BANK, &music_banks[index]);

    PUSH_BANK(DATA_PTRS_BANK);
    gbt_play(music_tracks[index], music_bank, 7);
    gbt_loop(loop);
    POP_BANK;
    SWITCH_ROM(return_bank);
  }
}

void MusicStop(UBYTE return_bank) {
  gbt_stop();
  current_index = MAX_MUSIC;
  SWITCH_ROM(return_bank);
}

void MusicUpdate() {
  UINT8 _bank = _current_bank;
  gbt_update();
  SWITCH_ROM(_bank);

  if (tone_frames != 0) {
    tone_frames--;
    if (tone_frames == 0) {
      SoundStopTone();
    }
  }
}

void SoundPlayTone(UWORD tone, UBYTE frames) {
  tone_frames = frames;
  
  gbsa_sound_channel1_update(SOUND_MODE_DISABLE, 0, 0, 0);
  gbsa_sound_start(0);
  gbsa_sound_channel1_update(SOUND_MODE_TRIGGER, (0x0U << 6U) | 0x01U, (0x0FU << 4U) | 0x00U, tone);
  gbsa_sound_pan_update(gbsa_sound_pan_get() | 0x11);
}

void SoundStopTone() {
  gbsa_sound_channel1_update(SOUND_MODE_DISABLE, 0, 0, 0);
}

void SoundPlayBeep(UBYTE pitch) {
  gbsa_sound_channel4_update(SOUND_MODE_DISABLE, 0, 0);
  gbsa_sound_start(0);
  gbsa_sound_channel4_update(SOUND_MODE_CH4_BEEP, (0x0F << 4), (0x20 | 0x08 | pitch));
  gbsa_sound_pan_update(gbsa_sound_pan_get() | 0x88);
}

void SoundPlayCrash() {
  gbsa_sound_channel4_update(SOUND_MODE_DISABLE, 0, 0);
  gbsa_sound_start(0);
  gbsa_sound_channel4_update(SOUND_MODE_TRIGGER, (0x0F << 4) | 0x02, 0x13);
  gbsa_sound_pan_update(gbsa_sound_pan_get() | 0x88);
}
