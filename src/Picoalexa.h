#ifndef Picoalexa_h
#define Picoalexa_h

/*
 * Alexa Voice On/Off/Brightness/Color Control. Emulates a Philips Hue bridge to Alexa.
 * 
 * This was put together from these two excellent projects:
 * https://github.com/kakopappa/arduino-esp8266-alexa-wemo-switch
 * https://github.com/probonopd/ESP8266HueEmulator
 */
/*
 * @title Espalexa library
 * @version 2.7.0
 * @author Christian Schwinne
 * @license MIT
 * @contributors d-999
 */

#include "Arduino.h"

//you can use these defines for library config in your sketch. Just use them before #include <Picoalexa.h>
//#define PICOALEXA_ASYNC

//in case this is unwanted in your application (will disable the /Picoalexa value page)
//#define PICOALEXA_NO_SUBPAGE

#ifndef PICOALEXA_MAXDEVICES
 #define PICOALEXA_MAXDEVICES 10 //this limit only has memory reasons, set it higher should you need to, max 128
#endif

//#define PICOALEXA_DEBUG

#include <WiFi.h>
#include <WebServer.h>
#include <WiFiUdp.h>

#ifdef PICOALEXA_DEBUG
 #pragma message "Picoalexa 2.7.0 debug mode"
 #define EA_DEBUG(x)  Serial.print (x)
 #define EA_DEBUGLN(x) Serial.println (x)
#else
 #define EA_DEBUG(x)
 #define EA_DEBUGLN(x)
#endif

#include "PicoalexaDevice.h"

#define DEVICE_UNIQUE_ID_LENGTH 12

class Picoalexa {
private:
  //private member vars
  WebServer* server;
  uint8_t currentDeviceCount = 0;
  bool discoverable = true;
  bool udpConnected = false;

  PicoalexaDevice* devices[PICOALEXA_MAXDEVICES] = {};
  //Keep in mind that Device IDs go from 1 to DEVICES, cpp arrays from 0 to DEVICES-1!!
  
  WiFiUDP PicoalexaUdp;
  IPAddress ipMulti;
  uint32_t mac24; //bottom 24 bits of mac
  String escapedMac=""; //lowercase mac address
  
  //private member functions
  const char* modeString(PicoalexaColorMode m)
  {
    if (m == PicoalexaColorMode::xy) return "xy";
    if (m == PicoalexaColorMode::hs) return "hs";
    return "ct";
  }
  
  const char* typeString(PicoalexaDeviceType t)
  {
    switch (t)
    {
      case PicoalexaDeviceType::dimmable:      return "Dimmable light";
      case PicoalexaDeviceType::whitespectrum: return "Color temperature light";
      case PicoalexaDeviceType::color:         return "Color light";
      case PicoalexaDeviceType::extendedcolor: return "Extended color light";
      default: return "";
    }
  }
  
  const char* modelidString(PicoalexaDeviceType t)
  {
    switch (t)
    {
      case PicoalexaDeviceType::dimmable:      return "LWB010";
      case PicoalexaDeviceType::whitespectrum: return "LWT010";
      case PicoalexaDeviceType::color:         return "LST001";
      case PicoalexaDeviceType::extendedcolor: return "LCT015";
      default: return "";
    }
  }
  
  void encodeLightId(uint8_t idx, char* out)
  {
    uint8_t mac[6];
    WiFi.macAddress(mac);

    sprintf_P(out, PSTR("%02X:%02X:%02X:%02X:%02X:%02X:00:11-%02X"), mac[0],mac[1],mac[2],mac[3],mac[4],mac[5], idx);
  }

  // construct 'globally unique' Json dict key fitting into signed int
  inline int encodeLightKey(uint8_t idx)
  {
    static_assert(PICOALEXA_MAXDEVICES <= 128, "");
    return (mac24<<7) | idx;
  }

  // get device index from Json key
  uint8_t decodeLightKey(int key)
  {
    return (((uint32_t)key>>7) == mac24) ? (key & 127U) : 255U;
  }

