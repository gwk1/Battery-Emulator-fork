#ifdef BMW_PHEV_CSC_BATTERY
#include "../datalayer/datalayer.h"
#include "../datalayer/datalayer_extended.h"
#include "../devboard/utils/events.h"
#include "../communication/can/comm_can.h"
#include "BMW-PHEV-CSC-BATTERY.h"
#include "CRC8.h"

/* Do not change code below unless you are sure what you are doing */
static unsigned long LastTxFrame = 0;  // will store last cyclic frame has been sent
static unsigned long balancetime = 0;

CRC8 crc8;
const uint8_t finalxor[12] = {0xCF, 0xF5, 0xBB, 0x81, 0x27, 0x1D, 0x53, 0x69, 0x02, 0x38, 0x76, 0x4C};

#define BALANCE_SEQ_TIME 300000   // balance sequence active time
#define BALANCE_PAUSE_TIME 60000  // balance sequence pause time

#define MAX_VOLT_READ_FAILS 5

void BmwPhevCscBattery::print_units(char* header, int value, char* units) {
  logging.print(header);
  logging.print(value);
  logging.print(units);
}

void BmwPhevCscBattery::printCanFrame(CAN_frame msg) {
  logging.print(" RX ");
  logging.print(msg.ID, HEX);
  logging.print(" ");
  logging.print(msg.DLC);
  logging.print(" ");
  for (int i = 0; i < msg.DLC; ++i) {
    logging.print(msg.data.u8[i], HEX);
    logging.print(" ");
  }
  logging.println("");
}

