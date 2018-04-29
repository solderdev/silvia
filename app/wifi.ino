
#include <WiFi.h>
#include <ESPmDNS.h>

const char* ssid     = PRIVATE_SSID;     // your network SSID (name of wifi network)
const char* password = PRIVATE_WIFIPW;   // your network password

WiFiServer server(80);

void WIFI_setup(void)
{
  Serial.print("Attempting to connect to SSID: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  int millis_stop = millis() + 5000;
  while (WiFi.status() != WL_CONNECTED && (millis() < millis_stop)) {
    delay(500);
    Serial.print(".");
    // TODO - do this in a separate task
  }
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Error setting WIFI!");
    return;
  }

  // be reachable under silvia.local
  if (!MDNS.begin("silvia"))
  {
    Serial.println("Error setting up MDNS responder!");
    return;
  }
  Serial.println("mDNS responder started");
  MDNS.addService("http", "tcp", 80);

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  server.begin();
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
    delay(1);
 
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
 
  //delay(1);
  Serial.println("Client disonnected\n");
}

void WIFI_service_led(void)
{
  // Check if a client has connected
  WiFiClient client = server.available();
  if (!client)
    return;
 
  // Wait until the client sends some data
  Serial.println("new client");
  while(!client.available())
    delay(1);
 
  // Read the first line of the request
  String request = client.readStringUntil('\r');
  Serial.println(request);
  client.flush();
 
  // Match the request
 
  int value = LOW;
  if (request.indexOf("/LED=ON") != -1)
  {
    digitalWrite(LED_BUILTIN, HIGH);
    value = HIGH;
  }
  
  if (request.indexOf("/LED=OFF") != -1)
  {
    digitalWrite(LED_BUILTIN, LOW);
    value = LOW;
  }
 
// Set ledPin according to the request
//digitalWrite(ledPin, value);
 
  // Return the response
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println(""); //  do not forget this one
  client.println("<!DOCTYPE HTML>");
  client.println("<html>");
 
  client.print("Led pin is now: ");
  if(value == HIGH)
    client.print("On");
  else
    client.print("Off");
  
  client.println("<br><br>");
  client.println("<a href=\"/LED=ON\"\"><button>Turn On </button></a>");
  client.println("<a href=\"/LED=OFF\"\"><button>Turn Off </button></a><br />");  
  client.println("</html>");
 
  //delay(1);
  Serial.println("Client disonnected\n");
}

