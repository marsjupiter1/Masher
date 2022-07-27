#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <WiFiManager.h>
#include <math.h>
#include <stdio.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <exception>
#include <vector>
#include <aREST.h>
#include <EEPROM.h>
#define EEPROM_SIZE 100

#include <ezButton.h>
#include <SHA256.h>
#include "mbedtls/md.h"

#include "time.h"
#include "SPIFFS.h"
#include "TuyaAuth.h"

#include <mutex>

std::mutex display_mtx;

void(* resetFunc) (void) = 0; //declare reset function @ address 0


const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 3600;
const char *SELECTED = "selected=\"selected\"";

#define BUTTON_MENU 15
#define BUTTON_UP 16
#define BUTTON_DOWN 2
ezButton menu_button(BUTTON_MENU);


const int thermistor_output1 = 35;
const int thermistor_output2 = 32;

#define MID 150

#define D_CLK 21 //blue
#define D_DATA 22 // green
#include <Arduino.h>
#include <U8x8lib.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
//0x3F
/*      USB
  o           o
  o           o
  o           o
  o           MEN(y)15
  o           DOW(g)2
  GND(lcd)    o
  o           UP(r)16
  o           o
  o           o
  o           o
  o           o
  Two (W32)   o
  One(BR35)   CLK 21
  o           o
  o           DAT 22
  o           o
  3.2          GND
*/
U8X8_ST7567_ENH_DG128064I_SW_I2C u8x8(D_CLK, D_DATA);

TuyaAuth *tuya;


TaskHandle_t Task1;
TaskHandle_t TaskStatus;

#define VOLTS 3.2
#define RESISTANCEK 10
#define ADC_RANGE 4095.0
#define SMOOTH 20
WebServer server(80);

WiFiManager wifiManager;



#define AVERAGE 0
#define ONE_CALIB 2
#define TWO_CALIB 3
#define ONE_RAW 4
#define TWO_RAW 5
#define CALIB 1
#define MODE_STATES 6

#define TUYA 0
#define TASMOTA 1

float tmin, tmax;
int  device_type = TASMOTA;
char tuya_keyc[40];
char tuya_deviceh[40];
char tuya_keyh[40];
char tuya_devicec[40];
char ipc[21];
char iph[21];
char tuya_api_key[33];
char tuya_api_client[21];
char tuya_host[100];
String tuya_switch_command_c;
String tuya_switch_command_h;
int mode = AVERAGE;
int smart = TASMOTA;

char device_name[20];
#define MAX_CALIB 5
float real_point[MAX_CALIB];
float device_point[MAX_CALIB];

#define BOOT 0
#define FOUND_SWITCH 1
#define TUYA_WAITING 2
#define WIFI_CONF 3
#define WIFI_CONNECTED 4

int white_state = 0;
int red_state = 0;
int green_state = 0;

#define DATA 0
#define SET_HIGH 1
#define SET_LOW 2
#define SET_MODE 3
#define SET_H 4
#define SET_C 5
#define DISPLAY_STATES 6

#define DATA_BASE      0
#define DATA_DEVICE    1
#define DATA_CONFIG    2


int display_mode = DATA;
int data_mode = DATA_BASE;


void readId(char *buffer) {
  for (int i = 0; i < 20; i++) {

    buffer[i] = EEPROM.read(i);
    buffer[i + 1] = '\0';
  }
}

void writeId(const char *id) {
  for (int i = 0; i <= strlen(id); i++) {
    EEPROM.write(i, id[i]);
  }
  unsigned long calculatedCrc = eeprom_crc();
  EEPROM.put(EEPROM.length() - 4, calculatedCrc);

}

void init_eeprom() {
  unsigned long calculatedCrc = eeprom_crc();

  // get stored crc
  unsigned long storedCrc;
  EEPROM.get(EEPROM.length() - 4, storedCrc);

  if (storedCrc != calculatedCrc) {
    writeId("thermistor");
  }

}

unsigned long eeprom_crc(void)
{

  const unsigned long crc_table[16] =
  {
    0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
    0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
    0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
    0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
  };

  unsigned long crc = ~0L;

  for (int index = 0 ; index < EEPROM.length() - 4  ; ++index)
  {
    byte b = EEPROM.read(index);
    crc = crc_table[(crc ^ b) & 0x0f] ^ (crc >> 4);
    crc = crc_table[(crc ^ (b >> 4)) & 0x0f] ^ (crc >> 4);
    crc = ~crc;
  }
  return crc;
}

String ip2Str(IPAddress ip) {
  String s = "";
  for (int i = 0; i < 4; i++) {
    s += i  ? "." + String(ip[i]) : String(ip[i]);
  }
  return s;
}
/*
                 Login page
*/
const char* loginIndex =
  "<form name='loginForm'>"
  "<table width='20%' bgcolor='A09F9F' align='center'>"
  "<tr>"
  "<td colspan=2>"
  "<center><font size=4><b>Big Button Login Page</b></font></center>"
  "<br>"
  "</td>"
  "<br>"
  "<br>"
  "</tr>"
  "<td>Username:</td>"
  "<td><input type='text' size=25 name='userid'><br></td>"
  "</tr>"
  "<br>"
  "<br>"
  "<tr>"
  "<td>Password:</td>"
  "<td><input type='Password' size=25 name='pwd'><br></td>"
  "<br>"
  "<br>"
  "</tr>"
  "<tr>"
  "<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
  "</tr>"
  "</table>"
  "</form>"
  "<script>"
  "function check(form)"
  "{"
  "if(form.userid.value=='admin' && form.pwd.value=='admin')"
  "{"
  "window.open('/serverIndex')"
  "}"
  "else"
  "{"
  " alert('Error Password or Username')/*displays error message*/"
  "}"
  "}"
  "</script>";

/*
   Server Index Page
*/

const char* serverIndex =
  "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
  "<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
  "<input type='file' name='update'>"
  "<input type='submit' value='Update'>"
  "</form>"
  "<div id='prg'>progress: 0%</div>"
  "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')"
  "},"
  "error: function (a, b, c) {"
  "}"
  "});"
  "});"
  "</script>";