void BmwPhevCscBattery::update_values() {

  datalayer.battery.status.soh_pptt = 9900;         // 99.00%
  datalayer.battery.info.total_capacity_Wh = 9000;  // 30kWh

#ifdef BMW_SBOX
  datalayer.battery.status.current_dA = datalayer.shunt.measured_amperage_mA / 100;
  datalayer.battery.status.voltage_dV = datalayer.shunt.measured_voltage_mV / 100;
#else
  datalayer.battery.status.current_dA = 0;  // 0 A
#endif

  datalayer.battery.status.active_power_W =
      datalayer.battery.status.current_dA * datalayer.battery.status.voltage_dV / 100;

  //  allvoltsvalid = true;
  for (int i = 0; i < CELLS_PER_MODULE * MODULES_PER_STRING * PARALLEL_STRINGS; ++i) {

    allvoltsvalid = allvoltsvalid && battery_cell_voltages_valid[i];
    if (!battery_cell_voltage_timeout[i]) {
      if (!battery_cell_voltages_valid[i]) {

        logging.print("CSC Voltage read fault: ");
        logging.println(i);
        notvalidcount++;
        if (notvalidcount > 10) {
          set_event(EVENT_BATTERY_VALUE_UNAVAILABLE, 0);
          value_event = true;
        }
      } else {
        battery_cell_voltage_timeout[i]--;
      }
      battery_cell_voltages_valid[i] = false;
    }
    if (allvoltsvalid) {
      //    notvalidcount=0;
      //    if(value_event) {
      //      value_event=false;
      //      clear_event(EVENT_BATTERY_VALUE_UNAVAILABLE);
      //    }
      memcpy(datalayer.battery.status.cell_voltages_mV, battery_cell_voltages_mV,
             CELLS_PER_MODULE * MODULES_PER_STRING * PARALLEL_STRINGS * sizeof(uint16_t));
      datalayer.system.status.battery_allows_contactor_closing = true;
      datalayer.battery.info.number_of_cells = CELLS_PER_MODULE * MODULES_PER_STRING * PARALLEL_STRINGS;
    }

    MinCell_mV = 4200;
    MaxCell_mV = 0;
    for (int idx = 0; idx < datalayer.battery.info.number_of_cells; idx++) {
      if (datalayer.battery.status.cell_voltages_mV[idx] < MinCell_mV &&
          datalayer.battery.status.cell_voltages_mV[idx] > 0) {
        MinCell_mV = datalayer.battery.status.cell_voltages_mV[idx];
      }
      if (datalayer.battery.status.cell_voltages_mV[idx] > MaxCell_mV) {
        MaxCell_mV = datalayer.battery.status.cell_voltages_mV[idx];
      }
    }

    unsigned long currentMillis = millis();

    if ((balancetime + BALANCE_SEQ_TIME + BALANCE_PAUSE_TIME) < currentMillis) {
      datalayer_extended.bmwphevcsc.balancing_allowed = true;
    } else if ((balancetime + BALANCE_SEQ_TIME) < currentMillis) {
      datalayer_extended.bmwphevcsc.balancing_allowed = false;
    }

    if ((MinCell_mV <= MaxCell_mV - 20) && MaxCell_mV > 3900 && datalayer_extended.bmwphevcsc.balancing_allowed) {

      /** Dont mess with new target while we are balancing **/
      if (!datalayer_extended.bmwphevcsc.balancing_active) {
        if (MinCell_mV > 3900) {
          datalayer_extended.bmwphevcsc.balance_target_mV = (MaxCell_mV + MinCell_mV) / 2 + 5;
        } else {
          datalayer_extended.bmwphevcsc.balance_target_mV = 3910;
        }
        datalayer_extended.bmwphevcsc.balancing_active = 1;
        balancetime = currentMillis;
      }
    } else {
      datalayer_extended.bmwphevcsc.balancing_active = 0;
      datalayer_extended.bmwphevcsc.balance_target_mV = 4220;
    }
    datalayer.battery.status.temperature_min_dC = celltemp[0] * 10;
    datalayer.battery.status.temperature_max_dC = celltemp[0] * 10;

    for (int idx = 1; idx < 18; idx++) {
      if (celltemp[idx] * 10 < datalayer.battery.status.temperature_min_dC) {
        datalayer.battery.status.temperature_min_dC = celltemp[idx] * 10;
      }
      if (celltemp[idx] * 10 > datalayer.battery.status.temperature_max_dC) {
        datalayer.battery.status.temperature_max_dC = celltemp[idx] * 10;
      }
    }

    /** This is extreme inaccurate method and causes SOC drifting **/
    datalayer.battery.status.real_soc = map(MinCell_mV, 3100, 4100, 1000, 9000);  //3.1v = 10%, 4.1v=90%

    datalayer.battery.status.cell_max_voltage_mV = MaxCell_mV;
    datalayer.battery.status.cell_min_voltage_mV = MinCell_mV;

    datalayer.battery.status.remaining_capacity_Wh =
        datalayer.battery.info.total_capacity_Wh * datalayer.battery.status.real_soc / 10000;

    datalayer.battery.status.max_discharge_power_W = MAX_DISCHARGE_POWER;
    datalayer.battery.status.max_charge_power_W = MAX_CHARGE_POWER;

    /** Limit discharge current at low cell voltages **/
    if (MinCell_mV < 3150) {

      /** Slow down SOC/Discharge power oscillation **/

      max_dischargepower = datalayer.battery.status.max_discharge_power_W -
                           map(MaxCell_mV, 3090, 3200, datalayer.battery.status.max_discharge_power_W, 0);

      if (max_dischargepower > datalayer.battery.status.max_discharge_power_W) {
        datalayer.battery.status.max_discharge_power_W =
            (49 * datalayer.battery.status.max_discharge_power_W + max_dischargepower) / 50;
      } else {
        datalayer.battery.status.max_discharge_power_W = max_dischargepower;
      }
    }

    /** Limit charge current at high cell voltages **/
    if (MaxCell_mV > 3900) {
      datalayer.battery.status.max_charge_power_W =
          map(MaxCell_mV, 4000, 4120, datalayer.battery.status.max_charge_power_W, 0);
    }

    /*Finally print out values to serial if configured to do so*/
#ifdef DEBUG_VIA_USB
    logging.println("Values going to inverter");
    print_units("SOH%: ", (datalayer.battery.status.soh_pptt * 0.01), "% ");
    print_units(", SOC%: ", (datalayer.battery.status.reported_soc * 0.01), "% ");
    print_units(", Voltage: ", (datalayer.battery.status.voltage_dV * 0.1), "V ");
    print_units(", Max discharge power: ", datalayer.battery.status.max_discharge_power_W, "W ");
    print_units(", Max charge power: ", datalayer.battery.status.max_charge_power_W, "W ");
    print_units(", Max temp: ", (datalayer.battery.status.temperature_max_dC * 0.1), "°C ");
    print_units(", Min temp: ", (datalayer.battery.status.temperature_min_dC * 0.1), "°C ");
    print_units(", Max cell voltage: ", datalayer.battery.status.cell_max_voltage_mV, "mV ");
    print_units(", Min cell voltage: ", datalayer.battery.status.cell_min_voltage_mV, "mV ");
    logging.println("");
#endif
  }
}

