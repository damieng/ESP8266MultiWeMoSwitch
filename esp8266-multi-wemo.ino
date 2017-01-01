#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <functional>

// NTP
static const char ntpServerName[] = "us.pool.ntp.org";
const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
const int NTP_PORT = 123;
const int timeZone = 1;     // We always use GMT as that's what HTTP expects
const int NTP_TIMEOUT = 5000; // 8 seconds for response

// UDP
WiFiUDP UDP;
IPAddress ipMulti(239, 255, 255, 250);
unsigned int portMulti = 1900;
char packetBuffer[UDP_TX_PACKET_MAX_SIZE];

// HTTP
ESP8266WebServer HTTP(80); 
bool isConnected = true;
bool flashingState = HIGH;
String persistentUuid;
String deviceName = "WeeMultiMo";

String eventservice_xml = 
"<?scpd xmlns=\"urn:Belkin:service-1-0\"?>"
  "<actionList>"
    "<action>"
      "<name>SetBinaryState</name>"
      "<argumentList>"
        "<argument>"
          "<retval/>"
          "<name>BinaryState</name>"
          "<relatedStateVariable>BinaryState</relatedStateVariable>"
          "<direction>in</direction>"
        "</argument>"
      "</argumentList>"
       "<serviceStateTable>"
        "<stateVariable sendEvents=\"yes\">"
          "<name>BinaryState</name>"
          "<dataType>Boolean</dataType>"
          "<defaultValue>0</defaultValue>"
        "</stateVariable>"
        "<stateVariable sendEvents=\"yes\">"
          "<name>level</name>"
          "<dataType>string</dataType>"
          "<defaultValue>0</defaultValue>"
        "</stateVariable>"
      "</serviceStateTable>"
    "</action>"
  "</actionList>"
"</scpd>\r\n\r\n";

void setup() {
  Serial.begin(115200);
  setupPins();
  persistentUuid = makeUuid();
  
  if (connectWiFi()) {
    startNtp();
    if (connectUdp()) {
      isConnected = true;
      startHttpServer();
    }
  }
}

void startNtp() {
  UDP.begin(8888);
  setSyncProvider(getNtpTime);
  setSyncInterval(60*60);  
}

void setupPins() {
  pinMode(BUILTIN_LED, OUTPUT);  
  pinMode(BUILTIN_LED, LOW);  

  pinMode(D1, OUTPUT);
  pinMode(D2, OUTPUT);  

  digitalWrite(D1, LOW);  
  digitalWrite(D2, LOW);
}

void loop() {
  HTTP.handleClient();
  delay(1);

  if (!isConnected) {
    // Flash LED to indicate WiFi not connected
    flashingState = flashingState == HIGH ? LOW : HIGH;
    digitalWrite(BUILTIN_LED, flashingState);
    delay(500);
  } else { 
    int packetSize = UDP.parsePacket();
    if (packetSize) {
       processUdpPacket(packetSize);
    } else {
      delay(10);
    }
  }
}

void processUdpPacket(int packetSize) {
  UDP.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE);  
  String request = packetBuffer;

  if (request.indexOf("M-SEARCH") > 0 && request.indexOf("urn:Belkin:device:**") > 0) {
    respondToSearch(UDP.remoteIP(), UDP.remotePort());
  }
}

String makeUuid() {
  char uuid[64];
  uint32_t chipId = ESP.getChipId();
   // ChipId has 3 unique-per-chip bytes we use to generate a unique ID that doesn't change
  sprintf_P(uuid, PSTR("38323636-4558-4dda-9188-cda0e6%02x%02x%02x"),
    (uint16_t) ((chipId >> 16) & 0xff),
    (uint16_t) ((chipId >>  8) & 0xff),
    (uint16_t)   chipId        & 0xff);
  return String(uuid);
}

void respondToSearch(IPAddress ip, int port) {
  Serial.print("UDP Search response to ");
  Serial.print(ip);
  Serial.print(" port ");
  Serial.println(port);

  String response = 
     "HTTP/1.1 200 OK\r\n"
     "CACHE-CONTROL: max-age=86400\r\n"
     "DATE: " + getHttpDate() + "\r\n"
     "EXT:\r\n"
     "LOCATION: http://" + getIpAddress(WiFi.localIP()) + ":80/setup.xml\r\n"
     "OPT: \"http://schemas.upnp.org/upnp/1/0/\"; ns=01\r\n"
     "01-NLS: b9200ebb-736d-4b93-bf03-835149d13983\r\n"
     "SERVER: Unspecified, UPnP/1.0, Unspecified\r\n"
     "ST: urn:Belkin:device:**\r\n"
     "USN: uuid:Socket-1_0-" + persistentUuid + "::urn:Belkin:device:**\r\n"
     "X-User-Agent: redsonic\r\n\r\n";

  UDP.beginPacket(ip, port);
  UDP.write(response.c_str());
  UDP.endPacket();                    
}

