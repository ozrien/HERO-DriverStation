/*
 *  Software License Agreement
 *
 * Copyright (C) Cross The Road Electronics.  All rights
 * reserved.
 * 
 * Cross The Road Electronics (CTRE) licenses to you the right to 
 * use, publish, and distribute copies of CRF (Cross The Road) binary firmware files (*.crf) 
 * and software example source ONLY when in use with Cross The Road Electronics hardware products.
 * 
 * THE SOFTWARE AND DOCUMENTATION ARE PROVIDED "AS IS" WITHOUT
 * WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTY OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * CROSS THE ROAD ELECTRONICS BE LIABLE FOR ANY INCIDENTAL, SPECIAL, 
 * INDIRECT OR CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF
 * PROCUREMENT OF SUBSTITUTE GOODS, TECHNOLOGY OR SERVICES, ANY CLAIMS
 * BY THIRD PARTIES (INCLUDING BUT NOT LIMITED TO ANY DEFENSE
 * THEREOF), ANY CLAIMS FOR INDEMNITY OR CONTRIBUTION, OR OTHER
 * SIMILAR COSTS, WHETHER ASSERTED ON THE BASIS OF CONTRACT, TORT
 * (INCLUDING NEGLIGENCE), BREACH OF WARRANTY, OR OTHERWISE
 */
 
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include "FS.h"
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>

byte computer[4];//Computer Address
byte module[4];//ESP Module Address
byte battery[2] = {33, 84};//Battery bytes
const uint16_t sendPort = 1150;//DS Expected port to send data
const uint16_t receivePort = 1110;//DSD Expected port to receive data

WiFiUDP Udp;//UDP Server
ESP8266WebServer server(80);//Webpage server
ESP8266HTTPUpdateServer httpUpdater;

uint8_t lastLen = 0;
uint16_t webCounter = 0;
uint16_t packetCounter = 0;
File f;

uint16_t currentTime;
const uint16_t timeout = 30;
bool writing = true;
byte hearbeat = 0x10;
int timer = 0;


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
String webPage()
{
  String s = "<!DOCTYPE HTML><html>";
  s += "<header><h1>Counters for counting</h1></header><br>";
  s += "<p>Counter is: ";
  s += String(webCounter);
  s += "<br>Packet Counter is: ";
  s += String(packetCounter);
  s += "</p><script language = \"javascript\">";
  s += "setTimeout(function(){location.reload();}, 5000);";
  s += "</script></html>";
  return s;
}

void handleRoot()
{
  server.send(200, "text/html", webPage());
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println();
  Serial.println("Test special");

  Udp.begin(receivePort);
  SPIFFS.begin();
  f = SPIFFS.open("/config.txt", "r");//Read config file from file system to initialize stuff
  if(!f)
  {
    //File not there, probably needs a reformat
    SPIFFS.format();
    f = SPIFFS.open("/config.txt", "w+");
    f.close();
  }
  else
  {
    for(int i = 0; i < 4; i++)
    {
      computer[i] = f.parseInt();
    }
    for(int i = 0; i < 4; i++)
    {
      module[i] = f.parseInt();
    }
    f.close();
  }
  
  Serial.println(WiFi.softAP("CTRE Radio", "password1") ? "Ready" : "Failed");//Configure Software Access Point mode
  WiFi.softAPConfig(module, computer, {255, 255, 255, 0});
  Serial.println(WiFi.softAPIP());//Confirm IP is correct

  
  server.on("/", handleRoot);
  httpUpdater.setup(&server);
  server.begin();
  
  //pinMode(0, OUTPUT);//Hardware flow control
  
  Serial.printf("%d.%d.%d.%d", computer[0], computer[1], computer[2], computer[3]);
}

void loop() {
//Intake data from UDP stream, send back required data/////////////////////////////////////////////////////////////////////////////////////////////////
  uint8_t data[100];
  int dSize = Udp.parsePacket();
  if(dSize)
  {
    packetCounter++;
    byte len = Udp.read(data, 100);
    if(writing)
    {
      Serial.write(0xAA);//First byte is header
      Serial.write(len);//Second byte is how many bytes should be received
      for(int i = 0; i < len; i++)
      {
        Serial.write(data[i]);
      }
      Serial.println();
      writing = false;
    }
    //                                                        0x10 or 0x31 depending on connected to HERO or not
    byte writeData[] = { data[0],  data[1], data[2], data[3], hearbeat, battery[0], battery[1], 0 };
    Udp.beginPacket(computer, sendPort);
    Udp.write(writeData, 8);
    Udp.endPacket();
  }
  if(currentTime > timeout) { currentTime = 0; writing = true; }
  else { currentTime++; }

//Read data from Serial bus and process it////////////////////////////////////////////////////////////////////////////////////////////////////////////
  if(Serial.available() && Serial.available() == lastLen)
  {
    char buf[255];
    for(int i = 0; i < 255; i++) buf[i] = 0x00;
    for(int i = 0; Serial.available() && i < 255; i++)
    {
      buf[i] = Serial.read();
    }
    int index = 0;
    if(buf[index] == 'i' && buf[index + 1] =='p')//starts with IP, configuring target and current ip
    {
      index++;
      f = SPIFFS.open("/config.txt", "w+");
      for(int i = 3; buf[i] != 0x00; i++) f.print(buf[i]);
      f.close();
      f = SPIFFS.open("/config.txt", "r");
      byte target[4];
      byte current[4];

      for(int i = 0; i < 4; i++)
      {
        target[i] = f.parseInt();
      }
      for(int i = 0; i < 4; i++)
      {
        current[i] = f.parseInt();
      }
      
      memcpy(computer, target, 4);
      WiFi.softAPConfig(current, target, {255, 255, 255, 0});
    }
    if(buf[index] = 'b' && buf[index + 1] == 'a')//Starts with BA, getting battery voltage and sending it to DS
    {
      battery[0] = buf[index + 3];
      battery[1] = buf[index + 5];
      index += 6;//Push index into next area to check if there's a UDP stream coming in
    }
    if(buf[index] = 'u' && buf[index + 1] == 's')//Starts with US, Send UDP data to specified port
    {
      index += 2;
      char tempPort[10];
      for(int i = 0; buf[++index] != ' '; i++)
      {
        tempPort[i] = buf[index];
      }
      int port = atoi(tempPort);
      int dataSize = buf[++index];
      byte sendDat[dataSize];
      for(int i = 0; i < dataSize; i++)
      {
        sendDat[i] = buf[++index];
      }
      Udp.beginPacket(computer, port);
      Udp.write(sendDat, dataSize);
      Udp.endPacket();
    }
    timer = 0;
  }
  lastLen = Serial.available();

//Host Webpage///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  server.handleClient();

  timer++;
  if(timer < 100) hearbeat = 0x31;
  else hearbeat = 0x10;
  
  webCounter++;
  delay(1);//Delay for delaying purposes
}


