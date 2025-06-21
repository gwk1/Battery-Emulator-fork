#ifndef BMW_PHEV_CSC_BATTERY_H
#define BMW_PHEV_CSC_BATTERY_H
#include "../include.h"

#define BATTERY_SELECTED

#define MAX_DISCHARGE_POWER 5000
#define MAX_CHARGE_POWER 5000

#define MAX_CELL_DEVIATION_MV 9999

/** If S-BOX at the same can-bus, first module id 0 can't be used!! **/
#define FIRST_MODULE 1
#define CELLS_PER_MODULE 16

/** MODULES * STRINGS =< 14**/
#define MODULES_PER_STRING 1
#define PARALLEL_STRINGS 1

void setup_battery(void);
void transmit_can_frame(CAN_frame* tx_frame, int interface);

#endif
