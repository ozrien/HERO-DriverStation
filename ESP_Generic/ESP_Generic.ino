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

WiFiUDP Udp;//UDP Server
ESP8266WebServer server(80);//Webpage server
ESP8266HTTPUpdateServer httpUpdater;

uint8_t lastLen = 0;
uint16_t webCounter = 0;
uint16_t packetCounter = 0;
File f;

uint16_t currentTime;
const uint16_t timeout = 10;
bool writing = true;
int timer = 0;
int receivePort = 0;

enum ProcessState
{
  header1,
  header2,
  lenAssign,
  payload,
  ipAssign,
  udpSend,
  portAssign
};
ProcessState currentState = header1;
ProcessState payloadState = ipAssign;
byte h1;
byte len = 0;
byte iterator = 0;
byte serialData[255];

byte tx[255];
uint16_t txIn = 0;
uint16_t txOut = 0;
uint16_t txCnt = 0;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void pushByte(byte b)
{
  tx[txIn++] = b;
  txCnt++;
  if(txIn >= 255)
    txIn = 0;
}

byte popByte()
{
  byte ret = tx[txOut++];
  txCnt--;
  if(txOut >= 255)
    txOut = 0;
  return ret;
}

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
    char ipconfigs[8];
    f.readBytes(ipconfigs, 8);
    module[0] = ipconfigs[0];
    module[1] = ipconfigs[1];
    module[2] = ipconfigs[2];
    module[3] = ipconfigs[3];
    computer[0] = ipconfigs[4];
    computer[1] = ipconfigs[5];
    computer[2] = ipconfigs[6];
    computer[3] = ipconfigs[7];
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
//Intake data from UDP stream, send to HERO/////////////////////////////////////////////////////////////////////////////////////////////////
  uint8_t data[100];
  int dSize = Udp.parsePacket();
  if(dSize)
  {
    packetCounter++;
    byte len = Udp.read(data, 100);
    uint16_t checksum = 0xAA;
    Serial.write(0xAA);
    Serial.write(len);
    checksum += len;
    for(int i = 0; i < len; i++)
    {
      Serial.write(data[i]);
      checksum += data[i];
    }
    checksum = 0 - checksum;
    uint8_t c1 = checksum >> 8;
    uint8_t c2 = checksum - (c1 << 8);
    Serial.write(c1);
    Serial.write(c2);
  }

//Read data from Serial bus and process it////////////////////////////////////////////////////////////////////////////////////////////////////////////
  if(Serial.available())
  {
    for(int i = 0; i < Serial.available(); i++)
    {
      pushByte(Serial.read());
    }
    timer = 0;
  }

  while(txCnt > 0)
  {
    switch(currentState)
    {
      default:
      {
        currentState = header1;
        break;
      }
        
      //Header 1, find first byte of header
      case header1:
      {
        h1 = popByte();
        if(h1 == 0x33)
          currentState = header2;
        break;
      }

      //Header 2, find second byte of header and decide what state I will be in
      case header2:
      {
        byte h2 = popByte();
        currentState = lenAssign;
        if( h2 == 'i')
        {
          payloadState = ipAssign;
        }
        else if(h2 == 'u')
        {
          payloadState = udpSend;
        }
        else if(h2 == 'p')
        {
          payloadState = portAssign;
        }
        else
        {
          currentState = header1;
        }
        break;
      }
        
      //Length, find the length of the payload
      case lenAssign:
      {
        len = popByte();
        currentState = payload;
        iterator = 0;
        break;
      }

      //Payload, assign data to array for use later
      case payload:
      {
        while(txCnt > 0 && iterator < len)
        {
          serialData[iterator++] = popByte();
        }
        if(iterator == len)
          currentState = payloadState;
        break;
      }
        
      //IP Assign, assign an IP address
      case ipAssign:
      {
        module[0] = serialData[0];
        module[1] = serialData[1];
        module[2] = serialData[2];
        module[3] = serialData[3];
        computer[0] = serialData[4];
        computer[1] = serialData[5];
        computer[2] = serialData[6];
        computer[3] = serialData[7];
        
        f = SPIFFS.open("/config.txt", "w+");
        f.write(serialData, 8);
        f.close();
        
        WiFi.softAPConfig(module, computer, {255, 255, 255, 0});
        currentState = header1;
        break;
      }

      case udpSend:
      {
        uint16_t udpPort = ((uint16_t)serialData[1] << 8) + (serialData[0]);
        byte dataSize = len - 2;
        byte udpData[dataSize];
        for(int i = 0; i < dataSize; i++)  udpData[i] = serialData[i+2];
        Udp.beginPacket(computer, udpPort);
        Udp.write(udpData, dataSize);
        Udp.endPacket();
        currentState = header1;
        break;
      }

      case portAssign:
      {
        receivePort = ((uint16_t)serialData[1] << 8 ) + (serialData[0]);
        currentState = header1;
        Udp.begin(receivePort);
        break;
      }
    }
  }

//Host Webpage///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  server.handleClient();

  
  webCounter++;
  delay(1);//Delay for delaying purposes
}


