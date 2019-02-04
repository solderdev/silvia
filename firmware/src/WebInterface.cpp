#include "WebInterface.hpp"
#include "private_defines.hpp"
#include "TaskConfig.hpp"
#include <FS.h>  // needed to fix asyncwebserver compile issues
#include <WiFi.h>
#include <ArduinoOTA.h>
// #include <SPIFFS.h>
#include "Sensors.hpp"
#include "WaterControl.hpp"
#include "SSRHeater.hpp"
#include "SSRPump.hpp"
#include "HWInterface.hpp"
#include "PIDHeater.hpp"
#include <cstring>
#include "Pins.hpp"

// from https://techtutorialsx.com/2017/12/16/esp32-arduino-async-http-server-serving-a-html-page-from-flash-memory/
// HTML compressor: https://htmlcompressor.com/compressor/ or https://www.willpeavy.com/minifier/
// text to C converter: http://tomeko.net/online_tools/cpp_text_escape.php?lang=en
static const char HTML_CODE[] = "<!DOCTYPE html> <html> <head> <title>Silvia</title> <meta name=viewport content=\"width=device-width, initial-scale=1\"> <link rel=icon href=data:,> <link rel=stylesheet type=text/css href=style.css> <script>function DisplayCurrentTime(){var b=new Date();var a=b.getHours()<10?\"0\"+b.getHours():b.getHours();var c=b.getMinutes()<10?\"0\"+b.getMinutes():b.getMinutes();var e=b.getSeconds()<10?\"0\"+b.getSeconds():b.getSeconds();time=a+\":\"+c+\":\"+e;var d=document.getElementById(\"currentTime\");d.innerHTML=time}function GetReadings(){var a=new XMLHttpRequest();a.onreadystatechange=function(){if(this.status==200){if(this.responseXML!=null){var c;var b=this.responseXML.getElementsByTagName(\"rd\").length;for(c=0;c<b;c++){document.getElementsByClassName(\"rd\")[c].innerHTML=this.responseXML.getElementsByTagName(\"rd\")[c].childNodes[0].nodeValue}b=this.responseXML.getElementsByTagName(\"pwr\").length;for(c=0;c<b;c++){document.getElementsByClassName(\"pwr\")[c].innerHTML=this.responseXML.getElementsByTagName(\"pwr\")[c].childNodes[0].nodeValue}DisplayCurrentTime()}}};a.open(\"GET\",\"/update_readings\",true);a.send(null);setTimeout(\"GetReadings()\",1000)}function powerOnButtonFunction(){var a=new XMLHttpRequest();a.open(\"POST\",\"/on\",true);a.send(null)}function powerOffButtonFunction(){var a=new XMLHttpRequest();a.open(\"POST\",\"/off\",true);a.send(null)}function resetButtonFunction(){var a=new XMLHttpRequest();a.open(\"POST\",\"/reset\",true);a.send(null)}document.addEventListener(\"DOMContentLoaded\",function(){GetReadings()},false);</script> </head> <body> <h1>Silvia</h1> <h3>Last update: <span id=currentTime></span></h3> <p>Status: <span class=pwr>...</span></p> <table> <tr> <th width=150px>SENSOR</th> <th width=100px>VALUE</th> </tr> <tr> <td><span class=sensor>Top</span></td> <td><span class=rd>...</span> &deg;C</td> </tr> <tr> <td><span class=sensor>Side</span></td> <td><span class=rd>...</span> &deg;C</td> </tr> <tr> <td><span class=sensor>Average (%TARGETTEMP_BOILER%)</span></td> <td><span class=rd>...</span> &deg;C</td> </tr> <tr> <td><span class=sensor>Brewhead (%TARGETTEMP_BREWHEAD%)</span></td> <td><span class=rd>...</span> &deg;C</td> </tr> <tr> <td><span class=sensor>Heater</span></td> <td><span class=rd>...</span> &#37;</td> </tr> <tr> <td><span class=sensor>Shot Time</span></td> <td><span class=rd>...</span> s</td> </tr> </table> <button onclick=powerOnButtonFunction()>Power On</button> <button onclick=powerOffButtonFunction()>Power Off</button> <button onclick=resetButtonFunction()>Reset</button> </body> </html>";
static const char CSS_CODE[] = "body{text-align:center;font-family:\"Trebuchet MS\",Arial}table{border-collapse:collapse;margin-left:auto;margin-right:auto}th{padding:16px;background-color:#0043af;color:white}tr{border:1px solid #ddd;padding:16px}td{border:0;padding:16px}.sensor{color:white;font-weight:bold;background-color:#bcbcbc;padding:8px}.button{display:inline-block;background-color:#008cba;border:0;border-radius:4px;color:white;padding:16px 40px;text-decoration:none;font-size:12px;margin:2px;cursor:pointer}.button2{background-color:#f44336}";
static const char XML_CODE[] = "<?xml version = \"1.0\"?>\n<inputs>\n<rd>\n%TEMP_TOP%\n</rd>\n<rd>\n%TEMP_SIDE%\n</rd>\n<rd>\n%TEMP_AVG%\n</rd>\n<rd>\n%TEMP_BREWHEAD%\n</rd>\n<rd>\n%PERC_HEATER%\n</rd>\n<rd>\n%SHOT_TIME%\n</rd>\n<pwr>\n%POWERSTATE%\n</pwr>\n</inputs>\n";

