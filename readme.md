## Picoalexa allows you to easily control your Raspberry Pi Pico W with the Alexa voice assistant.
It comes in an easy to use Arduino library.
Now compatible with Raspberry Pi Pico W!

#### What does this do similar projects don't already?

It allows you to set a ranged value (e.g. Brightness, Temperature) and optionally a color, additionally to standard on/off control.
For example, you can say "Alexa, turn the light to 75% / 21 degrees".  

By default, it's possible to add up to a total of 10 devices (read below on how to increase the cap).  
Each device has a brightness range from 0 to 255, where 0 is off and 255 is fully on.
You can get a percentage from that value using `Picoalexa.toPercent(brightness);`

**⚠️ Do not use Picoalexa in a production environment! The API utilized is known to not be permanently stable and may cease functioning at any point.**

#### How do I install the library?

It's a standard Arduino library. Just download it and add it as ZIP library in the IDE.

#### What has to be done to use it?

Picoalexa is designed to be as simple to use as possible.

First, you'll need a global object declaration and a prototype for the function that Picoalexa will call when the device is changed:
```cpp
#include <Picoalexa.h>

void firstDeviceChanged(uint8_t brightness);

Picoalexa Picoalexa;
```

You then want to actually add the callback function (one for each device)
```cpp
void firstDeviceChanged(uint8_t brightness) {
  //brightness parameter contains the new device state (0:off,255:on,1-254:dimmed)
  
  //do what you'd like to happen here (e.g. control an LED)
}
```

In your setup function, after you connected to WiFi, you'd want to add your devices:
```cpp
Picoalexa.addDevice("Alexa name of the device", firstDeviceChanged);
```
The first parameter of the function is a string with the invocation name, the second is the name of your callback function (the one Picoalexa will call when the state of the device was changed)
You may also add a third `uint8_t` parameter that will specify the default brightness at boot.

Below the device definition in setup, add:
```cpp
Picoalexa.begin();
```

Finally, in the loop() function, add:
```cpp
Picoalexa.loop();
```

And that's it!


There is a second way to add devices which is more complicated, but allows you to update device values yourself.
In global:
```cpp
PicoalexaDevice* d;
```
In setup:
```cpp
d = new PicoalexaDevice("Alexa name of the device", firstDeviceChanged);
Picoalexa.addDevice(d);
```
As you can see, `PicoalexaDevice` takes the same parameters. However, you can now do stuff like:
```cpp
d->setValue(22);
uint8_t bri = d->getValue(); //bri will have the device value
String name = d->getName(); //just in case you forget it
```

You can find a complete example implementation in the examples folder. Just change your WiFi info and try it out!

Picoalexa uses an internal WebServer. You can got to `http://[yourEspIP]/Picoalexa` to see all devices and their current state.

#### What devices types and Echos does Picoalexa support?

The library aims to work with every Echo on the market, but there are a few things to look out for.  
Picoalexa only works with a genuine Echo speaker, it probably wont work with Echo emulators, RPi homebrew devices or just the standalone app.  
On an Echo Dot 1st and 2nd gen and the first gen Echo, color temperature adjustment (white spectrum) does not work as of March 2019.   

Here is an overview of the devices (light types) Picoalexa can emulate:  

| Device type                              | Notes                                           |
|------------------------------------------|-------------------------------------------------|
| PicoalexaDeviceType::dimmable             | Works as intended (dimming)                     |
| PicoalexaDeviceType::whitespectrum        | Color temperature adjustment not working on Dot |
| PicoalexaDeviceType::color                | Works as intended (dimming + color)             |
| PicoalexaDeviceType::extendedcolor        | Color temperature adjustment not working on Dot |
| PicoalexaDeviceType::onoff (experimental) | Deprecated. Treated as dimmable.                |

See the example `PicoalexaFullyFeatured` to learn how to define each device type and use the new PicoalexaDevice pointer callback function type!

#### My devices are not found?!

Confirm your Raspberry Pi Pico W is connected. Go to the /Picoalexa subpage to confirm all your devices are defined.  
Then ask Alexa to discover devices again or try it via the Alexa app.  
Often, it also helps to reboot your Echo once!  
If nothing helps, open a Github issue and we will help.  
If you can, add `#define PICOALEXA_DEBUG` before `#include <Picoalexa.h>` and include the serial monitor output that is printed while the issue occurs.  

#### Why only 10 virtual devices?

Each device "slot" occupies memory, even if no device is initialized.  
You can change the maximum number of devices by adding `#define PICOALEXA_MAXDEVICES 20` (for example) before `#include <Picoalexa.h>`  
I recommend setting MAXDEVICES to the exact number of devices you want to add to optimize memory usage.

#### How does this work?

Picoalexa emulates parts of the SSDP protocol and the Philips hue API, just enough so it can be discovered and controlled by Alexa.
Parts of the code are based on:
- [arduino-esp8266-alexa-wemo-switch](https://github.com/kakopappa/arduino-esp8266-alexa-wemo-switch) by kakopappa (Foundation)
- [ESP8266HueEmulator](https://github.com/probonopd/ESP8266HueEmulator) by probonopd