double getThermistorReading(int pin) {
  int thermistor_adc_val;
  double output_voltage, thermistor_resistance, therm_res_ln, temperature;
  char buffer[100];
  thermistor_adc_val = 0;

  for (int i = 0; i < SMOOTH; i++) {
    thermistor_adc_val += analogRead(pin);
    //Serial.print(thermistor_adc_val);
  }
  thermistor_adc_val /= SMOOTH;
  output_voltage = ( (thermistor_adc_val * VOLTS) / ADC_RANGE );
  thermistor_resistance = ( (VOLTS * ( RESISTANCEK / output_voltage ) ) - RESISTANCEK ); /* Resistance in kilo ohms */
  thermistor_resistance = thermistor_resistance * 1000 ; /* Resistance in ohms   */
  therm_res_ln = log(thermistor_resistance);
  /*  Steinhart-Hart Thermistor Equation: */
  /*  Temperature in Kelvin = 1 / (A + B[ln(R)] + C[ln(R)]^3)   */
  /*  where A = 0.001129148, B = 0.000234125 and C = 8.76741*10^-8  */
  temperature = ( 1 / ( 0.001129148 + ( 0.000234125 * therm_res_ln ) + ( 0.0000000876741 * therm_res_ln * therm_res_ln * therm_res_ln ) ) ); /* Temperature in Kelvin */
  temperature = temperature - 273.15; /* Temperature in degree Celsius */
  //Serial.print("Temperature in degree Celsius = ");
  //Serial.print(temperature);
  //Serial.print("\t\t");
  //Serial.print("Resistance in ohms = ");
  //.print(thermistor_adc_val);
  //Serial.print("\n\n");
  //sprintf(buffer, "Temperature in degree Celsius = % f, % d, % f % f % f",temperature,thermistor_adc_val ,thermistor_resistance,therm_res_ln,output_voltage);
  return temperature;
}

class CReadings {
  public:
    double temperature1;
    double temperature2;
    double rtemperature1;
    double rtemperature2;
    double temperature;
};

void getReadings(class CReadings &r) {
  r.temperature1 = getThermistorReading(thermistor_output1);
  r.temperature2 = getThermistorReading(thermistor_output2);
  r.rtemperature1 = r.temperature1;
  r.rtemperature2 = r.temperature2;
  r.temperature = (r.temperature1 + r.temperature2) / 2;

  switch (mode) {
    case AVERAGE:
      r.temperature = calibrate((r.temperature1 + r.temperature2) / 2);
      break;
    case   ONE_CALIB:
      r.temperature = calibrate(r.temperature1 );
      break;
    case   TWO_CALIB:
      r.temperature = calibrate(r.temperature2 );
      break;
    case   ONE_RAW:
      r.temperature = r.temperature1 ;
      break;
    case   TWO_RAW:
      r.temperature = r.temperature2 ;
      break;
    case CALIB:
      r.temperature = (r.temperature1 + r.temperature2) / 2;
      break;
  }
}


void handleRoot() {
  char buffer[160];
  class CReadings r;
  getReadings(r);
  sprintf(buffer, " {\"c\":%f,\"low\":%f,\"high\":%f,\"one\":%f,\"two\":%f,\"rone\":%f,\"rtwo\":%f}",
          r.temperature, r.temperature1 < r.temperature2 ? r.temperature1 : r.temperature2, r.temperature1 > r.temperature2 ? r.temperature1 : r.temperature2, r.temperature1, r.temperature2, r.rtemperature1, r.rtemperature2);
  server.send(200, "text/plain", buffer);

}



const char style_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
<style type="text/css">
.form-style-2{
  max-width: 800px;
  padding: 20px 12px 10px 20px;
  font: 13px Arial, Helvetica, sans-serif;
}
.form-style-2-heading{
  font-weight: bold;
  font-style: italic;
  border-bottom: 2px solid #ddd;
  margin-bottom: 20px;
  font-size: 15px;
  padding-bottom: 3px;
}
.form-style-2 label{
  display: block;
  margin: 0px 0px 15px 0px;
}
.form-style-2 label > span{
  width: 170px;
  font-weight: bold;
  float: left;
  padding-top: 8px;
  padding-right: 5px;
}
.form-style-2 span.required{
  color:red;

.form-style-2 input.input-field, .form-style-2 .select-field{
  width: 48%; 
}
.form-style-2 input.input-field, 
.form-style-2 .tel-number-field, 
.form-style-2 .textarea-field, 
 .form-style-2 .select-field{
  box-sizing: border-box;
  -webkit-box-sizing: border-box;
  -moz-box-sizing: border-box;
  border: 1px solid #C2C2C2;
  box-shadow: 1px 1px 4px #EBEBEB;
  -moz-box-shadow: 1px 1px 4px #EBEBEB;
  -webkit-box-shadow: 1px 1px 4px #EBEBEB;
  border-radius: 3px;
  -webkit-border-radius: 3px;
  -moz-border-radius: 3px;
  padding: 7px;
  outline: none;
}
.form-style-2 .input-field:focus, 
.form-style-2 .tel-number-field:focus, 
.form-style-2 .textarea-field:focus,  
.form-style-2 .select-field:focus{
  border: 1px solid #0C0;
}

.form-style-2 input[type=submit],
.form-style-2 input[type=button]{
  border: none;
  padding: 8px 15px 8px 15px;
  background: #FF8500;
  color: #fff;
  box-shadow: 1px 1px 4px #DADADA;
  -moz-box-shadow: 1px 1px 4px #DADADA;
  -webkit-box-shadow: 1px 1px 4px #DADADA;
  border-radius: 3px;
  -webkit-border-radius: 3px;
  -moz-border-radius: 3px;
}
.form-style-2 input[type=submit]:hover,
.form-style-2 input[type=button]:hover{
  background: #EA7B00;
  color: #fff;
}
</style>)rawliteral";

const char index_html[] PROGMEM = R"rawliteral(
  <title>Thermistor Setup</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  </head><body>
  %REBOOT%
  <div class="form-style-2">
  <div class="form-style-2-heading">Provide your information</div>
  <form action="/setting">
    <label for="min"><span>Min</span><input id="min" type="number" name="min" value="%MIN%"></label>
    <label for="max"><span>Max</span><input id="max" type="number" name="max" value="%MAX%"></label>
    <label for "mode"><span>Mode</span><select name="mode" id="mode" >
  <option value="average"  %AVERAGE%>Average of calibrated readings
  <option value="onec" %MODE_ONEC%>Probe 1 calibrated reading
  <option value="twoc" %MODE_TWOC%>Probe 2 calibrated reading
  <option value="oner" %MODE_ONER%>Probe 1 raw reading
  <option value="twor" %MODE_TWOR%>Probe 2 raw reading
  <option value="calib" %MODE_CALIB%>Average raw reading