void BmwPhevCscBattery::decodetemp(CAN_frame& msg) {
  int CMU = ((msg.ID & 0x00F) - 1);
  for (int g = 0; g < 3; g++) {
    celltemp[g + CMU * 3] = msg.data.u8[g] - 40;
  }
}

void BmwPhevCscBattery::decodecandata(int CMU, uint8_t Id, CAN_frame& msg) {
  if (Id == 0) {
    datalayer_extended.bmwphevcsc.error[CMU] =
        msg.data.u8[0] + (msg.data.u8[1] << 8) + (msg.data.u8[2] << 16) + (msg.data.u8[3] << 24);
    datalayer_extended.bmwphevcsc.balance_status[CMU] = (msg.data.u8[5] << 8) + msg.data.u8[4];
  }
  int BASE = (CMU) * 16 + (Id - 1) * 3;
  if (0 < Id && Id < 6) {
    battery_cell_voltage_timeout[BASE] = MAX_VOLT_READ_FAILS;
    battery_cell_voltage_timeout[BASE + 1] = MAX_VOLT_READ_FAILS;
    battery_cell_voltage_timeout[BASE + 2] = MAX_VOLT_READ_FAILS;
    battery_cell_voltages_mV[BASE] = uint16_t(msg.data.u8[0] + (msg.data.u8[1] & 0x3F) * 256);
    battery_cell_voltages_mV[BASE + 1] = uint16_t(msg.data.u8[2] + (msg.data.u8[3] & 0x3F) * 256);
    battery_cell_voltages_mV[BASE + 2] = uint16_t(msg.data.u8[4] + (msg.data.u8[5] & 0x3F) * 256);
    battery_cell_voltages_valid[BASE] = true;
    battery_cell_voltages_valid[BASE + 1] = true;
    battery_cell_voltages_valid[BASE + 2] = true;
  }
  if (Id == 6) {
    battery_cell_voltage_timeout[BASE] = MAX_VOLT_READ_FAILS;
    battery_cell_voltages_mV[BASE] = uint16_t(msg.data.u8[0] + (msg.data.u8[1] & 0x3F) * 256);
    battery_cell_voltages_valid[BASE] = true;
  }
}

void BmwPhevCscBattery::decodecan(CAN_frame& msg) {
  int Id = (msg.ID & 0x0F0);
  int CMU = ((msg.ID & 0x00F) - FIRST_MODULE);
  uint8_t DataId;

  switch (Id) {
    case 0x000:
      DataId = 0;
      break;
    case 0x020:
      DataId = 1;
      break;
    case 0x030:
      DataId = 2;
      break;
    case 0x040:
      DataId = 3;
      break;
    case 0x050:
      DataId = 4;
      break;
    case 0x060:
      DataId = 5;
      break;
    case 0x070:
      DataId = 6;
      break;
  }
  if (CMU < 14 && DataId < 7) {
    decodecandata(CMU, DataId, msg);
  }
}

void BmwPhevCscBattery::handle_incoming_can_frame(CAN_frame rx_frame) {
  unsigned long currentMillis = millis();

  //ID not assigned//
  if (rx_frame.ID == 0xF0) {
    Unassigned++;
    logging.print(millis());
    if ((rx_frame.ID & 0x80000000) == 0x80000000)  // Determine if ID is standard (11 bits) or extended (29 bits)
      sprintf(msgString, "Extended ID: 0x%.8lX  DLC: %1d  Data:", (rx_frame.ID & 0x1FFFFFFF), rx_frame.DLC);
    else
      sprintf(msgString, ",0x%.3lX,false,%1d", rx_frame.ID, rx_frame.DLC);

    logging.print(msgString);

    if ((rx_frame.ID & 0x40000000) == 0x40000000) {  // Determine if message is a remote request frame.
      sprintf(msgString, " REMOTE REQUEST FRAME");
      logging.print(msgString);
    } else {
      for (byte i = 0; i < rx_frame.DLC; i++) {
        sprintf(msgString, ", 0x%.2X", rx_frame.data.u8[i]);
        CSC_addr[i] = rx_frame.data.u8[i];
        logging.print(msgString);
      }
    }
    logging.println();
    for (byte i = 0; i < 8; i++) {
      logging.print(CSC_addr[i], HEX);
      logging.print("|");
    }
    logging.println();
  }

  //  if (rx_frame.ID > 0x99 && rx_frame.ID < 0x180) {
  if (rx_frame.ID > 0x100 && rx_frame.ID < 0x180) {
    datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
    decodecan(rx_frame);
  }
  if ((rx_frame.ID & 0xFF0) == 0x180) {
    decodetemp(rx_frame);
  }
}

