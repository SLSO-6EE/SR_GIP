# 1 "h:\\Documents\\GitHub\\Home-Control-GIP\\Home Control Protocol\\HCP_MCU_v4\\HCP_MCU_v4.ino"
# 1 "h:\\Documents\\GitHub\\Home-Control-GIP\\Home Control Protocol\\HCP_MCU_v4\\HCP_MCU_v4.ino"
/*

  Home Control Protocol v0.4.0

    by Stijn Rogiest (copyright 2019)



  Random console characters legend: 

    _: The last packet was resent, caused by faulty integrity at the receiver.

    !: The last request did not get answered and was disposed.

    .: The last request was resent.



  Sources:

    https://tttapa.github.io/ESP8266/Chap10%20-%20Simple%20Web%20Server.html

    https://www.arduino.cc/en/Reference/EEPROM

    http://www.cplusplus.com/doc/tutorial/pointers/

    https://www.arduino.cc/en/Reference/softwareSerial

    https://stackoverflow.com/questions/3698043/static-variables-in-c

    https://randomnerdtutorials.com/esp8266-web-server/

    http://arduino.esp8266.com/stable/package_esp8266com_index.json

    https://en.wikipedia.org/wiki/Multicast_DNS

    https://en.wikipedia.org/wiki/Cyclic_redundancy_check#CRC-32_algorithm



  Packet types/prefixes:

    0x20: Set slave properties.

    0x1: Ping slave.

    0x15: Refresh slave live data.

    0x10: Bind slave.

    0x2: Unbind slave.

*/
# 29 "h:\\Documents\\GitHub\\Home-Control-GIP\\Home Control Protocol\\HCP_MCU_v4\\HCP_MCU_v4.ino"
# 30 "h:\\Documents\\GitHub\\Home-Control-GIP\\Home Control Protocol\\HCP_MCU_v4\\HCP_MCU_v4.ino" 2
# 31 "h:\\Documents\\GitHub\\Home-Control-GIP\\Home Control Protocol\\HCP_MCU_v4\\HCP_MCU_v4.ino" 2
# 32 "h:\\Documents\\GitHub\\Home-Control-GIP\\Home Control Protocol\\HCP_MCU_v4\\HCP_MCU_v4.ino" 2

# 34 "h:\\Documents\\GitHub\\Home-Control-GIP\\Home Control Protocol\\HCP_MCU_v4\\HCP_MCU_v4.ino" 2

# 36 "h:\\Documents\\GitHub\\Home-Control-GIP\\Home Control Protocol\\HCP_MCU_v4\\HCP_MCU_v4.ino" 2
# 37 "h:\\Documents\\GitHub\\Home-Control-GIP\\Home Control Protocol\\HCP_MCU_v4\\HCP_MCU_v4.ino" 2
# 38 "h:\\Documents\\GitHub\\Home-Control-GIP\\Home Control Protocol\\HCP_MCU_v4\\HCP_MCU_v4.ino" 2
# 39 "h:\\Documents\\GitHub\\Home-Control-GIP\\Home Control Protocol\\HCP_MCU_v4\\HCP_MCU_v4.ino" 2
# 40 "h:\\Documents\\GitHub\\Home-Control-GIP\\Home Control Protocol\\HCP_MCU_v4\\HCP_MCU_v4.ino" 2
# 41 "h:\\Documents\\GitHub\\Home-Control-GIP\\Home Control Protocol\\HCP_MCU_v4\\HCP_MCU_v4.ino" 2
# 42 "h:\\Documents\\GitHub\\Home-Control-GIP\\Home Control Protocol\\HCP_MCU_v4\\HCP_MCU_v4.ino" 2


// Note: HC12 TX to RX and RX to TX


// This masters addr, can be 1, 2 or 3.





SoftwareSerial ss(12, 14);
PacketSenderReceiver sr = PacketSenderReceiver(&ss, false, 2);
Packet temp;
Device* devices[32];