  //device JSON string: color+temperature device emulates LCT015, dimmable device LWB010, (TODO: on/off Plug 01, color temperature device LWT010, color device LST001)
  void deviceJsonString(PicoalexaDevice* dev, char* buf)
  {
    char buf_lightid[27];
    encodeLightId(dev->getId() + 1, buf_lightid);
    
    char buf_col[80] = "";
    //color support
    if (static_cast<uint8_t>(dev->getType()) > 2)
      sprintf_P(buf_col,PSTR(",\"hue\":%u,\"sat\":%u,\"effect\":\"none\",\"xy\":[%f,%f]")
        ,dev->getHue(), dev->getSat(), dev->getX(), dev->getY());
      
    char buf_ct[16] = "";
    //white spectrum support
    if (static_cast<uint8_t>(dev->getType()) > 1 && dev->getType() != PicoalexaDeviceType::color)
      sprintf(buf_ct, ",\"ct\":%u", dev->getCt());
    
    char buf_cm[20] = "";
    if (static_cast<uint8_t>(dev->getType()) > 1)
      sprintf(buf_cm,PSTR("\",\"colormode\":\"%s"), modeString(dev->getColorMode()));
    
    sprintf_P(buf, PSTR("{\"state\":{\"on\":%s,\"bri\":%u%s%s,\"alert\":\"none%s\",\"mode\":\"homeautomation\",\"reachable\":true},"
                   "\"type\":\"%s\",\"name\":\"%s\",\"modelid\":\"%s\",\"manufacturername\":\"Philips\",\"productname\":\"E%u"
                   "\",\"uniqueid\":\"%s\",\"swversion\":\"Picoalexa-2.7.0\"}")
                   
    , (dev->getValue())?"true":"false", dev->getLastValue()-1, buf_col, buf_ct, buf_cm, typeString(dev->getType()),
    dev->getName().c_str(), modelidString(dev->getType()), static_cast<uint8_t>(dev->getType()), buf_lightid);
  }
  
  //Picoalexa status page /Picoalexa
  #ifndef PICOALEXA_NO_SUBPAGE
  void servePage()
  {
    EA_DEBUGLN("HTTP Req Picoalexa ...\n");
    String res = "Hello from Picoalexa!\r\n\r\n";
    for (int i=0; i<currentDeviceCount; i++)
    {
      PicoalexaDevice* dev = devices[i];
      res += "Value of device " + String(i+1) + " (" + dev->getName() + "): " + String(dev->getValue()) + " (" + typeString(dev->getType());
      if (static_cast<uint8_t>(dev->getType()) > 1) //color support
      {
        res += ", colormode=" + String(modeString(dev->getColorMode())) + ", r=" + String(dev->getR()) + ", g=" + String(dev->getG()) + ", b=" + String(dev->getB());
        res +=", ct=" + String(dev->getCt()) + ", hue=" + String(dev->getHue()) + ", sat=" + String(dev->getSat()) + ", x=" + String(dev->getX()) + ", y=" + String(dev->getY());
      }
      res += ")\r\n";
    }
    res += "\r\nFree Heap: " + (String)rp2040.getFreeHeap();
    res += "\r\nUptime: " + (String)millis();
    res += "\r\n\r\nPicoalexa library v2.7.0 by Christian Schwinne 2021";
    server->send(200, "text/plain", res);
  }
  #endif

  //not found URI (only if internal webserver is used)
  void serveNotFound()
  {
    EA_DEBUGLN("Not-Found HTTP call:");
    EA_DEBUGLN("URI: " + server->uri());
    EA_DEBUGLN("Body: " + server->arg(0));
    if(!handleAlexaApiCall(server->uri(), server->arg(0)))
      server->send(404, "text/plain", "Not Found (Picoalexa)");
  }

