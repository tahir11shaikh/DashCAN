#pragma once

#include <QString>
#include <QStringList>
#include <QList>
#include <QPair>
#include <QByteArray>

class CanDBC
{
public:
    enum class ByteOrder {
        LittleEndian,
        BigEndian
    };

    struct Signal {
        QString   name;
        int       startBit;
        int       bitLength;
        ByteOrder byteOrder;
        bool      isSigned;
        double    scale;
        double    offset;
        QString   unit;
        QString   receiver;
    };

    struct Message {
        int           id;
        QString       name;
        QString       description;
        QString       sender;
        QList<Signal> canSignals;
    };

    CanDBC();
    ~CanDBC();

    /// Load and parse the DBC file. Returns false on error.
    bool loadDBC(const QString &dbcFilePath);

    /// Returns the list of parsed messages (in the order they appeared).
    const QList<Message>& msgList() const;

    /// Given a raw CAN frame (id + 0–8 bytes), decode to (signalName, value).
    QList<QPair<QString,double>> decodeFrame(int messageId, const QByteArray &rawData) const;

private:
    struct Impl;
    Impl *d;
};
