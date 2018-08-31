
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <InfluxDb.h>
#include "private_defines.h"

// idle: 0, main-loop: 1, wifi: 2, wifi_db: 3, pid: 5, sensors: 4
#define WIFI_TASK_STACKSIZE  4000u  // [words]
#define WIFI_TASK_PRIORITY   2
#define WIFI_DB_TASK_STACKSIZE  4000u  // [words]
#define WIFI_DB_TASK_PRIORITY   3
#define WIFI_BUFFER_SIZE     120    // number of saved samples

const char* ssid     = PRIVATE_SSID;     // your network SSID (name of wifi network)
const char* password = PRIVATE_WIFIPW;   // your network password

static uint32_t wifi_buffer_count = 0;  // current number of samples in buffer
static uint32_t wifi_buffer_idx_current = WIFI_BUFFER_SIZE - 1;   // index of newest value

static float wifi_buffer_temp_top[WIFI_BUFFER_SIZE];
static float wifi_buffer_temp_side[WIFI_BUFFER_SIZE];
static float wifi_buffer_temp_brewhead[WIFI_BUFFER_SIZE];
static uint8_t wifi_buffer_perc_heater[WIFI_BUFFER_SIZE];
static uint8_t wifi_buffer_perc_pump[WIFI_BUFFER_SIZE];
static float wifi_buffer_pid_p[WIFI_BUFFER_SIZE];
static float wifi_buffer_pid_i[WIFI_BUFFER_SIZE];
static float wifi_buffer_pid_d[WIFI_BUFFER_SIZE];
static float wifi_buffer_pid_u[WIFI_BUFFER_SIZE];

WiFiServer server(80);  // TODO maybe port to WebServer when more stable?

Influxdb influx(INFLUXDB_HOST);

static TaskHandle_t wifi_task_handle = NULL;
static TaskHandle_t wifi_db_task_handle = NULL;

SemaphoreHandle_t wifi_influxdb_sem_update = NULL;

static void wifi_task(void * pvParameters);
static void sendXMLFile(WiFiClient cl);
static void sendHTMLFile(WiFiClient cl);

void WIFI_setup(void)
{
  if (wifi_task_handle == NULL)
  {
    if (xTaskCreate(wifi_task, "task_wifi", WIFI_TASK_STACKSIZE, NULL, WIFI_TASK_PRIORITY, &wifi_task_handle) != pdPASS)
      return; // error
  }
  // sync semaphore
  if (wifi_influxdb_sem_update == NULL)
  {
    wifi_influxdb_sem_update = xSemaphoreCreateBinary();
    if (wifi_influxdb_sem_update == NULL)
      return; // error
  }
  influx.setDb("silvia");
}

static void wifi_task(void * pvParameters)
{
  Serial.print("Attempting to connect to SSID: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    vTaskDelay(pdMS_TO_TICKS(501));
    // Serial.print(".");
  }

  // be reachable under silvia.local
  if (!MDNS.begin("silvia"))
    Serial.println("Error setting up MDNS responder!");
  else
    Serial.println("mDNS responder started");

  MDNS.addService("http", "tcp", 80);

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  server.begin();

  // over-the-air update:
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
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

  // start database task when WIFI is available
  if (wifi_db_task_handle == NULL)
  {
    xTaskCreate(wifi_db_task, "task_db_wifi", WIFI_DB_TASK_STACKSIZE, NULL, WIFI_DB_TASK_PRIORITY, &wifi_db_task_handle);
  }
  
  while (1)
  {
    WIFI_service();
    ArduinoOTA.handle();
    vTaskDelay(pdMS_TO_TICKS(57));
  }
}

static void wifi_db_task(void * pvParameters)
{
  while(1)
  {
    if (xSemaphoreTake(wifi_influxdb_sem_update, portMAX_DELAY) == pdTRUE)
    {
      WIFI_sendInfluxDBdata();
    }
  }
}

void WIFI_updateInfluxDB(void)
{
  xSemaphoreGive(wifi_influxdb_sem_update);
}

