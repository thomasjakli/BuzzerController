#include <Arduino.h>
#include <FamFest_Buzzer.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         // https://github.com/tzapu/WiFiManager
#include <WiFiUdp.h>
#include <OSCMessage.h>  

#define BOYS_PIN D1
#define GIRLS_PIN D3
#define LED D5

// Wifi: SSID and password
const char* WIFI_SSID = "***";
const char* WIFI_PASSWORD = "***";

WiFiUDP Udp;                                // A UDP instance to let us send and receive packets over UDP
IPAddress outIp(192,168,1,100);        // remote IP of your computer
long outPort = 53000;          // remote port to receive OSC
const unsigned int localPort = 8888;        // local port to listen for OSC packets (actually not used for sending)
long buzzerDelay = 500;

//---WiFi Management & Settings---
// Set web server port number to 80
ESP8266WebServer server(80);
// Variable to store the HTTP request
String header;
// Setting page HTML
const String settingPage = "<!DOCTYPE html><html>\
<head>\
  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\
  <link rel=\"icon\" href=\"data:,\">\
</head>\
<body>\
  <h1>Buzzer configuration</h1>\
  <form action=\"/osctarget\" method=\"post\">\
    <input type=\"text\" name=\"targetAddress\" placeholder=\"IP Address\"/>\
    <input type=\"text\" name=\"targetPort\" placeholder=\"Port\"/>\
    <input type=\"submit\"/>\
  </form>\
  <form action=\"/delay\" method=\"post\">\
    <input type=\"number\" min=\"0.5\" step=\"0.5\" name=\"delay\" placeholder=\"Delay in milliseconds\"/>\
    <input type=\"submit\"/>\
  </form>\
</body>\
</html>";
//---------------------

enum buzzer_t
{
  NONE,
  BOYS,
  GIRLS
};

enum state_t
{
  WAIT,
  WAITRESET
};

uint8_t currState;
volatile uint8_t currBuzzer;

IRAM_ATTR void BuzzBoysInterruptHandler()
{
  if(currBuzzer == NONE)
  {
    currBuzzer = BOYS;
  }
}

IRAM_ATTR void BuzzGirlsInterruptHandler()
{
  if(currBuzzer == NONE)
  {
    currBuzzer = GIRLS;
  }
}



void setup() {
  Serial.begin(115200);

  currState = WAIT;

  connectWifiManaged();
  
  pinMode(LED, OUTPUT);
  pinMode(BOYS_PIN, INPUT_PULLUP);
  pinMode(GIRLS_PIN, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(BOYS_PIN), BuzzBoysInterruptHandler, FALLING);
  attachInterrupt(digitalPinToInterrupt(GIRLS_PIN), BuzzGirlsInterruptHandler, FALLING);
  
  server.on("/", sendMainPage);
  server.on("/osctarget", handleOSCTarget);
  server.on("/delay", handleDelay);
  server.onNotFound([](){
    server.send(404, "text/plain", "Ned gfunden oida");
  });
  server.begin();
}


void SendMessage()
{
  if(currBuzzer == BOYS)
  {
    Serial.println("Sending message to boys");
    OSCMessage msg("/cue/BUZ_BOYS/start");
    Udp.beginPacket(outIp, outPort);
    msg.send(Udp);
    Udp.endPacket();
    msg.empty();
  }
  else
  {
    Serial.println("Sending message to girls");
    OSCMessage msg("/cue/BUZ_GIRLS/start");
    Udp.beginPacket(outIp, outPort);
    msg.send(Udp);
    Udp.endPacket();
    msg.empty();
  }

  
}

void loop() {
  if(currBuzzer != NONE)
  {
    SendMessage();
    delay(buzzerDelay);
    currBuzzer = NONE;
  }

  server.handleClient();
}

void connectWifi(){
  Serial.println();
  Serial.println();
  Serial.print("INFO: Connecting to ");
  WiFi.mode(WIFI_STA);
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("INFO: WiFi connected");
  Serial.print("INFO: IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(localPort);
}

void connectWifiManaged()
{
  // WiFiManager
  // Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  
  // Uncomment and run it once, if you want to erase all the stored information
  //wifiManager.resetSettings();
  
  // set custom ip for portal
  //wifiManager.setAPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

  // fetches ssid and pass from eeprom and tries to connect
  // if it does not connect it starts an access point with the specified name
  // and goes into a blocking loop awaiting configuration
  wifiManager.autoConnect("BuzzerAP");
  // or use this for auto generated name ESP + ChipID
  //wifiManager.autoConnect();
  
  // if you get here you have connected to the WiFi
  Serial.println("Connected.");
}

void sendMainPage()
{
  Serial.println("Serving main page");
  server.send(200, "text/html", settingPage);
}

void goHome()
{
  server.sendHeader("Location","/");
  server.send(301, "text/plain", "");
}

void handleOSCTarget()
{
  Serial.println("Handling OSC Target");
  if (!server.hasArg("targetAddress") && !server.hasArg("targetPort"))
  {
    server.send(400, "text/plain", "400: Invalid Request");
    return;
  }
  String ipString = server.arg("targetAddress");
  IPAddress addr;
  if (addr.fromString(ipString))
  {
    Serial.println("Set new target IP");
    outIp.fromString(ipString);
  }
  else if (ipString.length() > 0)
  {
    Serial.println("Setting IP failed");
    server.send(400, "text/plain", "400: Invalid IP format?");
    return;
  }

  String portString = server.arg("targetPort");
  if (portString.length() > 0)
  {
    long newPort = portString.toInt();
    if (newPort > 0)
    {
      Serial.println("Set new target port");
      outPort = newPort;
    }
    else
    {
      Serial.println("Setting port failed");
      server.send(400, "text/plain", "400: Invalid port format?");
      return;
    }
    
  }

  goHome();
}

void handleDelay()
{
  Serial.println("Handling Delay");
  if (!server.hasArg("delay"))
  {
    server.send(400, "text/plain", "400: Invalid Request");
    return;
  }
  
  String delayString = server.arg("delay");
  if (delayString.length() > 0)
  {
    buzzerDelay = min(delayString.toInt(), 500l); 
  }

  goHome();
}