static const char INFLUX_URL[] = "http://" PRIVATE_INFLUXDB_HOST_IP "/write?db=" PRIVATE_INFLUXDB_NAME;

static WebInterface *instance = nullptr;

WebInterface::WebInterface() :
  influx_sem_update(nullptr),
  server_(80),
  task_handle_http_(nullptr),
  task_handle_influx_(nullptr),
  task_handle_ota_(nullptr)
{
  if (instance)
  {
    Serial.println("ERROR: more than one WebInterface generated");
    ESP.restart();
    return;
  }

  Serial.println(INFLUX_URL);

  // // initialize SPIFFS
  // if (!SPIFFS.begin(true))
  // {
  //   Serial.println("SPIFFS: An Error has occurred while mounting");
  //   return;
  // }

  if (xTaskCreate(&WebInterface::task_http_wrapper, "task_http", TaskConfig::WiFi_http_stacksize, this, TaskConfig::WiFi_http_priority, &task_handle_http_) != pdPASS)
    Serial.println("WebInterface ERROR init failed");

  instance = this;
}

void WebInterface::wifiReconnect()
{
  WiFi.disconnect();
  vTaskDelay(pdMS_TO_TICKS(1000));
  WiFi.enableSTA(true);
  vTaskDelay(pdMS_TO_TICKS(1000));
  WiFi.begin(PRIVATE_SSID, PRIVATE_WIFIPW);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
}

