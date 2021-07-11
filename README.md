# gbsenpai

**gbsenpai** - GB Studio Extended Nominal Player Adaptation/Interface - is a project to port the GB Studio player to additional, non-GB/GBC platforms, with support for potential enhancements.

![GB Studio v2 color sample running within gbsenpai.](https://img.asie.pl/atKY.png)

[Proof of concept video](https://img.asie.pl/adKi.mp4)

## Compatibility

gbsenpai should be compatible with source exports made in GB Studio v2.0.0-beta5. Other versions are currently not supported.

## Porting your game

First, you need to prepare your game's source code.

1. Create a directory, for example "source_sample". This will be your "game directory".
2. Open GB Studio. From the menu, run Game -> Advanced -> Export project source.
3. There should now be a "build/src" directory in your project folder. From there, copy the following files to your "game directory":
    - all files from src/data,
    - all files from src/music,
    - include/banks.h,
    - include/data_ptrs.h.

Note that if you ejected the engine and made modifications to it, you will have to reproduce the 
modifications in gbsenpai. This is a port, not an emulator - the ported engine matches vanilla
GB Studio.

In addition, you will need:

- a reasonably recent version of Python 3.x.

From here, the instructions differ depending on port.

### GBA

The GBA port is currently based on the devkitARM toolchain created by the devkitPro organization, alongside the libtonc library.

1. Edit Makefile.gba. In particular:
    - Set GAME_DIR to your game directory name,
    - Set GAME_CGB to true if your game targets the GBC, and to false otherwise.
2. Run `make -f Makefile.gba`.
3. Enjoy. (Hopefully. I've only tested this on Linux...)

### NDS

The NDS port is currently based on the devkitARM toolchain created by the devkitPro organization, alongside the libnds library.

There is some missing functionality:

* Sound is not yet implemented.
* Saving is not yet implemented.

1. Edit Makefile.nds. In particular:
    - Set GAME_DIR to your game directory name,
    - Set GAME_CGB to true if your game targets the GBC, and to false otherwise.
2. Run `make -f Makefile.nds`.
3. Enjoy.

## License

**gbsenpai** is licensed under the terms of the MIT license, similar to the original GB Studio engine.
