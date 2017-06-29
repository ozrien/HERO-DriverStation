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

using System;
using System.Collections;
using CTRE.HERO;
using Microsoft.SPOT;

namespace CTRE.FRC
{
    public class DriverStation : IRobotStateProvider, Controller.IGameControllerValuesProvider
    {
        //public HERO.Module.WiFiESP12F _wifiModule;
        byte[] _data;
        bool _enabled;
        bool _connected;
        bool _updateFlag;
        byte[] _sendingMessage;
        /** Cache for reading out bytes in serial driver. */
        static int lastBytesToRead = 0;
        Stopwatch _timeout = new Stopwatch();
        Stopwatch _enableTimeout = new Stopwatch();
        
        System.IO.Ports.SerialPort _uart;
        Microsoft.SPOT.Hardware.OutputPort _restart;
        Microsoft.SPOT.Hardware.OutputPort _flashPin;

        State _currentState;
        private CTRE.Controller.GameController[] _controllers;
        public CTRE.Controller.GameControllerValues[] _joysticks;
        public enum State
        {
            teleopDisabled,
            testDisabled,
            autonDisabled,
            teleopEnabled,
            testEnabled,
            autonEnabled
        }

        public DriverStation(PortDefinition wifiPort)
        {
            if (wifiPort is IPortUart)
            {
                IPortUart p = (IPortUart)wifiPort;
                _uart = new System.IO.Ports.SerialPort(p.UART, 115200, System.IO.Ports.Parity.None, 8, System.IO.Ports.StopBits.One);
                _restart = new Microsoft.SPOT.Hardware.OutputPort(p.Pin6, true);
                if (wifiPort is Port1Definition) _flashPin = new Microsoft.SPOT.Hardware.OutputPort(((Port1Definition)wifiPort).Pin3, true);
                if (wifiPort is Port4Definition) _flashPin = new Microsoft.SPOT.Hardware.OutputPort(((Port4Definition)wifiPort).Pin3, true);
                if (wifiPort is Port6Definition) _flashPin = new Microsoft.SPOT.Hardware.OutputPort(((Port6Definition)wifiPort).Pin3, true);
            }
            else
            {
                throw new ArgumentException("Port is not UART compatible");
            }


            _uart.Open();
            _uart.Flush();


            _enabled = false;
            _connected = false;
            CTRE.Controller.GameControllerValues _g = new Controller.GameControllerValues();
            CTRE.Controller.GameControllerValues _h = new Controller.GameControllerValues();
            CTRE.Controller.GameControllerValues _j = new Controller.GameControllerValues();
            CTRE.Controller.GameControllerValues _k = new Controller.GameControllerValues();
            CTRE.Controller.GameControllerValues _l = new Controller.GameControllerValues();
            CTRE.Controller.GameControllerValues _m = new Controller.GameControllerValues();
            _joysticks = new CTRE.Controller.GameControllerValues[6] { _g, _h, _j, _k, _l, _m };
            _controllers = new CTRE.Controller.GameController[6];
            _data = new byte[255];
            _timeout.Start();
            _enableTimeout.Start();
            _sendingMessage = new byte[1] { 0x33 };
        }
        
        public void update()
        {
            _sendingMessage = combine(_sendingMessage, new byte[1] { 0x33 });
            _uart.Write(_sendingMessage, 0, _sendingMessage.Length);
            _sendingMessage = new byte[0];
            _updateFlag = false;
            _sendingMessage = new byte[1];

            int startCount = 0;
            //Check if there's data coming in, write to data cache if there is
            if (lastBytesToRead == _uart.BytesToRead && _uart.BytesToRead > 0)
            {
                _updateFlag = true;
                _timeout.Start();
                /*transfer processed data to cache*/
                byte[] buffer = new byte[255];
                _uart.Read(buffer, 0, 255);
                _data = buffer;
                CTRE.Native.CAN.Send(0x33, (ulong)_data[0], 1, 0);
                foreach (byte b in _data)
                {
                    if (b == 170) break;
                    else startCount++;
                    if (startCount > 75)
                    {
                        _updateFlag = false;
                        break;
                    }
                }
            }
            lastBytesToRead = _uart.BytesToRead;
            
            int len = 0;
            if(startCount < _data.Length && _updateFlag)
                len = _data[1 + startCount];
            if (len > 5 && len < 100)
            {
                _enableTimeout.Start();
                int joystickIndex = 0;//Initialize gamepad index
                int index = 7 + startCount;//Initialize data index just before gamepad data
                _currentState = (State)_data[5 + startCount];
                _enabled = (_currentState == State.autonEnabled || _currentState == State.teleopEnabled || _currentState == State.testEnabled);

                //GamePad Data Parsing
                
                float[] tempAxis = new float[6] { 0, 0, 0, 0, 0, 0 };
                uint tempButtons = 0;
                int tempHat = 0xffff;

                
                while (len - index > 8 && joystickIndex < 6)//Check if there's gamepad data & make sure there isn't more than 6 joysticks
                {
                    if (_data[++index] == 0) break;

                    index += 2;//Push index into slot that checks number of axis
                    int numJoysticks = _data[index];
                    for (int i = 0; i < numJoysticks && i < 6; i++)//Run for loop for number of axis
                    {
                        tempAxis[i] = ((_data[++index] / 128f));
                        if (tempAxis[i] >= 1) tempAxis[i] = tempAxis[i] - 2;
                    }

                    index++;//Push index into slot that checks number of buttons
                    if (_data[index] > 0)
                        tempButtons = (uint)_data[++index] << 8 | _data[++index];

                    if (_data[++index] > 0)//Check to ensure there is/are hats
                    {
                        tempHat = (_data[++index] << 8) | _data[++index];//Assign hat value
                    }

                    _joysticks[joystickIndex].axes[0] = tempAxis[0];
                    _joysticks[joystickIndex].axes[1] = tempAxis[1];
                    _joysticks[joystickIndex].axes[2] = tempAxis[2];
                    _joysticks[joystickIndex].axes[3] = tempAxis[3];
                    _joysticks[joystickIndex].axes[4] = tempAxis[4];
                    _joysticks[joystickIndex].axes[5] = tempAxis[5];
                    _joysticks[joystickIndex].btns = tempButtons;
                    _joysticks[joystickIndex].pov = tempHat;
                    ++joystickIndex;
                }
            }
            if (_timeout.DurationMs > 5000)
            {
                _restart.Write(false);//Restart module if no data is coming
                _timeout.Start();
                _connected = false;
            }
            else
            {
                _restart.Write(true);
                _connected = true;
            }

            if(_enableTimeout.DurationMs > 100)
            {
                for (int i = 0; i < 6; i++)
                {
                    _joysticks[i].axes[0] = 0;
                    _joysticks[i].axes[1] = 0;
                    _joysticks[i].axes[2] = 0;
                    _joysticks[i].axes[3] = 0;
                    _joysticks[i].axes[4] = 0;
                    _joysticks[i].axes[5] = 0;
                    _joysticks[i].btns = 0;
                    _joysticks[i].pov = 0xffff;
                }
                _enabled = false;
            }

            if (_enabled)
            {
                CTRE.Watchdog.Feed();
            }
        }
        