ESP8266WiFiMulti wifiMulti;
WiFiServer server(80);
WebRequest* requesters[8];

const unsigned int retryBindMillisInterval = 400;
unsigned long lastRetryBindMillis = 1;
const unsigned int refreshMillisInterval = 2220;
unsigned long lastRefreshMillis = 1;

unsigned long lastLedBlink = 0;
unsigned int ledBlinks = 0;
unsigned int ledBlinkInterval = 200;
void led(int blinks, int interval = 200)
{
  ledBlinks = blinks * 2;
  ledBlinkInterval = interval;
}

// Console
unsigned char currentArg = 0;
String args[16];

// Prototypes
unsigned char refreshSlave(unsigned char addr);
unsigned char pingSlave(unsigned char addr);
unsigned char unbindSlave(unsigned char withAddress);
unsigned char setSlaveProperties(unsigned char addr, unsigned char startPos, unsigned char* values, unsigned char valueCount);
unsigned char bindSlave(unsigned char ufid[7], unsigned char withAddress);
unsigned char bindSlave(unsigned char ufid[7]);
unsigned char rebindSlave(unsigned char ufid[7], unsigned char withAddress);

void setup()
{
  pinMode(2, 0x01);
  digitalWrite(2, false);

  Serial.begin(19200);
  delay(5000);
  veryCoolSplashScreen();
  Serial.print("----> My address (master): ");
  Serial.println(2);
  Serial.println("----> Loading devices...");
  EEPROM.begin(4096);
  //clearRomDevices();
  loadDevicesFromRom();
  printDevices();

  Serial.print("----> Connecting to WiFi");
  wifiMulti.addAP("PollenPatatten", "Ziektes123");
  wifiMulti.addAP("RogiestHuis", "Vrijdag1!");
  wifiMulti.addAP("pollenpattten", "ziektes123");
  wifiMulti.addAP("Stijn Rogiest", "HoiDaag2");
  while (wifiMulti.run() != WL_CONNECTED)
  {
    delay(250);
    Serial.print('.');
  }
  Serial.println();
  Serial.print("----> Connected to ");
  Serial.println(WiFi.SSID());
  Serial.print("----> IP addr: ");
  Serial.println(WiFi.localIP());
  if (MDNS.begin("homecontrol")) // Start the mDNS responder for esp8266.local
  {
    Serial.print("\t-> mDNS responder started: ");
    Serial.println("homecontrol");
  }
  else
  {
    Serial.println("\t-> FATAL: Error setting up MDNS responder!");
  }
  server.begin();
  Serial.println("----> Starting...");
  delay(500);
  ss.begin(4800);
  Serial.println("\t-> OK");
}

