/*
 * GBT Player v2.2.1 rulz
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2009-2020, Antonio Niño Díaz <antonio_nd@outlook.com>
 * C rewrite Copyright (c) 2021, Adrian "asie" Siekierka <kontakt@asie.pl>
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// GBSA - hooks

#include "shim/platform.h"

const uint8_t *gbsa_patch_get_bank(uint8_t id);

// end GBSA - hooks

/* typedef volatile unsigned char vu8;

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
#define NR52_REG *(vu8*)(0x04000084) */

typedef struct {
    uint8_t base_index;
    uint8_t base_index_x;
    uint8_t base_index_y;
} gbt_arpeggio_index_t;

static uint8_t gbt_playing;
static uint16_t gbt_song; // pointer to the pointer array
static uint8_t gbt_bank;
static uint8_t gbt_speed; // playing speed
static uint8_t gbt_loop_enabled;
static uint8_t gbt_ticks_elapsed;
static uint8_t gbt_current_step;
static uint8_t gbt_current_pattern;
static uint16_t gbt_current_step_data_ptr; // pointer to next step data
static uint8_t gbt_channels_enabled;
static uint8_t gbt_pan[4];
static uint8_t gbt_vol[4];
static uint8_t gbt_instr[4];
static uint16_t gbt_freq[3];
static uint8_t gbt_channel3_loaded_instrument; // 0xFF if none
static gbt_arpeggio_index_t gbt_arpeggio_freq_index[3];
static uint8_t gbt_arpeggio_enabled[3]; // 0 - disabled, 1 - arp, 2 - sweep
static uint8_t gbt_arpeggio_tick[3];
static uint8_t gbt_sweep[3]; // bit 7 is subtract mode, bit 0-6 is how much.
static uint8_t gbt_cut_note_tick[4]; // if tick == gbt_cut_note_tick, stop note.
static uint8_t gbt_have_to_stop_next_step; // last step of last pattern this is set to 1
static uint8_t gbt_update_pattern_pointers; // set to 1 by jump effects

// Extracted helpers

static inline void _gbt_skip_channel(const uint8_t **data) {
    uint8_t a = *((*data)++);
    if (a & 0x80) {
        a = *((*data)++);
        if (a & 0x80) {
            (*data)++;
        }
    } else if (a & 0x40) {
        (*data)++;
    }
}

static void gbt_update_bank1(const uint8_t **data);
static void gbt_update_effects_bank1(void);

// Transcribed ASM code

/**
 * @brief loads a pointer to pattern into gbt_current_step_data_ptr
 * 
 * @param pattern_number pattern number
 */
static void gbt_get_pattern_ptr(uint8_t pattern_number) {
    const uint8_t *bank = gbsa_patch_get_bank(gbt_bank) - 0x4000;
    uint8_t *ptr_table = (uint8_t*) (bank + gbt_song) + (pattern_number << 1);
    gbt_current_step_data_ptr = ptr_table[0] | ((ptr_table[1] << 8));
}

/**
 * @brief loads a pointer to pattern into gbt_current_step_data_ptr
 * 
 * @param pattern_number pattern number
 */
static void gbt_get_pattern_ptr_banked(uint8_t pattern_number) {
    const uint8_t *bank = gbsa_patch_get_bank(gbt_bank) - 0x4000;
    uint8_t *ptr_table = (uint8_t*) (bank + gbt_song);
    gbt_current_step_data_ptr = ptr_table[pattern_number << 1] | ((ptr_table[(pattern_number << 1) + 1] << 8));
    if (gbt_current_step_data_ptr == 0) {
        gbt_current_pattern = 0;
    }
}