  //send description.xml device property page
  void serveDescription()
  {
    EA_DEBUGLN("# Responding to description.xml ... #\n");
    IPAddress localIP = WiFi.localIP();
    char s[16];
    sprintf(s, "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);
    char buf[1024];
    
    sprintf_P(buf,PSTR("<?xml version=\"1.0\" ?>"
        "<root xmlns=\"urn:schemas-upnp-org:device-1-0\">"
        "<specVersion><major>1</major><minor>0</minor></specVersion>"
        "<URLBase>http://%s:80/</URLBase>"
        "<device>"
          "<deviceType>urn:schemas-upnp-org:device:Basic:1</deviceType>"
          "<friendlyName>Picoalexa (%s:80)</friendlyName>"
          "<manufacturer>Royal Philips Electronics</manufacturer>"
          "<manufacturerURL>http://www.philips.com</manufacturerURL>"
          "<modelDescription>Philips hue Personal Wireless Lighting</modelDescription>"
          "<modelName>Philips hue bridge 2012</modelName>"
          "<modelNumber>929000226503</modelNumber>"
          "<modelURL>http://www.meethue.com</modelURL>"
          "<serialNumber>%s</serialNumber>"
          "<UDN>uuid:2f402f80-da50-11e1-9b23-%s</UDN>"
          "<presentationURL>index.html</presentationURL>"
        "</device>"
        "</root>"),s,s,escapedMac.c_str(),escapedMac.c_str());
          
    server->send(200, "text/xml", buf);
    
    EA_DEBUGLN("Send setup.xml");
    EA_DEBUGLN(buf);
  }
  
  //init the server
  void startHttpServer()
  {

    if (server == nullptr) {
      server = new WebServer(80);
      server->onNotFound([=](){serveNotFound();});
    }

    #ifndef PICOALEXA_NO_SUBPAGE
    server->on("/Picoalexa", HTTP_GET, [=](){servePage();});
    #endif
    server->on("/description.xml", HTTP_GET, [=](){serveDescription();});
    server->begin();

  }

  //respond to UDP SSDP M-SEARCH
  void respondToSearch()
  {
    IPAddress localIP = WiFi.localIP();
    char s[16];
    sprintf(s, "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);

    char buf[1024];

    sprintf_P(buf,PSTR("HTTP/1.1 200 OK\r\n"
      "EXT:\r\n"
      "CACHE-CONTROL: max-age=100\r\n" // SSDP_INTERVAL
      "LOCATION: http://%s:80/description.xml\r\n"
      "SERVER: FreeRTOS/6.0.5, UPnP/1.0, IpBridge/1.17.0\r\n" // _modelName, _modelNumber
      "hue-bridgeid: %s\r\n"
      "ST: urn:schemas-upnp-org:device:basic:1\r\n"  // _deviceType
      "USN: uuid:2f402f80-da50-11e1-9b23-%s::upnp:rootdevice\r\n" // _uuid::_deviceType
      "\r\n"),s,escapedMac.c_str(),escapedMac.c_str());

    PicoalexaUdp.beginPacket(PicoalexaUdp.remoteIP(), PicoalexaUdp.remotePort());
    PicoalexaUdp.write((uint8_t*)buf, strlen(buf));
    PicoalexaUdp.endPacket();                    
  }

public:
  Picoalexa(){}

  //initialize interfaces
  bool begin(WebServer* externalServer = nullptr)
  {
    EA_DEBUGLN("Picoalexa Begin...");
    EA_DEBUG("MAXDEVICES ");
    EA_DEBUGLN(PICOALEXA_MAXDEVICES);
    escapedMac = WiFi.macAddress();
    escapedMac.replace(":", "");
    escapedMac.toLowerCase();

    String macSubStr = escapedMac.substring(6, 12);
    mac24 = strtol(macSubStr.c_str(), 0, 16);

    server = externalServer;
    udpConnected = PicoalexaUdp.beginMulticast(IPAddress(239, 255, 255, 250), 1900);

    if (udpConnected){
      
      startHttpServer();
      EA_DEBUGLN("Done");
      return true;
    }
    EA_DEBUGLN("Failed");
    return false;
  }

  //service loop
  void loop() {
    if (server == nullptr) return; //only if begin() was not called
    server->handleClient();
    
    if (!udpConnected) return;   
    int packetSize = PicoalexaUdp.parsePacket();    
    if (packetSize < 1) return; //no new udp packet
    
    EA_DEBUGLN("Got UDP!");

    unsigned char packetBuffer[packetSize+1]; //buffer to hold incoming udp packet
    PicoalexaUdp.read(packetBuffer, packetSize);
    packetBuffer[packetSize] = 0;
  
    PicoalexaUdp.flush();
    if (!discoverable) return; //do not reply to M-SEARCH if not discoverable
  
    const char* request = (const char *) packetBuffer;
    if (strstr(request, "M-SEARCH") == nullptr) return;

    EA_DEBUGLN(request);
    if (strstr(request, "ssdp:disc")  != nullptr &&  //short for "ssdp:discover"
        (strstr(request, "upnp:rootd") != nullptr || //short for "upnp:rootdevice"
         strstr(request, "ssdp:all")   != nullptr ||
         strstr(request, "asic:1")     != nullptr )) //short for "device:basic:1"
    {
      EA_DEBUGLN("Responding search req...");
      respondToSearch();
    }
  }

  // returns device index or 0 on failure
  uint8_t addDevice(PicoalexaDevice* d)
  {
    EA_DEBUG("Adding device ");
    EA_DEBUGLN((currentDeviceCount+1));
    if (currentDeviceCount >= PICOALEXA_MAXDEVICES) return 0;
    if (d == nullptr) return 0;
    d->setId(currentDeviceCount);
    devices[currentDeviceCount] = d;
    return ++currentDeviceCount;
  }
  
  //brightness-only callback
  uint8_t addDevice(String deviceName, BrightnessCallbackFunction callback, uint8_t initialValue = 0)
  {
    EA_DEBUG("Constructing device ");
    EA_DEBUGLN((currentDeviceCount+1));
    if (currentDeviceCount >= PICOALEXA_MAXDEVICES) return 0;
    PicoalexaDevice* d = new PicoalexaDevice(deviceName, callback, initialValue);
    return addDevice(d);
  }
  
  //brightness-only callback
  uint8_t addDevice(String deviceName, ColorCallbackFunction callback, uint8_t initialValue = 0)
  {
    EA_DEBUG("Constructing device ");
    EA_DEBUGLN((currentDeviceCount+1));
    if (currentDeviceCount >= PICOALEXA_MAXDEVICES) return 0;
    PicoalexaDevice* d = new PicoalexaDevice(deviceName, callback, initialValue);
    return addDevice(d);
  }


  uint8_t addDevice(String deviceName, DeviceCallbackFunction callback, PicoalexaDeviceType t = PicoalexaDeviceType::dimmable, uint8_t initialValue = 0)
  {
    EA_DEBUG("Constructing device ");
    EA_DEBUGLN((currentDeviceCount+1));
    if (currentDeviceCount >= PICOALEXA_MAXDEVICES) return 0;
    PicoalexaDevice* d = new PicoalexaDevice(deviceName, callback, t, initialValue);
    return addDevice(d);
  }

  void renameDevice(uint8_t id, const String& deviceName)
  {
    unsigned int index = id - 1;
    if (index < currentDeviceCount)
      devices[index]->setName(deviceName);
  }

  //basic implementation of Philips hue api functions needed for basic Alexa control

  bool handleAlexaApiCall(String req, String body)
  {  
    EA_DEBUGLN("AlexaApiCall");
    if (req.indexOf("api") <0) return false; //return if not an API call
    EA_DEBUGLN("ok");

    if (body.indexOf("devicetype") > 0) //client wants a hue api username, we don't care and give static
    {
      EA_DEBUGLN("devType");
      body = "";
      server->send(200, "application/json", F("[{\"success\":{\"username\":\"2WLEDHardQrI3WHYTHoMcXHgEspsM8ZZRpSKtBQr\"}}]"));
      return true;
    }

    if ((req.indexOf("state") > 0) && (body.length() > 0)) //client wants to control light
    {
      server->send(200, "application/json", F("[{\"success\":{\"/lights/1/state/\": true}}]"));

      uint32_t devId = req.substring(req.indexOf("lights")+7).toInt();
      EA_DEBUG("ls"); EA_DEBUGLN(devId);
      EA_DEBUGLN(devId);
      unsigned idx = decodeLightKey(devId);
      if (idx >= currentDeviceCount) return true; //return if invalid ID
      PicoalexaDevice* dev = devices[idx];
      
      dev->setPropertyChanged(PicoalexaDeviceProperty::none);
      
      if (body.indexOf("false")>0) //OFF command
      {
        dev->setValue(0);
        dev->setPropertyChanged(PicoalexaDeviceProperty::off);
        dev->doCallback();
        return true;
      }
      
      if (body.indexOf("true") >0) //ON command
      {
        dev->setValue(dev->getLastValue());
        dev->setPropertyChanged(PicoalexaDeviceProperty::on);
      }
      
      if (body.indexOf("bri")  >0) //BRIGHTNESS command
      {
        uint8_t briL = body.substring(body.indexOf("bri") +5).toInt();
        if (briL == 255)
        {
         dev->setValue(255);
        } else {
         dev->setValue(briL+1); 
        }
        dev->setPropertyChanged(PicoalexaDeviceProperty::bri);
      }
      
      if (body.indexOf("xy")   >0) //COLOR command (XY mode)
      {
        dev->setColorXY(body.substring(body.indexOf("[") +1).toFloat(), body.substring(body.indexOf(",0") +1).toFloat());
        dev->setPropertyChanged(PicoalexaDeviceProperty::xy);
      }
      
      if (body.indexOf("hue")  >0) //COLOR command (HS mode)
      {
        dev->setColor(body.substring(body.indexOf("hue") +5).toInt(), body.substring(body.indexOf("sat") +5).toInt());
        dev->setPropertyChanged(PicoalexaDeviceProperty::hs);
      }
      
      if (body.indexOf("ct")   >0) //COLOR TEMP command (white spectrum)
      {
        dev->setColor(body.substring(body.indexOf("ct") +4).toInt());
        dev->setPropertyChanged(PicoalexaDeviceProperty::ct);
      }
      
      dev->doCallback();
      
      #ifdef PICOALEXA_DEBUG
      if (dev->getLastChangedProperty() == PicoalexaDeviceProperty::none)
        EA_DEBUGLN("STATE REQ WITHOUT BODY (likely Content-Type issue #6)");
      #endif
      return true;
    }
    
    int pos = req.indexOf("lights");
    if (pos > 0) //client wants light info
    {
      int devId = req.substring(pos+7).toInt();
      EA_DEBUG("l"); EA_DEBUGLN(devId);

      if (devId == 0) //client wants all lights
      {
        EA_DEBUGLN("lAll");
        String jsonTemp = "{";
        for (int i = 0; i<currentDeviceCount; i++)
        {
          jsonTemp += '"';
          jsonTemp += encodeLightKey(i);
          jsonTemp += '"';
          jsonTemp += ':';

          char buf[512];
          deviceJsonString(devices[i], buf);
          jsonTemp += buf;
          if (i < currentDeviceCount-1) jsonTemp += ',';
        }
        jsonTemp += '}';
        server->send(200, "application/json", jsonTemp);
      } else //client wants one light (devId)
      {
        EA_DEBUGLN(devId);
        unsigned idx = decodeLightKey(devId);
        if (idx < currentDeviceCount)
        {
          char buf[512];
          deviceJsonString(devices[idx], buf);
          server->send(200, "application/json", buf);
        } else {
          server->send(200, "application/json", "{}");
        }
      }
      
      return true;
    }

    //we don't care about other api commands at this time and send empty JSON
    server->send(200, "application/json", "{}");
    return true;
  }
  
  //set whether Alexa can discover any devices
  void setDiscoverable(bool d)
  {
    discoverable = d;
  }
  
  //get PicoalexaDevice at specific index
  PicoalexaDevice* getDevice(uint8_t index)
  {
    if (index >= currentDeviceCount) return nullptr;
    return devices[index];
  }
  
  //is an unique device ID
  String getEscapedMac()
  {
    return escapedMac;
  }
  
  //convert brightness (0-255) to percentage
  uint8_t toPercent(uint8_t bri)
  {
    uint16_t perc = bri * 100;
    return perc / 255;
  }
  
  ~Picoalexa(){} //note: Picoalexa is NOT meant to be destructed
};

#endif