void loop()
{
  if (ledBlinks > 0 && (millis() - lastLedBlink) > ledBlinkInterval)
  {
    digitalWrite(2, ledBlinks % 2 == 0);

    ledBlinks--;
    lastLedBlink = millis();
  }

  if (Serial.available() > 0)
  {
    char c = Serial.read();
    if (c == ' ' || c == ',')
    {
      if (args[currentArg].length() > 0)
      {
        currentArg++;
        args[currentArg] = "";
      }
    }
    else if (c == ';' || c == '\n')
    {
      command(args, currentArg + 1);

      currentArg = 0;
      args[currentArg] = "";
    }
    else
    {
      args[currentArg] += c;
    }
  }

  if (sr.receive(&temp))
  {
    led(1);

    // Slave is bound.
    if (temp.getMultiPurposeByte() == 130)
    {
      Serial.print("Received bind response from ");
      Serial.println(temp.getSlave());

      Device* bound = getDeviceWithAddress(temp.getSlave());

      if (bound)
      {
        Serial.print("----> Slave is now getting bound (1): ");
        bound->printTo(Serial);
        Serial.println();

        bound->working = true;
        bound->online = true;
        memcpy(bound->deviceType, temp.getData(), temp.getDataLength());
        saveDevicesToRom();

        Serial.print("----> Slave is now bound (2): ");
        bound->printTo(Serial);
        Serial.println();

        WebRequest* request = getWebRequestFor(130);
        if (request)
        {
          request->println("okey");
          request->close();
        }
      }
      else
      {
        Serial.println("----> FATAL: Count not let slave work!");
      }
    }
  }

  sr.resendUnansweredRequests();

  if ((millis() - lastRetryBindMillis) > retryBindMillisInterval)
  {
    retryNotWorkingBinds();

    lastRetryBindMillis = millis();
  }

  if ((millis() - lastRefreshMillis) > refreshMillisInterval)
  {
    refreshSlaves();

    lastRefreshMillis = millis();
  }

  WiFiClient newClient = server.available();
  if (newClient)
  {
    Serial.println("New client?");
    bool alreadyRequesting = false;
    for(unsigned char i = 0; i < 8; i++)
    {
      if (requesters[i] && requesters[i]->client == newClient)
      {
        alreadyRequesting = true;
        break;
      }
    }
    if (!alreadyRequesting)
    {
      for(unsigned char i = 0; i < 8; i++)
      {
        Serial.print("Requester #");
        Serial.print(i);
        Serial.println(requesters[i] ? ": active" : ": not active");

        if (!requesters[i])
        {
          requesters[i] = new WebRequest(newClient);
          Serial.println("New request");
          led(2);
          break;
        }
      }
    }
  }
  for(unsigned char i = 0; i < 8; i++)
  {
    if (requesters[i])
    {
        requesters[i]->update(requested);

        if (requesters[i]->shouldBeDisposed())
        {
          Serial.println("WebRequest is kermitting suicide... (3)");

          delete requesters[i];
          requesters[i] = nullptr;

          Serial.println("WebRequest kermitted suicide (4)");
        }
    }
  }

  /*if (newClient && (newClient != client) && (!client || !client.connected()))

  {

    client = newClient;

    clientData = "";

  }*/
# 283 "h:\\Documents\\GitHub\\Home-Control-GIP\\Home Control Protocol\\HCP_MCU_v4\\HCP_MCU_v4.ino"
  /*while (client && client.available())

  {

    char c = client.read();



    if (c == '\r')

      continue;



    clientData += c;



    if (clientData.length() > 2 && c == '\n' && clientData[clientData.length() - 2] == '\n')

    {

      int i = clientData.indexOf("GET "), j = clientData.indexOf(" HTTP/");

      bool open = false;

      if (i >= 0 && j >= 0)

      {

        String request = clientData.substring(i + 4, j);

        request.trim();

        open = requested(request);       

      }

      if (!open)

        client.stop();

    }

  }*/
# 306 "h:\\Documents\\GitHub\\Home-Control-GIP\\Home Control Protocol\\HCP_MCU_v4\\HCP_MCU_v4.ino"
}