void WebInterface::wifiCheckConnectionOrReconnect()
{
  if (WiFi.isConnected())
    return;
  
  Serial.println("WIFI down .. attempting connect!");
  wifiReconnect();
  
  Serial.println("WIFI reconnecting .. waiting for network");
  uint32_t trycount = 0;
  while (!WiFi.isConnected())
  {
    trycount++;
    if (trycount > 150)  // 15s
    {
      trycount = 0;
      Serial.println("WIFI still down .. attempting reconnect!");
      wifiReconnect();
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  Serial.println("WIFI up again!");
}

// replaces placeholder with values in static html
static String processor_static(const String& var)
{
  if (var == "TARGETTEMP_BOILER")
  {
    char str[6];
    snprintf(str, sizeof(str), "%.1f", WaterControl::getInstance()->getBoilerPID()->getTarget());
    return String(str);
  }
  else if (var == "TARGETTEMP_BREWHEAD")
    return String(90);
  
  return String();
}

// replaces placeholder with values in xml file
static String processor_xml(const String& var)
{
  if (var == "TEMP_TOP")
    return String(SensorsHandler::getTempBoilerTop());
  else if (var == "TEMP_SIDE")
    return String(SensorsHandler::getTempBoilerSide());
  else if (var == "TEMP_AVG")
    return String(SensorsHandler::getTempBoilerAvg());
  else if (var == "TEMP_BREWHEAD")
    return String(SensorsHandler::getTempBrewhead());
  else if (var == "PERC_HEATER" && SSRHeater::getInstance())
    return String(SSRHeater::getInstance()->getPWM());
  else if (var == "SHOT_TIME" && WaterControl::getInstance())
    return String(WaterControl::getInstance()->getShotTime()/1000.0f);
  else if (var == "POWERSTATE" && HWInterface::getInstance())
    return String((HWInterface::getInstance()->isActive()) ? "ON" : "OFF");

  return String();
}

void WebInterface::task_http_wrapper(void *arg)
{
  static_cast<WebInterface *>(arg)->task_http();
}
void WebInterface::task_http()
{
  Serial.print("Attempting to connect to SSID: ");
  Serial.println(PRIVATE_SSID);

  wifiCheckConnectionOrReconnect();

//  // be reachable under silvia.local
//  if (!MDNS.begin("silvia"))
//    Serial.println("Error setting up MDNS responder!");
//  else
//    Serial.println("mDNS responder started");
//  MDNS.addService("http", "tcp", 80);

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // FIXME: this crashes?? SPIFFS seems to conflict with hardware timers!
  // server_.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html").setTemplateProcessor(processor_static);
  // this alternative just crashes less/same :(
  // route for root / web page
  // server_.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
  //   request->send(SPIFFS, "/index.html", String(), false, processor_static);
  // });
  // it works with the html in flash
  server_.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", HTML_CODE, processor_static);
  });
  
  // route to load style.css file
  // server_.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
  //   request->send(SPIFFS, "/style.css", "text/css");
  // });
  server_.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/css", CSS_CODE);
  });

  // route to update values
  // server_.on("/update_readings", HTTP_GET, [](AsyncWebServerRequest *request) {
  //   request->send(SPIFFS, "/readings.xml", "text/xml", false, processor_xml);
  // });
  server_.on("/update_readings", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/xml", XML_CODE, processor_xml);
  });

  // route to power on machine
  server_.on("/on", HTTP_POST, [](AsyncWebServerRequest *request) {
    HWInterface::getInstance()->powerOn();
    request->send(200);
  });
  
  // route to power off machine
  server_.on("/off", HTTP_POST, [](AsyncWebServerRequest *request) {
    HWInterface::getInstance()->powerOff();
    request->send(200);
  });
  
  // route to reset
  server_.on("/reset", HTTP_POST, [](AsyncWebServerRequest *request) {
    request->send(200);
    ESP.restart();
  });

  // respond to GET requests on URL /heap
  server_.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });

  // start webserver
  server_.begin();

  // start database task when WIFI is available
  if (task_handle_influx_ == nullptr)
    xTaskCreate(task_influx_wrapper, "task_influx", TaskConfig::WiFi_influx_stacksize, this, TaskConfig::WiFi_influx_priority, &task_handle_influx_);
  
  // start OTA task when WIFI is available
  if (task_handle_ota_ == nullptr)
    xTaskCreate(task_ota_wrapper, "task_ota", TaskConfig::WiFi_ota_stacksize, this, TaskConfig::WiFi_ota_priority, &task_handle_ota_);
  
  while (1)
  {
    wifiCheckConnectionOrReconnect();

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void WebInterface::task_influx_wrapper(void *arg)
{
  static_cast<WebInterface *>(arg)->task_influx();
  vTaskDelete(NULL);
}
void WebInterface::task_influx()
{
  esp_err_t err = ESP_FAIL;
  int response_code = 0;

  // sync semaphore
  if (influx_sem_update == nullptr)
    influx_sem_update = xSemaphoreCreateBinary();
  
  if (influx_sem_update == nullptr)
  {
    Serial.println("influx_sem_update creation failed!!");
    return;
  }

  while(1)
  {
    // if we had an error or start for the first time, re-init client
    if (err != ESP_OK || response_code != 204)
    {
      // make sure the config struct is zeroed out
      std::memset(&http_client_config_, 0, sizeof(http_client_config_));

      // set config for client init
      http_client_config_.url = INFLUX_URL;
      http_client_config_.port = 8086;
      http_client_config_.method = HTTP_METHOD_POST;
      http_client_config_.timeout_ms = 500;
      http_client_ = esp_http_client_init(&http_client_config_);
    }

    if (xSemaphoreTake(influx_sem_update, portMAX_DELAY) == pdTRUE)
    {
      if (WiFi.isConnected())
      {
        uint32_t httpclient_start_ms = millis();

        // re-set client for next request
        esp_http_client_set_url(http_client_, INFLUX_URL);
        esp_http_client_set_method(http_client_, HTTP_METHOD_POST);
        esp_http_client_set_header(http_client_, "Content-Type", "application/json");

        String data = "";
        data += "temperature,pos=top value=" + String(SensorsHandler::getTempBoilerTop()) + "\n";
        data += "temperature,pos=side value=" + String(SensorsHandler::getTempBoilerSide()) + "\n";
        data += "temperature,pos=brewhead value=" + String(SensorsHandler::getTempBrewhead()) + "\n";
        data += "temperature,pos=avg value=" + String(SensorsHandler::getTempBoilerAvg()) + "\n";

        data += "power,device=heater value=" + String(SSRHeater::getInstance()->getPWM()) + "\n";
        data += "power,device=pump value=" + String(SSRPump::getInstance()->getPWM()) + "\n";

        data += "pid,part=p value=" + String(WaterControl::getInstance()->getBoilerPID()->getPShare()) + "\n";
        data += "pid,part=i value=" + String(WaterControl::getInstance()->getBoilerPID()->getIShare()) + "\n";
        data += "pid,part=d value=" + String(WaterControl::getInstance()->getBoilerPID()->getDShare()) + "\n";
        data += "pid,part=u value=" + String(WaterControl::getInstance()->getBoilerPID()->getUncorrectedOutput());

        err = esp_http_client_set_post_field(http_client_, data.c_str(), data.length());
        if (err != ESP_OK)
        {
          Serial.println("INFLUX HTTP esp_http_client_set_post_field issue: " + String(esp_err_to_name(err)));
          esp_http_client_cleanup(http_client_);
          continue;
        }

        err = esp_http_client_perform(http_client_);
        if (err != ESP_OK)
        {
          Serial.println("INFLUX HTTP esp_http_client_perform issue: " + String(esp_err_to_name(err)));
          esp_http_client_cleanup(http_client_);
          continue;
        }

        response_code = esp_http_client_get_status_code(http_client_);
        if (response_code != 204)
        {
          Serial.println("INFLUX HTTP ERROR - response was: " + String(response_code));
          esp_http_client_cleanup(http_client_);
          continue;
        }

        // signal, if the operation took longer than expected
        if (millis() - httpclient_start_ms > 500)
          Serial.println("INFLUX duration longer than expected: " + String(millis() - httpclient_start_ms));
      }
    }
  }
  esp_http_client_cleanup(http_client_);
}

void WebInterface::updateInfluxDB()
{
  if (instance && instance->influx_sem_update)
    xSemaphoreGive(instance->influx_sem_update);
}

void WebInterface::task_ota_wrapper(void *arg)
{
  static_cast<WebInterface *>(arg)->task_ota();
}
void WebInterface::task_ota()
{
  // over-the-air update:
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      HWInterface::getInstance()->powerOff();

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      // SPIFFS.end();
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd of OTA ..");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
  ArduinoOTA.begin();
  
  while(1)
  {
    vTaskDelay(pdMS_TO_TICKS(2));
    
    if (WiFi.isConnected())
      ArduinoOTA.handle();
    else
      vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