void WIFI_addToBuffer(float temp_top,
                      float temp_side,
                      float temp_brewhead,
                      uint8_t perc_heater,
                      uint8_t perc_pump,
                      float pid_p,
                      float pid_i,
                      float pid_d,
                      float pid_u)
{
  // go to next slot in buffer
  wifi_buffer_idx_current += 1;
  wifi_buffer_idx_current %= WIFI_BUFFER_SIZE;

  wifi_buffer_temp_top[wifi_buffer_idx_current] = temp_top;
  wifi_buffer_temp_side[wifi_buffer_idx_current] = temp_side;
  wifi_buffer_temp_brewhead[wifi_buffer_idx_current] = temp_brewhead;
  wifi_buffer_perc_heater[wifi_buffer_idx_current] = perc_heater;
  wifi_buffer_perc_pump[wifi_buffer_idx_current] = perc_pump;
  wifi_buffer_pid_p[wifi_buffer_idx_current] = pid_p;
  wifi_buffer_pid_i[wifi_buffer_idx_current] = pid_i;
  wifi_buffer_pid_d[wifi_buffer_idx_current] = pid_d;
  wifi_buffer_pid_u[wifi_buffer_idx_current] = pid_u;

  if (++wifi_buffer_count > WIFI_BUFFER_SIZE)
    wifi_buffer_count = WIFI_BUFFER_SIZE;
}

void WIFI_resetBuffer(void)
{
  wifi_buffer_count = 0;
  wifi_buffer_idx_current = WIFI_BUFFER_SIZE - 1;
}

//void WIFI_sendInfluxDBdata(float temp_top,
//                           float temp_side,
//                           float temp_brewhead,
//                           uint8_t perc_heater,
//                           uint8_t perc_pump,
//                           float pid_p,
//                           float pid_i,
//                           float pid_d,
//                           float pid_u)
void WIFI_sendInfluxDBdata(void)
{
  InfluxData t1("temperature");
  t1.addTag("pos", "top");
  t1.addValue("value", wifi_buffer_temp_top[wifi_buffer_idx_current]);
  influx.prepare(t1);
  
  InfluxData t2("temperature");
  t2.addTag("pos", "side");
  t2.addValue("value", wifi_buffer_temp_side[wifi_buffer_idx_current]);
  influx.prepare(t2);
  
  InfluxData t3("temperature");
  t3.addTag("pos", "brewhead");
  t3.addValue("value", wifi_buffer_temp_brewhead[wifi_buffer_idx_current]);
  influx.prepare(t3);
  
  InfluxData t4("temperature");
  t4.addTag("pos", "avg");
  t4.addValue("value", (wifi_buffer_temp_top[wifi_buffer_idx_current] + wifi_buffer_temp_side[wifi_buffer_idx_current])/2);
  influx.prepare(t4);
  
  InfluxData p1("power");
  p1.addTag("device", "heater");
  p1.addValue("value", wifi_buffer_perc_heater[wifi_buffer_idx_current]);
  influx.prepare(p1);
  
  InfluxData p2("power");
  p2.addTag("device", "pump");
  p2.addValue("value", wifi_buffer_perc_pump[wifi_buffer_idx_current]);
  influx.prepare(p2);
  
  InfluxData tc_p("pid");
  tc_p.addTag("part", "p");
  tc_p.addValue("value", wifi_buffer_pid_p[wifi_buffer_idx_current]);
  influx.prepare(tc_p);
  
  InfluxData tc_i("pid");
  tc_i.addTag("part", "i");
  tc_i.addValue("value", wifi_buffer_pid_i[wifi_buffer_idx_current]);
  influx.prepare(tc_i);
  
  InfluxData tc_d("pid");
  tc_d.addTag("part", "d");
  tc_d.addValue("value", wifi_buffer_pid_d[wifi_buffer_idx_current]);
  influx.prepare(tc_d);
  
  InfluxData tc_u("pid");
  tc_u.addTag("part", "u");
  tc_u.addValue("value", wifi_buffer_pid_u[wifi_buffer_idx_current]);
  influx.prepare(tc_u);

  influx.write();
}

