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

#include <TM1637Display.h>
#include <aREST.h>
#include <EEPROM.h>
#define EEPROM_SIZE 100

#include <ezButton.h>
#include <SHA256.h>
#include "mbedtls/md.h"

#include "time.h"
#include "SPIFFS.h"
#include "TuyaAuth.h"

void(* resetFunc) (void) = 0; //declare reset function @ address 0


const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 3600;


#define BUTTON_PIN 15
ezButton button(BUTTON_PIN);  // create ezButton object that attach to pin 7;

#define CLK 18 //blue
#define DIO 19 // green
//    A1
// F2    B3
//    G4
// E5    C4
//    D8
const uint8_t SEG_DONE[] = {
  SEG_B | SEG_C | SEG_D | SEG_E | SEG_G, // d
  SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F, // O
  SEG_C | SEG_E | SEG_G, // n
  SEG_A | SEG_D | SEG_E | SEG_F | SEG_G
}; // E
const uint8_t SEG_CONF[] = {
  SEG_B | SEG_C | SEG_D | SEG_E | SEG_G, // C
  SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F, // O
  SEG_C | SEG_E | SEG_G, // n
  SEG_A | SEG_D | SEG_E | SEG_F //F
};

const uint8_t SEG_CONN[] = {
  SEG_B | SEG_C | SEG_D | SEG_E | SEG_G, // C
  SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F, // O
  SEG_C | SEG_E | SEG_G, // n
  SEG_C | SEG_E | SEG_G, // n
}; // F

#define SHA256_HASH_SIZE 32


const uint8_t SEG_MINUS =  SEG_G; // -
TM1637Display display(CLK, DIO);
TuyaAuth *tuya;

uint8_t data[] = {0xff, 0xff, 0xff, 0xff};
uint8_t blank[] = {0x00, 0x00, 0x00, 0x00};


TaskHandle_t Task1;

#define VOLTS 3.2
#define RESISTANCEK 10
#define ADC_RANGE 4095.0
#define SMOOTH 20
WebServer server(80);

WiFiManager wifiManager;

const int thermistor_output1 = 35;
const int thermistor_output2 = 32;

float tmin, tmax;
char tuya_keyc[40];
char tuya_deviceh[40];
char tuya_keyh[40];
char tuya_devicec[40];
char tuya_ipc[21];
char tuya_iph[21];
char tuya_api_key[33];
char tuya_api_client[21];
char tuya_host[100];
String tuya_switch_c;
String tuya_switch_h;

char device_name[20];
#define MAX_CALIB 5
float real_point[MAX_CALIB];
float device_point[MAX_CALIB];

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


void handleRoot() {
  char buffer[60];
  double temperature1 = getThermistorReading(thermistor_output1);
  double temperature2 = getThermistorReading(thermistor_output2);
  double temperature = (temperature1 + temperature2) / 2;
  sprintf(buffer, " {\"c\":%f,\"low\":%f,\"high\":%f}", temperature, temperature1 < temperature2 ? temperature1 : temperature2, temperature1 > temperature2 ? temperature1 : temperature2);
  server.send(200, "text/plain", buffer);

}

void handleGet() {
  char buffer[1000];
  if (readSettings(buffer)) {
    server.send(200, "text/plain", buffer);
  } else {
    server.send(500, "text/plain", "Setting cannot be read from spiffs file");
  }
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
     <label for="api_host"><span>Tuya API url</span><input  id="api_host" type="text" maxlength="100"  name="tuya_host" value="%TUYA_HOST%"></label>
    <label for="api_id"><span>API Client Id</span><input  id="api_id" type="text" maxlength="20"  name="tuya_api_client" value="%TUYA_CLIENT%"></label>
    <label for="kh"><span>API Secret Key</span><input  id="api_key" type="text" maxlength="32" name="tuya_api_key" value="%TUYA_APIKEY%"></label>
    <label for="idh"><span>Heating Device Id</span><input  id="idh" type="text" maxlength="22"  name="tuya_deviceh" value="%TUYA_DEVICEH%"></label>
     <label for="idc"><span>Cooling Device Id</span><input  id="idc" type="text" maxlength="22"  name="tuya_devicec" value="%TUYA_DEVICEC%"></label>
    <input type="submit" value="Submit">
  </form>
 </div>
</body></html>)rawliteral";

const char calib_html[] PROGMEM = R"rawliteral(
  <title>Thermistor Calibration</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  </head><body>
  <div class="form-style-2">
  <div class="form-style-2-heading">Provide your reference points</div>
  <form action="/calibrate">
    <div>
    <label for="r1"><span>True </span><input id="r1" type="number" name="r1" value="%R1%"></label>
    <label for="d1"><span>Device</span><input id="d1" type="number" name="d1" value="%D1%"></label>
    </div>
    <input type="submit" value="Submit">
  </form>
 </div>
</body></html>)rawliteral";