void gbt_play(uint16_t offset, uint8_t bank, uint8_t speed) {
    gbt_song = offset;
    gbt_bank = bank;
    gbt_speed = speed;

    debug_printf(LOG_DEBUG, "gbt_play offset=%d, bank=%d, speed=%d", offset, bank, speed);

    gbt_get_pattern_ptr(0);

    gbt_current_step = 0;
    gbt_current_pattern = 0;
    gbt_ticks_elapsed = 0;
    gbt_loop_enabled = 0;
    gbt_have_to_stop_next_step = 0;
    gbt_update_pattern_pointers = 0;
    gbt_channel3_loaded_instrument = 0xFF;
    gbt_channels_enabled = 0x0F;

    gbt_pan[0] = 0x11; // L and R
    gbt_pan[1] = 0x22;
    gbt_pan[2] = 0x44;
    gbt_pan[3] = 0x88;

    gbt_vol[0] = 0xF0; // 100%
    gbt_vol[1] = 0xF0;
    gbt_vol[2] = 0x20;
    gbt_vol[3] = 0xF0;

    gbt_instr[0] = 0;
    gbt_instr[1] = 0;
    gbt_instr[2] = 0;
    gbt_instr[3] = 0;

    gbt_freq[0] = 0;
    gbt_freq[1] = 0;
    gbt_freq[2] = 0;

    gbt_arpeggio_enabled[0] = 0;
    gbt_arpeggio_enabled[1] = 0;
    gbt_arpeggio_enabled[2] = 0;

    gbt_cut_note_tick[0] = 0xFF;
    gbt_cut_note_tick[1] = 0xFF;
    gbt_cut_note_tick[2] = 0xFF;
    gbt_cut_note_tick[3] = 0xFF;

    gbsa_sound_start(1);

    gbt_playing = 1;
}

void gbt_pause(uint8_t unpause) {
    gbt_playing = unpause;
    gbsa_sound_pause(!unpause);
}

void gbt_loop(uint8_t loop) {
    gbt_loop_enabled = loop;
}

void gbt_stop(void) {
    gbt_playing = 0;
    gbsa_sound_stop();
}

void gbt_enable_channels(uint8_t mask) {
    gbt_channels_enabled = mask;
}

void gbt_update(void) {
    if (!gbt_playing) return;

    gbt_ticks_elapsed++;
    if (gbt_ticks_elapsed != gbt_speed) {
        gbt_update_effects_bank1();
        return;
    }
    gbt_ticks_elapsed = 0;

    // Clear tick-based effects
    gbt_arpeggio_enabled[0] = 0;
    gbt_arpeggio_enabled[1] = 0;
    gbt_arpeggio_enabled[2] = 0;

    gbt_cut_note_tick[0] = 0xFF;
    gbt_cut_note_tick[1] = 0xFF;
    gbt_cut_note_tick[2] = 0xFF;
    gbt_cut_note_tick[3] = 0xFF;

    // Update effects
    gbt_update_effects_bank1();

    // Check if last step
    if (gbt_have_to_stop_next_step) {
        gbt_stop();
        gbt_have_to_stop_next_step = 0;
        return;
    }

    if (gbt_update_pattern_pointers) {
        gbt_update_pattern_pointers = 0;
        gbt_have_to_stop_next_step = 0;

        gbt_get_pattern_ptr(gbt_current_pattern);

        // Search the step
        {
            const uint8_t *bank = gbsa_patch_get_bank(gbt_bank) - 0x4000;
            const uint8_t *data = bank + gbt_current_step_data_ptr;
            const uint8_t *data_start = data;

            // iterators = step * 4 (number of channels)
            for (int i = 0; i < (gbt_current_step << 2); i++) {
                _gbt_skip_channel(&data);
            }

            gbt_current_step_data_ptr = data - data_start;
        }
    }

    const uint8_t *bank = gbsa_patch_get_bank(gbt_bank) - 0x4000;
    const uint8_t *data = bank + gbt_current_step_data_ptr;
    const uint8_t *data_start = data;

    gbt_update_bank1(&data);

    gbt_current_step_data_ptr += (data - data_start);

    // If any effect has changed the pattern or step, update
    if (!gbt_update_pattern_pointers) {
        // Increment step
        gbt_current_step++;
        if (gbt_current_step == 64) {
            // Increment pattern
            gbt_current_step = 0;
            gbt_current_pattern++;

            gbt_get_pattern_ptr(gbt_current_pattern);

            // If pointer is 0, song has ended
            if (gbt_current_step_data_ptr == 0) {
                if (gbt_loop_enabled) {
                    // If loop is enabled, jump to pattern 0
                    gbt_current_pattern = 0;
                    gbt_get_pattern_ptr(gbt_current_pattern);
                } else {
                    // If loop is disabled, stop song
                    // Stop it next step, if not this step won't be played
                    gbt_have_to_stop_next_step = 1;
                }
            }
        }
    }
}

// bank 1 code