        public void SendBattery(float voltage)
        {
            if(_uart.CanWrite)
            {
                string s = voltage.ToString();
                string[] p = s.Split('.');
                byte p1 = (byte)(int)voltage;
                voltage -= (float)(p1 + 0.005);
                byte p2 = (byte)(int)((voltage / 10) * 255);
                p1 = byte.Parse(p[0]);
                p2 = (byte)((int.Parse(p[1]) / (float)1000) * 255);
                _sendingMessage = combine(_sendingMessage, (new byte[] { (byte)'b', (byte)'a', (byte)' ', (byte)p1, (byte)' ', (byte)p2 }));
            }
        }

        public void SendIP(string targetIP, string moduleIP)
        {
            if (_uart.CanWrite)
            {
                string content = "ip " + targetIP + " " + moduleIP;
                byte[] b = System.Text.Encoding.UTF8.GetBytes(content);
                _sendingMessage = combine(_sendingMessage, b);
            }
        }

        public void SendUDP(string port, byte[] data)
        {
            if(_uart.CanWrite && data.Length < 150)
            {
                string beginning = "us " + port + " ";
                byte[] b = System.Text.Encoding.UTF8.GetBytes(beginning);
                _sendingMessage = combine(_sendingMessage, b);
                _sendingMessage = combine(_sendingMessage, data);
            }
        }

        public bool IsConnected()
        {
            return _connected;
        }
        public bool IsEnabled()
        {
            return _enabled;
        }
        public bool IsAuton()
        {
            return _currentState == State.autonDisabled || _currentState == State.autonEnabled;
        }
        public State GetState()
        {
            return _currentState;
        }

        public String GetConnectionStatus()
        {
            if (_connected) return "Connected";
            else return "Not Connected";
        }

        public int Get(ref CTRE.Controller.GameControllerValues toFill, uint idx)
        {
            if (idx >= 0 && idx <= 5)
                return SyncGet(ref toFill, idx);
            else return 0;
        }
        public int Sync(ref CTRE.Controller.GameControllerValues toFill, uint rumbleL, uint rumbleR, uint ledCode, uint controlFlags, uint idx)
        {
            if (idx >= 0 && idx <= 5)
                return SyncGet(ref toFill, idx);
            else return 0;
        }

        public CTRE.Controller.GameController getController(uint idx)
        {
            if (idx >= 0 && idx <= 5)
                return _controllers[idx];
            else
                return null;
        }

        public void SetRef(CTRE.Controller.GameController reference, uint idx)
        {
            if(idx >= 0 && idx <= 5)
                _controllers[idx] = reference;
        }

        private int SyncGet(ref CTRE.Controller.GameControllerValues toFill, uint idx)
        {
            /* always get latest data for now */
            //updateCnt = 0;
            if (_updateFlag)
            {
                /* new data, copy it over */
                //if (toFill != null)
                //{
                toFill = _joysticks[idx];
                //}
            }
            return _updateFlag ? 1 : 0;
        }

        private byte[] combine(byte[] a, byte[] b)
        {
            byte[] c = new byte[a.Length + b.Length];
            Array.Copy(a, c, a.Length);
            Array.Copy(b, 0, c, a.Length, b.Length);
            return c;
        }
    }
}
