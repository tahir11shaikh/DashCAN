    #include "loginDialog.h"
#include "mainWindow.h"
#include "ui_mainWindow.h"
#include <QMainWindow>
#include <QDesktopServices>

#include <CanApp.h>
#include <CanDbc.h>

CanDBC myDBC;
extern CanApp myApp;

QPlainTextEdit* MainWindow::s_logViewer = nullptr;
MainWindow* MainWindow::instance = nullptr;

MainWindow::MainWindow(const QString &workspacePath, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , workspacePath(workspacePath)
{
    ui->setupUi(this);
    if (!instance) {
        instance = this;
    }

    // Set up logs viewer
    s_logViewer = ui->plainTextEditLogs;
    s_logViewer->setContextMenuPolicy(Qt::CustomContextMenu);

    // Set up mainTab
    connect(myApp.v.chartUpdateTimer, &QTimer::timeout, this, &MainWindow::onChartUpdateTimer);
    myApp.v.chartUpdateTimer->start(200);

    // Set up periodic UI update timer
    connect(myApp.v.uiUpdateTimer, &QTimer::timeout, this, [=]() {
        QMutexLocker locker(&myApp.v.uiUpdateBufferMutex);
        qint64 now = QDateTime::currentMSecsSinceEpoch();

        for (auto it = myApp.v.lastSignalUIValues.begin(); it != myApp.v.lastSignalUIValues.end(); ++it) {
            const QString &sigName = it.key();
            double val = it.value();

            if (myApp.v.signalWidgetMap.contains(sigName)) {
                QWidget *w = myApp.v.signalWidgetMap[sigName];

                if (auto *lcd = qobject_cast<QLCDNumber *>(w)) lcd->display(val);
                else if (auto *bar = qobject_cast<QProgressBar *>(w)) bar->setValue(static_cast<int>(val));
                else if (auto *slider = qobject_cast<QSlider *>(w)) slider->setValue(static_cast<int>(val));
                else if (auto *dial = qobject_cast<QDial *>(w)) dial->setValue(static_cast<int>(val));
                else if (auto *lbl = qobject_cast<QLabel *>(w)) lbl->setText(QString::number(val, 'f', 1));
                else if (auto *chk = qobject_cast<QCheckBox *>(w)) chk->setChecked(val > 0.5);
                else if (auto *combo = qobject_cast<QComboBox *>(w)) {
                    int index = static_cast<int>(val);
                    if (index >= 0 && index < combo->count())
                        combo->setCurrentIndex(index);
                }

                myApp.v.lastSignalUIValues[sigName] = val;
                myApp.v.lastSignalUITimestamps[sigName] = now;
            }
        }
    });
    myApp.v.uiUpdateTimer->start(100);

    // Set up subLiveData: header
    ui->treeWidgetCanMessage->setColumnCount(6);
    ui->treeWidgetCanMessage->setHeaderLabels(QStringList{ tr("Name"), tr("Id(Hex)"), tr("Description"), tr("Sender/Receiver"), tr("Count"), tr("CycleTime(ms)") });

    // Set up subRxTx: RX-header
    ui->tableWidgetRx->setColumnCount(5);
    ui->tableWidgetRx->setHorizontalHeaderLabels(QStringList() << "Msg Id(Hex)" << "Msg DLC" << "Msg Data" << "Msg Count" << "Msg CycleTime(ms)");
    ui->tableWidgetRx->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);

    // Set up subRxTx: TX-header
    ui->tableWidgetTx->setColumnCount(5);
    ui->tableWidgetTx->setHorizontalHeaderLabels(QStringList() << "Msg Id(Hex)" << "Msg DLC" << "Msg Data" << "Msg Count" << "Msg CycleTime(ms)");
    ui->tableWidgetTx->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);

    // Set up subPlot: chart
    if (!myApp.v.chartView) {
        setupPlotChart();
    }

    connect(myApp.v.traceFlickerTimer, &QTimer::timeout, this, [=]() {
        if (myApp.v.traceFlickerTimerFlag) {
            ui->pushButtonTraceRecordStartStop->setStyleSheet("background-color: #FFFFFF; color: black; border-radius: 6px; padding: 8px 16px;");
        } else {
            ui->pushButtonTraceRecordStartStop->setStyleSheet("background-color: #EF4444; color: white; border-radius: 6px; padding: 8px 16px;");
        }
        myApp.v.traceFlickerTimerFlag = !myApp.v.traceFlickerTimerFlag;
    });

    connect(ui->listWidgetCanSignals, &QListWidget::itemClicked, this, [=](QListWidgetItem *item) {
        if (item) {
            currentHoveredSignal = item->text();
        }
    });

    // File menu actions
    connect(ui->actionSaveProject, &QAction::triggered, this, [=]() {
        saveAppData(workspacePath + "/" + APP_WORKSPACE_FILE_NAME);
    });
    connect(ui->actionSaveLogs, &QAction::triggered, this, [=]() {
        on_pushButtonSaveLog_clicked();
    });
    connect(ui->actionExit, &QAction::triggered, this, [=]() {
        this->close();
    });
    connect(ui->actionRestart, &QAction::triggered, this, [=]() {
        QString appPath = QCoreApplication::applicationFilePath();
        bool started = QProcess::startDetached(appPath, QStringList(), QDir::currentPath());

        if (!started) {
            QMessageBox::critical(this, "Restart Failed", "Unable to restart the application!");
            return;
        }

#ifdef Q_OS_WIN
        ::ExitProcess(0);  // Force close immediately on Windows
#else
        QCoreApplication::exit(0);
#endif
    });
    connect(ui->actionExit, &QAction::triggered, this, [=]() {
        if (QMessageBox::question(this, "Exit Application",
                                  "Are you sure you want to exit DashCAN?") == QMessageBox::Yes)
        {
            QCoreApplication::quit();
        }
    });

    // Device menu actions
    connect(ui->actionDeviceConnect, &QAction::triggered, this, [=]() {
        on_pushButtonDeviceConnectDisconnect_clicked();
    });
    connect(ui->actionDeviceDisconnect, &QAction::triggered, this, [=]() {
        on_pushButtonDeviceConnectDisconnect_clicked();
    });
    connect(ui->actionDeviceConfigure, &QAction::triggered, this, [=]() {
        if (!myApp.s.deviceConnected) {
            on_pushButtonDeviceConfigure_clicked();
        } else {
            QMessageBox::warning(this, "Device Configuration",
                                 "Please disconnect the device before changing configuration.");
        }
    });

    // Database menu actions
    connect(ui->actionOpenDbcFile, &QAction::triggered, this, [=]() {
        on_pushButtonDbcFileBrowse_clicked();
    });
    connect(ui->actionApplyDbcFile, &QAction::triggered, this, [=]() {
        on_pushButtonApplyDbc_clicked();
    });

    // Transmit menu actions
    connect(ui->actionTransmitStart, &QAction::triggered, this, [=]() {
        on_pushButtonTxStartStop_clicked();
    });
    connect(ui->actionTransmitStop, &QAction::triggered, this, [=]() {
        on_pushButtonTxStartStop_clicked();
    });
    connect(ui->actionTransmitPause, &QAction::triggered, this, [=]() {
        on_pushButtonTxPauseResume_clicked();
    });
    connect(ui->actionTransmitResume, &QAction::triggered, this, [=]() {
        on_pushButtonTxPauseResume_clicked();
    });

    // Receive menu actions
    connect(ui->actionReceiveStart, &QAction::triggered, this, [=]() {
        on_pushButtonRxStartStop_clicked();
    });
    connect(ui->actionReceiveStop, &QAction::triggered, this, [=]() {
        on_pushButtonRxStartStop_clicked();
    });
    connect(ui->actionReceivePause, &QAction::triggered, this, [=]() {
        on_pushButtonRxPauseResume_clicked();
    });
    connect(ui->actionReceiveResume, &QAction::triggered, this, [=]() {
        on_pushButtonRxPauseResume_clicked();
    });

    //  Playback menu actions
    connect(ui->actionImportTraceFile, &QAction::triggered, this, [=]() {
        on_pushButtonTraceFileBrowse_clicked();
    });
    connect(ui->actionLoadTraceFile, &QAction::triggered, this, [=]() {
        on_pushButtonTraceFileLoad_clicked();
    });
    connect(ui->actionPlaybackStart, &QAction::triggered, this, [=]() {
        on_pushButtonTraceRePlayStartStop_clicked();
    });
    connect(ui->actionPlaybackStop, &QAction::triggered, this, [=]() {
        on_pushButtonTraceRePlayStartStop_clicked();
    });
    connect(ui->actionPlaybackPause, &QAction::triggered, this, [=]() {
        on_pushButtonTraceRePlayPauseResume_clicked();
    });
    connect(ui->actionPlaybackResume, &QAction::triggered, this, [=]() {
        on_pushButtonTraceRePlayPauseResume_clicked();
    });

    // Recording menu actions
    connect(ui->actionRecordingStart, &QAction::triggered, this, [=]() {
        on_pushButtonTraceRecordStartStop_clicked();
    });
    connect(ui->actionRecordingStop, &QAction::triggered, this, [=]() {
        on_pushButtonTraceRecordStartStop_clicked();
    });

    // Help menu actions
    connect(ui->actionGitHubRepo, &QAction::triggered, this, [=]() {
        // Open GitHub repository in default browser
        QDesktopServices::openUrl(QUrl("https://github.com/tahir11shaikh/DashCAN"));
    });
    connect(ui->actionUserManual, &QAction::triggered, this, [=]() {
        // Open user manual PDF or help file
        QString manualPath = QDir(workspacePath).filePath("UserManual.pdf"); // or absolute path
        if (QFile::exists(manualPath)) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(manualPath));
        } else {
            QMessageBox::warning(this, tr("Error"), tr("User manual not found!"));
        }
    });
    connect(ui->actionAbout, &QAction::triggered, this, [=]() {
        // Show an About dialog
        QMessageBox::about(this, tr("About DashCAN"),
                           tr("DashCAN Application\nVersion 1.0\n© 2025 by Tahir.Shaikh"));
    });

    // Load data from workspace-specific path
    loadAppData(workspacePath+ "/" + APP_WORKSPACE_FILE_NAME);
}

