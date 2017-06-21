using System;
using Microsoft.SPOT;

namespace CTRE.FRC
{
    public interface IRobotStateProvider
    {
        bool IsConnected();
        bool IsEnabled();
        bool IsAuton();
    }
}
