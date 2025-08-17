#ifndef BMW_PHEV_CSC_BATTERY_H
#define BMW_PHEV_CSC_BATTERY_H
#include "BMW-PHEV-CSC-HTML.h"
#include "CanBattery.h"

#ifdef BMW_PHEV_CSC_BATTERY
#define SELECTED_BATTERY_CLASS BmwPhevCscBattery
#endif

class BmwPhevCscBattery : public CanBattery {
 public:
  virtual void setup(void);
  virtual void handle_incoming_can_frame(CAN_frame rx_frame);
  virtual void update_values();
  virtual void transmit_can(unsigned long currentMillis);
  virtual void print_units(char* header, int value, char* units);
  virtual void printCanFrame(CAN_frame msg);
  virtual void decodetemp(CAN_frame& msg);
  virtual void decodecandata(int CMU, uint8_t Id, CAN_frame& msg);
  virtual void decodecan(CAN_frame& msg);
  virtual void findUnassigned();
  virtual void assignID();
  virtual void ResetCSCid();
  static constexpr const char* Name = "BMW PHEV CSC invidual module battery";

  BatteryHtmlRenderer& get_status_renderer() { return renderer; }

 private:
  BmwPhevCscHtmlRenderer renderer;
  uint8_t mescycle = 0;
  uint8_t testcycle = 0;
  uint8_t CSC_addr[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  uint8_t Unassigned;
  char msgString[128];
  bool allvoltsvalid = false;
  char notvalidcount = 0;
  bool value_event = false;
  int balancecells;
  uint16_t MinCell_mV;
  uint16_t MaxCell_mV;

  uint32_t max_dischargepower;
  uint32_t max_chargepower;

  static const int CELLS_PER_MODULE = 16;

  /** If S-BOX at the same can-bus, first module id 0 can't be used!! **/
  static const int FIRST_MODULE = 1;
  /** MODULES * STRINGS =< 14**/
  static const int MODULES_PER_STRING = 1;
  static const int PARALLEL_STRINGS = 1;
  static const int MAX_DISCHARGE_POWER = 5000;
  static const int MAX_CHARGE_POWER = 5000;

  static const int MAX_CELL_DEVIATION_MV = 9999;

  uint16_t battery_cell_voltages_mV[CELLS_PER_MODULE * MODULES_PER_STRING * PARALLEL_STRINGS];
  bool battery_cell_voltages_valid[CELLS_PER_MODULE * MODULES_PER_STRING * PARALLEL_STRINGS];
  char battery_cell_voltage_timeout[CELLS_PER_MODULE * MODULES_PER_STRING * PARALLEL_STRINGS];
  int8_t celltemp[32];

  CAN_frame TxFrame = {.FD = false,
                       .ext_ID = false,
                       .DLC = 8,
                       .ID = 0x080,
                       .data = {0xC7, 0x10, 0x00, 0x00, 0x20, 0x00, 0x00, 0x8F}};
};
#endif
