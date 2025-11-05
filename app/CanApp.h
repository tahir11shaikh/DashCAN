#ifndef CANAPP_H
#define CANAPP_H

#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

#include <windows.h>
#include <PCANBasic.h>
#include <CanDbc.h>

#include <QMenu>
#include <QDebug>
#include <QString>
#include <QFile>
#include <QFuture>
#include <QPlainTextEdit>
#include <QTableWidget>
#include <QTextEdit>
#include <QComboBox>
#include <QMessageBox>
#include <QFileInfo>
#include <QFileDialog>
#include <QtConcurrent>
#include <QDateTime>
#include <QTextStream>
#include <QDir>
#include <QStandardPaths>
#include <QList>
#include <QRegularExpression>
#include <QMutex>
#include <QMutexLocker>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QValueAxis>
#include <QMouseEvent>
#include <QToolTip>
#include <QInputDialog>
#include <QLCDNumber>
#include <QProgressBar>
#include <QCheckBox>
#include <QDial>
#include <QTimer>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QSpinBox>
#include <QFormLayout>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <qlistwidget.h>
#include <qmenubar.h>

struct CANMessageData {
    TPCANMsg msg;
    TPCANTimestamp ts;
    QList<QPair<QString, double>> decodedSignals;
    qint64 timestampInMs;
};

struct TraceFileData {
    int traceMsgCounter;
    double traceTimeMs;
    int traceMsgId;
    int traceMsgDlc;
    QByteArray traceMsgData;
};

class CanApp
{
public:
    explicit CanApp(void) noexcept;
    ~CanApp(void) noexcept;

    struct
    {
        TPCANHandle handle;
        TPCANBaudrate bitRate;
    } h;

    struct
    {
        QString dbcFilePath;
        QTimer *uiUpdateTimer;
        QFuture<void> RxProducerFuture;
        QFuture<void> RxConsumerFuture;
        QFuture<void> trcRePlayProducerFuture;
        QFuture<void> trcRePlayCosumerFuture;

        //subTabPlot
        QChart *chart;
        QChartView *chartView;
        QDateTimeAxis *axisX;
        QMap<QString, QValueAxis*> axisYMap;

        QGraphicsLineItem *vLine;
        QGraphicsLineItem *hLine;
        QMap<QString, QLineSeries*> seriesMap;
        QGraphicsSimpleTextItem *labelX ;
        QGraphicsSimpleTextItem *labelY;

        QList<QWidget*> signalContainers;
        QMap<QString, QWidget*> signalWidgetMap;

        QMap<QString, QVector<QPointF>> bufferedPoints;
        QTimer *chartUpdateTimer;
        QMutex bufferMutex;
        QMutex chartAccessMutex;
        QMap<QString, double> lastSignalUIValues;
        QMap<QString, qint64> lastSignalUITimestamps;
        QMutex uiUpdateBufferMutex;
        QMap<QString, qint64> lastTimestamps;
        QMap<QString, double> lastSignalValues;

        //subTabRxTx
        QMap<uint32_t, int> msgIdToRowMap;
        QMap<uint32_t, qint64> msgIdToLastTimestamp;
        QMap<uint32_t, int> msgIdToCount;

        //subTabTrace
        QTimer *traceFlickerTimer;
        bool traceFlickerTimerFlag = true;
        QFile traceFile;
        QTextStream traceStream;
        quint64 traceStartTime;
        int traceMsgCounter;
        QString trcFilePath;
        QList<TraceFileData> traceFileDataList;
        TraceFileData traceFileData;
    } v;

    struct
    {
        bool deviceConnected;
        bool dbcAttached;
        bool rxRunning;
        bool rxPaused;
        bool txRunning;
        bool txPaused;
        bool trcRecording;
        bool trcRunning;
        bool trcPaused;
        bool trcFileLoaded;
    } s;

    // Methods Declaration
    int DeviceConnect(void);
    int DeviceDisconnect(void);
    void DeviceBufferReset(void);
    void DeviceSetConfiguration(TPCANHandle handle, TPCANBaudrate bitrate);
    QString DeviceGetErrorDescription(uint32_t errorCode);
};

#endif // CANAPP_H