</select></label>
    <label for "smart"><span>Smart Socket</span><select name="smart" id="smart" >
  <option value="tasmota"  %TASMOTA%>Tasmota
  <option value="tuya" %TUYA%>Tuya
</select></label>
     <label for="api_host"><span>Tuya API url</span><input  id="api_host" type="text" maxlength="100"  name="tuya_host" value="%TUYA_HOST%"></label>
    <label for="api_id"><span>API Client Id</span><input  id="api_id" type="text" maxlength="20"  name="tuya_api_client" value="%TUYA_CLIENT%"></label>
    <label for="kh"><span>API Secret Key</span><input  id="api_key" type="text" maxlength="32" name="tuya_api_key" value="%TUYA_APIKEY%"></label>
    <label for="idh"><span>Heating Device Id</span><input  id="idh" type="text" maxlength="22"  name="tuya_deviceh" value="%TUYA_DEVICEH%"></label>
     <label for="idc"><span>Cooling Device Id</span><input  id="idc" type="text" maxlength="22"  name="tuya_devicec" value="%TUYA_DEVICEC%"></label>
     <label for="iph"><span>Heating Device IP</span><input  id="iph" type="text" maxlength="22"  name="iph" value="%IPH%"></label>
     <label for="ipc"><span>Cooling Device IP</span><input  id="ipc" type="text" maxlength="22"  name="ipc" value="%IPC%"></label>
    <input type="submit" value="Submit">
  </form>
 </div>
</body></html>)rawliteral";

const char calib_html[] PROGMEM = R"rawliteral(
  <title>Thermistor Calibration</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  </head><body>
  <div class="form-style-2">
  <div class="form-style-2-heading">Provide your reference points in ascending order, read values in a range will be linear interpolated  </div>
  <form action="/calib">
   <table><tr><td>
    <label for="r1"><span>True </span><input id="r1" type="number" name="r1" value="%R1%"></label></td>
    <td><label for="d1"><span>Device</span><input id="d1" type="number" name="d1" value="%D1%"></label></td>
    </tr>
    <tr><td>
    <label for="r2"><span>True </span><input id="r2" type="number" name="r2" value="%R2%"></label></td>
    <td><label for="d2"><span>Device</span><input id="d2" type="number" name="d2" value="%D2%"></label></td>
    </tr>
    <tr><td>
    <label for="r3"><span>True </span><input id="r3" type="number" name="r3" value="%R3%"></label></td>
    <td><label for="d3"><span>Device</span><input id="d3" type="number" name="d1" value="%D3%"></label></td>
    </tr>
    <tr><td>
    <label for="r4"><span>True </span><input id="r4" type="number" name="r4" value="%R4%"></label></td>
    <td><label for="d4"><span>Device</span><input id="d4" type="number" name="d4" value="%D4%"></label></td>
    </tr>
    <tr><td>
    <label for="r5"><span>True </span><input id="r5" type="number" name="r5" value="%R5%"></label></td>
    <td><label for="d5"><span>Device</span><input id="d5" type="number" name="d5" value="%D5%"></label></td>
    </tr>
    </table>
    <input type="submit" value="Submit">
  </form>
 </div>
</body></html>)rawliteral";

void saveSettings() {
  char buffer[1000];
  Serial.println("save settings");
  sprintf(buffer, "{\"min\":\"%f\",\"max\":\"%f\",\"tuya_deviceh\":\"%s\",\"tuya_keyh\":\"%s\",\"tuya_devicec\":\"%s\",\"tuya_keyc\":\"%s\",\"ipc\":\"%s\",\"iph\":\"%s\",\"tuya_api_key\": \"%s\",\"tuya_api_client\": \"%s\",\"tuya_host\": \"%s\",\"mode\": \"%d\",\"smart\": \"%d\"}",
          tmin, tmax, tuya_deviceh, tuya_keyh, tuya_devicec, tuya_keyc, ipc, iph, tuya_api_key, tuya_api_client, tuya_host, mode, smart);

  File file = SPIFFS.open("/settings.txt", FILE_WRITE);
  int l = strlen(buffer);

  for (int i = 0; i <= l; i++) {
    file.write(buffer[i]);
  }
  file.close();
}