void BmwPhevCscBattery::findUnassigned() {
  Unassigned = 0;
  //check for found unassigned CSC
  TxFrame.ID = 0x0A0;  //broadcast to all CSC
  TxFrame.DLC = 8;
  TxFrame.ext_ID = 0;
  TxFrame.data.u8[0] = 0x37;
  TxFrame.data.u8[1] = 0xFF;
  TxFrame.data.u8[2] = 0xFF;
  TxFrame.data.u8[3] = 0xFF;
  TxFrame.data.u8[4] = 0xFF;
  TxFrame.data.u8[5] = 0xFF;
  TxFrame.data.u8[6] = 0xFF;
  TxFrame.data.u8[7] = 0xFF;

  transmit_can_frame(&TxFrame, can_config.battery);
  //  Can0.write(msg);
}

void BmwPhevCscBattery::assignID() {
  logging.print("ASSIGN ID: ");
  logging.println(datalayer_extended.bmwphevcsc.configuredStartingModuleID);

  TxFrame.ID = 0x0A0;  //broadcast to all CSC
  TxFrame.DLC = 8;
  TxFrame.ext_ID = 0;
  TxFrame.data.u8[0] = 0x12;
  TxFrame.data.u8[1] = 0xAB;
  TxFrame.data.u8[2] = CSC_addr[0];
  TxFrame.data.u8[3] = CSC_addr[1];
  TxFrame.data.u8[4] = CSC_addr[2];
  TxFrame.data.u8[5] = CSC_addr[3];
  TxFrame.data.u8[6] = 0xFF;
  TxFrame.data.u8[7] = 0xFF;

  transmit_can_frame(&TxFrame, can_config.battery);

  delay(30);

  TxFrame.data.u8[1] = 0xBA;
  TxFrame.data.u8[2] = CSC_addr[4];
  TxFrame.data.u8[3] = CSC_addr[5];
  TxFrame.data.u8[4] = CSC_addr[6];
  TxFrame.data.u8[5] = CSC_addr[7];

  transmit_can_frame(&TxFrame, can_config.battery);

  delay(10);
  TxFrame.data.u8[0] = 0x5B;
  TxFrame.data.u8[1] = datalayer_extended.bmwphevcsc.configuredStartingModuleID;
  transmit_can_frame(&TxFrame, can_config.battery);

  delay(10);
  TxFrame.data.u8[0] = 0x37;
  TxFrame.data.u8[1] = datalayer_extended.bmwphevcsc.configuredStartingModuleID;
  transmit_can_frame(&TxFrame, can_config.battery);

  datalayer_extended.bmwphevcsc.configuredStartingModuleID++;

  findUnassigned();
}

// Reset ID of all CSC's at canbus
void BmwPhevCscBattery::ResetCSCid() {

  //Rest all possible Ids
  for (int ID = 0; ID < 15; ID++) {
    TxFrame.ID = 0x0A0;  //broadcast to all CSC
    TxFrame.data.u8[0] = 0xA1;
    TxFrame.data.u8[1] = ID;
    TxFrame.data.u8[2] = 0xFF;
    TxFrame.data.u8[3] = 0xFF;
    TxFrame.data.u8[4] = 0xFF;
    TxFrame.data.u8[5] = 0xFF;
    TxFrame.data.u8[6] = 0xFF;
    TxFrame.data.u8[7] = 0xFF;
    transmit_can_frame(&TxFrame, can_config.battery);
    //    delay(2);
  }

  TxFrame.ID = 0x0A0;  //broadcast to all CSC
  TxFrame.data.u8[0] = 0x37;
  TxFrame.data.u8[1] = 0xFF;
  TxFrame.data.u8[2] = 0xFF;
  TxFrame.data.u8[3] = 0xFF;
  TxFrame.data.u8[4] = 0xFF;
  TxFrame.data.u8[5] = 0xFF;
  TxFrame.data.u8[6] = 0xFF;
  TxFrame.data.u8[7] = 0xFF;

  transmit_can_frame(&TxFrame, can_config.battery);
}