MainWindow::~MainWindow()
{
    delete ui;
}

QPlainTextEdit* MainWindow::logViewer()
{
    return s_logViewer;
}

bool MainWindow::eventFilter(QObject *obj, QEvent *ev)
{
    static bool isDraggingX = false;
    static QPoint lastMousePos;

    QChartView *view = myApp.v.chartView;
    if (!view || obj != view->viewport()) {
        return QMainWindow::eventFilter(obj, ev);
    }

    QChart *chart = view->chart();
    auto *vLine = myApp.v.vLine;
    auto *hLine = myApp.v.hLine;

    switch (ev->type())
    {
    case QEvent::MouseButtonPress: {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(ev);
        if (mouseEvent->button() == Qt::LeftButton) {
            isDraggingX = true;
            lastMousePos = mouseEvent->pos();
        }
        return true;
    }
    case QEvent::MouseButtonRelease: {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(ev);
        if (mouseEvent->button() == Qt::LeftButton) {
            isDraggingX = false;
        }
        return true;
    }
    case QEvent::Wheel: {
        QWheelEvent *wheelEvent = static_cast<QWheelEvent *>(ev);
        const auto xAxes = chart->axes(Qt::Horizontal);
        const auto yAxes = chart->axes(Qt::Vertical);
        QDateTimeAxis *axisX = xAxes.isEmpty() ? nullptr : qobject_cast<QDateTimeAxis *>(xAxes.first());
        if (!axisX)
            return true;

        // Build a list of all QValueAxis pointers
        QList<QValueAxis*> allY;
        for (QAbstractAxis *ax : yAxes) {
            if (auto *ya = qobject_cast<QValueAxis*>(ax))
                allY.append(ya);
        }

        // ZOOM both X and Y when Ctrl is held:
        if (QApplication::keyboardModifiers() == Qt::ControlModifier) {
            const qreal zoomFactor = wheelEvent->angleDelta().y() > 0 ? 0.9 : 1.1;

            // --- Zoom X axis (DateTimeAxis) ---
            QDateTime minX = axisX->min();
            QDateTime maxX = axisX->max();
            qint64 spanX = minX.msecsTo(maxX);
            qint64 halfX = spanX / 2;
            QDateTime midX = minX.addMSecs(halfX);
            qint64 newHalfSpanX = static_cast<qint64>(halfX * zoomFactor);
            axisX->setRange(midX.addMSecs(-newHalfSpanX), midX.addMSecs(newHalfSpanX));

            // --- Zoom **all** Y axes ---
            for (QValueAxis *axisY : allY) {
                qreal minY = axisY->min();
                qreal maxY = axisY->max();
                qreal spanY = maxY - minY;
                qreal centerY = (minY + maxY) / 2.0;
                qreal newHalfSpanY = (spanY / 2.0) * zoomFactor;
                axisY->setRange(centerY - newHalfSpanY, centerY + newHalfSpanY);
            }
        }
        // Pan Y up/down if no modifier
        else if (QApplication::keyboardModifiers() == Qt::NoModifier) {
            for (QValueAxis *axisY : allY) {
                qreal delta = (axisY->max() - axisY->min()) * 0.05;
                if (wheelEvent->angleDelta().y() > 0) {
                    axisY->setRange(axisY->min() + delta, axisY->max() + delta);
                } else {
                    axisY->setRange(axisY->min() - delta, axisY->max() - delta);
                }
            }
        }
        // Pan X left/right if Shift is held
        else if (QApplication::keyboardModifiers() == Qt::ShiftModifier) {
            QDateTime minX = axisX->min();
            QDateTime maxX = axisX->max();
            qint64 spanX = minX.msecsTo(maxX);
            qint64 shiftX = spanX / 20;
            if (wheelEvent->angleDelta().y() > 0) {
                axisX->setRange(minX.addMSecs(shiftX), maxX.addMSecs(shiftX));
            } else {
                axisX->setRange(minX.addMSecs(-shiftX), maxX.addMSecs(-shiftX));
            }
        }

        return true;
    }
    case QEvent::MouseMove: {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(ev);
        QPoint mousePos = mouseEvent->pos();
        QRectF plotArea = chart->plotArea();
        QPointF xMapped = chart->mapToValue(mousePos);
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(xMapped.x());

        // --- Always show crosshair lines ---
        vLine->setLine(mousePos.x(), plotArea.top(), mousePos.x(), plotArea.bottom());
        hLine->setLine(plotArea.left(), mousePos.y(), plotArea.right(), mousePos.y());
        vLine->setVisible(true);
        hLine->setVisible(true);

        // --- Show X label ---
        myApp.v.labelX->setText(dt.toString("hh:mm:ss.zzz"));
        myApp.v.labelX->setPos(mousePos.x() + 6, plotArea.bottom() - 20);
        myApp.v.labelX->setVisible(true);

        // --- Show Y label based on selected signal ---
        if (myApp.v.axisYMap.contains(currentHoveredSignal)) {
            QValueAxis *axisY = myApp.v.axisYMap.value(currentHoveredSignal);

            // Find attached series
            QAbstractSeries *attachedSeries = nullptr;
            for (QAbstractSeries *series : chart->series())
            {
                if (series->attachedAxes().contains(axisY)) {
                    attachedSeries = series;
                    break;
                }
            }

            if (attachedSeries) {
                double yMin = axisY->min();
                double yMax = axisY->max();

                QPointF posTop = chart->mapToPosition(QPointF(xMapped.x(), yMax), attachedSeries);
                QPointF posBottom = chart->mapToPosition(QPointF(xMapped.x(), yMin), attachedSeries);

                double axisPixelTop = posTop.y();
                double axisPixelBottom = posBottom.y();
                double axisHeight = axisPixelBottom - axisPixelTop;

                if (axisHeight != 0) {
                    double yRatio = (mousePos.y() - axisPixelTop) / axisHeight;
                    double yVal = yMax - (yRatio * (yMax - yMin));

                    myApp.v.labelY->setText(QString::number(yVal, 'f', 3));
                    myApp.v.labelY->setPos(plotArea.left() + 6, mousePos.y() - 12);
                    myApp.v.labelY->setVisible(true);
                }
            }
        }
        return true;
    }
    case QEvent::Leave: {
        vLine->setVisible(false);
        hLine->setVisible(false);
        if (myApp.v.labelX) myApp.v.labelX->setVisible(false);
        if (myApp.v.labelY) myApp.v.labelY->setVisible(false);
        return true;
    }
    default:
        break;
    }
    return QMainWindow::eventFilter(obj, ev);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    myApp.s.txRunning = false;
    myApp.s.rxRunning = false;
    myApp.s.deviceConnected = false;
    myApp.DeviceDisconnect();

    // Stop timers
    myApp.v.chartUpdateTimer->stop();

    // terminate reader/consumer if still running
    if (consumerThreadRunning) {
        consumerThreadRunning = false;
        queueNotEmpty.wakeAll();
    }
    saveAppData(workspacePath + "/" + APP_WORKSPACE_FILE_NAME);
    QMainWindow::closeEvent(event);
}