void handleSetting() {
  bool reboot = false;
  bool changed = false;
  bool init = false;

  Serial.println("/setting");

  if (server.hasArg("min")) {
    float ntmin = strtof(server.arg("min").c_str(), NULL);
    if (tmin != ntmin) {
      tmin = ntmin;
      changed = true;
    }
  }
  if (server.hasArg("max")) {
    float ntmax = strtof(server.arg("max").c_str(), NULL);
    if (tmax != ntmax) {
      tmax = ntmax;
      changed = true;
    }
  }
  if (server.hasArg("tuya_keyh") ) {
    const  char *tuya_device_arg = server.arg("tuya_keyh").c_str();
    if (strlen(tuya_device_arg) <= 18 && strcmp(tuya_keyh, tuya_device_arg) != 0) {
      strcpy(tuya_keyh, tuya_device_arg);
      changed = true;
      reboot = true;
    }

  }

  if (server.hasArg("tuya_deviceh") ) {
    const char *tuya_device_arg = server.arg("tuya_deviceh").c_str();
    if (strlen(tuya_device_arg) == 22 &&  strcmp(tuya_deviceh, tuya_device_arg) != 0) {
      strcpy(tuya_deviceh, tuya_device_arg);
      changed = true;
      reboot = true;
    }

  }

  if (server.hasArg("tuya_keyc") ) {
    const  char *tuya_device_arg = server.arg("tuya_keyc").c_str();
    if (strlen(tuya_device_arg) == 18 &&  strcmp(tuya_keyh, tuya_device_arg) != 0 ) {
      strcpy(tuya_keyh, tuya_device_arg);
      changed = true;
      reboot = true;
    }

  }

  if (server.hasArg("tuya_devicec") ) {
    const char *tuya_device_arg = server.arg("tuya_devicec").c_str();
    if (strlen(tuya_device_arg) == 22 &&  strcmp(tuya_devicec, tuya_device_arg) != 0) {
      strcpy(tuya_devicec, tuya_device_arg);
      changed = true;
      reboot = true;
    }

  }

  if (server.hasArg("ipc") ) {
    const char *tuya_device_arg = server.arg("ipc").c_str();
    if (strlen(tuya_device_arg) < 16 &&  strcmp(ipc, tuya_device_arg) != 0) {
      strcpy(ipc, tuya_device_arg);
      changed = true;
      reboot = true;
    }
  }

  if (server.hasArg("iph") ) {
    const char *tuya_device_arg = server.arg("iph").c_str();
    if (strlen(tuya_device_arg) < 17 &&  strcmp(iph, tuya_device_arg) != 0) {
      strcpy(iph, tuya_device_arg);
      changed = true;
      reboot = true;
    }
  }


  if (server.hasArg("tuya_api_key") ) {
    const char *tuya_device_arg = server.arg("tuya_api_key").c_str();
    if (strlen(tuya_device_arg) < 33 &&  strcmp(tuya_api_key, tuya_device_arg) != 0) {
      strcpy(tuya_api_key, tuya_device_arg);
      changed = true;
      reboot = true;
    }
  }
  if (server.hasArg("tuya_api_client") ) {
    const char *tuya_device_arg = server.arg("tuya_api_client").c_str();
    if (strlen(tuya_device_arg) < 21 &&  strcmp(tuya_api_client, tuya_device_arg) != 0) {
      strcpy(tuya_api_client, tuya_device_arg);
      changed = true;
      reboot = true;
    }
  }

  if (server.hasArg("tuya_host") ) {
    const char *tuya_device_arg = server.arg("tuya_host").c_str();
    if (strlen(tuya_device_arg) < 100 &&  strcmp(tuya_host, tuya_device_arg) != 0)  {
      strcpy(tuya_host, tuya_device_arg);
      changed = true;
      reboot = true;
    }
  }
  if (server.hasArg("mode") ) {
    String nmode_string = server.arg("mode");
    int nmode = mode;
    if (nmode_string == "average") {
      nmode = AVERAGE;
    } else if (nmode_string == "oner") {
      nmode = ONE_RAW;
    } else if (nmode_string == "twor") {
      nmode = TWO_RAW;
    } else if (nmode_string == "onec") {
      nmode = ONE_CALIB;
    } else if (nmode_string == "twoc") {
      nmode = TWO_CALIB;
    } else if (nmode_string == "calib") {
      nmode = CALIB;
    }
    if (mode != nmode) {
      mode = nmode;
      changed = true;
    }
  }

  if (server.hasArg("smart") ) {
    String nsmart_string = server.arg("smart");
    int nsmart = smart;
    if (nsmart_string == "tuya") {
      nsmart = TUYA;
    } else if (nsmart_string == "tasmota") {
      nsmart = TASMOTA;
    }
    if (smart != nsmart) {
      smart = nsmart;
      changed = true;
      reboot = true;
    }
  }

  if (changed) {
    saveSettings();

    ;
  }
  String new_index_html = String(style_html) + String(index_html);
  new_index_html.replace("%MIN%", String(tmin, 2));
  new_index_html.replace("%MAX%", String(tmax, 2));
  new_index_html.replace("%TUYA_DEVICEC%", tuya_devicec);
  new_index_html.replace("%TUYA_DEVICEH%", tuya_deviceh);
  new_index_html.replace("%TUYA_KEYH%", tuya_keyh);
  new_index_html.replace("%TUYA_KEYC%", tuya_keyc);
  new_index_html.replace("%IPH%", iph);
  new_index_html.replace("%IPC%", ipc);
  new_index_html.replace("%TUYA_CLIENT%", tuya_api_client);
  new_index_html.replace("%TUYA_APIKEY%", tuya_api_key);
  new_index_html.replace("%TUYA_HOST%", tuya_host);
  new_index_html.replace("%MODE_AVERAGE%", mode == AVERAGE ? SELECTED : "");
  new_index_html.replace("%MODE_ONER%", mode == ONE_RAW ? SELECTED : "");
  new_index_html.replace("%MODE_TWOR%", mode ==  TWO_RAW ? SELECTED : "");
  new_index_html.replace("%MODE_ONEC%", mode == ONE_CALIB ? SELECTED : "");
  new_index_html.replace("%MODE_TWOC%", mode == TWO_CALIB ? SELECTED : "");
  new_index_html.replace("%MODE_CALIB%", mode == CALIB ? SELECTED : "");
  new_index_html.replace("%TASMOTA%", smart == TASMOTA ? SELECTED : "");
  new_index_html.replace("%TUYA%", smart == TUYA ? SELECTED : "");
  if (reboot) {
    new_index_html.replace("%REBOOT%", "<h1>Device rebooting, please connect again in a few seconds</h1>");
  } else {
    new_index_html.replace("%REBOOT%", "");
  }
  server.send(200, "text/html", new_index_html);

  if (reboot) {
    delay(1000);
    resetFunc();
  }
}

void handleCalib() {

  Serial.println("handleCalib");
  bool changed = false;
  bool init = false;

  for (auto i = 0; i < MAX_CALIB; i++) {

    String arg = String("r") + String(i + 1);
    if (server.hasArg(arg)) {

      float f = atof( server.arg(arg).c_str());
      if ( f != real_point[i])  {
        real_point[i] = f;
        changed = true;
      }
    }
    arg = String("d") + String(i + 1);
    if (server.hasArg(arg)) {
      float f = atof( server.arg(arg).c_str());
      if (f != device_point[i])  {
        device_point[i] = f;
        changed = true;
      }
    }
  }

  if (changed) {
    String buffer = "{\"real\": [";
    for (auto i = 0; i < MAX_CALIB; i++) {
      buffer += String(real_point[i]);
      if (i < MAX_CALIB - 1) {
        buffer += ",";
      }
    }
    buffer += "],\"device\": [";
    for (auto i = 0; i < MAX_CALIB; i++) {
      buffer += String(device_point[i]);
      if (i < MAX_CALIB - 1) {
        buffer += ",";
      }
    }
    buffer += "]}";
    Serial.println(buffer);

    File file = SPIFFS.open("/calib.txt", FILE_WRITE);
    int l = buffer.length();

    for (int i = 0; i <= l; i++) {
      file.write(buffer[i]);
    }
    file.write('\0');
    file.close();
  }
  String new_index_html = String(style_html) + String(calib_html);
  for (auto i = 0; i < MAX_CALIB; i++) {
    new_index_html.replace(String("%D") + String(i + 1) + String("%"), String(device_point[i]));
    new_index_html.replace(String("%R") + String(i + 1) + String("%"), String(real_point[i]));
  }
  server.send(200, "text/html", new_index_html);

}

char state = 'N';

bool haveHeater() {
  if (smart == TUYA && strlen(tuya_deviceh) > 0) return true;
  if (smart == TASMOTA && strlen(iph) > 0) return true;
  return false;
}

