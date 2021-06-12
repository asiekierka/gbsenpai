#include "shim/gb_shim.h"

UBYTE GetToken_b(unsigned char * src, unsigned char term, UWORD* res) {
	// Is this accurate?
	uint16_t r = 0;
	for (int pos = 0; *src != '\0'; src++, pos++) {
		if (*src >= '0' && *src <= '9') {
			r = (r * 10) + ((*src) - '0');
		} else if (*src == term) {
			if (pos <= 0 || pos >= 5) {
				return 0;
			} else {
				*res = r;
				return pos + 1;
			}
		} else {
			return 0;
		}
	}
	return 0;
}