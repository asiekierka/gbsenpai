#include "shim/gb_shim.h"
#include "shim/platform.h"

void SetTile(UINT16 r, UINT8 t) {
	if (VBK_REG) {
		gbsa_map_set_bg_attr((r & 0x400) ? BG_ID_WINDOW : BG_ID_BG, (r & 0x1F), (r >> 5) & 0x1F, t);
	} else {
		gbsa_map_set_bg_tile((r & 0x400) ? BG_ID_WINDOW : BG_ID_BG, (r & 0x1F), (r >> 5) & 0x1F, t);
	}
}

UINT8 * GetWinAddr() {
	return (UINT8*) 0x9C00;
}

UINT8 * GetBkgAddr() {
	return (UINT8*) 0x9800;
}