bool haveCooler() {
  if (smart == TUYA && strlen(tuya_devicec) > 0) return true;
  if (smart == TASMOTA && strlen(ipc) > 0) return true;
  return false;
}

bool blink = false;

void blinkText(int x, int y, const char *text) {

  if (!blink) {
    for (int i = 0; i < strlen(text); i++) {
      u8x8.drawString(x + i, y, " ");
    }
  } else {
    u8x8.drawString(x, y, text);
  }
}


#define ACTION_POSX 0
#define ACTION_POSY 2

#define STATUS_LINE 0
#define MINMAX_LINE 1
#define PLUG_STATUSX 0
#define WIFI_STATUSX 0
#define TIMEX 80
#define BLINK_TIME 500

void writeTmin() {

  if (display_mode != DATA || data_mode != DATA_BASE) return;
  char buffer[8];
  sprintf(buffer, "L:%2.1f", tmin);

  u8x8.drawString(0, MINMAX_LINE, buffer);
}

void writeTmax() {
  if (display_mode != DATA || data_mode != DATA_BASE) return;
  char buffer[8];
  sprintf(buffer, "H:%2.1f", tmax);
  u8x8.drawString(8, MINMAX_LINE, buffer);
}

void showDataDevice() {
  Serial.println("showDataDevice");
  class CReadings r;
  getReadings(r);
  std::lock_guard<std::mutex> lck(display_mtx);
  u8x8.setFont(u8x8_font_chroma48medium8_r);
  char buffer[10];
  sprintf(buffer, "%2.1f", r.temperature);
  u8x8.drawString(0, 0, "T:"); u8x8.drawString(2, 0, buffer);
  sprintf(buffer, "%2.1f", r.temperature1);
  u8x8.drawString(0, 1, "1:"); u8x8.drawString(2, 1, buffer);
  sprintf(buffer, "%2.1f", r.temperature2);
  u8x8.drawString(8, 1, "2:"); u8x8.drawString(11, 1, buffer);
  sprintf(buffer, "%2.1f", r.rtemperature1);
  u8x8.drawString(0, 2, "R:"); u8x8.drawString(2, 2, buffer);
  sprintf(buffer, "%2.1f", r.rtemperature2);
  u8x8.drawString(8, 2, "R:"); u8x8.drawString(11, 2, buffer);
  sprintf(buffer, "%2.1f", tmin);
  u8x8.drawString(0, 3, "L:"); u8x8.drawString(2, 3, buffer);
  sprintf(buffer, "%2.1f", tmax);
  u8x8.drawString(8, 3, "H:"); u8x8.drawString(11, 3, buffer);
}

void showDataConfig() {
   char buffer[30];
  std::lock_guard<std::mutex> lck(display_mtx);
  u8x8.setFont(u8x8_font_chroma48medium8_r);
  u8x8.drawString(0, 0, device_name);
  time_t t=  time(NULL);
  sprintf(buffer,"%ld",t);
  u8x8.drawString(0, 1,buffer);
  struct tm* tm_info;

  tm_info = localtime(&t);
  strftime(buffer, 26, "%H:%M:%S", tm_info);
  u8x8.drawString(0, 2,buffer);
  u8x8.drawString(0, 3,"Heating ip");
  u8x8.drawString(0, 4,iph);
  u8x8.drawString(0, 5,"Cooling ip");
  u8x8.drawString(0, 6,ipc);
}

void showDataBase() {
  String local_ip = ip2Str(WiFi.localIP());

  std::lock_guard<std::mutex> lck(display_mtx);
  u8x8.setFont(u8x8_font_chroma48medium8_r);

  bool written = false;
  if (haveHeater() && state == 'H') {
    if (red_state == FOUND_SWITCH) {
      u8x8.drawString(PLUG_STATUSX, STATUS_LINE, "Heat  ");
    } else {
      blinkText(PLUG_STATUSX, STATUS_LINE, "E:Heat");
    }
    written = true;
  }

  if (haveCooler() && state == 'C') {
    if (green_state == FOUND_SWITCH) {
      u8x8.drawString(PLUG_STATUSX, STATUS_LINE, "Cool  ");
    } else {
      blinkText(PLUG_STATUSX, STATUS_LINE, "E:Cool");
    }
      written = true;

  }
  if (!written){
    u8x8.drawString(PLUG_STATUSX, STATUS_LINE, " off ");
  }

  if (white_state == WIFI_CONF) {
    blinkText(WIFI_STATUSX, 7, "WiFi: Setup");
  } else if (white_state == WIFI_CONNECTED) {
    u8x8.drawString(WIFI_STATUSX, 7, local_ip.c_str());
  } else if (white_state == BOOT) {
    blinkText(WIFI_STATUSX, 7, "WiFi: Boot");
  }

  writeTmin();
  writeTmax();
}

void showData() {
  if (display_mode != DATA) return;



  blink = !blink;
  switch (data_mode) {
    case DATA_BASE:
      showDataBase();
      break;
    case DATA_DEVICE:
      showDataDevice();
      break;
    case DATA_CONFIG:
      showDataConfig();
      break;
  }
}

void statusTask( void * parameter) {

  int count = 0;


  for (;;) {
    count++;
    if ((count % 200) == 0) {

    }
    if (display_mode == DATA) {
      showData();
    }


    delay(BLINK_TIME);
  }
}

bool tasmota_switch_h(bool on) {


  HTTPClient http;
  String command = String("http://") + String(iph) + String("/cm?cmnd=Power%20") + (on ? String("On") : String("off"));
  Serial.println(command);
  http.begin(command.c_str());
  int err = http.GET();

  if (err == 200) {

    return true;
  }
  String body = http.getString();
  Serial.println(body);
  return false;
}

bool tasmota_switch_c(bool on) {

  HTTPClient http;
  String command = String("http://") + String(ipc) + String("/cm?cmnd=Power%20") + (on ? String("On") : String("off"));
  Serial.println(command);
  http.begin(command.c_str());
  int err = http.GET();

  if (err == 200) {

    return true;
  }
  String body = http.getString();
  Serial.println(body);
  return false;
}

bool tuya_switch_h(bool on) {
  return tuya->TCommand(tuya_deviceh, (String("{\"commands\":[{\"code\":\"") + tuya_switch_command_h + String("\",\"value\":") + String(on ? "true" : "false") + String("}]}")).c_str());
}


bool switch_h(bool on) {
 
  if (smart == TUYA) {
    if (strlen(tuya_deviceh) == 0) return true;
    return tuya_switch_h(on);
  } else {
    return tasmota_switch_h(on);
  }
  return false;
}