void MainWindow::saveAppData(const QString &filePath)
{
    if (filePath.isEmpty()) {
        qWarning() << "saveAppData(): filePath is empty!";
        return;
    }

    QFileInfo fi(filePath);
    QDir dir = fi.dir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // --- subTabLiveData: liveDataMessages ---
    QJsonArray liveDataMessages;
    auto *tree = ui->treeWidgetCanMessage;
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *top = tree->topLevelItem(i);
        QJsonObject topObj;
        topObj["name"]        = top->text(0);
        topObj["idHex"]       = top->text(1);
        topObj["description"] = top->text(2);
        topObj["sender"]      = top->text(3);

        // collect its child-signals
        QJsonArray sigs;
        for (int j = 0; j < top->childCount(); ++j) {
            QTreeWidgetItem *ch = top->child(j);
            QJsonObject chObj;
            chObj["signalName"] = ch->text(0);
            chObj["unit"]       = ch->text(2);
            chObj["receiver"]   = ch->text(3);
            sigs.append(chObj);
        }
        topObj["signals"] = sigs;
        liveDataMessages.append(topObj);
    }

    // --- subTabPlot: plotSignals ---
    QJsonArray plotSignals;
    for (int i = 0; i < ui->listWidgetCanSignals->count(); ++i) {
        QListWidgetItem *item = ui->listWidgetCanSignals->item(i);
        if (!item)
            continue;

        QString signalName = item->text();
        QJsonObject sigObj;
        sigObj["name"] = signalName;

        // lookup signal unit
        QString signalUnit;
        for (const auto &msg : myDBC.msgList()) {
            for (const auto &sig : msg.canSignals) {
                if (sig.name == signalName) {
                    signalUnit = sig.unit;
                    break;
                }
            }
            if (!signalUnit.isEmpty()) break;
        }
        sigObj["unit"] = signalUnit;

        // axis range
        if (myApp.v.axisYMap.contains(signalName)) {
            if (QValueAxis *axis = myApp.v.axisYMap.value(signalName)) {
                sigObj["min"] = axis->min();
                sigObj["max"] = axis->max();
            }
        }

        plotSignals.append(sigObj);
    }

    // --- subTabPanel: panelSignals ---
    QJsonArray panelSignals;
    for (QWidget *container : std::as_const(myApp.v.signalContainers)) {
        QString signalName = container->objectName();
        QWidget *panel = myApp.v.signalWidgetMap.value(signalName, nullptr);
        if (!panel)
            continue;

        QString widgetType;
        if      (qobject_cast<QLCDNumber*>(panel))   widgetType = "QLCDNumber";
        else if (qobject_cast<QProgressBar*>(panel)) widgetType = "QProgressBar";
        else if (qobject_cast<QSlider*>(panel))      widgetType = "QSlider";
        else if (qobject_cast<QDial*>(panel))        widgetType = "QDial";
        else if (qobject_cast<QLabel*>(panel))       widgetType = "QLabel";
        else if (qobject_cast<QCheckBox*>(panel))    widgetType = "QCheckBox";
        else if (qobject_cast<QComboBox*>(panel))    widgetType = "QComboBox";
        else continue;

        QJsonObject panelObj;
        panelObj["signalName"] = signalName;
        panelObj["widgetType"] = widgetType;
        panelObj["x"] = container->pos().x();
        panelObj["y"] = container->pos().y();
        panelSignals.append(panelObj);
    }

    // --- root object ---
    QJsonObject root;
    root["deviceHandle"]    = QString("0x%1").arg(myApp.h.handle, 0, 16).toUpper();
    root["deviceBitrate"]   = QString("0x%1").arg(myApp.h.bitRate, 0, 16).toUpper();
    root["dbcFilePath"]     = ui->lineEditDbcFileUpload->text();
    root["trcFilePath"]     = ui->lineEditTraceFileUpload->text();
    root["liveDataMessages"]= liveDataMessages;
    root["plotSignals"]     = plotSignals;
    root["panelSignals"]    = panelSignals;

    // --- write to file safely ---
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "Failed to open file for writing:" << filePath;
        return;
    }

    QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