void command(String args[16], unsigned char argsLen)
{
  if (argsLen >= 4 && args[0] == "prop")
  {
    Serial.print("----> Trying to set property ");
    unsigned char addr = args[1].toInt();
    unsigned char startPos = args[2].toInt();
    unsigned char data[16] = {0x20, startPos};
    for (unsigned char i = 0; i < argsLen - 3; i++)
    {
      data[i + 2] = args[i + 3].toInt();

      Serial.print('[');
      Serial.print(startPos + i);
      Serial.print(" = ");
      Serial.print(args[i + 3]);
      Serial.print("] ");
    }
    Serial.print("of addr ");
    Serial.println(addr);

    sr.sendRequest(addr, propertySetAnswer, data, argsLen - 1);
  }
  else if (args[0] == "wifi")
  {
    Serial.print("----> Connected to ");
    Serial.println(WiFi.SSID());
    Serial.print("----> IP addr: ");
    Serial.println(WiFi.localIP());
  }
  else if (argsLen == 2 && args[0] == "ping")
  {
    pingSlave(args[1].toInt());
  }
  else if (args[0] == "device")
  {
    if (argsLen == 1 || args[1] == "list")
    {
      printDevices();
    }
    else if (argsLen == 2 && args[1] == "unbindall")
    {
      Serial.println("----> Unbinding all slaves, please wait...");

      for(unsigned char i = 0; i < 32; i++)
      {
        if (devices[i])
        {
          Serial.print("Unbinding slave ");
          Serial.print(devices[i]->address);
          Serial.println("...");

          unbindSlave(devices[i]->address);
          delay(300);
        }
      }
      clearRomDevices();

      Serial.println("----> All bound slaves are now not bound anymore.");
    }
    else if (argsLen >= 3 && argsLen <= 9 && args[1] == "bind")
    {
      unsigned char ufid[7];
      memset(ufid, 0x0, sizeof(ufid));
      Serial.print("----> Binding slave with ufid [");
      for (unsigned char i = 2; i < argsLen; i++)
      {
        ufid[i - 2] = args[i].toInt();

        Serial.print(ufid[i - 2]);
        Serial.print(' ');
      }
      Serial.println(']');

      bindSlave(ufid);
    }
    else if (argsLen == 3 && args[1] == "unbind")
    {
      unsigned char addr = args[2].toInt();

      Serial.print("----> Unbinding slave ");
      Serial.print(addr);
      Serial.println("...");

      unbindSlave(addr);
    }
    else
    {
      Serial.println("Command syntax invalid: device [list|bind <ufid...>|unbind <addr>|unbindall]");
    }
  }
  else
  {
    Serial.print("Unknown command: ");
    Serial.print(args[0]);
    Serial.print(" (");
    Serial.print(argsLen);
    Serial.println(")");
  }
}

bool requested(WebRequest* webRequest, String path)
{
  WiFiClient& client = webRequest->client;

  Serial.println("Requested path: " + path);

  String sub[20];
  unsigned char subCount = 0;
  for(int i = 1; i < path.length() && subCount < 20; i++)
  {
    char c = path[i];

    if (c == '/')
    {
        subCount++;
        continue;
    }

    sub[subCount] += c;
  }
  subCount++;

  // HEADER
  client.println("HTTP/1.1 200 OK");
  client.println("Connection: Keep-Alive");
  client.println("Keep-Alive: timeout=15, max=1000");
  client.println("Content-type: text/html");
  client.println();

  if (sub[0] == "interface")
  {
    // CSS + HTML HEAD
    client.println("<!DOCTYPE html><html>");
    client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
    client.println("<link rel=\"icon\" href=\"data:,\">");
    client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
    client.println(".button { background-color: #195B6A; border: none; color: white; padding: 16px 40px;");
    client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
    client.println(".button2 { background-color: #77878A; }</style></head>");
    // HTML
    client.println("<body><h1>Home Control</h1>");
    client.println("<p>TESTING:</p>");
    client.println("<p><a href=\"/nice\"><button class=\"button\">OKE COOL</button></a></p>");
    client.println("</body></html>");
    client.println();
    return false;
  }
  else if (sub[0] == "test")
  {
    client.println("okey");
    return false;
  }
  else if (sub[0] == "deviceList")
  {
    for(unsigned char i = 0; i < 32; i++)
    {
      if (devices[i])
      {
        devices[i]->printAsListTo(client);
        client.print(';');
      }
    }
    return false;
  }
  else if (sub[0] == "device" && subCount == 2)
  {
    unsigned char addr = sub[1].toInt();

    Device* d = getDeviceWithAddress(addr);
    if (d)
    {
      d->printAsListTo(client);
    }

    return false;
  }
  else if (sub[0] == "setDeviceName" && subCount == 3)
  {
    unsigned char addr = sub[1].toInt();

    Device* d = getDeviceWithAddress(addr);
    if (d && sub[2].length() > 1 && sub[2].length() < 25)
    {
      sub[2].toCharArray(d->name, sub[2].length() + 1);
      saveDevicesToRom();
      client.println("okey");
    }
    else
    {
      client.println("not okey");
    }
    return false;
  }
  else if (sub[0] == "ping" && subCount == 2)
  {
    unsigned char addr = sub[1].toInt();
    webRequest->requestId = pingSlave(addr);
    return true;
  }
  else if (sub[0] == "bind" && subCount > 1 && subCount <= 8)
  {
    unsigned char ufid[7];
    memset(ufid, 0x0, sizeof(ufid));
    for (unsigned char i = 1; i < subCount; i++)
      ufid[i - 1] = sub[i].toInt();
    webRequest->requestId = bindSlave(ufid);
    return true;
  }
  else if (sub[0] == "unbind" && subCount == 2)
  {
    unsigned char addr = sub[1].toInt();
    webRequest->requestId = unbindSlave(addr);
    return true;
  }
  else if (sub[0] == "prop" && subCount > 3 && subCount < 20)
  {
    unsigned char addr = sub[1].toInt();
    unsigned char startPos = sub[2].toInt();
    unsigned char data[16] = {0x20, startPos};
    for (unsigned char i = 0; i < subCount - 3; i++)
      data[i + 2] = sub[i + 3].toInt();
    webRequest->requestId = sr.sendRequest(addr, propertySetAnswer, data, subCount - 1);
    return true;
  }
  else
  {
    client.println("nope");
    return false;
  }

  return false;
}

