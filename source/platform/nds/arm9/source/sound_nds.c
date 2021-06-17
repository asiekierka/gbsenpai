#include <nds.h>
#include "shim/platform.h"

void gbsa_sound_start(uint8_t reinit) {
	// stub
}

void gbsa_sound_pause(uint8_t paused) {
	// stub
}

void gbsa_sound_stop(void) {
	// stub
}

void gbsa_sound_channel1_update(uint8_t mode, uint8_t instr, uint8_t vol, uint16_t freq) {
	// stub
}

void gbsa_sound_channel2_update(uint8_t mode, uint8_t instr, uint8_t vol, uint16_t freq) {
	// stub
}

void gbsa_sound_channel3_load_instrument(const uint8_t *wave) {
	// stub
}

void gbsa_sound_channel3_update(uint8_t mode, uint8_t vol, uint16_t freq) {
	// stub
}

void gbsa_sound_channel4_update(uint8_t mode, uint8_t instr, uint8_t vol) {
	// stub
}

uint8_t gbsa_sound_pan_get(void) {
	// stub
	return 0;
}

void gbsa_sound_pan_update(uint8_t pan) {
	// stub
}