#ifdef Q_OS_WIN
    // Hide the app data file on Windows
    SetFileAttributesW((LPCWSTR)filePath.utf16(), FILE_ATTRIBUTE_HIDDEN);
#endif

    qDebug() << "Application data saved to:" << filePath;
}

void MainWindow::loadAppData(const QString &filePath)
{
    QFile file(filePath);
    if (!file.exists()) {
        qWarning() << "App file not found, creating default:" << filePath;

        // Create minimal default JSON
        QJsonObject defaultObj;
        defaultObj["version"] = "1.0";
        defaultObj["deviceHandle"] = 0X51;//PCAN_USBBUS1;
        defaultObj["deviceBitrate"] = 0X1C;//PCAN_BAUD_500K;
        defaultObj["created"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        defaultObj["dbcFilePath"] = "";
        defaultObj["trcFilePath"] = "";
        defaultObj["liveDataMessages"] = QJsonArray();
        defaultObj["plotSignals"] = QJsonArray();
        defaultObj["panelSignals"] = QJsonArray();

        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QJsonDocument doc(defaultObj);
            file.write(doc.toJson(QJsonDocument::Indented));
            file.close();
        }
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Could not open layout file:" << filePath;
        return;
    }

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "Invalid JSON in" << filePath << ":" << parseError.errorString();

        // Auto-reset invalid file with defaults
        QJsonObject defaultObj;
        defaultObj["version"] = "1.0";
        defaultObj["deviceHandle"] = 0X51;//PCAN_USBBUS1;
        defaultObj["deviceBitrate"] = 0X1C;//PCAN_BAUD_500K;
        defaultObj["created"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        defaultObj["dbcFilePath"] = "";
        defaultObj["trcFilePath"] = "";
        defaultObj["liveDataMessages"] = QJsonArray();
        defaultObj["plotSignals"] = QJsonArray();
        defaultObj["panelSignals"] = QJsonArray();

        QFile fixFile(filePath);
        if (fixFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QJsonDocument fixed(defaultObj);
            fixFile.write(fixed.toJson(QJsonDocument::Indented));
            fixFile.close();
        }
        return;
    }

    const QJsonObject root = doc.object();

    // device configuration
   if (root.contains("deviceHandle"))
    {
        QString handleStr = root["deviceHandle"].toString();
        bool ok = false;
        myApp.h.handle = static_cast<TPCANHandle>(handleStr.toUInt(&ok, 16));
    }
    if (root.contains("deviceBitrate"))
    {
        QString bitrateStr = root["deviceBitrate"].toString();
        bool ok = false;
        myApp.h.bitRate = static_cast<TPCANBaudrate>(bitrateStr.toUInt(&ok, 16));
    }

    // dbcFilePath
    if (root.contains("dbcFilePath")) {
        const QString dbcFilePath = root["dbcFilePath"].toString();
        ui->lineEditDbcFileUpload->setText(dbcFilePath);

        const bool valid = !dbcFilePath.isEmpty() &&
                           dbcFilePath.endsWith(".dbc", Qt::CaseInsensitive) &&
                           QFile::exists(dbcFilePath);

        ui->pushButtonApplyDbc->setEnabled(valid);
        myApp.v.dbcFilePath = dbcFilePath;
        ui->pushButtonAddItem->setEnabled(valid);
        ui->pushButtonRemoveItem->setEnabled(valid);
    } else {
        ui->pushButtonApplyDbc->setEnabled(false);
        ui->pushButtonAddItem->setEnabled(false);
        ui->pushButtonRemoveItem->setEnabled(false);
    }

    // trcFilePath
    if (root.contains("trcFilePath")) {
        const QString trcFilePath = root["trcFilePath"].toString();
        ui->lineEditTraceFileUpload->setText(trcFilePath);

        const bool valid = !trcFilePath.isEmpty() &&
                           trcFilePath.endsWith(".trc", Qt::CaseInsensitive) &&
                           QFile::exists(trcFilePath);

        myApp.v.trcFilePath = trcFilePath;
        ui->pushButtonTraceFileLoad->setEnabled(valid);
    }

    // subTabLiveData: liveDataMessages
    auto *tree = ui->treeWidgetCanMessage;
    tree->clear();
    const QJsonArray liveDataMessages = root["liveDataMessages"].toArray();
    for (const QJsonValue &tval : liveDataMessages) {
        const QJsonObject topObj = tval.toObject();
        auto *top = new QTreeWidgetItem(tree);
        top->setText(0, topObj["name"].toString());
        top->setText(1, topObj["idHex"].toString());
        top->setText(2, topObj["description"].toString());
        top->setText(3, topObj["sender"].toString());

        QFont f = top->font(0);
        f.setBold(true);
        for (int c = 0; c < 6; ++c) {
            top->setFont(c, f);
            top->setBackground(c, QBrush(Qt::red));
            top->setForeground(c, QBrush(Qt::white));
        }

        const QJsonArray sigs = topObj["signals"].toArray();
        for (const QJsonValue &sval : sigs) {
            const QJsonObject sigObj = sval.toObject();
            auto *ch = new QTreeWidgetItem(top);
            ch->setText(0, sigObj["signalName"].toString());
            ch->setText(1, QString());
            ch->setText(2, sigObj["unit"].toString());
            ch->setText(3, sigObj["receiver"].toString());
        }
    }

    // subTabPlot: plotSignals
    auto *list = ui->listWidgetCanSignals;
    list->clear();

    const QJsonArray plotSignals = root["plotSignals"].toArray();
    for (const QJsonValue &val : plotSignals) {
        QString signalName, signalUnit;
        double yMin = 0.0, yMax = 100.0;

        if (val.isObject()) {
            const QJsonObject obj = val.toObject();
            signalName = obj["name"].toString();
            signalUnit = obj["unit"].toString();
            yMin = obj.contains("min") ? obj["min"].toDouble() : yMin;
            yMax = obj.contains("max") ? obj["max"].toDouble() : yMax;
        } else {
            signalName = val.toString();
        }

        if (myApp.v.seriesMap.contains(signalName))
            continue;

        if (!myApp.v.chart || !myApp.v.axisX) {
            qWarning() << "Chart or axisX is not initialized!";
            continue;
        }

        QLineSeries *series = new QLineSeries();
        series->setName(signalName);
        myApp.v.seriesMap.insert(signalName, series);
        myApp.v.chart->addSeries(series);
        series->attachAxis(myApp.v.axisX);

        QValueAxis *yAxis = nullptr;
        if (myApp.v.axisYMap.contains(signalName)) {
            yAxis = myApp.v.axisYMap.value(signalName);
        } else {
            yAxis = new QValueAxis;
            yAxis->setTitleText(signalName + (signalUnit.isEmpty() ? "" : " (" + signalUnit + ")"));
            yAxis->setRange(yMin, yMax);
            myApp.v.chart->addAxis(yAxis, Qt::AlignRight);
            myApp.v.axisYMap.insert(signalName, yAxis);
        }

        series->attachAxis(yAxis);
        list->addItem(signalName);
    }

    // subTabPanel: panelSignals
    const QJsonArray panelSignals = root["panelSignals"].toArray();
    for (const QJsonValue &val : panelSignals) {
        const QJsonObject obj = val.toObject();
        const QString sigName = obj["signalName"].toString();
        const QString widgetType = obj["widgetType"].toString();
        const QPoint pos(obj["x"].toInt(), obj["y"].toInt());

        if (myApp.v.signalWidgetMap.contains(sigName))
            continue;

        QWidget *widget = nullptr;
        if      (widgetType == "QLCDNumber")  widget = new QLCDNumber();
        else if (widgetType == "QProgressBar")widget = new QProgressBar();
        else if (widgetType == "QSlider")     widget = new QSlider(Qt::Horizontal);
        else if (widgetType == "QDial")       widget = new QDial();
        else if (widgetType == "QLabel")      widget = new QLabel("0.0");
        else if (widgetType == "QCheckBox")   widget = new QCheckBox(sigName);
        else if (widgetType == "QComboBox") {
            auto *combo = new QComboBox();
            combo->addItems({"N","D","R","P"});
            widget = combo;
        }
        if (!widget) continue;

        auto *container = new QWidget();
        container->setObjectName(sigName);
        container->setStyleSheet(
            "border: 1px solid #ccc;"
            "background-color: #1a1a1a;"
            "color: white;"
            "border-radius: 4px;"
            );
        container->setFixedSize(140, 180);
        container->move(pos);

        auto *layout = new QVBoxLayout(container);
        layout->setContentsMargins(4,4,4,4);
        layout->setSpacing(4);

        auto *closeBtn = new QPushButton("Close");
        closeBtn->setFixedHeight(16);
        closeBtn->setStyleSheet(
            "QPushButton { background: red; color: white; border-radius: 2px; }"
            "QPushButton:hover { background: darkred; }"
            );
        connect(closeBtn, &QPushButton::clicked, this, [=]() {
            container->deleteLater();
            myApp.v.signalContainers.removeAll(container);
            myApp.v.signalWidgetMap.remove(sigName);
        });

        auto *label = new QLabel(sigName);
        label->setAlignment(Qt::AlignCenter);
        label->setMaximumHeight(20);
        label->setStyleSheet(
            "border: 1px solid #ccc;"
            "background: white;"
            "color: black;"
            "border-radius: 2px;"
            );

        layout->addWidget(closeBtn);
        layout->addWidget(label);
        layout->addWidget(widget);

        if (!ui->scrollAreaPanel->widget()) {
            auto *canvas = new QWidget();
            canvas->setMinimumSize(1600, 1200);
            ui->scrollAreaPanel->setWidget(canvas);
        }

        container->setParent(ui->scrollAreaPanel->widget());
        container->show();

        myApp.v.signalContainers.append(container);
        myApp.v.signalWidgetMap.insert(sigName, widget);
    }
}

void MainWindow::on_pushButtonClearLog_clicked()
{
    ui->plainTextEditLogs->clear();
}

void MainWindow::on_pushButtonSaveLog_clicked()
{
    // Define the path to save the log file
    QString logDirectoryPath = workspacePath + "/logs";

    // Get current date and time in the specified format
    QString dateTime = QDateTime::currentDateTime().toString("dd_MM_yyyy_HH_mm_ss");
    QString fileName = dateTime + ".log";

    // Create the logs directory if it doesn't exist
    QDir logDir(logDirectoryPath);
    if (!logDir.exists()) {
        if (!logDir.mkpath(".")) {
            QMessageBox::warning(this, tr("Error"), tr("Failed to create 'logs' directory."));
            return;
        }
    }

    // Full path to the log file
    QString filePath = logDirectoryPath + "/" + fileName;

    // Open the file in write mode
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Error"), tr("Unable to save log file."));
        return;
    }

    // Write the content of the log text edit to the file
    QTextStream out(&file);
    out << ui->plainTextEditLogs->toPlainText();
    file.close();

    // Show a confirmation message
    QMessageBox::information(this, tr("Success"), tr("Logs saved successfully at: ") + filePath);
}
