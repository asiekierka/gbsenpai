#include "shim/gb_shim.h"
#include "shim/platform.h"

#ifdef CGB
void set_bkg_palette(int first, int count, const uint16_t *data) {
	for (int i = 0; i < count; i++, data += 4) {
		gbsa_palette_set_bkg(i + first, data);
	}
}

void set_sprite_palette(int first, int count, const uint16_t *data) {
	for (int i = 0; i < count; i++, data += 4) {
		gbsa_palette_set_sprite(i + first, data);
	}
}
#endif

uint8_t VBK_REG = 0;

void set_bkg_data(int first, int count, const uint8_t *data) {
	for (int i = 0; i < count; i++, data += 16) {
		gbsa_tile_set_data(i + first, data);
	}
}

void move_sprite(int id, int x, int y) {
	gbsa_sprite_move(id, x - 8, y - 16);
}

void set_sprite_data(int first, int count, const uint8_t *data) {
	for (int i = 0; i < count; i++, data += 16) {
		gbsa_sprite_set_data(i + first, data);
	}
}

void set_sprite_tile(int id, int idx) {
	gbsa_sprite_set_tile(id, idx & 0xFE);
}

void set_sprite_prop(int id, int props) {
	gbsa_sprite_set_props(id, props);
}

void fill_win_rect(int x, int y, int width, int height, int id) {
	if (VBK_REG) {
		for (int iy = 0; iy < height; iy++) {
			for (int ix = 0; ix < width; ix++) {
				gbsa_map_set_bg_attr(BG_ID_WINDOW, ix + x, iy + y, id);
			}
		}
	} else {
		for (int iy = 0; iy < height; iy++) {
			for (int ix = 0; ix < width; ix++) {
				gbsa_map_set_bg_tile(BG_ID_WINDOW, ix + x, iy + y, id);
			}
		}
	}
}

void set_win_tiles(int x, int y, int width, int height, const uint8_t *tiles) {
	if (VBK_REG) {
		for (int iy = 0; iy < height; iy++) {
			for (int ix = 0; ix < width; ix++) {
				gbsa_map_set_bg_attr(BG_ID_WINDOW, ix + x, iy + y, *(tiles++));
			}
		}
	} else {
		for (int iy = 0; iy < height; iy++) {
			for (int ix = 0; ix < width; ix++) {
				gbsa_map_set_bg_tile(BG_ID_WINDOW, ix + x, iy + y, *(tiles++));
			}
		}
	}
}