void WIFI_service(void)
{
  // Variable to store the HTTP request
  String header = "";
  boolean currentLineIsBlank = true;
  uint32_t time_connected;
  
  // Check if a client has connected
  WiFiClient client = server.available();
  if (!client)
    return;

  // Wait until the client sends some data
  //Serial.println("new client");
  time_connected = millis();

  // set a timeout of 10s - then kick the client..
  while (client.connected() && (millis() < time_connected + 10000))
  {
    if (!client.available())
      vTaskDelay(pdMS_TO_TICKS(1));
    else
    {
      // new data available to read
      char c = client.read(); // read 1 byte (character) from client
      header += c;
      if (0)
        Serial.print(c);  // print the received requests
      // if the current line is blank, you got two newline characters in a row.
      // that's the end of the client HTTP request, so send a response:
      if (c == '\n' && currentLineIsBlank)
      {
        // send a standard http response header
        client.println("HTTP/1.1 200 OK");
        // Send XML file or Web page
        // If client already on the web page, browser requests with AJAX the latest
        // sensor readings (ESP32 sends the XML file)
        if (header.indexOf("update_readings") >= 0)
        {
          // send rest of HTTP header
          client.println("Content-Type: text/xml");
          client.println("Connection: keep-alive");
          client.println();
          // Send XML file with sensor readings
          sendXMLFile(client);
        }
        else if (header.indexOf("turn_silvia_on") >= 0)
        {
          Serial.println("received command to turn on!");
          PWR_powerOn();
        }
        else if (header.indexOf("turn_silvia_off") >= 0)
        {
          Serial.println("received command to turn off!");
          PWR_powerOff();
        }
        // When the client connects for the first time, send it the index.html file
        else
        {
          // send rest of HTTP header
          client.println("Content-Type: text/html");
          client.println("Connection: keep-alive");
          client.println();
          // send web page to client
          sendHTMLFile(client);
        }
        break;
      }
      // every line of text received from the client ends with \r\n
      if (c == '\n')
      {
        // last character on line of received text
        // starting new line with next character read
        currentLineIsBlank = true;
      }
      else if (c != '\r')
      {
        // a text character was received from client
        currentLineIsBlank = false;
      }
    }
  }
  // Clear the header variable
  header = "";
  // Close the connection
  client.stop();
  //Serial.println("Client disconnected.");
}

// Send XML file with the latest sensor readings
static void sendXMLFile(WiFiClient cl)
{
  // Prepare XML file
  cl.print("<?xml version = \"1.0\" ?>");
  cl.print("<inputs>");

  cl.print("<reading>");
  cl.print(wifi_buffer_temp_top[wifi_buffer_idx_current]);
  cl.println("</reading>");
  
  cl.print("<reading>");
  cl.print(wifi_buffer_temp_side[wifi_buffer_idx_current]);
  cl.println("</reading>");
  
  cl.print("<reading>");
  cl.print((wifi_buffer_temp_side[wifi_buffer_idx_current] + wifi_buffer_temp_top[wifi_buffer_idx_current]) / 2);
  cl.println("</reading>");
  
  cl.print("<reading>");
  cl.print(wifi_buffer_temp_brewhead[wifi_buffer_idx_current]);
  cl.println("</reading>");
  
  cl.print("<reading>");
  cl.print(wifi_buffer_perc_heater[wifi_buffer_idx_current]);
  cl.println("</reading>");
  
  cl.print("<reading>");
  cl.print(SHOT_getTimeOfShot() / 1000.0f);
  cl.println("</reading>");
  
  cl.print("<status>");
  if (PWR_isPoweredOn())
    cl.print("ON");
  else
    cl.print("OFF");
  cl.println("</status>");
  
  cl.print("</inputs>");
}