uint8_t Calculate_frame_crc(CAN_frame& msg, int id) {
  unsigned char canmes[11];
  int meslen = msg.DLC + 1;  //remove one for crc and add two for id bytes
  canmes[1] = msg.ID;
  canmes[0] = msg.ID >> 8;

  for (int i = 0; i < (msg.DLC - 1); i++) {
    canmes[i + 2] = msg.data.u8[i];
  }

  /*    logging.println();
        for (int i = 0; i <  meslen; i++)
        {
        logging.print(canmes[i], HEX);
        logging.print("|");
        }
        */
  return (crc8.get_crc8(canmes, meslen, finalxor[id]));
}

void BmwPhevCscBattery::transmit_can(unsigned long currentMillis) {

  if (Unassigned) {
    Unassigned = 0;
    assignID();
  }

  if (datalayer_extended.bmwphevcsc.csc_id_reset_allowed) {
    ResetCSCid();
    datalayer_extended.bmwphevcsc.csc_id_reset_allowed = false;
    logging.println("Reset ID debug");
  }

  // Send 100ms CAN Message
  if (currentMillis - LastTxFrame >= INTERVAL_100_MS) {
    LastTxFrame = currentMillis;

    for (uint8_t nextmes = 0; nextmes < 7; ++nextmes) {

      TxFrame.ID = 0x080 | (nextmes);

      /* Set balancing target to 4295mv when balancing not needed */
      if (!datalayer_extended.bmwphevcsc.balancing_active) {
        TxFrame.data.u8[0] = 0xC7;
        TxFrame.data.u8[1] = 0x10;
      } else {
        TxFrame.data.u8[0] = lowByte(datalayer_extended.bmwphevcsc.balance_target_mV);
        TxFrame.data.u8[1] = highByte(datalayer_extended.bmwphevcsc.balance_target_mV);
      }

      TxFrame.data.u8[2] = 0x00;  //balancing bits
      TxFrame.data.u8[3] = 0x00;  //balancing bits

      if (testcycle < 3) {
        TxFrame.data.u8[4] = 0x20;
        TxFrame.data.u8[5] = 0x00;
      } else {
        if (!datalayer_extended.bmwphevcsc.balancing_active) {
          TxFrame.data.u8[4] = 0x40;
        } else {
          TxFrame.data.u8[4] = 0x48;
        }
        TxFrame.data.u8[5] = 0x01;
      }

      TxFrame.data.u8[6] = mescycle << 4;
      if (testcycle == 2) {
        TxFrame.data.u8[6] = TxFrame.data.u8[6] + 0x04;
      }

      TxFrame.data.u8[7] = Calculate_frame_crc(TxFrame, nextmes);
      transmit_can_frame(&TxFrame, can_config.battery);
    }
    mescycle++;
    if (testcycle < 4) {
      testcycle++;
    }

    if (mescycle == 0xF) {
      mescycle = 0;
    }
  }
}

void BmwPhevCscBattery::setup(void) {  // Performs one time setup at startup
#ifdef DEBUG_VIA_USB
  logging.println("Test mode with fake battery selected");

#endif
  crc8.begin();

  strncpy(datalayer.system.info.battery_protocol, "BMW PHEV CSC", 63);
  datalayer.system.info.battery_protocol[63] = '\0';

  datalayer.battery.info.max_design_voltage_dV =
      CELLS_PER_MODULE * MODULES_PER_STRING * 4200;  // over this, charging is not possible (goes into forced discharge)
  datalayer.battery.info.min_design_voltage_dV =
      CELLS_PER_MODULE * MODULES_PER_STRING * 3800;  // under this, discharging further is disabled
}

#endif