unsigned char setSlaveProperties(unsigned char addr, unsigned char startPos, unsigned char* values, unsigned char valueCount)
{
  if (valueCount == 0)
    return 0xFF;

  unsigned char data[16] = {0x20, startPos};
  for (unsigned char i = 0; i < valueCount && i < 14; i++)
    data[i + 2] = values[i];
  return sr.sendRequest(addr, propertySetAnswer, data, valueCount + 2);
}

void propertySetAnswer(ResponseStatus status, Request* requested)
{
  if (status == Okay)
  {
    Serial.print("\t-> Propery for slave ");
    Serial.print(requested->fromAddress);
    Serial.println(" was set successfully!");

    Device* setDevice = getDeviceWithAddress(requested->fromAddress);
    if (setDevice)
    {
      unsigned char startPos = requested->sentData[1];
      unsigned char valueCount = requested->sentDataLength - 2;
      for(unsigned char i = 0; i < valueCount; i++)
        setDevice->knownProperties[startPos + i] = requested->sentData[i + 2];
    }
  }

  WebRequest* request = getWebRequestFor(requested->id);
  if (request)
  {
    request->println(static_cast<int>(status));
    request->close();
  }
}

unsigned char pingSlave(unsigned char addr)
{
  unsigned char data[1] = {0x1};

  return sr.sendRequest(addr, pingAnswer, data, sizeof(data));
}

void pingAnswer(ResponseStatus status, Request* requested)
{
  Serial.print("\t-> Slave ");
  Serial.print(requested->fromAddress);
  Serial.print(" was pinged: ");
  Serial.println(status == Okay ? "Okay" : (status == Failed ? "Failed" : "No response"));

  Device* dev = getDeviceWithAddress(requested->fromAddress);
  if (dev)
  {
    bool online = status == Okay;

    if (dev->online != online)
    {
      dev->online = online;
      saveDevicesToRom();
    }
  }

  WebRequest* request = getWebRequestFor(requested->id);
  if (request)
  {
    request->println(static_cast<int>(status));
    request->close();
  }
  /*if (requested->state)

  {

    WiFiClient* wc = (WiFiClient*)requested->state;

    wc->println(status);

    wc->stop();

  }*/
# 617 "h:\\Documents\\GitHub\\Home-Control-GIP\\Home Control Protocol\\HCP_MCU_v4\\HCP_MCU_v4.ino"
}

unsigned char refreshSlave(unsigned char addr)
{
  unsigned char data[1] = {0x15};

  return sr.sendRequest(addr, refreshAnswer, data, sizeof(data));
}