String getHttpDate() {
  char dayBuf[3];
  char buf[32];
  strcpy(dayBuf, dayShortStr(day()));
  sprintf(buf, "%s, %02d %s %d %02d:%02d:%02d GMT",
    dayBuf, day(), monthShortStr(month()), year(),
    hour(), minute(), second());
  return String(buf);
}

void startHttpServer() {
  HTTP.on("/debug", HTTP_POST, []() {
    Serial.println("Http request /debug");      

    HTTP.send(200, "text/plain", getHttpDate());
  });
  
  HTTP.on("/upnp/control/basicevent1", HTTP_POST, []() {
    Serial.println("Http request /upnp/control/basicevent1");      

    String request = HTTP.arg(0);

    if (request.indexOf("<BinaryState>1</BinaryState>") >= 0) {
      printf("Relay HIGH request \n");
      digitalWrite(D1, HIGH);
    }

    if (request.indexOf("<BinaryState>0</BinaryState>") >= 0) {
      Serial.println("Relay LOW request");
      analogWrite(D1, LOW);
    }
    
    HTTP.send(200, "text/plain", "");
  });

  HTTP.on("/eventservice.xml", HTTP_GET, []() {
    Serial.println("HTTP responding to /eventservice.xml");
    HTTP.send(200, "text/plain", eventservice_xml.c_str());
  });
  
  HTTP.on("/setup.xml", HTTP_GET, []() {
    Serial.println("Http responding to /setup.xml");

    String setup_xml = "<?xml version=\"1.0\"?>"
      "<root>"
       "<device>"
          "<deviceType>urn:Belkin:device:controllee:1</deviceType>"
          "<friendlyName>"+ deviceName +"</friendlyName>"
          "<manufacturer>Belkin International Inc.</manufacturer>"
          "<modelName>Emulated Socket</modelName>"
          "<modelNumber>3.1415</modelNumber>"
          "<UDN>uuid:"+ persistentUuid +"</UDN>"
          "<serialNumber>221517K0101769</serialNumber>"
          "<binaryState>0</binaryState>"
          "<serviceList>"
            "<service>"
                "<serviceType>urn:Belkin:service:basicevent:1</serviceType>"
                "<serviceId>urn:Belkin:serviceId:basicevent1</serviceId>"
                "<controlURL>/upnp/control/basicevent1</controlURL>"
                "<eventSubURL>/upnp/event/basicevent1</eventSubURL>"
                "<SCPDURL>/eventservice.xml</SCPDURL>"
            "</service>"
        "</serviceList>" 
        "</device>"
      "</root>\r\n\r\n";
      
      HTTP.send(200, "text/xml", setup_xml.c_str());
  });
  
  HTTP.begin();  
  Serial.println("HTTP Server started");
}

String getIpAddress(IPAddress ip) {
  char s[16];
  sprintf(s, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
  return String(s);
}

void startWiFiManager() {
  Serial.println("WiFi connecting via manager");
  WiFiManager wiFiManager;
  // wiFiManager.resetSettings(); // Uncomment to test WiFi Manager function
  wiFiManager.setAPCallback(wiFiConfigModeCallback);
  wiFiManager.autoConnect();
}

bool waitForWiFi() {
  int attempts = 10;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if (attempts == 10)
      Serial.print("WiFi trying to connect");
    Serial.print(".");
    if (attempts-- < 0) {
       Serial.println(" failed");
      return false;
    }
  }
  Serial.println("");
  return true;
}

bool connectWiFi() {
  startWiFiManager();
    
  if (waitForWiFi()) {
    Serial.print("\nWiFi connected to ");
    Serial.print(WiFi.SSID());
    Serial.print(" IP ");
    Serial.println(WiFi.localIP());
    return true;
  }

  Serial.println("WiFi connection failed");
  return false;
}

bool connectUdp() {
  if (UDP.beginMulticast(WiFi.localIP(), ipMulti, portMulti)) {
    Serial.println("UDP connected");
    return true;
  }

  Serial.println("UDP connection failed");
  return false;
}

void wiFiConfigModeCallback(WiFiManager *myWiFiManager) {
  Serial.print("Connect to network ");
  Serial.print(myWiFiManager->getConfigPortalSSID());
  Serial.print(" and open browser to http://");
  Serial.println(WiFi.softAPIP());
}

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (UDP.parsePacket() > 0) ; // discard any previously received packets
  Serial.print("NTP time request via ");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerIP);
  Serial.print(" ... ");
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < NTP_TIMEOUT) {
    int size = UDP.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      UDP.read(packetBuffer, NTP_PACKET_SIZE);
      unsigned long secsSince1900;
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      Serial.print("success (");
      Serial.print(secsSince1900);
      Serial.println(")");
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("failed - no response");
  return 0; // return 0 if unable to get the time
}

void sendNTPpacket(IPAddress &address)
{
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  UDP.beginPacket(address, NTP_PORT);
  UDP.write(packetBuffer, NTP_PACKET_SIZE);
  UDP.endPacket();
}
