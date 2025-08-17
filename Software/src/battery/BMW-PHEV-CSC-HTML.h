#ifndef _BMW_PHEV_CSC_HTML_H
#define _BMW_PHEV_CSC_HTML_H

#include "../datalayer/datalayer.h"
#include "../datalayer/datalayer_extended.h"
#include "src/devboard/webserver/BatteryHtmlRenderer.h"

class BmwPhevCscHtmlRenderer : public BatteryHtmlRenderer {
 public:
  String get_status_html() {
    String content;

    content += "<h4>Balancing setPoint: " + String(datalayer_extended.bmwphevcsc.balance_target_mV) + " mV</h4>" +
               "<h4>Balancing active: " + String(datalayer_extended.bmwphevcsc.balancing_active) + "</h4>" +
               "<h4>Balancing status: " + String(datalayer_extended.bmwphevcsc.balance_status[0], BIN) + "</h4>" +
               "<h4>Balancing status: " + String(datalayer_extended.bmwphevcsc.balance_status[1], BIN) + "</h4>" +
               "<h4>Balancing status: " + String(datalayer_extended.bmwphevcsc.balance_status[2], BIN) + "</h4>" +
               "<h4>Balancing status: " + String(datalayer_extended.bmwphevcsc.balance_status[3], BIN) + "</h4>" +
               "<h4>Balancing status: " + String(datalayer_extended.bmwphevcsc.balance_status[4], BIN) + "</h4>" +
               "<h4>Balancing status: " + String(datalayer_extended.bmwphevcsc.balance_status[5], BIN) + "</h4>" +
               "<h4>Balancing status: " + String(datalayer_extended.bmwphevcsc.balance_status[6], BIN) + "</h4>" +
               "<h4>Balancing status: " + String(datalayer_extended.bmwphevcsc.balance_status[7], BIN) + "</h4>" +
               "<h4>Balancing status: " + String(datalayer_extended.bmwphevcsc.balance_status[8], BIN) + "</h4>" +
               "<h4>Balancing status: " + String(datalayer_extended.bmwphevcsc.balance_status[9], BIN) + "</h4>" +
               "<h4>Balancing status: " + String(datalayer_extended.bmwphevcsc.balance_status[10], BIN) + "</h4>" +
               "<h4>Balancing status: " + String(datalayer_extended.bmwphevcsc.balance_status[11], BIN) + "</h4>" +
               "<h4>FirstID: " + String(datalayer_extended.bmwphevcsc.configuredStartingModuleID) + "</h4>" +
               "<h4>resetallowd: " + String(datalayer_extended.bmwphevcsc.csc_id_reset_allowed) + "</h4>";

    content += "<button onclick='ReprogramCscIdQueryValue()'>Reprogram CSC id's</button>";
    content += "<script>";
    content +=
        "function ReprogramCscIdQueryValue(){"
        "  var value=prompt('ID of all connected csc modules will be reprogrammed startig from id entered (0-14):');"
        "  if(value!==null){"
        "     if(value>=0&&value<=14){"
        "        var xhr=new XMLHttpRequest();"
        "        xhr.open('GET','/ReprogramCscId?value='+value,true);"
        "        xhr.send();"
        "     }else{"
        "       alert('Invalid value. Please enter a value between 0 and 14');"
        "     }"
        "  }"
        "}";
    content += "</script>";

    return content;
  }
};
#endif
