# What is the Driver Station?
The driver station is a program created by FIRST that allows teams to control their robot wirelessly through a 2.4ghz Wifi radio.
Normally, the driver station is allowed only to work with the RoboRIO and its own Driver station class, which handles the UDP frames by itself.
The software in this repo is what is needed to make an [ESP12F module](http://www.ctr-electronics.com/gadgeteer-wifi-module.html#product_tabs_technical_resources) along with a [HERO development board](http://www.ctr-electronics.com/hro.html) to talk with the Driver station and provide enough functionality to get a robot to drive using the driver station.
# What is needed for functionality?
All the hardware needed for this functionality is the HERO board and an ESP12F module along with a ribbon cable connecting the two.
Software wise, a new firmware image needs to be flashed onto the ESP module, but the ability to do so is done through the HERO itself, and an example project is set up to work with the new driverstation class.
### Process for flashing the Module
Plug in the ESP12F Module into port 1 of the HERO board. Open the solution "HERO_ESP_Writer" and deploy that to the HERO. The HERO will flash the module with the new firmware and it will be ready as soon as it's done.
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
### Datagram breakdown
* Driver Station to Robot (Port 57655 -> Port 1110)
  * 1,2 Packet # - These two bytes, big endian, specify what number packet this is, and is used for latency control on the Driver Station
  * 3 Unknown currently
  * 4 Robot State - Specifies what state the robot is in, 0-2 is Teleop, Test, Auton disabled respectively, 4-6 is the enabled version
  * 5,6 Unknown currently
* Robot to Driver Station (Port 34959 -> Port 1150)
  * 1-4 Same first four bytes from Driver station, ensures packets were sent correctly
  * 5-7 Battery voltage, first byte is integer voltage, second and third is decimal voltage
  * 8- Unknown currently
