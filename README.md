# What is the Driver Station?
The driver station is a program created by FIRST that allows teams to control their robot wirelessly through a 2.4ghz Wifi radio.
Normally, the driver station is allowed only to work with the RoboRIO and its own Driver station class, which handles the UDP frames by itself.
The software in this repo is what is needed to make an [ESP12F module](http://www.ctr-electronics.com/gadgeteer-wifi-module.html#product_tabs_technical_resources) along with a [HERO development board](http://www.ctr-electronics.com/hro.html) to talk with the Driver station and provide enough functionality to get a robot to drive using the driver station.
# What is in here?
* Bin File - The .bin file for driver station functionality
* BinSplitter - The project that takes a bin file and splits it into 11 25kb files that the HERO can use
* Documentation - Folder for pictures and files that show how this was created
* ESP_DriverStation_Source - Arduino source code for ESP module that enables driver station
* HERO_DriverStationExample - Example project for enabling the driver station and controlling a robot using Arcade drive
* HERO_ESP_Writer - Project that flashes the bin files to the ESP module
# What is needed for functionality?
All the hardware needed for this functionality is the HERO board and an ESP12F module along with a ribbon cable connecting the two.
You are able to flash the module without a HERO, however it is much more complicated and requires a gadgeteer breakout, a 3.3v source, and a usb to ttl cable. Once you flash the module once it is possible to perform an Over the Air (OTA) update.
Software wise, a new firmware image needs to be flashed onto the ESP module, but the ability to do so is done through the HERO itself, and an example project is set up to work with the new driverstation class.
### Process for flashing the Module with HERO
Plug in the ESP12F Module into port 1 of the HERO board. Open the solution "HERO_ESP_Writer" and deploy that to the HERO. The HERO will flash the module with the new firmware and it will be ready as soon as it's done.
### Process for flashing the Module with computer
1. Set up hardware for connecting to ESP - I used a [GHI Extender module](https://www.ghielectronics.com/catalog/product/273), however these are discontinued and you can use [Gadgeteer breakout modules](http://www.ctr-electronics.com/breakoutmodule.html) to gain access to the module pinout.
2. Power module and connect Rx and Tx lines to computer - I used a HERO for power, however any 3.3V source will work. It must be 3.3 volts, 5 volts will not work. After, I used an [Adafruit TTL Cable](https://www.adafruit.com/product/954?gclid=Cj0KCQjw1a3KBRCYARIsABNRnxtK8RTC_wKpGkn1eU4h5SmxbFH8F3RiO4gLpn29Okpeme-1WKFXd1MaAh4YEALw_wcB) to connect the uart lines to the computer and commonized the ground as seen below
![TTl connection](Documentation/ESPHardwareFlash.jpg)
3. Download either the [Arduino IDE](https://github.com/esp8266/Arduino) or the official [Espressif tool](http://bbs.espressif.com/viewtopic.php?f=5&t=433)
4. Put ESP in bootloader mode by holding pin 3 to ground, and pulling pin 6 to ground for a second. You should see the blue light blink
5. If using Arduino, open the source code and upload, if using the Espressif tool, point to the bin file and direct it to address 0x0 and start as seen below


<img src="Documentation/EspressifFlasher.PNG" alt="ESP Tool" width="200"/>

### Process for flashing the module OTA
Connect to the module over wifi and find its IP. Put the IP into a web browser of your choice, and append /update onto the address. You will be presented with two buttons, click the left one, choose the bin file, and click update. The bin file will be uploaded to the module and flashed by itself
# How to use the Driver Station
An example project is included that shows how to use the driver station class. The basic steps are:

*Define the Driver Station object, specify a port*
```c#
  CTRE.FRC.DriverStation ds = new CTRE.FRC.DriverStation(new new CTRE.HERO.Port1Definition());
```
*Define a controller using the Driver Station object as your provider*
```c#
  CTRE.Controller.GameController _gamepad = new CTRE.Controller.GameController(ds, 0);
```
*Call ds.update in your main loop*
```c#
  ds.update();
```
*Treat the controller as a normal controller*
```c#
  var y = _gamepad.getAxis(1);
```
# How this works
The Driver station's protocol for sending data is largely hidden behind the scenes. Using a program called [Wireshark](https://www.wireshark.org/) I was able to find the individual datagrams and figure out what each byte meant, along with the ports the Driver station expects to use for transmitting and receiving data.

Further documentation on this topic is inside the documentation folder, including a wireshark capture of the roborio-dashboard discussion over USB.
## Datagram breakdown
* Driver Station to Robot (Port 57655 -> Port 1110)
  * 1,2 Packet # - These two bytes, big endian, specify what number packet this is, and is used for latency control on the Driver Station
  * 3 Unknown currently
  * 4 Robot State - Specifies what state the robot is in, 0-2 is Teleop, Test, Auton disabled respectively, 4-6 is the enabled version
  * 5,6 Unknown currently
  * 7- Joystick data, length varies on type of joystick and how many are plugged in. For detailed information look at Driverstation Class
* Joystick Data
  * 1 - Number of bytes for joystick
  * 2,3 - Unknown currently (I think it's joystick model)
  * 4 - X number of joysticks
  * 5-(X+5) - Joystick axis data (signed byte) (each byte is an axis)
  * (X+6) - number of buttons
  * (X+7),(X+8) - bitmap of buttons
  * (X+9) - Number of POV's/Hats
  * (X+10),(x+11) - POV direction (unsigned byte)
* Robot to Driver Station (Port 34959 -> Port 1150)
  * 1-4 Same first four bytes from Driver station, ensures packets were sent correctly
  * 5-7 Battery voltage, first byte is integer voltage, second and third is decimal voltage
  * 8- Unknown currently
  
Below is a picture of a capture from Wireshark with the UDP packet from the computer to the RoboRIO, only one joystick connected.


<img src="Documentation/Wireshark.PNG" width="500"/>


## Firmware Flashing
The ESP's protocol to flashing an image into its flash is controlled through a ROM bootloader. This bootloader expects certain packets to come in, with details on the packet in the packet header. The overall process for flashing an image is
1. Put ESP into bootloader mode by pulling GPIO0 down (Pin 3 on the HERO Gadgeteer port) and resetting the module by pulling RESET down (Pin 6 on the HERO)
2. Sync baud rate of ESP to baud rate on flasher - this is done using a special packet that sends AA to the module multiple times
3. Erase flash on module - Another packet is sent that specifies the amount of space needed for the flash
4. Send .bin contents - Multiple packets are sent with the .bin contents inside them
5. Close the bootloader - A single packet is sent with the end command to take the module out of bootloader mode.
### Firmware protococol
For those wanting to create their own flashing tool, this may prove helpful
#### Packet header protocol
Largely based off [this](http://domoticx.com/esp8266-esptool-bootloader-communicatie/) sheet, it details exactly what is needed in the packet header and how to go about flashing the firmmware.

Key notes: 
* The module uses [SLIP](https://en.wikipedia.org/wiki/Serial_Line_Internet_Protocol) protocol, which means except for the header and footer, there are no 0xC0's in the packets, and so they must be replaced if they are needed.
* Checksum is created by doing XOR operation on all data bytes before the SLIP framing is done, and then that must be framed in case it is a 0xC0 or 0xDB
* Length of data is calculated before the SLIP framing
* You must make sure the other end is done talking before you can talk yourself
* Currently the amount of Flash to be erased is hard coded in this code, if you want to flash a large file you must change that byte word yourself
* I have only encounted two kinds of errors from the module, a 0x01 0x07 & 0x01 0x05
  * 0x07 - Not fatal, module can still be flashed (I believe it is warning the flasher that it is currently downloading the last bin packet)
  * 0x05 - Fatal, module must be reflashed (I believe it is a checksum error)