bool tuya_switch_c(bool on) {
  if (strlen(tuya_devicec) == 0) return true;

  return tuya->TCommand(tuya_devicec, (String("{\"commands\":[{\"code\":\"") + tuya_switch_command_c + String("\",\"value\":") + String(on ? "true" : "false") + String("}]}")).c_str());
}

bool switch_c(bool on) {
  if (smart == TUYA) {
    return tuya_switch_c(on);
  } else {
    return tasmota_switch_c(on);
  }
  return false;
}

double calibrate(double t) {

  if (t < device_point[0]) {
    if (device_point[0] < device_point[1] && real_point[0] < real_point[1]) {
      return real_point[0] + ( t - device_point[0]) / (device_point[1] - device_point[0]) * (real_point[1] - real_point[0]);
    }
  }
  for (auto i = 0; i < MAX_CALIB - 1; i++) {
    if (t >= device_point[i] && t <= device_point[i + 1]) {
      return real_point[i] + ( t - device_point[i]) / (device_point[i + 1] - device_point[i]) * (real_point[i + 1] - real_point[i]);
    }
  }
  if (device_point[MAX_CALIB - 2] < device_point[MAX_CALIB - 1] && real_point[MAX_CALIB - 2] < real_point[MAX_CALIB - 1]) {
    return real_point[MAX_CALIB - 2] + ( t - device_point[MAX_CALIB - 2]) / (device_point[MAX_CALIB - 1] - device_point[MAX_CALIB - 2]) * (real_point[MAX_CALIB - 1] - real_point[MAX_CALIB - 2]);
  }
  return t;
}

void findTuyaSwitches() {
  if (tuya_switch_command_h.length() == 0 && strlen(tuya_deviceh) > 0) {

    Serial.println("Get H Switch");
    tuya->TGetSwitch(tuya_deviceh,  tuya_switch_command_h);

  } else if (tuya_switch_command_h.length() > 0) {
    red_state = FOUND_SWITCH;
  }
  if (tuya_switch_command_c.length() == 0 && strlen(tuya_devicec) > 0) {
    Serial.println("Get C Switch");
    tuya->TGetSwitch(tuya_devicec,  tuya_switch_command_c);

  } else if (tuya_switch_command_c.length() > 0) {
    green_state = FOUND_SWITCH;
  }
}

bool tasmota_on(char *ip, bool &on) {

  if (strlen(ip) == 0) return true;
  HTTPClient http;
  String command = String("http://") + String(ip) + String("/cm?cmnd=Power%20");
  http.begin(command.c_str());
  int err = http.GET();

  String body = http.getString();
  Serial.println(body);
  if (err == 200) {

    if (body.indexOf("ON") != -1) {
      on = true;
    } else {
      on = false;
    }
    return true;
  }
  return false;
}

void findTasmotaSwitches() {

  bool on;
  if (red_state != FOUND_SWITCH && tasmota_on(iph, on)) {
    red_state = FOUND_SWITCH;
  }
  if (green_state != FOUND_SWITCH && tasmota_on(ipc, on)) {
    green_state = FOUND_SWITCH;
  }
}

void findSwitches() {

  if (smart == TUYA) {
    findTuyaSwitches();
  } else {
    findTasmotaSwitches();
  }

}

#define AVERAGE 0
#define ONE_CALIB 2
#define TWO_CALIB 3
#define ONE_RAW 4
#define TWO_RAW 5
#define CALIB 1

void Task1code( void * parameter) {

  class CReadings r;
  for (;;) {

    if (digitalRead(BUTTON_UP) == LOW) {
      if (white_state == WIFI_CONF) {
        resetFunc();
      }
    }
    getReadings(r);


    char buffer[60];
    sprintf(buffer, "%2.2f %2.2f %2.2f", r.temperature, r.temperature1, r.temperature2);
    //Serial.print("display mode:");Serial.println(display_mode);
    if (display_mode == DATA && data_mode == DATA_BASE) {
      std::lock_guard<std::mutex> lck(display_mtx);
      char t[6];
      sprintf(t, "%2.1f", r.temperature);
      u8x8.setFont(u8x8_font_profont29_2x3_f);
      u8x8.drawString(0, 3, t);

      sprintf(t, "One: %2.1f", r.temperature1);
      u8x8.setFont(u8x8_font_chroma48medium8_r);
      u8x8.drawString(6, 2, t);
      sprintf(t, "Two:%2.1f", r.temperature2);
      u8x8.drawString(6, 6, t);
    }

    findSwitches();


    int successHon = -1;
    int successCon = -1;
    int successHoff = -1;
    int successCoff = -1;
    Serial.println(state);
    if (r.temperature1 < tmin && r.temperature2 < tmin) {
      if ( state != 'H' ) {
        successHon = switch_h(true);
      }
      if (state != 'H' )
      {
        successCoff = switch_c(false);
      }
      if (successHon == 1) {
        state = 'H';
      }
    } else if  (r.temperature1 > tmax || r.temperature2 > tmax) {
      if ( state != 'C' ) {
        successHoff = switch_h(false);
      }
      if (state != 'C' )
      {
        successCon = switch_c(true);
      }
      if (successHoff == 1 || successCon == 1 ) {
        state = 'C';
      }
    }


    delay(500);
  }

}


bool readSettings(char *buffer) {
  if (SPIFFS.exists("/settings.txt")) {
    File file = SPIFFS.open("/settings.txt", FILE_READ);

    if (file) {

      int pos = 0;

      while (file.available()) {
        //Serial.print(pos);
        buffer[pos++] = file.read();
        Serial.print(buffer[pos - 1]);
        if (buffer[pos - 1] == '}') break;
      }
      buffer[pos] = '\0';

      file.close();
      Serial.println("Read buffer");
      Serial.println(pos);
      Serial.println(buffer);
      return true;
    }

  }
  return false;
}

void handleGet() {
  char buffer[1000];
  if (readSettings(buffer)) {
    server.send(200, "text/plain", buffer);
  } else {
    server.send(500, "text/plain", "Setting cannot be read from spiffs file");
  }
}
bool readCalib(char *buffer) {
  if (SPIFFS.exists("/calib.txt")) {
    File file = SPIFFS.open("/calib.txt", FILE_READ);

    if (file) {

      int pos = 0;

      while (file.available()) {
        //Serial.print(pos);
        buffer[pos++] = file.read();
        if (buffer[pos - 1] == '\0') break;
        Serial.print(buffer[pos - 1]);

      }
      buffer[pos] = '\0';

      file.close();
      Serial.println("Read buffer");
      Serial.println(pos);
      Serial.println(buffer);
      return true;
    }

  }
  return false;
}


