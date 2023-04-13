/*
 * An example that demonstrates most capabilities of Picoalexa v2.4.0
 */ 
#include <WiFi.h>
//#define PICOALEXA_ASYNC            //uncomment for async operation (can fix empty body issue)
//#define PICOALEXA_NO_SUBPAGE       //disable /Picoalexa status page
//#define PICOALEXA_DEBUG            //activate debug serial logging
//#define PICOALEXA_MAXDEVICES 15    //set maximum devices add-able to Picoalexa
#include <Picoalexa.h>

// Change this!!
const char* ssid = "...";
const char* password = "wifipassword";

// prototypes
bool connectWifi();

//callback functions
//new callback type, contains device pointer
void alphaChanged(PicoalexaDevice* dev);
void betaChanged(PicoalexaDevice* dev);
void gammaChanged(PicoalexaDevice* dev);
//you can now use one callback for multiple devices
void deltaOrEpsilonChanged(PicoalexaDevice* dev);

//create devices yourself
PicoalexaDevice* epsilon;

bool wifiConnected = false;

Picoalexa Picoalexa;

void setup()
{
  Serial.begin(115200);
  // Initialise wifi connection
  wifiConnected = connectWifi();
  
  if(!wifiConnected){
    while (1) {
      Serial.println("Cannot connect to WiFi. Please check data and reset the ESP.");
      delay(2500);
    }
  }
  
  // Define your devices here. 
  Picoalexa.addDevice("Alpha", alphaChanged, PicoalexaDeviceType::onoff); //non-dimmable device
  Picoalexa.addDevice("Beta", betaChanged, PicoalexaDeviceType::dimmable, 127); //Dimmable device, optional 4th parameter is beginning state (here fully on)
  Picoalexa.addDevice("Gamma", gammaChanged, PicoalexaDeviceType::whitespectrum); //color temperature (white spectrum) device
  Picoalexa.addDevice("Delta", deltaOrEpsilonChanged, PicoalexaDeviceType::color); //color device
  
  epsilon = new PicoalexaDevice("Epsilon", deltaOrEpsilonChanged, PicoalexaDeviceType::extendedcolor); //color + color temperature
  Picoalexa.addDevice(epsilon);
  epsilon->setValue(128); //creating the device yourself allows you to e.g. update their state value at any time!
  
  epsilon->setColor(200); //color temperature in mireds
  epsilon->setColor(255,160,0); //color in RGB
  epsilon->setColor(14000,255); //color in Hue + Sat
  epsilon->setColorXY(0.50,0.50); //color in XY

  PicoalexaDevice* d = Picoalexa.getDevice(3); //this will get "Delta", the index is zero-based
  d->setPercent(50); //set value "brightness" in percent

  Picoalexa.begin();
}
 
void loop()
{
 Picoalexa.loop();
 delay(1);
}

//our callback functions
void alphaChanged(PicoalexaDevice* d) {
  if (d == nullptr) return; //this is good practice, but not required

  //do what you need to do here
  //EXAMPLE
  Serial.print("A changed to ");
  if (d->getValue()){
    Serial.println("ON");
  }
  else {
    Serial.println("OFF");
  }
}

void betaChanged(PicoalexaDevice* d) {
  if (d == nullptr) return;

  uint8_t brightness = d->getValue();
  uint8_t percent = d->getPercent();
  uint8_t degrees = d->getDegrees(); //for heaters, HVAC, ...

  Serial.print("B changed to ");
  Serial.print(percent);
  Serial.println("%");
}

void gammaChanged(PicoalexaDevice* d) {
  if (d == nullptr) return;
  Serial.print("C changed to ");
  Serial.print(d->getValue());
  Serial.print(", colortemp ");
  Serial.print(d->getCt());
  Serial.print(" (");
  Serial.print(d->getKelvin()); //this is more common than the hue mired values
  Serial.println("K)");
}

void deltaOrEpsilonChanged(PicoalexaDevice* d)
{
  if (d == nullptr) return;

  if (d->getId() == 3) //device "delta"
  {
    Serial.print("D changed to ");
    Serial.print(d->getValue());
    Serial.print(", color R");
    Serial.print(d->getR());
    Serial.print(", G");
    Serial.print(d->getG());
    Serial.print(", B");
    Serial.println(d->getB());
    /*//alternative
    uint32_t rgb = d->getRGB();
    Serial.print(", R "); Serial.print((rgb >> 16) & 0xFF);
    Serial.print(", G "); Serial.print((rgb >>  8) & 0xFF);
    Serial.print(", B "); Serial.println(rgb & 0xFF); */
  } else { //device "epsilon"
    Serial.print("E changed to ");
    Serial.print(d->getValue());
    Serial.print(", colormode ");
    switch(d->getColorMode())
    {
      case PicoalexaColorMode::hs:
        Serial.print("hs, "); Serial.print("hue "); Serial.print(d->getHue()); Serial.print(", sat "); Serial.println(d->getSat()); break;
      case PicoalexaColorMode::xy:
        Serial.print("xy, "); Serial.print("x "); Serial.print(d->getX()); Serial.print(", y "); Serial.println(d->getY()); break;
      case PicoalexaColorMode::ct:
        Serial.print("ct, "); Serial.print("ct "); Serial.println(d->getCt()); break;
      case PicoalexaColorMode::none:
        Serial.println("none"); break;
    }
  }
}

// connect to wifi – returns true if successful or false if not
bool connectWifi(){
  bool state = true;
  int i = 0;
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");
  Serial.println("Connecting to WiFi");

  // Wait for connection
  Serial.print("Connecting...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (i > 20){
      state = false; break;
    }
    i++;
  }
  Serial.println("");
  if (state){
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
  else {
    Serial.println("Connection failed.");
  }
  return state;
}