static const uint8_t gbt_wave[8][16] = {
    {0xA5,0xD7,0xC9,0xE1,0xBC,0x9A,0x76,0x31,0x0C,0xBA,0xDE,0x60,0x1B,0xCA,0x03,0x93}, // random :P
    {0xF0,0xE1,0xD2,0xC3,0xB4,0xA5,0x96,0x87,0x78,0x69,0x5A,0x4B,0x3C,0x2D,0x1E,0x0F},
    {0xFD,0xEC,0xDB,0xCA,0xB9,0xA8,0x97,0x86,0x79,0x68,0x57,0x46,0x35,0x24,0x13,0x02}, // little up-downs
    {0xDE,0xFE,0xDC,0xBA,0x9A,0xA9,0x87,0x77,0x88,0x87,0x65,0x56,0x54,0x32,0x10,0x12},
    {0xAB,0xCD,0xEF,0xED,0xCB,0xA0,0x12,0x3E,0xDC,0xBA,0xBC,0xDE,0xFE,0xDC,0x32,0x10}, // triangular broken
    {0xFF,0xEE,0xDD,0xCC,0xBB,0xAA,0x99,0x88,0x77,0x66,0x55,0x44,0x33,0x22,0x11,0x00}, // triangular
    {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // square 50%
    {0x79,0xBC,0xDE,0xEF,0xFF,0xEE,0xDC,0xB9,0x75,0x43,0x21,0x10,0x00,0x11,0x23,0x45} // sine
};

/*
; gbt_noise: ; Moved to Mod2GBT for better note range & performance
    ; 7 bit, can adjust with pitch C D# F# A# C
    ;.DB	0x5F,0x4E,0x3E,0x2F,0x2E,0x2C,0x1F,0x0F
    ; 15 bit
    ;.DB	0x64,0x54,0x44,0x24,0x00
    ;.DB	0x67,0x56,0x46
*/

static const uint16_t gbt_frequencies[] = {
      44,  156,  262,  363,  457,  547,  631,  710,  786,  854,  923,  986,
    1046, 1102, 1155, 1205, 1253, 1297, 1339, 1379, 1417, 1452, 1486, 1517,
    1546, 1575, 1602, 1627, 1650, 1673, 1694, 1714, 1732, 1750, 1767, 1783,
    1798, 1812, 1825, 1837, 1849, 1860, 1871, 1881, 1890, 1899, 1907, 1915,
    1923, 1930, 1936, 1943, 1949, 1954, 1959, 1964, 1969, 1974, 1978, 1982,
    1985, 1988, 1992, 1995, 1998, 2001, 2004, 2006, 2009, 2011, 2013, 2015
};

// -------------------------------------------------------------------------------
// ---------------------------------- Channel 1 ----------------------------------
// -------------------------------------------------------------------------------

static void channel1_refresh_registers_trig(void);
static void channel1_refresh_registers_notrig(void);
static uint8_t gbt_channel_1_set_effect(uint8_t effect, const uint8_t **data);

static void gbt_channel_1_handle(const uint8_t **data) {
    if (!(gbt_channels_enabled & 0x01)) {
        _gbt_skip_channel(data);
        return;
    }

    uint8_t ch = *((*data)++);
    if (ch & 0x80) {
        uint8_t freq_idx = ch & 0x7F;
        ch = *((*data)++);
        gbt_arpeggio_freq_index[0].base_index = freq_idx;
        gbt_freq[0] = gbt_frequencies[freq_idx];

        if (ch & 0x80) {
            // Freq + Instr + Effect
            gbt_instr[0] = (ch & 0x30) << 2;
            gbt_channel_1_set_effect(ch & 0x0F, data);
            channel1_refresh_registers_trig();
        } else {
            // Freq + Instr + Volume
            gbt_instr[0] = (ch & 0x30) << 2;
            gbt_vol[0] = (gbt_vol[0] & 0x0F) | (ch << 4);
            channel1_refresh_registers_trig();
        }
    } else if (ch & 0x40) {
        // Set instrument and effect
        gbt_instr[0] = (ch & 0x30) << 2;
        gbt_channel_1_set_effect(ch & 0x0F, data);
        channel1_refresh_registers_notrig();
    } else if (ch & 0x20) {
        // Set volume
        gbt_vol[0] = (gbt_vol[0] & 0x0F) | (ch << 4);
        channel1_refresh_registers_trig();
    } else {
        // NOP
        return;
    }
}

static void channel1_refresh_registers_trig(void) {
    gbsa_sound_channel1_update(SOUND_MODE_TRIGGER, gbt_instr[0], gbt_vol[0], gbt_freq[0]);
}

static void channel1_refresh_registers_notrig(void) {
    gbsa_sound_channel1_update(SOUND_MODE_UPDATE, gbt_instr[0], gbt_vol[0], gbt_freq[0]);
}

/**
 * @return uint8_t 1 if it is needed to update sound registers
 */
static uint8_t channel1_update_effects() {
    // Cut note
    if (gbt_ticks_elapsed == gbt_cut_note_tick[0]) {
        gbt_cut_note_tick[0] = 0xFF;
        gbsa_sound_channel1_update(SOUND_MODE_DISABLE, 0, 0, 0);
    }

    // Arpeggio or sweep
    if (gbt_arpeggio_enabled[0] & 0xFE) {
        // PortA pitch sweep
        int freq = gbt_freq[0];
        uint8_t sweep = gbt_sweep[0];
        if (sweep & 0x80) {
            // Sweep down
            freq -= (sweep & 0x7F);
            if (freq < 0) {
                freq = 0;
                gbt_freq[0] = freq;
                gbt_arpeggio_enabled[0] = 0;
                return 0;
            } else {
                gbt_freq[0] = freq;
                return 1;
            }
        } else {
            // Sweep up
            freq += (sweep & 0x7F);
            if ((freq & 0x0700) == 0) {
                gbt_freq[0] = freq;
                gbt_arpeggio_enabled[0] = 0;
                gbsa_sound_channel1_update(SOUND_MODE_DISABLE, 0, 0, 0);
                return 1;
            } else {
                gbt_freq[0] = freq;
                return 1;
            }
        }
    } else if (gbt_arpeggio_enabled[0] != 0) {
        if (gbt_arpeggio_tick[0] == 0) {
            // Tick 0 - Set original frequency
            gbt_freq[0] = gbt_frequencies[gbt_arpeggio_freq_index[0].base_index];
            gbt_arpeggio_tick[0] = 1;
            return 1;
        } else if (gbt_arpeggio_tick[0] == 1) {
            // Tick 1
            gbt_freq[0] = gbt_frequencies[gbt_arpeggio_freq_index[0].base_index_x];
            gbt_arpeggio_tick[0] = 2;
            return 1;
        } else {
            // Tick 2
            gbt_freq[0] = gbt_frequencies[gbt_arpeggio_freq_index[0].base_index_y];
            gbt_arpeggio_tick[0] = 0;
            return 1;
        }
    }

    return 0;
}

static uint8_t gbt_channel_1_set_effect(uint8_t effect, const uint8_t **data) {
    uint8_t args = *((*data)++);

    switch (effect) {
        case 0: { // Pan
            gbt_pan[0] = args & 0x11;
            return 0;
        } break;
        case 1: { // Arpeggio
            gbt_arpeggio_index_t *idx = gbt_arpeggio_freq_index + 0;
            idx->base_index_x = idx->base_index + (args >> 4);
            idx->base_index_y = idx->base_index + (args & 0x0F);
            gbt_arpeggio_enabled[0] = 1;
            gbt_arpeggio_tick[0] = 1;
            return 1;
        } break;
        case 2: { // Cut note
            gbt_cut_note_tick[0] = args;
            if (gbt_cut_note_tick[0] == 0) {
                gbsa_sound_channel1_update(SOUND_MODE_DISABLE, 0, 0, 0);
            }
            return 0;
        } break;
        case 4: { // Sweep
            gbt_sweep[0] = args;
            gbt_arpeggio_enabled[0] = 2;
            return 0;
        } break;
        case 8: { // Jump pattern
            gbt_current_pattern = args;
            gbt_current_step = 0;
            gbt_have_to_stop_next_step = 0;
            gbt_update_pattern_pointers = 1;
            return 0;
        } break;
        case 9: { // Jump position
            gbt_current_step = args;
            gbt_current_pattern++;
            gbt_get_pattern_ptr_banked(gbt_current_pattern);
            gbt_update_pattern_pointers = 1;
            return 0;
        } break;
        case 10: { // Speed
            gbt_speed = args;
            gbt_ticks_elapsed = 0;
            return 0;
        } break;
        case 15: { // NRx2_VolEnv
            // Raw data into volume, VVVV APPP, bits 4-7 vol
            // bit 3 true = add, bits 0-2 wait period
            // 0xF1 = max volume, sub 1 every 1 tick.
            // 0x0A = min volume, add 1 every 2 ticks.
            gbt_vol[0] = args;
            return 0;
        } break;
        default: { // No-op
            return 0;
        } break;
    }
}

// -------------------------------------------------------------------------------
// ---------------------------------- Channel 2 ----------------------------------
// -------------------------------------------------------------------------------

static void channel2_refresh_registers_trig(void);
static void channel2_refresh_registers_notrig(void);
static uint8_t gbt_channel_2_set_effect(uint8_t effect, const uint8_t **data);

static void gbt_channel_2_handle(const uint8_t **data) {
    if (!(gbt_channels_enabled & 0x02)) {
        _gbt_skip_channel(data);
        return;
    }

    uint8_t ch = *((*data)++);
    if (ch & 0x80) {
        uint8_t freq_idx = ch & 0x7F;
        ch = *((*data)++);
        gbt_arpeggio_freq_index[1].base_index = freq_idx;
        gbt_freq[1] = gbt_frequencies[freq_idx];

        if (ch & 0x80) {
            // Freq + Instr + Effect
            gbt_instr[1] = (ch & 0x30) << 2;
            gbt_channel_2_set_effect(ch & 0x0F, data);
            channel2_refresh_registers_trig();

        } else {
            // Freq + Instr + Volume
            gbt_instr[1] = (ch & 0x30) << 2;
            gbt_vol[1] = (gbt_vol[1] & 0x0F) | (ch << 4);
            channel2_refresh_registers_trig();
        }
    } else if (ch & 0x40) {
        // Set instrument and effect
        gbt_instr[1] = (ch & 0x30) << 2;
        gbt_channel_2_set_effect(ch & 0x0F, data);
        channel2_refresh_registers_notrig();
    } else if (ch & 0x20) {
        // Set volume
        gbt_vol[1] = (gbt_vol[1] & 0x0F) | (ch << 4);
        channel2_refresh_registers_trig();
    } else {
        // NOP
        return;
    }
}

static void channel2_refresh_registers_trig(void) {
    gbsa_sound_channel2_update(SOUND_MODE_TRIGGER, gbt_instr[1], gbt_vol[1], gbt_freq[1]);
}

static void channel2_refresh_registers_notrig(void) {
    gbsa_sound_channel2_update(SOUND_MODE_UPDATE, gbt_instr[1], gbt_vol[1], gbt_freq[1]);
}

/**
 * @return uint8_t 1 if it is needed to update sound registers
 */
static uint8_t channel2_update_effects() {
    // Cut note
    if (gbt_ticks_elapsed == gbt_cut_note_tick[1]) {
        gbt_cut_note_tick[1] = 0xFF;
        gbsa_sound_channel2_update(SOUND_MODE_DISABLE, 0, 0, 0);
    }

    // Arpeggio or sweep
    if (gbt_arpeggio_enabled[1] & 0xFE) {
        // PortA pitch sweep
        int freq = gbt_freq[1];
        uint8_t sweep = gbt_sweep[1];
        if (sweep & 0x80) {
            // Sweep down
            freq -= (sweep & 0x7F);
            if (freq < 0) {
                freq = 0;
                gbt_freq[1] = freq;
                gbt_arpeggio_enabled[1] = 0;
                return 0;
            } else {
                gbt_freq[1] = freq;
                return 1;
            }
        } else {
            // Sweep up
            freq += (sweep & 0x7F);
            if ((freq & 0x700) == 0) {
                gbt_freq[1] = freq;
                gbt_arpeggio_enabled[1] = 0;
                gbsa_sound_channel2_update(SOUND_MODE_DISABLE, 0, 0, 0);
                return 1;
            } else {
                gbt_freq[1] = freq;
                return 1;
            }
        }
    } else if (gbt_arpeggio_enabled[1] != 0) {
        if (gbt_arpeggio_tick[1] == 0) {
            // Tick 0 - Set original frequency
            gbt_freq[1] = gbt_frequencies[gbt_arpeggio_freq_index[1].base_index];
            gbt_arpeggio_tick[1] = 1;
            return 1;
        } else if (gbt_arpeggio_tick[1] == 1) {
            // Tick 1
            gbt_freq[1] = gbt_frequencies[gbt_arpeggio_freq_index[1].base_index_x];
            gbt_arpeggio_tick[1] = 2;
            return 1;
        } else {
            // Tick 2
            gbt_freq[1] = gbt_frequencies[gbt_arpeggio_freq_index[1].base_index_y];
            gbt_arpeggio_tick[1] = 0;
            return 1;
        }
    }

    return 0;
}

static uint8_t gbt_channel_2_set_effect(uint8_t effect, const uint8_t **data) {
    uint8_t args = *((*data)++);

    switch (effect) {
        case 0: { // Pan
            gbt_pan[1] = args & 0x22;
            return 0;
        } break;
        case 1: { // Arpeggio
            gbt_arpeggio_index_t *idx = gbt_arpeggio_freq_index + 0;
            idx->base_index_x = idx->base_index + (args >> 4);
            idx->base_index_y = idx->base_index + (args & 0x0F);
            gbt_arpeggio_enabled[1] = 1;
            gbt_arpeggio_tick[1] = 1;
            return 1;
        } break;
        case 2: { // Cut note
            gbt_cut_note_tick[1] = args;
            if (gbt_cut_note_tick[1] == 0) {
                gbsa_sound_channel2_update(SOUND_MODE_DISABLE, 0, 0, 0);
            }
            return 0;
        } break;
        case 4: { // Sweep
            gbt_sweep[1] = args;
            gbt_arpeggio_enabled[1] = 2;
            return 0;
        } break;
        case 8: { // Jump pattern
            gbt_current_pattern = args;
            gbt_current_step = 0;
            gbt_have_to_stop_next_step = 0;
            gbt_update_pattern_pointers = 1;
            return 0;
        } break;
        case 9: { // Jump position
            gbt_current_step = args;
            gbt_current_pattern++;
            gbt_get_pattern_ptr_banked(gbt_current_pattern);
            gbt_update_pattern_pointers = 1;
            return 0;
        } break;
        case 10: { // Speed
            gbt_speed = args;
            gbt_ticks_elapsed = 0;
            return 0;
        } break;
        case 15: { // NRx2_VolEnv
            // Raw data into volume, VVVV APPP, bits 4-7 vol
            // bit 3 true = add, bits 0-2 wait period
            // 0xF1 = max volume, sub 1 every 1 tick.
            // 0x0A = min volume, add 1 every 2 ticks.
            gbt_vol[1] = args;
            return 0;
        } break;
        default: { // No-op
            return 0;
        } break;
    }
}

// -------------------------------------------------------------------------------
// ---------------------------------- Channel 3 ----------------------------------
// -------------------------------------------------------------------------------

static void channel3_refresh_registers_trig(void);
static void channel3_refresh_registers_notrig(void);
static uint8_t gbt_channel_3_set_effect(uint8_t effect, const uint8_t **data);

static void gbt_channel_3_handle(const uint8_t **data) {
    if (!(gbt_channels_enabled & 0x04)) {
        _gbt_skip_channel(data);
        return;
    }

    uint8_t ch = *((*data)++);
    if (ch & 0x80) {
        uint8_t freq_idx = ch & 0x7F;
        ch = *((*data)++);
        gbt_arpeggio_freq_index[2].base_index = freq_idx;
        gbt_freq[2] = gbt_frequencies[freq_idx];

        if (ch & 0x80) {
            // Freq + Instr + Effect
            gbt_instr[2] = (ch & 0x0F);
            gbt_channel_3_set_effect((ch & 0x70) >> 4, data);
            channel3_refresh_registers_trig();

        } else {
            // Freq + Instr + Volume
            gbt_instr[2] = (ch & 0x0F);
            gbt_vol[2] = ((ch & 0x30) << 1);
            channel3_refresh_registers_trig();
        }
    } else if (ch & 0x40) {
        // Set effect
        if (gbt_channel_3_set_effect(ch & 0x0F, data))
            channel3_refresh_registers_notrig();
    } else if (ch & 0x20) {
        // Set volume
        gbt_vol[2] = (ch << 4) | (ch >> 4);
        channel3_refresh_registers_trig();
    } else {
        // NOP
        return;
    }
}

static void channel3_refresh_registers_trig(void) {
    gbsa_sound_channel3_update(SOUND_MODE_DISABLE, 0, 0);

    if (gbt_channel3_loaded_instrument != gbt_instr[2]) {
        gbsa_sound_channel3_load_instrument(gbt_wave[gbt_instr[2]]);
    }

    gbsa_sound_channel3_update(SOUND_MODE_TRIGGER, gbt_vol[2], gbt_freq[2]);
}

static void channel3_refresh_registers_notrig(void) {
    gbsa_sound_channel3_update(SOUND_MODE_UPDATE, gbt_vol[2], gbt_freq[2]);
}

/**
 * @return uint8_t 1 if it is needed to update sound registers
 */
static uint8_t channel3_update_effects() {
    // Cut note
    if (gbt_ticks_elapsed == gbt_cut_note_tick[2]) {
        gbt_cut_note_tick[2] = 0xFF;
        gbsa_sound_channel3_update(SOUND_MODE_DISABLE, 0, 0);
    }

    // Arpeggio or sweep
    if (gbt_arpeggio_enabled[2] & 0xFE) {
        // PortA pitch sweep
        int freq = gbt_freq[2];
        uint8_t sweep = gbt_sweep[2];
        if (sweep & 0x80) {
            // Sweep down
            freq -= (sweep & 0x7F);
            if (freq < 0) {
                freq = 0;
                gbt_freq[2] = freq;
                gbt_arpeggio_enabled[2] = 0;
                return 0;
            } else {
                gbt_freq[2] = freq;
                return 1;
            }
        } else {
            // Sweep up
            freq += (sweep & 0x7F);
            if ((freq & 0x700) == 0) {
                gbt_freq[2] = freq;
                gbt_arpeggio_enabled[2] = 0;
                gbsa_sound_channel3_update(SOUND_MODE_DISABLE, 0, 0);
                return 1;
            } else {
                gbt_freq[2] = freq;
                return 1;
            }
        }
    } else if (gbt_arpeggio_enabled[2] != 0) {
        if (gbt_arpeggio_tick[2] == 0) {
            // Tick 0 - Set original frequency
            gbt_freq[2] = gbt_frequencies[gbt_arpeggio_freq_index[2].base_index];
            gbt_arpeggio_tick[2] = 1;
            return 1;
        } else if (gbt_arpeggio_tick[2] == 1) {
            // Tick 1
            gbt_freq[2] = gbt_frequencies[gbt_arpeggio_freq_index[2].base_index_x];
            gbt_arpeggio_tick[2] = 2;
            return 1;
        } else {
            // Tick 2
            gbt_freq[2] = gbt_frequencies[gbt_arpeggio_freq_index[2].base_index_y];
            gbt_arpeggio_tick[2] = 0;
            return 1;
        }
    }

    return 0;
}

static uint8_t gbt_channel_3_set_effect(uint8_t effect, const uint8_t **data) {
    uint8_t args = *((*data)++);

    switch (effect) {
        case 0: { // Pan
            gbt_pan[2] = args & 0x44;
            return 0;
        } break;
        case 1: { // Arpeggio
            gbt_arpeggio_index_t *idx = gbt_arpeggio_freq_index + 0;
            idx->base_index_x = idx->base_index + (args >> 4);
            idx->base_index_y = idx->base_index + (args & 0x0F);
            gbt_arpeggio_enabled[2] = 1;
            gbt_arpeggio_tick[2] = 1;
            return 1;
        } break;
        case 2: { // Cut note
            gbt_cut_note_tick[2] = args;
            if (gbt_cut_note_tick[2] == 0) {
                gbsa_sound_channel3_update(SOUND_MODE_DISABLE, 0, 0);
            }
            return 0;
        } break;
        case 4: { // Sweep
            gbt_sweep[2] = args;
            gbt_arpeggio_enabled[2] = 2;
            return 0;
        } break;
        case 8: { // Jump pattern
            gbt_current_pattern = args;
            gbt_current_step = 0;
            gbt_have_to_stop_next_step = 0;
            gbt_update_pattern_pointers = 1;
            return 0;
        } break;
        case 9: { // Jump position
            gbt_current_step = args;
            gbt_current_pattern++;
            gbt_get_pattern_ptr_banked(gbt_current_pattern);
            gbt_update_pattern_pointers = 1;
            return 0;
        } break;
        case 10: { // Speed
            gbt_speed = args;
            gbt_ticks_elapsed = 0;
            return 0;
        } break;
        default: { // No-op
            return 0;
        } break;
    }
}

// -------------------------------------------------------------------------------
// ---------------------------------- Channel 4 ----------------------------------
// -------------------------------------------------------------------------------

static void channel4_refresh_registers(void);
static uint8_t gbt_channel_4_set_effect(uint8_t effect, const uint8_t **data);

static void gbt_channel_4_handle(const uint8_t **data) {
    if (!(gbt_channels_enabled & 0x08)) {
        _gbt_skip_channel(data);
        return;
    }

    uint8_t ch = *((*data)++);
    if (ch & 0x80) {
        // Has instrument raw frequency data
        uint8_t freq_idx = ch & 0x7F;
        ch = *((*data)++);
        gbt_instr[3] = freq_idx | ((ch << 1) & 0x80);

        if (ch & 0x80) {
            // Instr + Effect
            gbt_channel_4_set_effect(ch & 0x0F, data);
            channel4_refresh_registers();
        } else {
            // Instr + Volume
            gbt_vol[3] = (gbt_vol[3] & 0x0F) | (ch << 4);
            channel4_refresh_registers();
        }
    } else if (ch & 0x40) {
        // Set effect
        if (gbt_channel_4_set_effect(ch & 0x0F, data))
            channel4_refresh_registers();
    } else if (ch & 0x20) {
        // Set volume
        gbt_vol[3] = (gbt_vol[3] & 0x0F) | (ch << 4);
        channel4_refresh_registers();
    } else {
        // NOP
        return;
    }
}

static void channel4_refresh_registers(void) {
        gbsa_sound_channel4_update(SOUND_MODE_UPDATE, gbt_instr[3], gbt_vol[3]);
}

/**
 * @return uint8_t 1 if it is needed to update sound registers
 */
static uint8_t channel4_update_effects() {
    // Cut note
    if (gbt_ticks_elapsed == gbt_cut_note_tick[3]) {
        gbt_cut_note_tick[3] = 0xFF;
        gbsa_sound_channel4_update(SOUND_MODE_DISABLE, 0, 0);
    }

    return 0;
}

static uint8_t gbt_channel_4_set_effect(uint8_t effect, const uint8_t **data) {
    uint8_t args = *((*data)++);

    switch (effect) {
        case 0: { // Pan
            gbt_pan[3] = args & 0x88;
            return 0;
        } break;
        case 2: { // Cut note
            gbt_cut_note_tick[3] = args;
            if (gbt_cut_note_tick[3] == 0) {
               gbsa_sound_channel4_update(SOUND_MODE_DISABLE, 0, 0);
            }
            return 0;
        } break;
        case 8: { // Jump pattern
            gbt_current_pattern = args;
            gbt_current_step = 0;
            gbt_have_to_stop_next_step = 0;
            gbt_update_pattern_pointers = 1;
            return 0;
        } break;
        case 9: { // Jump position
            gbt_current_step = args;
            gbt_current_pattern++;
            gbt_get_pattern_ptr_banked(gbt_current_pattern);
            gbt_update_pattern_pointers = 1;
            return 0;
        } break;
        case 10: { // Speed
            gbt_speed = args;
            gbt_ticks_elapsed = 0;
            return 0;
        } break;
        case 15: { // NRx2_VolEnv
            // Raw data into volume, VVVV APPP, bits 4-7 vol
            // bit 3 true = add, bits 0-2 wait period
            // 0xF1 = max volume, sub 1 every 1 tick.
            // 0x0A = min volume, add 1 every 2 ticks.
            gbt_vol[3] = args;
            return 0;
        } break;
        default: { // No-op
            return 0;
        } break;
    }
}

static void gbt_update_bank1(const uint8_t **data) {
    gbt_channel_1_handle(data);
    gbt_channel_2_handle(data);
    gbt_channel_3_handle(data);
    gbt_channel_4_handle(data);

    gbsa_sound_pan_update(gbt_pan[0] | gbt_pan[1] | gbt_pan[2] | gbt_pan[3]);
}

static void gbt_update_effects_bank1() {
    if (channel1_update_effects()) channel1_refresh_registers_notrig();
    if (channel2_update_effects()) channel2_refresh_registers_notrig();
    if (channel3_update_effects()) channel3_refresh_registers_notrig();
    if (channel4_update_effects()) channel4_refresh_registers();
}