void setup(void) {
  Serial.begin(115200);

  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  tmin = 24;
  tmax = 40;
  tuya_keyh[0] = '\0';
  tuya_deviceh[0] = '\0';
  tuya_keyc[0] = '\0';
  tuya_devicec[0] = '\0';
  ipc[0] = '\0';
  iph[0] = '\0';

  for (auto i = 0; i < MAX_CALIB; i++) {
    real_point[i] = 0;
    device_point[i] = 0;
  }

  strcpy(device_name, "thermistor");

  strcpy(tuya_host, "https://openapi.tuyaeu.com");

  Serial.println("read settings");
  char buffer[1000];
  if (readSettings(buffer)) {

    DynamicJsonDocument root(5000);

    DeserializationError err = deserializeJson(root, buffer);

    if (!err) {
      if (root.containsKey("min")) {
        tmin = root["min"];
      } else {
        Serial.println("missing min key");
      }
      if (root.containsKey("max")) {
        tmax = root["max"];
      } else {
        Serial.println("missing max key");
      }
      if (root.containsKey("iph")) {
        strcpy(iph, root["iph"]);
      } else {
        Serial.println("missing iph key");
      }
      if (root.containsKey("ipc")) {
        strcpy(ipc, root["ipc"]);
      } else {
        Serial.println("missing ipc key");
      }
      if (root.containsKey("tuya_deviceh")) {
        strcpy(tuya_deviceh, root["tuya_deviceh"]);
      } else {
        Serial.println("missing tuya_deviceh key");
      }
      if (root.containsKey("tuya_keyh")) {
        strcpy(tuya_keyh, root["tuya_keyh"]);
      } else {
        Serial.println("missing tuya_keyh key");
      }
      if (root.containsKey("tuya_devicec")) {
        strcpy(tuya_devicec, root["tuya_devicec"]);
      } else {
        Serial.println("missing tuya_devicec key");
      }
      if (root.containsKey("tuya_keyc")) {
        strcpy(tuya_keyc, root["tuya_keyc"]);
      } else {
        Serial.println("missing tuya_keyc key");
      }
      if (root.containsKey("tuya_api_client")) {
        strcpy(tuya_api_client, root["tuya_api_client"]);
      } else {
        Serial.println("missing tuya_api_client key");
      }
      if (root.containsKey("tuya_api_key")) {
        strcpy(tuya_api_key, root["tuya_api_key"]);

      } else {
        Serial.println("missing tuya_api_key");
      }
      if (root.containsKey("mode")) {
        mode = root["mode"];

      } else {
        Serial.println("missing mode");
      }
      if (root.containsKey("smart")) {
        mode = root["smart"];
      } else {
        Serial.println("missing smart");
      }
    } else {
      Serial.println("json parse failed:");
      Serial.println(err.f_str());
      Serial.println(buffer);
    }
  }
  Serial.println("read calibration");
  if (readCalib(buffer)) {

    Serial.println("calib file");
    Serial.println(buffer);
    DynamicJsonDocument root(5000);

    DeserializationError err = deserializeJson(root, buffer);

    if (!err) {

      for (auto i = 0; i < MAX_CALIB; i++) {

        real_point[i] = root["real"][i];
        device_point[i] = root["device"][i];
        Serial.print(i); Serial.print(" Real:"); Serial.print(real_point[i]); Serial.print(" Device:"); Serial.println(device_point[i]);
      }
    } else {
      Serial.println("json parse failed:");
      Serial.println(err.f_str());
      Serial.println(buffer);
    }
  }

  Serial.println("read eeprom");

  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  //pinMode(BUTTON_MENU, INPUT_PULLUP);


  EEPROM.begin(EEPROM_SIZE);
  init_eeprom();
  writeId("thermistor");
  EEPROM.commit();
  readId(device_name);
  EEPROM.end();

  if (strcmp(device_name, "") == 0) {
    Serial.print("reset blank device name");
    EEPROM.begin(EEPROM_SIZE);
    writeId("thermistor");
    EEPROM.commit();
    EEPROM.end();
  }
  Serial.println(device_name);

  Serial.println("create led status task");
  u8x8.setI2CAddress(0x3F * 2);
  u8x8.begin();
  u8x8.clearDisplay();

  white_state = BOOT;
  xTaskCreatePinnedToCore(
    statusTask,   /* Task function. */
    "Status",     /* name of task. */
    2000,       /* Stack size of task */
    NULL,        /* parameter of the task */
    1,           /* priority of the task */
    &TaskStatus,      /* Task handle to keep track of created task */
    0);
  Serial.println("wifi connection");
  WiFiManagerParameter MDNSName("device_name", "Device Name", device_name, 19);
  wifiManager.addParameter(&MDNSName);

  wifiManager.setEnableConfigPortal(false);

  if (!wifiManager.autoConnect("AutoConnectAP", "password")) {

    white_state = WIFI_CONF;
    std::lock_guard<std::mutex> lck(display_mtx);
    u8x8.setFont(u8x8_font_chroma48medium8_r);
    u8x8.clearDisplay();
    u8x8.drawString(0, 0, "Setup Wifi");
    u8x8.drawString(0, 1, "Connect to");
    u8x8.drawString(0, 2, "ConfWiFi then");
    u8x8.drawString(0, 3, "open 192.168.4.1");
    u8x8.drawString(0, 4, "In Browser");
    wifiManager.startConfigPortal("ConfWiFi");
    const char *new_name = MDNSName.getValue();
    if (strcmp(new_name, device_name) != 0) {
      EEPROM.begin(EEPROM_SIZE);
      writeId(new_name);
      EEPROM.commit();
      EEPROM.end();
      strcpy(device_name, new_name);
    }
  }


  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);


  white_state = WIFI_CONNECTED;

  xTaskCreatePinnedToCore(
    Task1code,   /* Task function. */
    "Task1",     /* name of task. */
    8000,       /* Stack size of task */
    NULL,        /* parameter of the task */
    1,           /* priority of the task */
    &Task1,      /* Task handle to keep track of created task */
    0);          /* pin task to core 0 */

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin(device_name)) {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);
  server.on("/setting", handleSetting);
  server.on("/settings", handleSetting);
  server.on("/calib", handleCalib);
  server.on("/calibrate", handleCalib);
  server.on("/get", handleGet);
  /*handling uploading firmware file */
  server.on("/login", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", loginIndex);
  });
  server.on("/serverIndex", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });

  server.begin();
  Serial.println("HTTP server started");

  if (strlen(tuya_api_client) && strlen(tuya_api_key) && strlen(tuya_host) ) {


    tuya = new TuyaAuth(tuya_host, tuya_api_client, tuya_api_key);
  }


}