static void sendHTMLFile(WiFiClient cl)
{
  cl.println("<!DOCTYPE html>");
  cl.println("<html>");
  cl.println("<head>");
  cl.println("<title>Silvia</title>");
  cl.println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
  cl.println("<link rel=\"icon\" href=\"data:,\">");
  cl.println("<script>");
  cl.println("function DisplayCurrentTime() {");
  cl.println("var date = new Date();");
  cl.println("var hours = date.getHours() < 10 ? \"0\" + date.getHours() : date.getHours();");
  cl.println("var minutes = date.getMinutes() < 10 ? \"0\" + date.getMinutes() : date.getMinutes();");
  cl.println("var seconds = date.getSeconds() < 10 ? \"0\" + date.getSeconds() : date.getSeconds();");
  cl.println("time = hours + \":\" + minutes + \":\" + seconds;");
  cl.println("var currentTime = document.getElementById(\"currentTime\");");
  cl.println("currentTime.innerHTML = time;");
  cl.println("};");
  cl.println("function GetReadings() {");
  cl.println("nocache = \"&nocache\";");
  cl.println("var request = new XMLHttpRequest();");
  cl.println("request.onreadystatechange = function() {");
  cl.println("if (this.status == 200) {");
  cl.println("if (this.responseXML != null) {");
  cl.println("var count;");
  cl.println("var num_an = this.responseXML.getElementsByTagName('reading').length;");
  cl.println("for (count = 0; count < num_an; count++) {");
  cl.println("document.getElementsByClassName(\"reading\")[count].innerHTML =");
  cl.println("this.responseXML.getElementsByTagName('reading')[count].childNodes[0].nodeValue;");
  cl.println("}");
  cl.println("num_an = this.responseXML.getElementsByTagName('status').length;");
  cl.println("for (count = 0; count < num_an; count++) {");
  cl.println("document.getElementsByClassName(\"status\")[count].innerHTML =");
  cl.println("this.responseXML.getElementsByTagName('status')[count].childNodes[0].nodeValue;");
  cl.println("}");
  cl.println("DisplayCurrentTime();");
  cl.println("}");
  cl.println("}");
  cl.println("}");
  cl.println("request.open(\"GET\", \"?update_readings\" + nocache, true);");
  cl.println("request.send(null);");
  cl.println("setTimeout('GetReadings()', 1000);");
  cl.println("};");
  cl.println("function powerOnButtonFunction() {");
  cl.println("nocache = \"&nocache\";");
  cl.println("var request = new XMLHttpRequest();");
  cl.println("request.open(\"POST\", \"turn_silvia_on\" + nocache, true);");
  cl.println("request.send(null);");
  cl.println("};");
  cl.println("function powerOffButtonFunction() {");
  cl.println("nocache = \"&nocache\";");
  cl.println("var request = new XMLHttpRequest();");
  cl.println("request.open(\"POST\", \"turn_silvia_off\" + nocache, true);");
  cl.println("request.send(null);");
  cl.println("};");
  cl.println("document.addEventListener('DOMContentLoaded', function() {");
  cl.println("GetReadings();");
  cl.println("}, false);");
  cl.println("</script>");
  cl.println("<style>");
  cl.println("body {");
  cl.println("text-align: center;");
  cl.println("font-family: \"Trebuchet MS\", Arial;");
  cl.println("}");
  cl.println("table {");
  cl.println("border-collapse: collapse;");
  cl.println("margin-left:auto;");
  cl.println("margin-right:auto;");
  cl.println("}");
  cl.println("th {");
  cl.println("padding: 16px;");
  cl.println("background-color: #0043af;");
  cl.println("color: white;");
  cl.println("}");
  cl.println("tr {");
  cl.println("border: 1px solid #ddd;");
  cl.println("padding: 16px;");
  cl.println("}");
  cl.println("tr:hover {");
  cl.println("background-color: #bcbcbc;");
  cl.println("}");
  cl.println("td {");
  cl.println("border: none;");
  cl.println("padding: 16px;");
  cl.println("}");
  cl.println(".sensor {");
  cl.println("color:white;");
  cl.println("font-weight: bold;");
  cl.println("background-color: #bcbcbc;");
  cl.println("padding: 8px;");
  cl.println("}");
  cl.println("</style>");
  cl.println("</head>");
  cl.println("<body>");
  cl.println("<h1>Silvia</h1>");
  cl.println("<h3>Last update: <span id=\"currentTime\"></span></h3>");
  cl.println("<p>Status: <span class=\"status\">...</span></p>");
  cl.println("<table>");
  cl.println("<tr>");
  cl.println("<th width=130px>SENSOR</th>");
  cl.println("<th width=100px>VALUE</th>");
  cl.println("</tr>");
  cl.println("<tr>");
  cl.println("<td><span class=\"sensor\">Top</span></td>");
  cl.println("<td><span class=\"reading\">...</span> &deg;C</td>");
  cl.println("</tr>");
  cl.println("<tr>");
  cl.println("<td><span class=\"sensor\">Side</span></td>");
  cl.println("<td><span class=\"reading\">...</span> &deg;C</td>");
  cl.println("</tr>");
  cl.println("<tr>");
  cl.println("<td><span class=\"sensor\">Average (99)</span></td>");
  cl.println("<td><span class=\"reading\">...</span> &deg;C</td>");
  cl.println("</tr>");
  cl.println("<tr>");
  cl.println("<td><span class=\"sensor\">Brewhead (90)</span></td>");
  cl.println("<td><span class=\"reading\">...</span> &deg;C</td>");
  cl.println("</tr>");
  cl.println("<tr>");
  cl.println("<td><span class=\"sensor\">Heater</span></td>");
  cl.println("<td><span class=\"reading\">...</span> &#37;</td>");
  cl.println("</tr>");
  cl.println("<tr>");
  cl.println("<td><span class=\"sensor\">Shot Time</span></td>");
  cl.println("<td><span class=\"reading\">...</span> s</td>");
  cl.println("</tr>");
  cl.println("</table>");
  cl.println("<button onclick=\"powerOnButtonFunction()\">Power On</button>");
  cl.println("<button onclick=\"powerOffButtonFunction()\">Power Off</button>");
  cl.println("</body>");
  cl.println("</html>");
}


