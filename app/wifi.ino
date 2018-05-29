
#include <WiFi.h>
#include <ESPmDNS.h>

#define WIFI_TASK_STACKSIZE  2000u  // [words]
#define WIFI_TASK_PRIORITY   2      // idle: 0, main-loop: 1, wifi: 2, pid: 5, sensors: 4
#define WIFI_BUFFER_SIZE     120    // number of saved samples


const char* ssid     = PRIVATE_SSID;     // your network SSID (name of wifi network)
const char* password = PRIVATE_WIFIPW;   // your network password

static uint32_t wifi_buffer_count = 0;  // current number of samples in buffer
static uint32_t wifi_buffer_idx_current = WIFI_BUFFER_SIZE-1;  // index of newest value

static float wifi_buffer_temp_top[WIFI_BUFFER_SIZE];
static float wifi_buffer_temp_side[WIFI_BUFFER_SIZE];
static float wifi_buffer_temp_brewhead[WIFI_BUFFER_SIZE];
static uint8_t wifi_buffer_perc_heater[WIFI_BUFFER_SIZE];
static uint8_t wifi_buffer_perc_pump[WIFI_BUFFER_SIZE];
static float wifi_buffer_pid_p[WIFI_BUFFER_SIZE];
static float wifi_buffer_pid_i[WIFI_BUFFER_SIZE];
static float wifi_buffer_pid_d[WIFI_BUFFER_SIZE];
static int32_t wifi_buffer_pid_u[WIFI_BUFFER_SIZE];

WiFiServer server(80);

static TaskHandle_t wifi_task_handle = NULL;

static void wifi_task(void * pvParameters);


void WIFI_setup(void)
{
  if (wifi_task_handle == NULL)
  {
    if (xTaskCreate(wifi_task, "task_wifi", WIFI_TASK_STACKSIZE, NULL, WIFI_TASK_PRIORITY, &wifi_task_handle) != pdPASS)
      return; // error
  }
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

  while (1)
  {
    WIFI_service();
    vTaskDelay(pdMS_TO_TICKS(57));
  }
}

void WIFI_addToBuffer(float temp_top, 
                      float temp_side, 
                      float temp_brewhead, 
                      uint8_t perc_heater,
                      uint8_t perc_pump,
                      float pid_p,
                      float pid_i,
                      float pid_d,
                      int32_t pid_u)
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
  wifi_buffer_idx_current = WIFI_BUFFER_SIZE-1;
}

void WIFI_service(void)
{
  // Check if a client has connected
  WiFiClient client = server.available();
  if (!client)
    return;
 
  // Wait until the client sends some data
  Serial.println("new client");
  while(!client.available())
    vTaskDelay(pdMS_TO_TICKS(1));
 
  // Read the first line of the request
  String request = client.readStringUntil('\r');
  Serial.println(request);
  client.flush();

  // Return the response
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println(""); //  do not forget this one
  client.println("<!DOCTYPE HTML>");
  
client.println("<html>");
client.println("<head>");
client.println("<!--Load the AJAX API-->");
client.println("<script type=\"text/javascript\" src=\"https://www.gstatic.com/charts/loader.js\"></script>");
client.println("<script type=\"text/javascript\">");
client.println("google.charts.load('current', {'packages':['corechart']});");
client.println("google.charts.setOnLoadCallback(drawChart);");
client.println("function drawChart() {");
client.println("var data = new google.visualization.DataTable();");
client.println("data.addColumn('string', 'Topping');");
client.println("data.addColumn('number', 'Slices');");
client.println("data.addRows([");
client.println("['Mushrooms', 3],");
client.println("['Onions', 1],");
client.println("['Olives', 1],");
client.println("['Zucchini', 1],");
client.println("['Pepperoni', 2]");
client.println("]);");
client.println("var options = {'title':'How Much Pizza I Ate Last Night',");
client.println("'width':400,");
client.println("'height':300};");
client.println("var chart = new google.visualization.PieChart(document.getElementById('chart_div'));");
client.println("chart.draw(data, options);");
client.println("}");
client.println("</script>");
client.println("</head>");
client.println("<body>");
client.println("<!--Div that will hold the pie chart-->");
client.println("<div id=\"chart_div\"></div>");
client.println("</body>");
client.println("</html>");
 
  //vTaskDelay(pdMS_TO_TICKS(1));
  Serial.println("Client disonnected\n");
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

