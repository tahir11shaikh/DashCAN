#include <CanApp.h>

CanApp myApp;

CanApp::CanApp(void) noexcept
{
    s.deviceConnected = false;
    h.handle = PCAN_USBBUS1;
    h.bitRate = PCAN_BAUD_500K;
    v.uiUpdateTimer = new QTimer();

    v.chart = nullptr;
    v.chartView = nullptr;
    v.axisX = nullptr;
    v.vLine = nullptr;
    v.hLine = nullptr;
    v.labelX = nullptr;
    v.labelY = nullptr;
    v.chartUpdateTimer = new QTimer();

    v.traceFlickerTimer = new QTimer();
    v.traceFlickerTimerFlag = true;
}

CanApp::~CanApp(void) noexcept
{
    // Nothing to clean up
}

int CanApp::DeviceConnect(void)
{
    TPCANStatus status = CAN_Initialize(h.handle, h.bitRate);
    qDebug() << "🔧 CAN connect:"
             << "Handle =" << QString("0x%1").arg(h.handle, 0, 16).toUpper()
             << ", Bitrate =" << QString("0x%1").arg(h.bitRate, 0, 16).toUpper();

    if (status == PCAN_ERROR_OK)
    {
        qDebug("CAN_Initialize(): %lu", status);
    } else {
        qDebug("CAN_Initialize() failed: %lu", status);
    }
    status = CAN_Reset(h.handle);
    qDebug("CAN_Reset(): %lX", status);

    status = CAN_FilterMessages(h.handle, 0x001, 0x7FF, PCAN_MODE_STANDARD);
    qDebug("CAN_FilterMessages(): %lX", status);
    return static_cast<int>(status);
}

int CanApp::DeviceDisconnect(void)
{
    TPCANStatus status = CAN_Uninitialize(h.handle);
    if (status == PCAN_ERROR_OK)
    {
        qDebug("CAN_Uninitialize(): %lu", status);
    } else {
        qDebug("CAN_Uninitialize() failed: %lu", status);
    }
    return static_cast<int>(status);
}

void CanApp::DeviceBufferReset(void)
{
    TPCANStatus status;
    CAN_Reset(h.handle);
    {
        TPCANMsg dummy{};
        TPCANTimestamp ts{};
        while (CAN_Read(h.handle, &dummy, &ts) == PCAN_ERROR_OK)
        {
            // discard
        }
    }
}

void CanApp::DeviceSetConfiguration(TPCANHandle handle, TPCANBaudrate bitrate)
{
    h.handle = handle;
    h.bitRate = bitrate;
    qDebug() << "🔧 CAN Configuration updated:"
             << "Handle =" << QString("0x%1").arg(handle, 0, 16).toUpper()
             << ", Bitrate =" << QString("0x%1").arg(bitrate, 0, 16).toUpper();
}

QString CanApp::DeviceGetErrorDescription(uint32_t errorCode)
{
    switch (errorCode)
    {
    case PCAN_ERROR_OK:             return "No error";
    case PCAN_ERROR_XMTFULL:        return "Transmit buffer in CAN controller is full";
    case PCAN_ERROR_OVERRUN:        return "CAN controller was read too late";
    case PCAN_ERROR_BUSLIGHT:       return "Bus error: error counter reached 'light' limit";
    case PCAN_ERROR_BUSHEAVY:       return "Bus error: error counter reached 'heavy' limit";
    case PCAN_ERROR_BUSPASSIVE:     return "Bus error: controller is error passive";
    case PCAN_ERROR_BUSOFF:         return "Bus error: controller is in bus-off state";
    case PCAN_ERROR_QRCVEMPTY:      return "Receive queue is empty";
    case PCAN_ERROR_QOVERRUN:       return "Receive queue was read too late";
    case PCAN_ERROR_QXMTFULL:       return "Transmit queue is full";
    case PCAN_ERROR_REGTEST:        return "Hardware register test failed (no hardware found)";
    case PCAN_ERROR_NODRIVER:       return "Driver not loaded";
    case PCAN_ERROR_HWINUSE:        return "Hardware already in use";
    case PCAN_ERROR_NETINUSE:       return "Network already in use";
    case PCAN_ERROR_ILLHW:          return "Invalid hardware handle";
    case PCAN_ERROR_ILLNET:         return "Invalid network handle";
    case PCAN_ERROR_ILLCLIENT:      return "Invalid client handle";
    case PCAN_ERROR_RESOURCE:       return "Resource (FIFO, Client, timeout) cannot be created";
    case PCAN_ERROR_ILLPARAMTYPE:   return "Invalid parameter type";
    case PCAN_ERROR_ILLPARAMVAL:    return "Invalid parameter value";
    case PCAN_ERROR_UNKNOWN:        return "Unknown error";
    case PCAN_ERROR_ILLDATA:        return "Invalid data or action";
    case PCAN_ERROR_ILLMODE:        return "Invalid driver mode for operation";
    case PCAN_ERROR_CAUTION:        return "Operation successful, but irregularities registered";
    case PCAN_ERROR_INITIALIZE:     return "Channel not initialized";
    case PCAN_ERROR_ILLOPERATION:   return "Invalid operation for current driver state";
    default:                        return "Unrecognized error code";
    }
}