void handleSetting() {
  bool reboot = false;
  bool changed = false;
  bool init = false;

  Serial.println("/setting");

  if (server.hasArg("min")) {
    float ntmin = strtof(server.arg("min").c_str(), NULL);
    if (tmin != ntmin){
      tmin = ntmin;
      changed = true;
    }
  }
  if (server.hasArg("max")) {
    float ntmax = strtof(server.arg("max").c_str(), NULL);
    if (tmax != ntmax){
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

  if (server.hasArg("tuya_ipc") ) {
    const char *tuya_device_arg = server.arg("tuya_ipc").c_str();
    if (strlen(tuya_device_arg) < 16 &&  strcmp(tuya_iph, tuya_device_arg) != 0) {
      strcpy(tuya_ipc, tuya_device_arg);
      changed = true;
      reboot = true;
    }
  }
  if (server.hasArg("tuya_iph") ) {
    const char *tuya_device_arg = server.arg("tuya_iph").c_str();
    if (strlen(tuya_device_arg) < 17 &&  strcmp(tuya_iph, tuya_device_arg) != 0) {
      strcpy(tuya_iph, tuya_device_arg);
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

  if (changed) {
    char buffer[1000];
    sprintf(buffer, "{\"min\":\"%f\",\"max\":\"%f\",\"tuya_deviceh\":\"%s\",\"tuya_keyh\":\"%s\",\"tuya_devicec\":\"%s\",\"tuya_keyc\":\"%s\",\"tuya_ipc\":\"%s\",\"tuya_iph\":\"%s\",\"tuya_api_key\": \"%s\",\"tuya_api_client\": \"%s\",\"tuya_host\": \"%s\"}",
            tmin, tmax, tuya_deviceh, tuya_keyh, tuya_devicec, tuya_keyc, tuya_ipc, tuya_iph, tuya_api_key, tuya_api_client, tuya_host);

    File file = SPIFFS.open("/settings.txt", FILE_WRITE);
    int l = strlen(buffer);

    for (int i = 0; i <= l; i++) {
      file.write(buffer[i]);
    }
    file.close();
  }
  String new_index_html = String(style_html)+String(index_html);
  new_index_html.replace("%MIN%", String(tmin, 2));
  new_index_html.replace("%MAX%", String(tmax, 2));
  new_index_html.replace("%TUYA_DEVICEC%", tuya_devicec);
  new_index_html.replace("%TUYA_DEVICEH%", tuya_deviceh);
  new_index_html.replace("%TUYA_KEYH%", tuya_keyh);
  new_index_html.replace("%TUYA_KEYC%", tuya_keyc);
  new_index_html.replace("%TUYA_IPH%", tuya_iph);
  new_index_html.replace("%TUYA_IPC%", tuya_ipc);
  new_index_html.replace("%TUYA_CLIENT%", tuya_api_client);
  new_index_html.replace("%TUYA_APIKEY%", tuya_api_key);
  new_index_html.replace("%TUYA_HOST%", tuya_host);

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
 
  bool changed = false;
  bool init = false;

  for (auto i = 0; i< MAX_CALIB; i++){
 
    String arg = server.arg(String("r")+String(i));
    if (atof(arg.c_str()) != real_point[i])  {
      real_point[i] = atof(arg.c_str());
      changed = true;
    }
    arg = server.arg(String("d")+String(i));
    if (atof(arg.c_str()) != real_point[i])  {
      device_point[i] = atof(arg.c_str());
      changed = true;
    }
  }

  if (changed) {
    String buffer = "{\"real\": [";
    for (auto i = 0;i < MAX_CALIB;i++){
        buffer += String(real_point[i]);
        if (i < MAX_CALIB-1){
          buffer +=",";
        }
    }
    for (auto i = 0;i < MAX_CALIB;i++){
        buffer += String(device_point[i]);
        if (i < MAX_CALIB-1){
          buffer +=",";
        }
    }
    buffer +="}";
    
    File file = SPIFFS.open("/calib.txt", FILE_WRITE);
    int l = buffer.length();

    for (int i = 0; i <= l; i++) {
      file.write(buffer[i]);
    }
    file.close();
  }
  String new_index_html = String(style_html)+String(calib_html);
  for (auto i = 0; i < MAX_CALIB;i++){
      new_index_html.replace(String("%D")+String(i)+String("%"), String(device_point[i]));
      new_index_html.replace(String("%R")+String(i)+String("%"), String(real_point[i]));
  }
  server.send(200, "text/html", new_index_html);

}




void Task1code( void * parameter) {

  char state = 'N';

  for (;;) {
    double temperature1 = getThermistorReading(thermistor_output1);
    double temperature2 = getThermistorReading(thermistor_output2);
    double temperature = (temperature1 + temperature2) / 2;

    char buffer[60];
    sprintf(buffer, "%2.2f %2.2f %2.2f", temperature, temperature1, temperature2);

    if (temperature < -10) {
      data[0] = SEG_MINUS;
      data[1] = display.encodeDigit(buffer[1]);
      data[2] = display.encodeDigit(buffer[2]);
      data[3] = display.encodeDigit(buffer[3]);
      display.setSegments(data);
    } else if (temperature < 0) {
      data[0] = SEG_MINUS;
      data[1] = display.encodeDigit(buffer[1]);
      data[2] = display.encodeDigit(buffer[2]);
      data[3] = display.encodeDigit(buffer[4]);
      display.setSegments(data);
    } else if (temperature < 10) {
      data[0] = 0;
      data[1] = display.encodeDigit(buffer[0]);
      data[2] = SEG_D;
      data[3] = display.encodeDigit(buffer[2]);
      display.setSegments(data);
    } else {
      int temp = temperature * 100;

      display.showNumberDecEx(temp, 0b01000000);

    }

    if (tuya_switch_h.length() == 0 && strlen(tuya_deviceh) > 0) {
      
     Serial.println("Get H Switch");
      tuya->TGetSwitch(tuya_deviceh,  tuya_switch_h);

    }
    if (tuya_switch_c.length() == 0 && strlen(tuya_devicec) > 0) {
   Serial.println("Get C Switch");
      tuya->TGetSwitch(tuya_devicec,  tuya_switch_c);

    }
    int successHon = -1;
    int successCon = -1;
    int successHoff = -1;
    int successCoff = -1;
    if (temperature1 < tmin && temperature2 < tmin) {
      if ( strlen(tuya_deviceh) > 0 && state != 'H' &&tuya_switch_h.length() >0) {
        
       
        successHon = tuya->TCommand(tuya_deviceh, (String("{\"commands\":[{\"code\":\"")+tuya_switch_h+String("\",\"value\":true}]}")).c_str());
      }
      if ( strlen(tuya_devicec) > 0  && state != 'H' &&tuya_switch_c.length() >0)
      {
        successCoff = tuya->TCommand(tuya_devicec, (String("{\"commands\":[{\"code\":\"")+tuya_switch_c+String("\",\"value\":false}]}")).c_str());
      }
      if (successHon == 1) {
        state = 'H';
      }
    } else if  (temperature1 > tmax || temperature2 > tmax) {
      if ( strlen(tuya_deviceh) > 0 && state != 'C'  &&tuya_switch_h.length() >0) {
        successHoff = tuya->TCommand(tuya_deviceh, "{\"commands\":[{\"code\":\"switch\",\"value\":false}]}");
      }
      if ( strlen(tuya_devicec) > 0 && state != 'C'  &&tuya_switch_c.length() >0)
      {
        successCon = tuya->TCommand(tuya_devicec, "{\"commands\":[{\"code\":\"switch\",\"value\":true}]}");
      }
      if (successHoff == 1) {
        state = 'C';
      }
    }


    delay(2000);
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


bool readCalib(char *buffer) {
  if (SPIFFS.exists("/calib.txt")) {
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
  tuya_ipc[0] = '\0';
  tuya_iph[0] = '\0';

 

  strcpy(tuya_host, "https://openapi.tuyaeu.com");
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
      if (root.containsKey("tuya_iph")) {
        strcpy(tuya_iph, root["tuya_iph"]);
      } else {
        Serial.println("missing tuya_iph key");
      }
      if (root.containsKey("tuya_ipc")) {
        strcpy(tuya_ipc, root["tuya_ipc"]);
      } else {
        Serial.println("missing tuya_ipc key");
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
    } else {
      Serial.println("json parse failed:");
      Serial.println(err.f_str());
      Serial.println(buffer);
    }
  }

   if (readSettings(buffer)) {

    DynamicJsonDocument root(5000);

    DeserializationError err = deserializeJson(root, buffer);

    if (!err) {
      
      for (auto i =0;i<MAX_CALIB;i++){
   
        real_point[i] = root["real"][i];
        device_point[i] = root["device"][i];
      }
    } else {
      Serial.println("json parse failed:");
      Serial.println(err.f_str());
      Serial.println(buffer);
    }
  }



  pinMode(BUTTON_PIN, INPUT_PULLUP); // config GIOP21 as input pin and enable the internal pull-up resistor

  display.setBrightness(0x0f);

  EEPROM.begin(EEPROM_SIZE);
  init_eeprom();
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

  WiFiManagerParameter MDNSName("device_name", "Device Name", device_name, 19);
  wifiManager.addParameter(&MDNSName);
  display.setSegments(SEG_CONN);
  wifiManager.setEnableConfigPortal(false);
  if (!wifiManager.autoConnect("AutoConnectAP", "password")) {
    display.setSegments(SEG_CONF);
    wifiManager.startConfigPortal("ConfigureWiFi");
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

  display.setSegments(SEG_DONE);

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
  server.on("/calib", handleCalib);
  server.on("/get", handleGet);
  /*handling uploading firmware file */
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

void loop(void) {
  static int counter = 0;
  button.loop(); // MUST call the loop() function first
  server.handleClient();

  int btnState = digitalRead(BUTTON_PIN);
  if (button.isPressed()) {
    counter++;
    display.showNumberDecEx(counter);
  }
  if(btnState== LOW){
      counter++;
      display.showNumberDecEx(counter);
   }
  delay(1000);//allow the cpu to switch to other tasks
  //int btnState = button.getState();
  //Serial.println(btnState);

}

void readId(char *buffer) {
  for (int i = 0; i < 20; i++) {

    buffer[i] = EEPROM.read(i);
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