// void WIFI_service_led(void)
// {
//   // Check if a client has connected
//   WiFiClient client = server.available();
//   if (!client)
//     return;
//   // Wait until the client sends some data
//   Serial.println("new client");
//   while(!client.available())
//     vTaskDelay(pdMS_TO_TICKS(1));
//   // Read the first line of the request
//   String request = client.readStringUntil('\r');
//   Serial.println(request);
//   client.flush();
//   // Match the request
//   int value = LOW;
//   if (request.indexOf("/LED=ON") != -1)
//   {
//     digitalWrite(LED_BUILTIN, HIGH);
//     value = HIGH;
//   }
//   if (request.indexOf("/LED=OFF") != -1)
//   {
//     digitalWrite(LED_BUILTIN, LOW);
//     value = LOW;
//   }
// // Set ledPin according to the request
// //digitalWrite(ledPin, value);
//   // Return the response
//   client.println("HTTP/1.1 200 OK");
//   client.println("Content-Type: text/html");
//   client.println(""); //  do not forget this one
//   client.println("<!DOCTYPE HTML>");
//   client.println("<html>");
//   client.print("Led pin is now: ");
//   if(value == HIGH)
//     client.print("On");
//   else
//     client.print("Off");
//   client.println("<br><br>");
//   client.println("<a href=\"/LED=ON\"\"><button>Turn On </button></a>");
//   client.println("<a href=\"/LED=OFF\"\"><button>Turn Off </button></a><br />");
//   client.println("</html>");
//   //vTaskDelay(pdMS_TO_TICKS(1));
//   Serial.println("Client disonnected\n");
// }