void refreshAnswer(ResponseStatus status, Request* requested)
{
  Device* dev = getDeviceWithAddress(requested->fromAddress);

  if (dev)
  {
    bool online = status != NoResponse;

    if (online)
    {
      Serial.print("Received ");
      Serial.print(requested->responseLength);
      Serial.println(" bytes for live data.");

      for(unsigned char i = 0; i < requested->responseLength; i++)
        dev->liveDeviceInfo[i] = requested->response[i];
    }

    if (dev->online != online)
    {
      dev->online = online;
      saveDevicesToRom();
    }
  }

  WebRequest* request = getWebRequestFor(requested->id);
  if (request)
  {
    request->println(static_cast<int>(status));
    request->close();
  }
}

unsigned char bindSlave(unsigned char ufid[7])
{
  return bindSlave(ufid, getNewAddress());
}

unsigned char bindSlave(unsigned char ufid[7], unsigned char withAddress)
{
  for(unsigned char i = 0; i < 32; i++)
  {
    if (devices[i] && (devices[i]->address == withAddress || memcmp(ufid, devices[i]->uniqueFactoryId, 7) == 0))
    {
      Serial.println("----> Warning: tried to bind 2 slaves with either the same addr or ufid.");

      return 0xFF;
    }
  }

  unsigned char id = rebindSlave(ufid, withAddress);

  registerNewDevice(ufid, withAddress);
  saveDevicesToRom();
  return id;
}

unsigned char rebindSlave(unsigned char ufid[7], unsigned char withAddress)
{
  unsigned char data[9];
  memcpy(&data[1], &ufid[0], 7);
  data[0] = 0x10;
  data[8] = withAddress;
  sr.broadcast(data, sizeof(data), DataRequest, 130);
  return 130;
}

unsigned char unbindSlave(unsigned char withAddress)
{
  unsigned char data[1] = { 0x2 };
  unsigned char id = sr.sendRequest(withAddress, unbindAnswer, data, sizeof(data));

  for(unsigned char i = 0; i < 32; i++)
  {
    if (devices[i] && devices[i]->address == withAddress)
    {
       delete devices[i];
       devices[i] = nullptr;

       saveDevicesToRom();

       Serial.println("\t-> Device is unregistered, waiting for unbind request... (no answer is ok)");
       break;
    }
  }

  return id;
}

void unbindAnswer(ResponseStatus status, Request* requested)
{
  if (status == Okay)
  {
    Serial.print("\t-> Slave ");
    Serial.print(requested->fromAddress);
    Serial.println(" was successfully unbound from this master.");
  }

  WebRequest* request = getWebRequestFor(requested->id);
  if (request)
  {
    request->println(static_cast<int>(status));
    request->close();
  }
}

void refreshSlaves()
{
  static unsigned char i = 0;

  if (i >= 32)
    i = 0;

  for(; i < 32; i++)
  {
    if (devices[i] && devices[i]->working)
    {
      refreshSlave(devices[i]->address);

      i++;
      break;
    }
  }
}

void pingSlaves()
{
  static unsigned char i = 0;

  if (i >= 32)
    i = 0;

  for(; i < 32; i++)
  {
    if (devices[i] && devices[i]->working)
    {
      pingSlave(devices[i]->address);

      i++;
      break;
    }
  }
}

void retryNotWorkingBinds()
{
  static unsigned char i = 0;

  if (i >= 32)
    i = 0;

  for(; i < 32; i++)
  {
    if (devices[i] && !(devices[i]->working))
    {
      Serial.print("----> Trying to let device ");
      devices[i]->printTo(Serial);
      Serial.println(" work...");

      rebindSlave(devices[i]->uniqueFactoryId, devices[i]->address);
      /*unsigned char data[9];

      memcpy(&data[1], devices[i]->uniqueFactoryId, 7);

      data[0] = 0x10;

      data[8] = devices[i]->address;

      sr.broadcast(data, sizeof(data), DataRequest, 130);*/
# 792 "h:\\Documents\\GitHub\\Home-Control-GIP\\Home Control Protocol\\HCP_MCU_v4\\HCP_MCU_v4.ino"
      i++;
      break;
    }
  }
}

