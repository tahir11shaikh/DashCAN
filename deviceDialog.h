#ifndef DEVICEDIALOG_H
#define DEVICEDIALOG_H

#pragma once

#include <QDialog>
#include "canapp.h"

namespace Ui {
class deviceDialog;
}

class deviceDialog : public QDialog
{
    Q_OBJECT

public:
    explicit deviceDialog(QWidget *parent = nullptr);
    ~deviceDialog();

    QString getPcanDeviceName(TPCANHandle handle);
    QString getPcanBitrateString(TPCANBaudrate bitrate);
    void loadConfiguration(const TPCANHandle &handle, const TPCANBaudrate &bitrate);

signals:
    void configurationUpdated(TPCANHandle handle, TPCANBaudrate bitrate);

private slots:
    void on_pushButtonDeviceDetect_clicked();
    void on_pushButtonDeviceConfigure_clicked();
    void on_pushButtonDeviceCancel_clicked();

private:
    Ui::deviceDialog *ui;

    struct {
        TPCANHandle handle;
        TPCANBaudrate bitrate;
    } selected;

    void populateDeviceList();
    void populateBitrateList();
};

#endif // DEVICEDIALOG_H
