#include <tonc_memmap.h>
#include "shim/platform.h"

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

void gbsa_sound_start(uint8_t reinit) {
    NR52_REG = 0x80;

    if (reinit) {
        NR50_REG = 0x00; // 0%
        NR51_REG = 0x00;

        NR10_REG = 0;
        NR11_REG = 0;
        NR12_REG = 0;
        NR13_REG = 0;
        NR14_REG = 0;
        NR21_REG = 0;
        NR22_REG = 0;
        NR23_REG = 0;
        NR24_REG = 0;
        NR30_REG = 0;
        NR31_REG = 0;
        NR32_REG = 0;
        NR33_REG = 0;
        NR34_REG = 0;
        NR41_REG = 0;
        NR42_REG = 0;
        NR43_REG = 0;
        NR44_REG = 0;
    }

    NR50_REG = 0x77; // 100%
}

void gbsa_sound_pause(uint8_t paused) {
    if (!paused) {
        NR50_REG = 0x77;
    } else {
        NR50_REG = 0;
    }
}

void gbsa_sound_stop(void) {
    NR50_REG = 0;
    NR51_REG = 0;
    NR52_REG = 0;
}

void gbsa_sound_channel1_update(uint8_t mode, uint8_t instr, uint8_t vol, uint16_t freq) {
    switch (mode) {
    case SOUND_MODE_UPDATE: {
        NR10_REG = 0;
        NR11_REG = instr;
        NR13_REG = freq & 0xFF;
        NR14_REG = (freq >> 8);
    } break;
    case SOUND_MODE_TRIGGER: {
        NR10_REG = 0;
        NR11_REG = instr;
        NR12_REG = vol;
        NR13_REG = freq & 0xFF;
        NR14_REG = (freq >> 8) | 0x80;
    } break;
    case SOUND_MODE_DISABLE: {
        NR12_REG = 0;
        NR14_REG = 0x80;      
    } break;
    }
}

void gbsa_sound_channel2_update(uint8_t mode, uint8_t instr, uint8_t vol, uint16_t freq) {
    switch (mode) {
    case SOUND_MODE_UPDATE: {
        NR21_REG = instr;
        NR23_REG = freq & 0xFF;
        NR24_REG = (freq >> 8);
    } break;
    case SOUND_MODE_TRIGGER: {
        NR21_REG = instr;
        NR22_REG = vol;
        NR23_REG = freq & 0xFF;
        NR24_REG = (freq >> 8) | 0x80;
    } break;
    case SOUND_MODE_DISABLE: {
        NR22_REG = 0;
        NR24_REG = 0x80;      
    } break;
    }
}

void gbsa_sound_channel3_load_instrument(const uint8_t *wave) {
    volatile uint16_t *wave_ram = (vu16*) REG_WAVE_RAM;
    for (int i = 0; i < 8; i++, wave += 2) {
        wave_ram[i] = (wave[0]) | (wave[1] << 8);
    }
    NR30_REG ^= 0x40;
}

void gbsa_sound_channel3_update(uint8_t mode, uint8_t vol, uint16_t freq) {
    switch (mode) {
    case SOUND_MODE_UPDATE: {
        // Don't Restart Waveform!
        NR33_REG = freq & 0xFF;
        NR34_REG = (freq >> 8);
    } break;
    case SOUND_MODE_TRIGGER: {
        NR30_REG |= 0x80;
        NR31_REG = 0;
        NR32_REG = vol;
        NR33_REG = freq & 0xFF;
        NR34_REG = (freq >> 8) | 0x80; // start
    } break;
    case SOUND_MODE_DISABLE: {
        NR30_REG &= 0x40;
        NR32_REG = 0;
        NR34_REG = 0x80;
    } break;
    }
}

void gbsa_sound_channel4_update(uint8_t mode, uint8_t instr, uint8_t vol) {
    switch (mode) {
    case SOUND_MODE_UPDATE:
    case SOUND_MODE_TRIGGER: {
        NR41_REG = 0;
        NR42_REG = vol;
        NR43_REG = instr;
        NR44_REG = 0x80; // start
    } break;
    case SOUND_MODE_CH4_BEEP: {
        NR41_REG = 0x01;
        NR42_REG = vol;
        NR43_REG = instr;
        NR44_REG = 0x80 | 0x40;
    } break;
    case SOUND_MODE_DISABLE: {
        NR42_REG = 0;
        NR44_REG = 0x80; // start
    } break;
    }
}

uint8_t gbsa_sound_pan_get(void) {
    return NR51_REG;
}

void gbsa_sound_pan_update(uint8_t pan) {
    NR51_REG = pan;
}