unsigned char getNewAddress()
{
  unsigned char s = EEPROM.read(0);
  if (s == 0xFF)
    s = 1;
  EEPROM.write(0, ++s);
  return s;
}

void printDevices()
{
  Serial.println("----> List of devices that are controlled by this master:");
  unsigned char deviceCount = 0;
  for(unsigned char i = 0; i < 32; i++)
  {
    if (devices[i])
    {
      Serial.print("\t");
      Serial.print(++deviceCount);
      Serial.print(": ");
      devices[i]->printTo(Serial);
      Serial.println();
    }
  }
}

void loadDevicesFromRom()
{
  /*Serial.print("Size of device: ");

  Serial.println(sizeof(Device));*/
# 828 "h:\\Documents\\GitHub\\Home-Control-GIP\\Home Control Protocol\\HCP_MCU_v4\\HCP_MCU_v4.ino"
  unsigned char deviceCount = 0;

  for (int i = 0; i < 32; i++)
  {
    if (EEPROM.read(i * 120 + 100 + 120 - 1) == 0xFF)
    {
      // Device save location is empty
      devices[i] = nullptr;
    }
    else
    {
      // Device save location is used, read it
      unsigned char bytes[120];
      for(int j = 0; j < 120; j++)
          bytes[j] = EEPROM.read(i * 120 + 100 + j);
      devices[i] = new Device(bytes);
      /*Serial.print("Red device: ");

      devices[i]->printToSerial();

      Serial.println();*/
# 847 "h:\\Documents\\GitHub\\Home-Control-GIP\\Home Control Protocol\\HCP_MCU_v4\\HCP_MCU_v4.ino"
      deviceCount++;
    }
  }

  Serial.print("\t-> ");
  Serial.print(deviceCount);
  Serial.println(" devices were loaded from ROM.");
}

void clearRomDevices()
{
  for (int i = 100; i < 100 + 32 * 120; i++)
    EEPROM.write(i, 0xFF);
  for(unsigned char i = 0; i < 32; i++)
  {
    if (devices[i])
    {
      delete devices[i];
      devices[i] = nullptr;
    }
  }

  EEPROM.commit();

  Serial.println("\t-> All devices were ereased from ROM.");
}

void saveDevicesToRom()
{
  unsigned char deviceCount = 0;

  for (int i = 0; i < 32; i++)
  {
    if (devices[i])
    {
      /*Serial.print("Saving device ");

      Serial.print(i);

      Serial.print(": ");

      devices[i]->printToSerial();

      Serial.println();*/
# 887 "h:\\Documents\\GitHub\\Home-Control-GIP\\Home Control Protocol\\HCP_MCU_v4\\HCP_MCU_v4.ino"
      unsigned char* bytes = devices[i]->getBytes();
      for(int j = 0; j < 120; j++)
          EEPROM.write(i * 120 + 100 + j, bytes[j]);
      deviceCount++;
    }
    else
    {
      EEPROM.write(i * 120 + 100 + 120 - 1, 0xFF);
    }
  }

  EEPROM.commit();

  Serial.print("\t-> ");
  Serial.print(deviceCount);
  Serial.println(" devices were saved to ROM.");
}

Device* registerNewDevice(unsigned char ufid[7], unsigned char addr)
{
  for(unsigned char i = 0; i < 32; i++)
  {
    if (!devices[i])
    {
      devices[i] = new Device(ufid, addr, "Test");

      return devices[i];
    }
  }

  return nullptr;
}

Device* getDeviceWithAddress(unsigned char addr)
{
  for(unsigned char i = 0; i < 32; i++)
  {
    if (devices[i] && devices[i]->address == addr)
      return devices[i];
  }

  return nullptr;
}

WebRequest* getWebRequestFor(unsigned char requestId)
{
  for(int i = 0; i < 8; i++)
  {
    if (requesters[i] && requesters[i]->requestId == requestId)
      return requesters[i];
  }

  return nullptr;
}
