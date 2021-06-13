#include "BankData.h"

#include <string.h>

#include "BankManager.h"
#include "Scroll.h"
#include "shim/gb_shim.h"

static UBYTE _save;

// GBSA
#include "banks.h"
#include "inject_music_banks.h"
UBYTE _current_bank;

const uint8_t *gbsa_patch_get_bank(UBYTE id) {
#include "inject_get_bank.h"
	debug_printf(LOG_ERROR, "Invalid get_bank call (%d)", id);
	return NULL;
}

void SetBankedBkgData(UBYTE i, UBYTE l, unsigned char* ptr, UBYTE bank) __naked {
/*   i; l; ptr; bank;
__asm
    ldh a, (__current_bank)
    ld  (#__save),a

    ldhl  sp,	#6
    ld  a, (hl)
    ldh	(__current_bank),a
    ld  (#0x2000), a

    pop bc
    call  _set_bkg_data

    ld  a, (#__save)
    ldh (__current_bank),a
    ld  (#0x2000), a
    ld  h, b
    ld  l, c
    jp  (hl)
__endasm;   */
	set_bkg_data(i, l, ptr);
}

void SetBankedSpriteData(UBYTE i, UBYTE l, unsigned char* ptr, UBYTE bank) __naked {
/*  i; l; ptr; bank;
__asm
    ldh a, (__current_bank)
    ld  (#__save),a

    ldhl  sp,	#6
    ld  a, (hl)
    ldh	(__current_bank),a
    ld  (#0x2000), a

    pop bc
    call  _set_sprite_data

    ld  a, (#__save)
    ldh (__current_bank),a
    ld  (#0x2000), a
    ld  h, b
    ld  l, c
    jp  (hl)
__endasm;   */
  set_sprite_data(i, l, ptr);
}

UBYTE ReadBankedUBYTE(UBYTE bank, unsigned char* ptr) __naked {
/*  ptr; bank;
__asm
    ldh a, (__current_bank)
    ld  (#__save),a

    ldhl  sp,	#2
    ld  a, (hl+)
    ldh	(__current_bank),a
    ld  (#0x2000), a

    ld  a, (hl+)
    ld  h, (hl)
    ld  l, a
    ld  e, (hl)

    ld  a, (#__save)
    ldh (__current_bank),a
    ld  (#0x2000), a
    ret
__endasm;  */
	return *ptr;
}

void ReadBankedBankPtr(UBYTE bank, BankPtr* to, BankPtr* from) __naked {
/*  bank; from; to; 
__asm
    ldh a, (__current_bank)
    ld  (#__save),a

    ldhl  sp,	#2
    ld  a, (hl+)
    ldh	(__current_bank),a
    ld  (#0x2000), a

    ld  a, (hl+)
    ld  e, a
    ld  a, (hl+)
    ld  d, a

    ld  a, (hl+)
    ld  h, (hl)
    ld  l, a

    ld  a, (hl+)
    ld  (de), a
g    .rept 2
      inc de
      ld  a, (hl+)
      ld  (de), a
    .endm

    ld  a, (#__save)
    ldh (__current_bank),a
    ld  (#0x2000), a
    ret
__endasm;   */
	*to = *from;
}

void MemcpyBanked(UBYTE bank, void* to, void* from, size_t n) {
/*  _save = _current_bank;
  SWITCH_ROM(bank); */
  memcpy(to, from, n);
//  SWITCH_ROM(_save);
}
