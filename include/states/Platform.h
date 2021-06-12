#ifndef STATE_PLATFORM_H
#define STATE_PLATFORM_H

#include "shim/gb_shim.h"

void Start_Platform();
void Update_Platform();

extern WORD pl_vel_x;
extern WORD pl_vel_y;
extern WORD pl_pos_x;
extern WORD pl_pos_y;

#endif
