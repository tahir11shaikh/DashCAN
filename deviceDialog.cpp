#include "deviceDialog.h"
#include "ui_deviceDialog.h"

extern CanApp myApp;

deviceDialog::deviceDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::deviceDialog)
{
    ui->setupUi(this);
    populateDeviceList();
    populateBitrateList();

    selected.handle = PCAN_USBBUS1;
    selected.bitrate = PCAN_BAUD_500K;
}

deviceDialog::~deviceDialog()
{
    delete ui;
}

void deviceDialog::populateDeviceList()
{
    ui->comboBoxDeviceList->clear();
    ui->comboBoxDeviceList->addItem("PCAN_USBBUS1 (0x51)", QVariant(PCAN_USBBUS1));
    ui->comboBoxDeviceList->addItem("PCAN_USBBUS2 (0x52)", QVariant(PCAN_USBBUS2));
    ui->comboBoxDeviceList->addItem("PCAN_USBBUS3 (0x53)", QVariant(PCAN_USBBUS3));
    ui->comboBoxDeviceList->addItem("PCAN_USBBUS4 (0x54)", QVariant(PCAN_USBBUS4));
    ui->comboBoxDeviceList->addItem("PCAN_USBBUS5 (0x55)", QVariant(PCAN_USBBUS5));
    ui->comboBoxDeviceList->addItem("PCAN_USBBUS6 (0x56)", QVariant(PCAN_USBBUS6));
}

void deviceDialog::populateBitrateList()
{
    ui->comboBoxDeviceBitrateList->clear();
    ui->comboBoxDeviceBitrateList->addItem("1 MBit/s", QVariant(PCAN_BAUD_1M));
    ui->comboBoxDeviceBitrateList->addItem("800 kBit/s", QVariant(PCAN_BAUD_800K));
    ui->comboBoxDeviceBitrateList->addItem("500 kBit/s", QVariant(PCAN_BAUD_500K));
    ui->comboBoxDeviceBitrateList->addItem("250 kBit/s", QVariant(PCAN_BAUD_250K));
    ui->comboBoxDeviceBitrateList->addItem("125 kBit/s", QVariant(PCAN_BAUD_125K));
    ui->comboBoxDeviceBitrateList->addItem("100 kBit/s", QVariant(PCAN_BAUD_100K));
}

QString deviceDialog::getPcanDeviceName(TPCANHandle handle)
{
    if (handle == 0x0) {
        return QString("UNKNOWN DEVICE").toUpper();
    }

    switch (handle)
    {
    case PCAN_USBBUS1: return "PCAN_USBBUS1";
    case PCAN_USBBUS2: return "PCAN_USBBUS2";
    case PCAN_USBBUS3: return "PCAN_USBBUS3";
    case PCAN_USBBUS4: return "PCAN_USBBUS4";
    case PCAN_USBBUS5: return "PCAN_USBBUS5";
    case PCAN_USBBUS6: return "PCAN_USBBUS6";
    case PCAN_USBBUS7: return "PCAN_USBBUS7";
    case PCAN_USBBUS8: return "PCAN_USBBUS8";
    default: return QString("Unknown (0x%1)").arg(handle, 0, 16).toUpper();
    }
}

QString deviceDialog::getPcanBitrateString(TPCANBaudrate bitrate)
{
    if (bitrate == 0x0) {
        return QString("UNKNOWN BITRATE").toUpper();
    }

    switch (bitrate)
    {
    case PCAN_BAUD_1M:   return "1 MBit/s";
    case PCAN_BAUD_800K: return "800 kBit/s";
    case PCAN_BAUD_500K: return "500 kBit/s";
    case PCAN_BAUD_250K: return "250 kBit/s";
    case PCAN_BAUD_125K: return "125 kBit/s";
    case PCAN_BAUD_100K: return "100 kBit/s";
    default: return QString("Custom (0x%1)").arg(bitrate, 0, 16).toUpper();
    }
}

void deviceDialog::loadConfiguration(const TPCANHandle &handle, const TPCANBaudrate &bitrate)
{
    // Preload existing configuration
    int devIndex = ui->comboBoxDeviceList->findData(QVariant(handle));
    int brIndex = ui->comboBoxDeviceBitrateList->findData(QVariant(bitrate));

    if (devIndex != -1)
        ui->comboBoxDeviceList->setCurrentIndex(devIndex);
    if (brIndex != -1)
        ui->comboBoxDeviceBitrateList->setCurrentIndex(brIndex);
}

void deviceDialog::on_pushButtonDeviceDetect_clicked()
{
    // Attempt to detect connected PCAN USB devices
    TPCANStatus status = CAN_Initialize(PCAN_NONEBUS, PCAN_BAUD_500K);
    if (status == PCAN_ERROR_INITIALIZE) {
        QMessageBox::warning(this, "Auto Detect", "No PCAN devices detected.");
        return;
    }
    QMessageBox::information(this, "Auto Detect", "PCAN device detected successfully.");
    CAN_Uninitialize(PCAN_NONEBUS);
}

void deviceDialog::on_pushButtonDeviceConfigure_clicked()
{
    selected.handle = static_cast<TPCANHandle>(ui->comboBoxDeviceList->currentData().toUInt());
    selected.bitrate = static_cast<TPCANBaudrate>(ui->comboBoxDeviceBitrateList->currentData().toUInt());

    myApp.DeviceSetConfiguration(selected.handle, selected.bitrate);

    emit configurationUpdated(selected.handle, selected.bitrate);

    QMessageBox::information(this, "CAN Configured",
                             QString("Device: %1\nBitrate: %2")
                                 .arg(ui->comboBoxDeviceList->currentText(), ui->comboBoxDeviceBitrateList->currentText()));
    accept();
}

void deviceDialog::on_pushButtonDeviceCancel_clicked()
{
    reject();
}