#define MODE_LINE 0
#define MODE_POS  7
void show_mode() {
  if (data_mode != DATA_BASE) return;
  std::lock_guard<std::mutex> lck(display_mtx);
  u8x8.setFont(u8x8_font_chroma48medium8_r);
  switch (mode) {
    case DATA:
      u8x8.drawString(MODE_POS, MODE_LINE, "data   ");
      break;
    case CALIB:
      u8x8.drawString(MODE_POS, MODE_LINE, "calib  ");
      break;
    case ONE_RAW:
      u8x8.drawString(MODE_POS, MODE_LINE, "raw:one");
      break;
    case TWO_RAW:
      u8x8.drawString(MODE_POS, MODE_LINE, "raw:two");
      break;
    case ONE_CALIB:
      u8x8.drawString(MODE_POS, MODE_LINE, "cal:one");
      break;
    case TWO_CALIB:
      u8x8.drawString(MODE_POS, MODE_LINE, "cal:two");
      break;
  }

}

void writeMode() {

  std::lock_guard<std::mutex> lck(display_mtx);
  u8x8.setFont(u8x8_font_chroma48medium8_r);
  switch (display_mode) {

    case SET_HIGH:
      u8x8.drawString(0, 4, (String("SET HIGH: ") + String(tmax)).c_str());
      break;
    case SET_LOW:
      u8x8.drawString(0, 4, (String("SET LOW: ") + String(tmin)).c_str());
      break;
    case SET_MODE: {

        String mode_string = "SET MODE: ";
        switch (mode) {
          case DATA:
            mode_string += "Data";
            break;
          case CALIB:
            mode_string += "Calib";
            break;
          case ONE_RAW:
            u8x8.drawString(MODE_POS, MODE_LINE, "raw:one");
            break;
          case TWO_RAW:
            u8x8.drawString(MODE_POS, MODE_LINE, "raw:two");
            break;
          case ONE_CALIB:
            u8x8.drawString(MODE_POS, MODE_LINE, "cal:one");
            break;
          case TWO_CALIB:
            u8x8.drawString(MODE_POS, MODE_LINE, "cal:two");
            break;
        }

        u8x8.drawString(0, 4, mode_string.c_str());
        break;
      }
    case SET_H:
      if (state == 'H') {
        u8x8.drawString(MODE_POS, MODE_LINE, "Heat: On");
      } else {
        u8x8.drawString(MODE_POS, MODE_LINE, "Heat: Off");
      }
    case SET_C:
      if (state == 'C') {
        u8x8.drawString(MODE_POS, MODE_LINE, "Cool: On");
      } else {
        u8x8.drawString(MODE_POS, MODE_LINE, "Cool: Off");
      }
  }

}

#define REPEAT_TIME 200
#define REPEAT_TIME_INC 5
long ms = 0;
int repeat_count = 0;
bool changed = false;
void loop(void) {

  menu_button.loop(); // MUST call the loop() function first

  server.handleClient();

  bool do_action = false;

  show_mode();
  if (menu_button.isPressed()) {
    //if (digitalRead(BUTTON_MENU) == HIGH) {

    Serial.println("Seen menu");
    if (mode != DISPLAY  && changed) {
      saveSettings();
      changed = false;
    }
    display_mode = (display_mode + 1) % DISPLAY_STATES;
    {

      std::lock_guard<std::mutex> lck(display_mtx);

      u8x8.clearDisplay();
    }
    if (display_mode == SET_H && strlen(tuya_deviceh) == 0) display_mode++;
    if (display_mode == SET_C && strlen(tuya_devicec) == 0) display_mode = 0;

    writeMode();

  }

  if (digitalRead(BUTTON_UP) == LOW) {
    if ( millis() > ms + REPEAT_TIME  ) {
      do_action = true;
      repeat_count++;
      ms = millis();
    }
    Serial.print("Seen up:"); Serial.println(repeat_count);
    if (do_action) {
      switch (display_mode) {

        case DATA: {
            std::lock_guard<std::mutex> lck(display_mtx);
            u8x8.begin();
            break;
          }
        case SET_HIGH:
          tmax += 0.1 + (0.1 * (repeat_count > 2 ? repeat_count - 3 : 0 ));
          writeMode();
          changed = true;
          break;
        case SET_LOW:
          tmin += + (0.1 * (repeat_count > 2 ? repeat_count - 3 : 0 ));
          writeMode();
          changed = true;
          break;
        case SET_MODE:
          mode = (mode + 1) % MODE_STATES;
          writeMode();
          changed = true;
        case SET_H:
          switch_h(true);

          state = 'H';
          break;
        case SET_C:
          switch_h(true);

          state = 'C';
          break;
      }
    }
  }
  if (digitalRead(BUTTON_DOWN) == LOW) {
    if ( millis() > ms + REPEAT_TIME) {
      do_action = true;
      repeat_count++;
      ms = millis();
    }
    if (do_action) {
      Serial.println("Seen down");

      switch (display_mode) {
        case DATA:  {
            data_mode = (data_mode + 1) % 3;
            std::lock_guard<std::mutex> lck(display_mtx);
            u8x8.begin();
            break;
          }
        case SET_HIGH:
          tmax -= 0.1 + (0.1 * (repeat_count > 2 ? repeat_count - 3 : 0 ));;
          writeMode();
          changed = true;
          break;
        case SET_LOW:
          tmin -= 0.1 + (0.1 * (repeat_count > 2 ? repeat_count - 3 : 0 ));;
          writeMode();
          changed = true;
          break;
        case SET_MODE:
          mode = (mode + MODE_STATES - 1) % MODE_STATES;
          changed = true;
          writeMode();
          break;
        case SET_H:
          switch_h(false);

          state = 'C';
          break;
        case SET_C:
          switch_h(false);

          state = 'H';
          break;
      }
    }
  }
  if (digitalRead(BUTTON_DOWN) == HIGH && digitalRead(BUTTON_UP) == HIGH) {
    repeat_count = 0;
    writeMode();
  }


  delay(10);//allow the cpu to switch to other tasks
  //int btnState = button.getState();
  //Serial.println(btnState);

}
