#include "mainWindow.h"
#include "ui_mainWindow.h"
#include "deviceDialog.h"
#include <CanApp.h>
#include <CanDbc.h>

extern CanDBC myDBC;
extern CanApp myApp;

void MainWindow::setupPlotChart()
{
    myApp.v.chart = new QChart();
    myApp.v.axisX = new QDateTimeAxis();

    myApp.v.chart->setTitle(tr("Live CAN Signals"));
    myApp.v.chart->legend()->setAlignment(Qt::AlignBottom);

    // Chart background
    myApp.v.chart->setBackgroundBrush(QBrush(Qt::white));
    myApp.v.chart->setBackgroundPen(Qt::NoPen);
    myApp.v.chart->setPlotAreaBackgroundBrush(QBrush(Qt::darkGray));
    myApp.v.chart->setPlotAreaBackgroundVisible(true);

    // X axis (time)
    QDateTime start = QDateTime::currentDateTime();
    myApp.v.axisX->setFormat("hh:mm:ss");
    myApp.v.axisX->setTitleText(tr("Time"));
    myApp.v.axisX->setGridLineVisible(true);
    myApp.v.axisX->setTickCount(10);
    myApp.v.axisX->setRange(start.addSecs(-30), start);
    myApp.v.chart->addAxis(myApp.v.axisX, Qt::AlignBottom);

    // Y-axis.

    // ChartView setup
    QChartView *cv = new QChartView(myApp.v.chart, ui->subTabPlot);
    cv->setRenderHint(QPainter::Antialiasing);
    cv->setRubberBand(QChartView::RectangleRubberBand);
    cv->setMouseTracking(true);
    cv->viewport()->installEventFilter(this);

    ui->graphicsViewCanMessage->hide();
    ui->graphicsViewCanMessage->setParent(nullptr);
    cv->setGeometry(ui->graphicsViewCanMessage->geometry());
    cv->show();
    myApp.v.chartView = cv;

    // Crosshair lines and labels
    auto *scene = myApp.v.chart->scene();
    QRectF plotArea = myApp.v.chart->plotArea();

    // Crosshair guide lines
    myApp.v.vLine = scene->addLine(plotArea.left(), plotArea.top(), plotArea.left(), plotArea.bottom(), QPen(Qt::white, 1, Qt::DashLine));
    myApp.v.hLine = scene->addLine(plotArea.left(), plotArea.top(), plotArea.right(), plotArea.top(), QPen(Qt::white, 1, Qt::DashLine));
    myApp.v.vLine->setVisible(false);
    myApp.v.hLine->setVisible(false);

    // Labels for coordinates
    myApp.v.labelX = new QGraphicsSimpleTextItem();
    myApp.v.labelY = new QGraphicsSimpleTextItem();
    myApp.v.labelX->setBrush(Qt::white);
    myApp.v.labelY->setBrush(Qt::white);
    QFont labelFont;
    labelFont.setPointSize(8);
    labelFont.setBold(true);
    myApp.v.labelX->setFont(labelFont);
    myApp.v.labelY->setFont(labelFont);
    scene->addItem(myApp.v.labelX);
    scene->addItem(myApp.v.labelY);
    myApp.v.labelX->setVisible(false);
    myApp.v.labelY->setVisible(false);
}

void MainWindow::resetPlotChart()
{
    if (!myApp.v.chart)
        return;

    // === Stop Chart Updates ===
    if (myApp.v.chartUpdateTimer)
        myApp.v.chartUpdateTimer->stop();

    // === Remove Series from Chart ===
    for (auto series : std::as_const(myApp.v.seriesMap)) {
        myApp.v.chart->removeSeries(series);
        delete series;
    }
    myApp.v.seriesMap.clear();
}

void MainWindow::repositionPanelWidgets()
{
    QGridLayout *gridLayout = qobject_cast<QGridLayout *>(ui->scrollAreaPanel->widget()->layout());
    if (!gridLayout) return;

    const int columns = 6;
    int index = 0;

    for (QWidget *container : std::as_const(myApp.v.signalContainers)) {
        int row = index / columns;
        int col = index % columns;
        gridLayout->addWidget(container, row, col);
        ++index;
    }
}

void MainWindow::onChartUpdateTimer()
{
    if (!myApp.s.rxRunning && !myApp.s.trcRunning)
        return;

    QMap<QString, QVector<QPointF>> localCopy;
    {
        QMutexLocker locker(&myApp.v.bufferMutex);
        localCopy = myApp.v.bufferedPoints;
        myApp.v.bufferedPoints.clear();
    }

    if (!myApp.v.chart || !myApp.v.axisX)
        return;

    QChart *chart = myApp.v.chart;
    const qint64 timeWindowMs = 30 * 1000;

    qint64 latestTimestamp = 0;
    const auto &constLocalCopy = localCopy;
    for (const auto &points : constLocalCopy) {
        if (!points.isEmpty()) {
            latestTimestamp = std::max(latestTimestamp, static_cast<qint64>(points.last().x()));
        }
    }

    if (latestTimestamp == 0) {
        return;
    }

    for (auto it = localCopy.constBegin(); it != localCopy.constEnd(); ++it) {
        const QString &sig = it.key();
        const QVector<QPointF> &newPoints = it.value();

        if (newPoints.isEmpty())
            continue;

        // Create and configure series if it doesn't exist
        if (!myApp.v.seriesMap.contains(sig)) {
            QLineSeries *newSeries = new QLineSeries();
            newSeries->setName(sig);
            chart->addSeries(newSeries);

            if (!newSeries->attachedAxes().contains(myApp.v.axisX))
                newSeries->attachAxis(myApp.v.axisX);

            if (myApp.v.axisYMap.contains(sig)) {
                QValueAxis *yAxis = myApp.v.axisYMap[sig];
                if (yAxis && !newSeries->attachedAxes().contains(yAxis)) {
                    newSeries->attachAxis(yAxis);
                }
            }

            myApp.v.seriesMap[sig] = newSeries;
        }

        QLineSeries *series = myApp.v.seriesMap[sig];
        QList<QPointF> existingPoints = series->points();
        bool updated = false;

        // Trim old points
        while (!existingPoints.isEmpty() && existingPoints.first().x() < latestTimestamp - timeWindowMs) {
            existingPoints.removeFirst();
            updated = true;
        }

        // Append new points
        existingPoints.append(newPoints);
        updated = true;

        if (updated) {
            series->replace(existingPoints);
        }
    }

    // Update X axis range
    QDateTime latest = QDateTime::fromMSecsSinceEpoch(latestTimestamp);
    myApp.v.axisX->setRange(latest.addMSecs(-timeWindowMs), latest);
}

void MainWindow::updateCanMessageUI(const CANMessageData &msgData, QMap<QString, qint64> &lastTimestamps, QMap<QString, double> &lastSignalValues)
{
    const QString hexId = QString::number(msgData.msg.ID, 16).toUpper();
    const QList<QPair<QString, double>> &decodedSignals = msgData.decodedSignals;
    qint64 timestamp;
    if (myApp.s.trcRunning && !myApp.s.rxRunning) {
        timestamp = msgData.timestampInMs;
    } else {
        timestamp = QDateTime::currentMSecsSinceEpoch();
    }

    if (!myApp.s.rxPaused && (myApp.s.rxRunning || myApp.s.trcRunning))
    {
        // === subTabPlot ===
        QStringList activeSignalNames;
        for (int i = 0; i < ui->listWidgetCanSignals->count(); ++i)
            activeSignalNames.append(ui->listWidgetCanSignals->item(i)->text());

        {
            QMutexLocker locker(&myApp.v.bufferMutex);
            for (const auto &signal : decodedSignals) {
                const QString &sigName = signal.first;
                double value = signal.second;
                if (activeSignalNames.contains(sigName))
                    myApp.v.bufferedPoints[sigName].append(QPointF(timestamp, value));
            }
        }

        // === subTabLiveData ===
        for (int i = 0; i < ui->treeWidgetCanMessage->topLevelItemCount(); ++i) {
            auto *it = ui->treeWidgetCanMessage->topLevelItem(i);
            if (it->text(1) == hexId) {
                int count = it->text(4).toInt();
                it->setText(4, QString::number(count + 1));

                if (lastTimestamps.contains(hexId)) {
                    qint64 cycleTimeMs = timestamp - lastTimestamps[hexId];
                    it->setText(5, QString::number(static_cast<double>(cycleTimeMs), 'f', 2));
                }
                lastTimestamps[hexId] = timestamp;

                for (const auto &p : decodedSignals) {
                    QString sigName = p.first;
                    double val = p.second;
                    QString valText = QString::number(val);

                    for (int ci = 0; ci < it->childCount(); ++ci) {
                        auto *child = it->child(ci);
                        if (child->text(0) == sigName) {
                            bool isChanging = !lastSignalValues.contains(sigName) ||
                                              !qFuzzyCompare(1.0 + lastSignalValues[sigName], 1.0 + val);

                            child->setBackground(1, isChanging ? QBrush(Qt::yellow) : QBrush(Qt::NoBrush));
                            child->setForeground(1, isChanging ? QBrush(Qt::black) : QBrush(Qt::NoBrush));
                            child->setText(1, valText);
                            lastSignalValues[sigName] = val;
                            break;
                        }
                    }
                }
                break;
            }
        }

        // === subTabPanel ===
        for (const auto &p : decodedSignals) {
            const QString &sigName = p.first;
            double val = p.second;

            if (!myApp.v.signalWidgetMap.contains(sigName)) continue;

            const double prevVal = myApp.v.lastSignalUIValues.value(sigName, std::numeric_limits<double>::quiet_NaN());
            const qint64 lastTs = myApp.v.lastSignalUITimestamps.value(sigName, 0);
            const bool changed = !qFuzzyCompare(1.0 + val, 1.0 + prevVal);
            const bool timeout = (timestamp - lastTs) >= 250;

            if (changed || timeout) {
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
                myApp.v.lastSignalUITimestamps[sigName] = timestamp;
            }
        }
    }
}

void MainWindow::on_pushButtonTxStartStop_clicked()
{
    // Check if device is connected first
    if (!myApp.s.deviceConnected)
    {
        QMessageBox::warning(this, "Device Not Connected", "Please connect the PCAN device before starting transmission.");
        return;
    }

    if (!myApp.s.txRunning)
    {
        myApp.DeviceBufferReset();

        myApp.s.trcRunning = false;
        myApp.s.txPaused = false;
        myApp.s.txRunning = true;

        //ui->pushButtonTxStartStop->setText("TX Stop");
        ui->pushButtonTxStartStop->setStyleSheet("background-color: red; color: white; border-radius: 6px; padding: 8px 16px;");
        ui->pushButtonTxStartStop->setIcon(QIcon::fromTheme("media-playback-stop"));

        ui->pushButtonTxPauseResume->setEnabled(true);
        //ui->pushButtonTxPauseResume->setText("TX Pause");
        ui->pushButtonTxPauseResume->setStyleSheet("background-color: orange; color: white; border-radius: 6px; padding: 8px 16px;");
        ui->pushButtonTxPauseResume->setIcon(QIcon::fromTheme("media-playback-pause"));

        ui->pushButtonTraceRePlayStartStop->setEnabled(false);
        ui->pushButtonTraceRePlayPauseResume->setEnabled(false);

        QFuture<void> future = QtConcurrent::run([this]() {
            QMap<int, qint64> lastSendTimestamps;

            while (myApp.s.deviceConnected && myApp.s.txRunning)
            {
                qint64 now = QDateTime::currentMSecsSinceEpoch();
                int rowCount = ui->tableWidgetTx->rowCount();
                for (int row = 0; row < rowCount; ++row) {
                    auto *idItem = ui->tableWidgetTx->item(row, 0);
                    auto *dlcItem = ui->tableWidgetTx->item(row, 1);
                    auto *dataItem = ui->tableWidgetTx->item(row, 2);
                    auto *countItem = ui->tableWidgetTx->item(row, 3);

                    QWidget *cellWidget = ui->tableWidgetTx->cellWidget(row, 4);
                    QLineEdit *cycleLineEdit = cellWidget ? cellWidget->findChild<QLineEdit *>() : nullptr;
                    QCheckBox *cycleCheckBox = cellWidget ? cellWidget->findChild<QCheckBox *>() : nullptr;

                    if (!idItem || !dlcItem || !dataItem || !cycleLineEdit || !cycleCheckBox)
                        continue;

                    int cycleTime = cycleLineEdit->text().toInt();
                    if (!cycleCheckBox->isChecked() || cycleTime < 1)
                        continue;

                    QString msgIdStr = idItem->text().trimmed().toUpper();
                    if (msgIdStr.isEmpty() || msgIdStr == "0" || msgIdStr == "00" || msgIdStr == "000")
                        continue;

                    // Check if it's time to send for this row
                    if (now - lastSendTimestamps.value(row, 0) < cycleTime)
                        continue;

                    int dlc = dlcItem->text().toInt();
                    QStringList dataList = dataItem->text().trimmed().split(' ', Qt::SkipEmptyParts);
                    if (dlc <= 0 || dataList.size() > 8)
                        continue;

                    TPCANMsg CANMsg;
                    CANMsg.ID = msgIdStr.toUInt(nullptr, 16);
                    CANMsg.LEN = static_cast<BYTE>(dlc);
                    CANMsg.MSGTYPE = PCAN_MESSAGE_STANDARD;

                    for (int i = 0; i < dataList.size(); ++i) {
                        bool ok = false;
                        uint byteVal = dataList[i].toUInt(&ok, 16);
                        CANMsg.DATA[i] = ok ? static_cast<BYTE>(byteVal) : 0x00;
                    }

                    if(!myApp.s.txPaused)
                    {
                        TPCANStatus sts = CAN_Write(myApp.h.handle, &CANMsg);
                        if (sts == PCAN_ERROR_OK) {
                            quint64 currentCount = 0;
                            if (countItem && !countItem->text().isEmpty()) {
                                currentCount = countItem->text().toULongLong();
                            }
                            currentCount++;

                            QMetaObject::invokeMethod(this, [=]() {
                                ui->tableWidgetTx->item(row, 3)->setText(QString::number(currentCount));
                            }, Qt::QueuedConnection);

                            lastSendTimestamps[row] = now;
                        } else {
                            qDebug() << "CAN_Write failed for row" << row << ":" << QString::number(sts, 16).toUpper();
                        }
                    }
                }
                QThread::msleep(1);  // Avoid CPU hogging
            }

            // On stop
            QMetaObject::invokeMethod(this, [this]() {
                myApp.s.txRunning = false;
                //ui->pushButtonTxStartStop->setText("TX Start");
                ui->pushButtonTxStartStop->setStyleSheet("background-color: green; color: white; border-radius: 6px; padding: 8px 16px;");
                ui->pushButtonTxStartStop->setIcon(QIcon::fromTheme("media-playback-start"));
            }, Qt::QueuedConnection);
        });
        qDebug() << "✅ TX Start";
    } else {
        // Stop threads
        myApp.s.txRunning = false;
        myApp.s.txPaused = false;
        //ui->pushButtonTxStartStop->setText("TX Start");
        ui->pushButtonTxStartStop->setStyleSheet("background-color: green; color: white; border-radius: 6px; padding: 8px 16px;");
        ui->pushButtonTxStartStop->setIcon(QIcon::fromTheme("media-playback-start"));

        ui->pushButtonTxPauseResume->setEnabled(false);
        //ui->pushButtonTxPauseResume->setText("TX Pause");
        //ui->pushButtonTxPauseResume->setStyleSheet("");
        ui->pushButtonTxPauseResume->setStyleSheet("background-color: gray; color: white; border-radius: 6px; padding: 8px 16px;");
        ui->pushButtonTxPauseResume->setIcon(QIcon::fromTheme("media-playback-pause"));

        if (!myApp.s.txRunning && !myApp.s.rxRunning && myApp.s.trcFileLoaded) {
            ui->pushButtonTraceRePlayStartStop->setEnabled(true);
        }
        qDebug() << "⛔ TX Stop";
    }
}

void MainWindow::on_pushButtonTxPauseResume_clicked()
{
    if (!myApp.s.txRunning)
        return;

    myApp.s.txPaused = !myApp.s.txPaused;

    if (myApp.s.txPaused) {
        //ui->pushButtonTxPauseResume->setText("TX Resume");
        ui->pushButtonTxPauseResume->setStyleSheet("background-color: gray; color: white; border-radius: 6px; padding: 8px 16px;");
        ui->pushButtonTxPauseResume->setIcon(QIcon::fromTheme("media-playback-start"));
        qDebug() << "⏸️ TX Paused";
    } else {
        //ui->pushButtonTxPauseResume->setText("TX Pause");
        ui->pushButtonTxPauseResume->setStyleSheet("background-color: orange; color: white; border-radius: 6px; padding: 8px 16px;");
        ui->pushButtonTxPauseResume->setIcon(QIcon::fromTheme("media-playback-pause"));
        qDebug() << "▶️ TX Resume";
    }
}

void MainWindow::on_pushButtonRxStartStop_clicked()
{
    // Check if device is connected first
    if (!myApp.s.deviceConnected)
    {
        QMessageBox::warning(this, "Device Not Connected", "Please connect the PCAN device before starting transmission.");
        return;
    }

    if (!myApp.s.rxRunning)
    {
        myApp.DeviceBufferReset();
        resetPlotChart();

        myApp.s.trcRunning = false;
        myApp.s.rxPaused = false;
        myApp.s.rxRunning = true;
        if (!myApp.v.chartUpdateTimer->isActive()) {
            myApp.v.chartUpdateTimer->start(200);
        }

        ui->pushButtonRxStartStop->setStyleSheet("background-color: red; color: white; border-radius: 6px; padding: 8px 16px;");
        ui->pushButtonRxStartStop->setIcon(QIcon::fromTheme("media-playback-stop"));
        ui->pushButtonRxPauseResume->setEnabled(true);
        ui->pushButtonRxPauseResume->setStyleSheet("background-color: orange; color: white; border-radius: 6px; padding: 8px 16px;");
        ui->pushButtonRxPauseResume->setIcon(QIcon::fromTheme("media-playback-pause"));
        ui->pushButtonTraceRePlayStartStop->setEnabled(false);
        ui->pushButtonTraceRePlayPauseResume->setEnabled(false);

        myApp.v.RxProducerFuture = QtConcurrent::run([this]() {
            while (myApp.s.rxRunning)
            {
                TPCANMsg msg;
                TPCANTimestamp ts;
                TPCANStatus sts = CAN_Read(myApp.h.handle, &msg, &ts);

                if (sts == PCAN_ERROR_OK) {
                    QByteArray canData(reinterpret_cast<char*>(msg.DATA), msg.LEN);
                    QList<QPair<QString, double>> decodedSignals = myDBC.decodeFrame(msg.ID, canData);
                    QMutexLocker locker(&queueMutex);
                    canMessageQueue.enqueue(CANMessageData{msg, ts, decodedSignals});
                    queueNotEmpty.wakeOne();
                }
                else if (sts == PCAN_ERROR_QRCVEMPTY) {
                    QThread::msleep(1);
                }
                else if (sts == PCAN_ERROR_QOVERRUN) {
                    qWarning() << "⚠️ CAN queue overrun: messages lost!";
                    myApp.DeviceBufferReset();
                }
                else {
                    qWarning() << "CAN Read Error: " << QString::number(sts, 16).toUpper();
                    QThread::msleep(2);
                }
            }
            queueNotEmpty.wakeAll();
        });

        consumerThreadRunning = true;
        myApp.v.RxConsumerFuture = QtConcurrent::run([this]() {
            QMap<QString, qint64> lastTimestamps;
            QMap<QString, double> lastSignalValues;
            while (consumerThreadRunning)
            {
                queueMutex.lock();
                if (canMessageQueue.isEmpty() && consumerThreadRunning) {
                    queueNotEmpty.wait(&queueMutex, 1000);
                }

                if (!consumerThreadRunning) {
                    queueMutex.unlock();
                    break;
                }

                if (!canMessageQueue.isEmpty()) {
                    CANMessageData msgData = canMessageQueue.dequeue();
                    queueMutex.unlock();

                    QMetaObject::invokeMethod(this, [=, &lastTimestamps, &lastSignalValues]() mutable {
                        if (!myApp.s.rxPaused && myApp.s.rxRunning)
                        {
                            updateCanMessageUI(msgData, lastTimestamps, lastSignalValues);
                            subTabRxTxReceive(msgData.msg);
                            if (myApp.s.trcRecording && myApp.v.traceFile.isOpen()) {
                                recordTraceFile(msgData.msg, msgData.ts);
                            } else if (myApp.v.traceFile.isOpen()) {
                                stopTraceFile();
                            }
                        }
                    }, Qt::QueuedConnection);
                } else {
                    queueMutex.unlock();
                }
            }

            QMetaObject::invokeMethod(this, [this]() {
                myApp.s.rxRunning = false;
                consumerThreadRunning = false;
                ui->pushButtonRxStartStop->setStyleSheet("background-color: green; color: white; border-radius: 6px; padding: 8px 16px;");
                ui->pushButtonRxStartStop->setIcon(QIcon::fromTheme("media-playback-start"));
            }, Qt::QueuedConnection);
        });
        qDebug() << "✅ RX Started";
    }
    else
    {
        myApp.s.rxRunning = false;
        consumerThreadRunning = false;
        queueNotEmpty.wakeAll();

        myApp.v.RxProducerFuture.waitForFinished();
        myApp.v.RxConsumerFuture.waitForFinished();

        for (int i = 0; i < ui->treeWidgetCanMessage->topLevelItemCount(); ++i) {
            QTreeWidgetItem *item = ui->treeWidgetCanMessage->topLevelItem(i);
            if (item) item->setText(4, QString::number(0));
        }

        myApp.v.bufferedPoints.clear();
        myApp.v.chartUpdateTimer->stop();
        if (myApp.v.axisX) {
            QDateTime now = QDateTime::currentDateTime();
            myApp.v.axisX->setRange(now.addSecs(-30), now);
        }

        ui->pushButtonRxStartStop->setStyleSheet("background-color: green; color: white; border-radius: 6px; padding: 8px 16px;");
        ui->pushButtonRxStartStop->setIcon(QIcon::fromTheme("media-playback-start"));
        ui->pushButtonRxPauseResume->setEnabled(false);
        ui->pushButtonRxPauseResume->setStyleSheet("background-color: gray; color: white; border-radius: 6px; padding: 8px 16px;");
        ui->pushButtonRxPauseResume->setIcon(QIcon::fromTheme("media-playback-pause"));

        if (!myApp.s.txRunning && !myApp.s.rxRunning && myApp.s.trcFileLoaded) {
            ui->pushButtonTraceRePlayStartStop->setEnabled(true);
        }
        qDebug() << "⛔ RX Stopped";
    }
}

void MainWindow::on_pushButtonRxPauseResume_clicked()
{
    if (!myApp.s.rxRunning) return;

    myApp.s.rxPaused = !myApp.s.rxPaused;
    if (myApp.s.rxPaused) {
        //ui->pushButtonRxPauseResume->setText("RX Resume");
        ui->pushButtonRxPauseResume->setStyleSheet("background-color: gray; color: white; border-radius: 6px; padding: 8px 16px;");
        ui->pushButtonRxPauseResume->setIcon(QIcon::fromTheme("media-playback-start"));
        qDebug() << "⏸️ RX Paused";
    } else {
        //ui->pushButtonRxPauseResume->setText("RX Pause");
        ui->pushButtonRxPauseResume->setStyleSheet("background-color: orange; color: white; border-radius: 6px; padding: 8px 16px;");
        ui->pushButtonRxPauseResume->setIcon(QIcon::fromTheme("media-playback-pause"));
        qDebug() << "▶️ RX Resume";
    }
}

void MainWindow::on_pushButtonDbcFileBrowse_clicked()
{
    const QString fileFilter = tr("DBC Files (*.dbc)");

    // Start browsing from workspacePath if valid, else fallback to Documents/home
    QString startDir = workspacePath;
    if (startDir.isEmpty() || !QDir(startDir).exists()) {
#ifdef Q_OS_WIN
        startDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
#else
        startDir = QDir::homePath();
#endif
    }

    // Open file dialog
    QString fileName = QFileDialog::getOpenFileName(this, tr("Select DBC file"), startDir, fileFilter);

    if (fileName.isEmpty())
        return;

    // Ensure the file has a .dbc extension
    if (!fileName.endsWith(".dbc", Qt::CaseInsensitive)) {
        QMessageBox::warning(this, tr("Error"), tr("Please select a valid .dbc file."));
        return;
    }

    qDebug() << "Selected DBC file:" << fileName;

    ui->lineEditDbcFileUpload->setText(fileName);
    myApp.v.dbcFilePath = fileName;
    ui->pushButtonApplyDbc->setEnabled(true);

    // --- Copy the selected DBC file to workspacePath ---
    if (!workspacePath.isEmpty() && QDir(workspacePath).exists()) {
        QString destFile = QDir(workspacePath).filePath(QFileInfo(fileName).fileName());
        if (QFile::exists(destFile)) {
            QFile::remove(destFile);
        }
        if (QFile::copy(fileName, destFile)) {
            //TVS: qDebug() << "Copied DBC file to workspace:" << destFile;
        } else {
            QMessageBox::warning(this, tr("Error"), tr("Failed to copy DBC file to workspace."));
        }
    }

    saveAppData(workspacePath + "/" + APP_WORKSPACE_FILE_NAME);
}

void MainWindow::on_pushButtonApplyDbc_clicked()
{
    // Check DBC path
    if (myApp.v.dbcFilePath.isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("No DBC file selected. Please upload a DBC file first."));
        return;
    }

    // Parse into messageDefinitions
    if (!myDBC.loadDBC(myApp.v.dbcFilePath)) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to load DBC file"));
        return;
    }

    // Prepare the tree
    ui->treeWidgetCanMessageList->clear();
    ui->treeWidgetCanMessageList->setColumnCount(1);
    ui->treeWidgetCanMessageList->setHeaderHidden(true);
    ui->treeWidgetCanMessageList->setRootIsDecorated(true);

    // Populate with messages and their signals
    for (const auto &msg : myDBC.msgList()) {
        // Make the top-level item
        QString header = QStringLiteral("[0x%1]%2").arg(QString::number(msg.id,16).toUpper(), msg.name);
        QTreeWidgetItem *parent = new QTreeWidgetItem(ui->treeWidgetCanMessageList, QStringList{ header });
        parent->setExpanded(false);

        QFont f = parent->font(0);
        f.setBold(true);
        for (int col = 0; col < ui->treeWidgetCanMessage->columnCount(); ++col) {
            parent->setFont(col, f);
            parent->setBackground(col, QBrush(Qt::red));
            parent->setForeground(col, QBrush(Qt::white));
        }

        // Add each signal as a child node
        for (const auto &sig : msg.canSignals) {
            // prefix arrow, no trailing comma
            QString text = QStringLiteral("%1").arg(sig.name);
            new QTreeWidgetItem(parent, QStringList{ text });
        }
    }
    // Optionally scroll to top
    ui->treeWidgetCanMessageList->scrollToTop();

    // Enable Run/Stop/Add/Remove
    if (!myApp.s.rxRunning && !myApp.s.trcRunning)
    {
        myApp.s.dbcAttached = true;
        ui->pushButtonRxStartStop->setEnabled(true);
        ui->pushButtonAddItem->setEnabled(true);
        ui->pushButtonRemoveItem->setEnabled(true);
        ui->lineEditFilterItem->clear();
    }

    if (!myApp.s.txRunning && !myApp.s.trcRunning)
    {
        ui->pushButtonTxStartStop->setEnabled(true);
        ui->pushButtonAddItem->setEnabled(true);
        ui->pushButtonRemoveItem->setEnabled(true);
    }
    QMessageBox::information(this, tr("Information"), tr("DBC file applied successfully."));
    qDebug() << "Total Messages Parsed:" << myDBC.msgList().size();
    ui->lineEditCanMessageCount->setText(QString::number(myDBC.msgList().size()));
    saveAppData(workspacePath + "/" + APP_WORKSPACE_FILE_NAME);
}

void MainWindow::on_lineEditFilterItem_textChanged(const QString &filterText)
{
    ui->treeWidgetCanMessageList->clear();
    ui->treeWidgetCanMessageList->setColumnCount(1);
    ui->treeWidgetCanMessageList->setHeaderHidden(true);
    ui->treeWidgetCanMessageList->setRootIsDecorated(true);

    const auto &msgs = myDBC.msgList();
    int matchCount = 0;

    for (const auto &msg : msgs)
    {
        QString header = QStringLiteral("[0x%1]%2").arg(QString::number(msg.id, 16).toUpper(), msg.name);

        bool parentMatches = msg.name.contains(filterText, Qt::CaseInsensitive)
                             || QString::number(msg.id, 16).contains(filterText, Qt::CaseInsensitive);

        // Check if any of the signals match
        QList<QString> matchedSignals;
        for (const auto &sig : msg.canSignals) {
            if (sig.name.contains(filterText, Qt::CaseInsensitive)) {
                matchedSignals.append(sig.name);
            }
        }

        if (parentMatches || !matchedSignals.isEmpty())
        {
            ++matchCount;

            QTreeWidgetItem *parent = new QTreeWidgetItem(ui->treeWidgetCanMessageList, QStringList{ header });
            parent->setExpanded(false);

            QFont f = parent->font(0);
            f.setBold(true);
            for (int col = 0; col < ui->treeWidgetCanMessage->columnCount(); ++col) {
                parent->setFont(col, f);
                parent->setBackground(col, QBrush(Qt::red));
                parent->setForeground(col, QBrush(Qt::white));
            }

            for (const auto &sig : msg.canSignals) {
                // Show all signals if parent matched; otherwise show only matched signals
                if (parentMatches || matchedSignals.contains(sig.name)) {
                    new QTreeWidgetItem(parent, QStringList{ sig.name });
                }
            }
        }
    }

    if (matchCount == 0) {
        auto *placeholder = new QTreeWidgetItem(ui->treeWidgetCanMessageList, QStringList{ tr("No matching messages.") });
        placeholder->setFlags(Qt::NoItemFlags);
    }

    ui->treeWidgetCanMessageList->scrollToTop();
    ui->lineEditCanMessageCount->setText(QString::number(matchCount));
}

void MainWindow::on_pushButtonAddItem_clicked()
{
    QWidget *current = ui->mainTab->currentWidget();
    if (current == ui->subTabLiveData)
    {
        // Grab selection from the CAN-message list
        auto sel = ui->treeWidgetCanMessageList->selectedItems();
        if (sel.isEmpty()) {
            QMessageBox::warning(this, tr("Warning"), tr("Please select a message to add."));
            return;
        }
        QString text = sel.first()->text(0);

        // parse “[0xID]Name”
        static const QRegularExpression re(R"(\[0[xX]([0-9A-Fa-f]+)\]([^\]]+))");
        auto m = re.match(text);
        if (!m.hasMatch()) {
            QMessageBox::warning(this, tr("Error"), tr("Please select a CAN message from the list!"));
            return;
        }
        bool ok = false;
        int msgId = m.captured(1).toInt(&ok, 16);
        QString msgName = m.captured(2);
        if (!ok) {
            QMessageBox::warning(this, tr("Error"), tr("Invalid message ID."));
            return;
        }

        // dup-check
        QString hexId = QString::number(msgId, 16).toUpper();
        for (int i = 0; i < ui->treeWidgetCanMessage->topLevelItemCount(); ++i) {
            if (ui->treeWidgetCanMessage->topLevelItem(i)->text(1) == hexId) {
                QMessageBox::information(this, tr("Information"), tr("Message [0x%1] %2 is already added!").arg(hexId, msgName));
                return;
            }
        }

        // find definition
        const auto &msgs = myDBC.msgList();
        auto it = std::find_if(msgs.cbegin(), msgs.cend(),
                               [&](auto const &mm){ return mm.id == msgId && mm.name == msgName; });
        if (it == msgs.cend()) {
            QMessageBox::warning(this, tr("Error"), tr("Message not found."));
            return;
        }
        const auto &msg = *it;

        // add to tree
        QStringList cols{msg.name, hexId, msg.description, msg.sender, QString::number(0), QString("-")};
        QTreeWidgetItem *parent = new QTreeWidgetItem(ui->treeWidgetCanMessage, cols);
        parent->setExpanded(true);
        // style parent
        QFont f = parent->font(0); f.setBold(true);
        for (int c = 0; c < cols.size(); ++c) {
            parent->setFont(c, f);
            parent->setBackground(c, QBrush(Qt::red));
            parent->setForeground(c, QBrush(Qt::white));
        }
        // children = signals
        for (auto const &sig : msg.canSignals) {
            new QTreeWidgetItem(parent, QStringList{sig.name, QString(), sig.unit, sig.receiver});
        }
        ui->treeWidgetCanMessage->scrollToItem(parent);
    }
    else if (current == ui->subTabPlot)
    {
        // You must select a signal child, not a message header:
        auto sel = ui->treeWidgetCanMessageList->selectedItems();
        if (sel.isEmpty()) {
            QMessageBox::warning(this, tr("Warning"), tr("Please select a signal to plot."));
            return;
        }

        QTreeWidgetItem *item = sel.first();
        if (!item->parent()) {
            QMessageBox::warning(this, tr("Warning"), tr("Please select a CAN signal, not a message."));
            return;
        }

        const QString signalName = item->text(0);

        // Make sure the signal isn't already added:
        for (int i = 0; i < ui->listWidgetCanSignals->count(); ++i) {
            if (ui->listWidgetCanSignals->item(i)->text() == signalName) {
                QMessageBox::information(this, tr("Information"), tr("Signal \"%1\" is already added!").arg(signalName));
                return;
            }
        }

        // Add to the list of signals to plot (UI list on the left)
        auto *newItem = new QListWidgetItem(signalName, ui->listWidgetCanSignals);
        static const QVector<QColor> palette = {
            Qt::red, Qt::green, Qt::blue, Qt::magenta,
            Qt::cyan, Qt::yellow, Qt::gray, Qt::darkRed,
            Qt::darkGreen, Qt::darkBlue
        };
        int colorIndex = (ui->listWidgetCanSignals->count() - 1) % palette.size();
        //newItem->setBackground(palette[colorIndex]);
        newItem->setForeground(Qt::white);
        ui->listWidgetCanSignals->addItem(newItem);

        // Now, add a new series + its own Y-axis
        if (myApp.v.axisX && myApp.v.chartView) {
            auto *chart = myApp.v.chartView->chart();

            // Create the QLineSeries
            auto *series = new QLineSeries();
            series->setName(signalName);
            series->setColor(palette[colorIndex]);
            series->setPointsVisible(true);

            // Add series to chart
            chart->addSeries(series);

            // Attach the shared X-axis
            if (!series->attachedAxes().contains(myApp.v.axisX)) {
                series->attachAxis(myApp.v.axisX);
            }

            QString unit;
            for (const auto &msg : myDBC.msgList()) {
                for (const auto &sig : msg.canSignals) {
                    if (sig.name == signalName) {
                        unit = sig.unit;
                        break;
                    }
                }
                if (!unit.isEmpty()) break;
            }

            // Create a new Y-axis for this signal
            auto *yAxis = new QValueAxis();
            yAxis->setTitleText(signalName + (unit.isEmpty() ? "" : " (" + unit + ")"));

            // Default range (e.g. 0–100).
            yAxis->setRange(0, 100);
            yAxis->setLabelFormat("%.2f");
            yAxis->setGridLineVisible(true);

            // Add this axis on the right side
            chart->addAxis(yAxis, Qt::AlignRight);

            // Attach the series to this Y-axis
            if (!series->attachedAxes().contains(yAxis)) {
                series->attachAxis(yAxis);
            }

            myApp.v.axisYMap.insert(signalName, yAxis);
            myApp.v.seriesMap.insert(signalName, series);
        }
    }
    else if (current == ui->subTabPanel)
    {
        auto sel = ui->treeWidgetCanMessageList->selectedItems();
        if (sel.isEmpty()) return;

        QTreeWidgetItem *item = sel.first();
        if (!item->parent()) {
            QMessageBox::warning(this, "Invalid", "Select a signal (child), not a message.");
            return;
        }

        QString signalName = item->text(0);
        if (myApp.v.signalWidgetMap.contains(signalName)) {
            QMessageBox::information(this, "Already Added", "This signal is already added.");
            return;
        }

        QStringList types = {"QLCDNumber","QProgressBar","QSlider","QDial","QLabel","QCheckBox","QComboBox","QPixmap"};
        bool ok;
        QString widgetType = QInputDialog::getItem(this, "Choose Widget Type", tr("Select UI for %1").arg(signalName), types, 0, false, &ok);
        if (widgetType.isEmpty()) return;

        QWidget *widget = nullptr;
        if (widgetType == "QLCDNumber") {
            auto *lcd = new QLCDNumber();
            lcd->setDigitCount(6);
            lcd->setSegmentStyle(QLCDNumber::Flat);
            widget = lcd;

        } else if (widgetType == "QProgressBar") {
            auto *bar = new QProgressBar();
            bar->setMinimum(0);
            bar->setMaximum(100);
            widget = bar;

        } else if (widgetType == "QSlider") {
            auto *slider = new QSlider(Qt::Horizontal);
            slider->setMinimum(0);
            slider->setMaximum(100);
            slider->setEnabled(false);
            widget = slider;

        } else if (widgetType == "QDial") {
            auto *dial = new QDial();
            dial->setMinimum(0);
            dial->setMaximum(100);
            dial->setNotchesVisible(true);
            dial->setEnabled(false);
            widget = dial;

        } else if (widgetType == "QLabel") {
            auto *label = new QLabel("0.0");
            widget = label;

        } else if (widgetType == "QCheckBox") {
            auto *chk = new QCheckBox(signalName);
            chk->setEnabled(false);
            widget = chk;

        } else if (widgetType == "QComboBox") {
            auto *combo = new QComboBox();
            combo->addItems({ "N", "D", "R", "P" });
            combo->setEnabled(false);
            widget = combo;

        } else if (widgetType == "QPixmap") {
            auto *iconLabel = new QLabel();
            QPixmap pix(":/icons/warning.png");
            iconLabel->setPixmap(pix.scaled(24, 24));
            widget = iconLabel;
        }

        if (!widget) return;

        // Create container with close button
        auto *container = new QWidget();
        container->setObjectName(signalName);
        container->setStyleSheet("border: 1px solid #ccc; background-color: #1a1a1a; color: white; border-radius: 4px;");
        container->setFixedSize(140, 180);

        auto *mainLayout = new QVBoxLayout(container);
        mainLayout->setContentsMargins(4, 4, 4, 4);
        mainLayout->setSpacing(4);

        auto *closeBtn = new QPushButton("   Close   ");
        closeBtn->setFixedSize(132, 16);
        closeBtn->setStyleSheet("QPushButton { border: 1px; background: red; color: white; font-weight: bold; border-radius: 2px; } QPushButton:hover { background: darkred; }");

        connect(closeBtn, &QPushButton::clicked, this, [=]() {
            container->deleteLater();
            myApp.v.signalWidgetMap.remove(signalName);
            myApp.v.signalContainers.removeAll(container);
            repositionPanelWidgets();
        });

        auto *closeLayout = new QHBoxLayout();
        closeLayout->addWidget(closeBtn);
        closeLayout->addStretch();

        auto *label = new QLabel(signalName);
        label->setAlignment(Qt::AlignCenter);
        label->setMaximumWidth(140);
        label->setMaximumHeight(20);
        label->setStyleSheet("border: 1px solid #ccc; background-color: white; color: black; border-radius: 2px;");

        mainLayout->addLayout(closeLayout);
        mainLayout->addWidget(label);
        mainLayout->addWidget(widget);

        if (!ui->scrollAreaPanel->widget()) {
            auto *scrollWidget = new QWidget();
            scrollWidget->setObjectName("scrollWidgetPanel");
            scrollWidget->setStyleSheet("background-color: #111;");
            scrollWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);

            auto *gridLayout = new QGridLayout(scrollWidget);
            gridLayout->setObjectName("gridLayoutPanel");
            gridLayout->setSpacing(8);
            scrollWidget->setLayout(gridLayout);

            ui->scrollAreaPanel->setWidget(scrollWidget);
        }

        QWidget *panelCanvas = ui->scrollAreaPanel->widget();
        container->setParent(panelCanvas);
        container->show();

        myApp.v.signalWidgetMap.insert(signalName, widget);
        myApp.v.signalContainers.append(container);
        repositionPanelWidgets();
    }
    else if (current == ui->subTabRxTx)
    {
        QTableWidget *table = ui->tableWidgetTx;

        int newRow = table->rowCount();
        table->insertRow(newRow);

        // Set default values for columns 0-3
        table->setItem(newRow, 0, new QTableWidgetItem("000"));
        table->setItem(newRow, 1, new QTableWidgetItem("8"));
        table->setItem(newRow, 2, new QTableWidgetItem("00 00 00 00 00 00 00 00"));
        table->setItem(newRow, 3, new QTableWidgetItem("0"));

        // ---- Column 4: Checkbox + LineEdit ----
        QWidget *container = new QWidget;
        container->setStyleSheet("background-color: transparent;");

        QHBoxLayout *hLayout = new QHBoxLayout(container);
        hLayout->setContentsMargins(0, 0, 0, 0);
        hLayout->setSpacing(4);

        QLineEdit *tableLineEdit = new QLineEdit;
        tableLineEdit->setFixedWidth(100);
        tableLineEdit->setValidator(new QIntValidator(0, 1000, container));
        tableLineEdit->setText("0");
        tableLineEdit->setEnabled(true);
        tableLineEdit->setStyleSheet("background-color: white; color: black;");

        QCheckBox *tableCheckBox = new QCheckBox;
        tableCheckBox->setChecked(false);
        tableCheckBox->setStyleSheet("background-color: transparent;");

        QObject::connect(tableCheckBox, &QCheckBox::toggled, this, [=](bool checked){
            tableLineEdit->setEnabled(checked);
            if (!checked)
                tableLineEdit->setStyleSheet("color: gray; background-color: #f0f0f0;");
            else
                tableLineEdit->setStyleSheet("background-color: white; color: black;");
        });

        hLayout->addWidget(tableLineEdit);
        hLayout->addWidget(tableCheckBox);
        container->setLayout(hLayout);

        // Set the container widget into column 4
        ui->tableWidgetTx->setCellWidget(newRow, 4, container);
    }
    else
    {
        QMessageBox::warning(this, tr("Warning"), tr("Adding is only supported in Live Data/Plot/Panel/Rx-Tx tab!"));
    }
}

void MainWindow::on_pushButtonRemoveItem_clicked()
{
    QWidget *current = ui->mainTab->currentWidget();

    if (current == ui->subTabLiveData)
    {
        QTreeWidgetItem *item = ui->treeWidgetCanMessage->currentItem();
        if (!item) {
            QMessageBox::warning(this, tr("Warning"), tr("Select a message/signal to remove."));
            return;
        }

        if (item->parent()) {
            item = item->parent();
        }

        int idx = ui->treeWidgetCanMessage->indexOfTopLevelItem(item);
        if (idx >= 0) {
            ui->treeWidgetCanMessage->takeTopLevelItem(idx);
        }
    }
    else if (current == ui->subTabPlot) {
        auto selItems = ui->listWidgetCanSignals->selectedItems();
        if (selItems.isEmpty()) {
            QMessageBox::warning(this, tr("Warning"), tr("Select a signal to remove."));
            return;
        }

        QMutexLocker chartLocker(&myApp.v.chartAccessMutex); // 🔒 Protect chart access

        for (QListWidgetItem *listItem : std::as_const(selItems)) {
            QString sigName = listItem->text();

            QChart *chart = myApp.v.chartView ? myApp.v.chartView->chart() : nullptr;
            if (!chart)
                continue;

            // Remove QLineSeries
            if (myApp.v.seriesMap.contains(sigName)) {
                QLineSeries *series = myApp.v.seriesMap.take(sigName);
                if (series) {
                    // Detach X axis if attached
                    if (myApp.v.axisX && series->attachedAxes().contains(myApp.v.axisX))
                        series->detachAxis(myApp.v.axisX);

                    // Detach and remove Y axis if it's specific
                    if (myApp.v.axisYMap.contains(sigName)) {
                        QValueAxis *yAxis = myApp.v.axisYMap.take(sigName);
                        if (yAxis && series->attachedAxes().contains(yAxis)) {
                            series->detachAxis(yAxis);
                            chart->removeAxis(yAxis);
                            delete yAxis;
                        }
                    }

                    chart->removeSeries(series);
                    delete series;
                }
            }

            // Remove signal from any data buffers too
            myApp.v.bufferedPoints.remove(sigName);

            // Finally, remove from list widget
            delete ui->listWidgetCanSignals->takeItem(ui->listWidgetCanSignals->row(listItem));
        }
    }
    else if (current == ui->subTabRxTx)
    {
        QTableWidget *table = ui->tableWidgetTx;

        int selectedRow = table->currentRow();
        if (selectedRow >= 0 && selectedRow < table->rowCount()) {
            table->removeRow(selectedRow);
        } else {
            QMessageBox::warning(this, "Remove Item", "Please select a row to remove.");
        }
    }
}

void MainWindow::on_tableWidgetTx_cellDoubleClicked(int row, int column)
{
    if (row < 0 || column < 0) return;

    if (column == 2)
    {
        QDialog dialog(this);
        dialog.setWindowTitle("Edit CAN Message");
        dialog.resize(400, 125);
        dialog.setStyleSheet(R"(
        QDialog { background-color: black; color: white; }
        QLabel { color: white; }
        QLineEdit { background-color: #222; color: white; border: 1px solid #555; }
        QPushButton { background-color: #444; color: white; border: 1px solid #666; padding: 4px 10px; }
        QPushButton:hover { background-color: #666; }
        )");

        // Msg ID
        QLabel *idLabel = new QLabel("Msg ID 0x(000–7FF):");
        QLineEdit *idEdit = new QLineEdit;
        idEdit->setValidator(new QRegularExpressionValidator(QRegularExpression("^[0-7A-Fa-f]{1,3}$"), this));
        QString fullId = ui->tableWidgetTx->item(row, 0)->text();
        QString hexPart = fullId.trimmed().remove("0x", Qt::CaseInsensitive);
        idEdit->setText(hexPart.toUpper());
        idEdit->setInputMask("HHH;_");

        connect(idEdit, &QLineEdit::textChanged, [=](const QString &text) {
            QString upperText = text.toUpper();
            if (text != upperText) {
                int cursorPos = idEdit->cursorPosition();
                idEdit->setText(upperText);
                idEdit->setCursorPosition(cursorPos);
            }
        });

        // DLC
        QLabel *dlcLabel = new QLabel("DLC (1–8):");
        QSpinBox *dlcSpin = new QSpinBox;
        dlcSpin->setRange(1, 8);
        dlcSpin->setValue(ui->tableWidgetTx->item(row, 1)->text().toInt());

        // Msg Data
        QLabel *dataLabel = new QLabel("Msg Data (hex):");
        QWidget *dataWidget = new QWidget;
        QHBoxLayout *dataLayout = new QHBoxLayout(dataWidget);
        dataLayout->setSpacing(6);
        dataLayout->setContentsMargins(0, 0, 0, 0);

        QVector<QLineEdit*> dataEdits;
        QStringList dataBytes = ui->tableWidgetTx->item(row, 2)->text().split(" ", Qt::SkipEmptyParts);

        auto createHexByteEdit = [&](int index) -> QLineEdit* {
            QLineEdit *byteEdit = new QLineEdit;
            byteEdit->setFixedWidth(30);
            byteEdit->setMaxLength(2);
            byteEdit->setAlignment(Qt::AlignCenter);
            byteEdit->setValidator(new QRegularExpressionValidator(QRegularExpression("^[0-9A-Fa-f]{0,2}$"), &dialog));

            if (index < dataBytes.size())
                byteEdit->setText(dataBytes[index].toUpper());

            connect(byteEdit, &QLineEdit::textChanged, [=](const QString &text) {
                QString upperText = text.toUpper();
                if (text != upperText) {
                    int pos = byteEdit->cursorPosition();
                    byteEdit->setText(upperText);
                    byteEdit->setCursorPosition(pos);
                }
            });

            return byteEdit;
        };

        // Initially add based on current DLC
        for (int i = 0; i < dlcSpin->value(); ++i) {
            QLineEdit *byteEdit = createHexByteEdit(i);
            dataLayout->addWidget(byteEdit);
            dataEdits.append(byteEdit);
        }

        // Adjust number of byte fields when DLC changes
        connect(dlcSpin, QOverload<int>::of(&QSpinBox::valueChanged), &dialog,
                [dataLayout = dataLayout,
                 &dialog,
                 dataWidget = dataWidget,
                 dataBytes = dataBytes,
                 createHexByteEdit,
                 dataEditsPtr = &dataEdits](int newDlc)
                {

                    QLayoutItem *child;
                    while ((child = dataLayout->takeAt(0)) != nullptr) {
                        delete child->widget();
                        delete child;
                    }

                    dataEditsPtr->clear();

                    for (int i = 0; i < newDlc; ++i) {
                        QLineEdit *byteEdit = createHexByteEdit(i);
                        dataLayout->addWidget(byteEdit);
                        dataEditsPtr->append(byteEdit);
                    }

                    dataWidget->updateGeometry();
                });

        // Buttons
        QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

        // Layout
        QFormLayout *layout = new QFormLayout;
        layout->addRow(idLabel, idEdit);
        layout->addRow(dlcLabel, dlcSpin);
        layout->addRow(dataLabel, dataWidget);
        layout->addWidget(buttons);
        dialog.setLayout(layout);
        dataWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        dialog.setFixedSize(dialog.size());

        if (dialog.exec() == QDialog::Accepted)
        {
            QString msgId = idEdit->text().trimmed().toUpper();
            if (msgId.isEmpty() || msgId == "0" || msgId == "00" || msgId == "000") {
                QMessageBox::warning(&dialog, "Invalid Input", "Message ID cannot be empty or zero (000).");
                return;
            }

            ui->tableWidgetTx->item(row, 0)->setText(msgId);
            ui->tableWidgetTx->item(row, 1)->setText(QString::number(dlcSpin->value()));

            QStringList hexValues;
            for (QLineEdit *edit : dataEdits) {
                QString val = edit->text().trimmed().toUpper();
                if (val.isEmpty()) val = "00";
                hexValues << val;
            }

            QString finalDataStr = hexValues.join(" ");
            ui->tableWidgetTx->item(row, 2)->setText(finalDataStr);
            ui->tableWidgetTx->item(row, 3)->setText(QString::number(0));
        }
    }
    else if (column == 0)
    {
        auto *idItem = ui->tableWidgetTx->item(row, 0);
        auto *dlcItem = ui->tableWidgetTx->item(row, 1);
        auto *dataItem = ui->tableWidgetTx->item(row, 2);

        if (!idItem || !dlcItem || !dataItem) {
            QMessageBox::warning(this, "Invalid Row", "Incomplete data in row.");
            return;
        }

        QString msgIdStr = idItem->text().trimmed().toUpper();
        if (msgIdStr.isEmpty() || msgIdStr == "0" || msgIdStr == "00" || msgIdStr == "000") {
            QMessageBox::warning(this, "Invalid Input", "Message ID cannot be empty or zero (000).");
            return;
        }

        int dlc = dlcItem->text().trimmed().toInt();
        QStringList dataList = dataItem->text().trimmed().split(' ', Qt::SkipEmptyParts);
        if (dataList.size() > 8) {
            QMessageBox::warning(this, "Invalid Data", "CAN Data must be at most 8 bytes.");
            return;
        }

        QWidget *cellWidget = ui->tableWidgetTx->cellWidget(row, 4);
        if (cellWidget) {
            QCheckBox *checkBox = cellWidget->findChild<QCheckBox *>();
            if (checkBox && checkBox->isChecked()) {
                qDebug() << "CAN Msg is not allowed to send while checkbox is checked in CycleTime!!";
                return;
            }
        }

        TPCANMsg txMsg;
        txMsg.ID = msgIdStr.toUInt(nullptr, 16);
        txMsg.LEN = static_cast<BYTE>(dlc);
        txMsg.MSGTYPE = PCAN_MESSAGE_STANDARD;

        for (int i = 0; i < dataList.size(); ++i) {
            bool ok;
            uint byteVal = dataList[i].toUInt(&ok, 16);
            if (!ok || byteVal > 0xFF) {
                QMessageBox::warning(this, "Invalid Data", "Invalid byte: " + dataList[i]);
                return;
            }
            txMsg.DATA[i] = static_cast<BYTE>(byteVal);
        }

        TPCANStatus status = CAN_Write(myApp.h.handle, &txMsg);
        if (status == PCAN_ERROR_OK) {
            QTableWidgetItem *countItem = ui->tableWidgetTx->item(row, 3);
            quint64 currentCount = 0;

            if (countItem && !countItem->text().trimmed().isEmpty()) {
                currentCount = countItem->text().trimmed().toULongLong();
            }
            ++currentCount;

            ui->tableWidgetTx->item(row, 3)->setText(QString::number(currentCount));
        } else {
            qDebug() << "Failed to send CAN message";
        }
    }
}

void MainWindow::on_listWidgetCanSignals_itemDoubleClicked(QListWidgetItem *item)
{
    // 'item' is the QListWidgetItem that was double-clicked
    if (!item) {
        QMessageBox::warning(this, tr("Warning"), tr("Select a signal in \"Signals to Plot\" first."));
        return;
    }

    QString sigName = item->text();
    if (!myApp.v.axisYMap.contains(sigName)) {
        QMessageBox::warning(this, tr("Warning"), tr("No Y-axis found for signal \"%1\".").arg(sigName));
        return;
    }

    QValueAxis *yAxis = myApp.v.axisYMap.value(sigName);
    if (!yAxis) return;

    bool okMin = false, okMax = false;
    double currentMin = yAxis->min();
    double currentMax = yAxis->max();

    double newMin = QInputDialog::getDouble(this, tr("Adjust Y-Axis Minimum"), tr("Minimum for \"%1\":").arg(sigName), currentMin, -1e6, 1e6, 2, &okMin);
    if (!okMin) return;

    double newMax = QInputDialog::getDouble(this, tr("Adjust Y-Axis Maximum"), tr("Maximum for \"%1\":").arg(sigName), currentMax, -1e6, 1e6, 2, &okMax);
    if (!okMax) return;

    if (newMax <= newMin) {
        QMessageBox::warning(this, tr("Invalid Range"), tr("Maximum must be greater than Minimum."));
        return;
    }

    yAxis->setRange(newMin, newMax);
}

void MainWindow::startTraceFile()
{
    // Get the current local time for filename and comment display
    QDateTime now = QDateTime::currentDateTime();
    QDateTime nowUtc = QDateTime::currentDateTimeUtc();
    QString timestamp = now.toString("yyyy-MM-dd_HH-mm-ss_zzz");
    QString fileName = QString("%1.trc").arg(timestamp);
    QString filePath = QDir::temp().filePath(fileName);

    myApp.v.traceFile.setFileName(filePath);

    if (myApp.v.traceFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        myApp.v.traceStream.setDevice(&myApp.v.traceFile);

        // Use UTC+5:30 based timestamp
        int utcOffsetSecs = now.offsetFromUtc(); // e.g., 19800 seconds for IST (+5:30)
        qint64 istAdjustedMsecs = now.toMSecsSinceEpoch() + (utcOffsetSecs * 1000);
        double daysSinceEpochIST = istAdjustedMsecs / (1000.0 * 60 * 60 * 24);

        myApp.v.traceMsgCounter = 0;

        myApp.v.traceStream << ";$FILEVERSION=2.1\n";
        myApp.v.traceStream << QString(";$STARTTIME=%1\n").arg(daysSinceEpochIST, 0, 'f', 13);
        myApp.v.traceStream << ";$COLUMNS=N,O,T,B,I,d,R,L,D\n;\n";
        myApp.v.traceStream << ";   Start time: " << nowUtc.toString("dd-MM-yyyy hh:mm:ss.zzz") << "\n";
        myApp.v.traceStream << ";   Generated by PCAN-Explorer v6.6.2.2770\n";
        myApp.v.traceStream << ";-------------------------------------------------------------------------------\n";
        myApp.v.traceStream << ";   Bus  Connection   Net Connection    Protocol  Bit rate\n";
        myApp.v.traceStream << ";   1    Connection1  SEPL_24@pcan_usb  CAN       500 kbit/s\n";
        myApp.v.traceStream << ";-------------------------------------------------------------------------------\n";
        myApp.v.traceStream << ";   Message    Time    Type    ID     Rx/Tx\n";
        myApp.v.traceStream << ";   Number     Offset  |  Bus  [hex]  |  Reserved\n";
        myApp.v.traceStream << ";   |          [ms]    |  |    |      |  |  Data Length Code\n";
        myApp.v.traceStream << ";   |          |       |  |    |      |  |  |    Data [hex] ...\n";
        myApp.v.traceStream << ";   |          |       |  |    |      |  |  |    |\n";
        myApp.v.traceStream << ";---+--- ------+------ +- +- --+----- +- +- +--- +- -- -- -- -- -- -- --\n";

        qDebug() << "Trace file created:" << filePath;
    } else {
        qDebug() << "Failed to open trace file:" << filePath;
    }
}

void MainWindow::recordTraceFile(const TPCANMsg &msg, const TPCANTimestamp &ts)
{
    if (!myApp.v.traceFile.isOpen()) return;

    // Convert timestamp to microseconds
    quint64 timestamp_us = ts.micros +
                           (1000ULL * ts.millis) +
                           (0x100000000ULL * 1000ULL * ts.millis_overflow);

    // Set traceStartTime on first message
    if (myApp.v.traceMsgCounter == 0) {
        myApp.v.traceStartTime = timestamp_us;
    }

    double offset_ms = (timestamp_us - myApp.v.traceStartTime) / 1000.0;
    myApp.v.traceMsgCounter++;

    QTextStream &stream = myApp.v.traceStream;
    stream << QString(" %1    %2 DT 1      %3 Rx - %4    ")
                  .arg(myApp.v.traceMsgCounter, 7)
                  .arg(offset_ms, 10, 'f', 3)
                  .arg(msg.ID, 4, 16, QLatin1Char('0')).toUpper()
                  .arg(msg.LEN, 2);

    for (int i = 0; i < msg.LEN; ++i) {
        stream << QString("%1 ").arg(msg.DATA[i], 2, 16, QLatin1Char('0')).toUpper();
    }

    stream << "\n";
}

void MainWindow::stopTraceFile()
{
    if (myApp.v.traceFile.isOpen()) {
        myApp.v.traceStream.flush();
        myApp.v.traceFile.close();
    }
}

void MainWindow::readTraceFileAndPopulate()
{
    QFile file(myApp.v.trcFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file:" << myApp.v.trcFilePath;
        return;
    }

    myApp.v.traceFileDataList.clear();
    int lineCount = 0;

    while (!file.atEnd())
    {
        QString line = QString::fromUtf8(file.readLine()).trimmed();
        lineCount++;

        // Skip comments and empty lines
        if (line.startsWith(';') || line.isEmpty()) {
            continue;
        }

        static QRegularExpression re("\\s+");
        QStringList parts = line.split(re, Qt::SkipEmptyParts);
        if (parts.size() < 8) {
            qDebug() << "[" << QTime::currentTime().toString("hh:mm:ss.zzz") << "] Skipping malformed line:" << line;
            continue;
        }

        bool ok = false;
        myApp.v.traceFileData.traceMsgCounter = parts[0].toInt(&ok);
        if (!ok) continue;

        double offsetMs = parts[1].toDouble(&ok);
        if (!ok) continue;
        myApp.v.traceFileData.traceTimeMs = static_cast<double>(offsetMs);

        myApp.v.traceFileData.traceMsgId = parts[4].toInt(&ok, 16);
        if (!ok) continue;

        myApp.v.traceFileData.traceMsgDlc = parts[7].toInt(&ok);
        if (!ok || parts.size() < 8 + myApp.v.traceFileData.traceMsgDlc) continue;

        myApp.v.traceFileData.traceMsgData.clear();
        for (int i = 0; i < myApp.v.traceFileData.traceMsgDlc; ++i) {
            int byteVal = parts[8 + i].toInt(&ok, 16);
            if (!ok) break;
            myApp.v.traceFileData.traceMsgData.append(static_cast<char>(byteVal));
        }

        if (ok) {
            myApp.v.traceFileDataList.append(myApp.v.traceFileData);
        }
    }

    file.close();
    qDebug() << "[INFO] Loaded trace entries:" << myApp.v.traceFileDataList.size();

    QStandardItemModel *model = new QStandardItemModel(this);
    model->setHorizontalHeaderLabels(QStringList() << "Msg #" << "Time(ms)" << "Msg Id" << "Msg DLC" << "Msg Data");

    for (const TraceFileData &data : std::as_const(myApp.v.traceFileDataList))
    {
        QList<QStandardItem *> row;
        row << new QStandardItem(QString::number(data.traceTimeMs));
        row << new QStandardItem(QString::number(data.traceTimeMs, 'f', 3));
        row << new QStandardItem(QString("0x%1").arg(data.traceMsgId, 0, 16).toUpper());
        row << new QStandardItem(QString::number(data.traceMsgDlc));

        QString dataStr;
        for (char byte : data.traceMsgData) {
            dataStr += QString("%1 ").arg(static_cast<quint8>(byte), 2, 16, QChar('0')).toUpper();
        }

        row << new QStandardItem(dataStr.trimmed());
        model->appendRow(row);
    }

    ui->tableViewTraceFile->setModel(model);
    ui->tableViewTraceFile->horizontalHeader()->setStretchLastSection(true);
    ui->tableViewTraceFile->resizeColumnsToContents();
    myApp.s.trcFileLoaded = true;
}

void MainWindow::subTabRxTxReceive(const TPCANMsg &msg)
{
    QTableWidget *table = ui->tableWidgetRx;
    uint32_t msgId = msg.ID;
    qint64 currentTimestamp = QDateTime::currentMSecsSinceEpoch();

    if (myApp.v.msgIdToRowMap.contains(msgId)) {
        int row = myApp.v.msgIdToRowMap[msgId];

        table->item(row, 1)->setText(QString::number(msg.LEN));

        QString dataStr;
        for (int i = 0; i < msg.LEN; ++i) {
            dataStr += QString("%1 ").arg(msg.DATA[i], 2, 16, QChar('0')).toUpper();
        }
        table->item(row, 2)->setText(dataStr.trimmed());

        int count = ++myApp.v.msgIdToCount[msgId];
        table->item(row, 3)->setText(QString::number(count));

        qint64 lastTime = myApp.v.msgIdToLastTimestamp.value(msgId, currentTimestamp);
        double cycleTime = static_cast<double>(currentTimestamp - lastTime);
        table->item(row, 4)->setText(QString("%1").arg(cycleTime, 0, 'f', 2));

        myApp.v.msgIdToLastTimestamp[msgId] = currentTimestamp;
    } else {
        int newRow = table->rowCount();
        table->insertRow(newRow);

        table->setItem(newRow, 0, new QTableWidgetItem(QString("0x%1").arg(msgId, 0, 16).toUpper()));
        table->setItem(newRow, 1, new QTableWidgetItem(QString::number(msg.LEN)));

        QString dataStr;
        for (int i = 0; i < msg.LEN; ++i) {
            dataStr += QString("%1 ").arg(msg.DATA[i], 2, 16, QChar('0')).toUpper();
        }
        table->setItem(newRow, 2, new QTableWidgetItem(dataStr.trimmed()));

        table->setItem(newRow, 3, new QTableWidgetItem("1"));
        table->setItem(newRow, 4, new QTableWidgetItem("0"));

        myApp.v.msgIdToRowMap[msgId] = newRow;
        myApp.v.msgIdToLastTimestamp[msgId] = currentTimestamp;
        myApp.v.msgIdToCount[msgId] = 1;
    }
}

void MainWindow::on_pushButtonTraceFileBrowse_clicked()
{
    const QString fileFilter = tr("Trace Files (*.trc)");

    // Start browsing from workspacePath if valid, else fallback to Documents/home
    QString startDir = workspacePath;
    if (startDir.isEmpty() || !QDir(startDir).exists()) {
#ifdef Q_OS_WIN
        startDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
#else
        startDir = QDir::homePath();
#endif
    }

    // Open file dialog
    QString fileName = QFileDialog::getOpenFileName(this, tr("Select trace file"), startDir, fileFilter);

    if (fileName.isEmpty())
        return;

    // Ensure the file has a .trc extension
    if (!fileName.endsWith(".trc", Qt::CaseInsensitive)) {
        QMessageBox::warning(this, tr("Error"), tr("Please select a valid .trc file."));
        return;
    }

    qDebug() << "Selected trace file:" << fileName;

    ui->lineEditTraceFileUpload->setText(fileName);
    myApp.v.trcFilePath = fileName;

    // Populate data from the trace file
    readTraceFileAndPopulate();

    saveAppData(workspacePath + "/" + APP_WORKSPACE_FILE_NAME);
}

void MainWindow::on_pushButtonTraceFileLoad_clicked()
{
    // Check TRC path
    if (myApp.v.trcFilePath.isEmpty()) {
        ui->pushButtonTraceRePlayStartStop->setEnabled(false);
        QMessageBox::warning(this, tr("Error"), tr("No TRC file selected. Please upload a TRC file first."));
        return;
    } else {
        if (myApp.v.trcFilePath.endsWith(".trc", Qt::CaseInsensitive) && QFile::exists(myApp.v.trcFilePath))
        {
            if (!myApp.s.rxRunning)
            {
                readTraceFileAndPopulate();
                ui->pushButtonTraceRePlayStartStop->setEnabled(true);
                QMessageBox::information(this, tr("Information"), tr("TRC file load successfully."));
                ui->pushButtonTraceFileLoad->setEnabled(true);
            }
        }
    }

    saveAppData(workspacePath + "/" + APP_WORKSPACE_FILE_NAME);
}

void MainWindow::on_pushButtonTraceRePlayStartStop_clicked()
{
    // Check if device is connected first
    if (!myApp.s.deviceConnected)
    {
        QMessageBox::warning(this, "Device Not Connected", "Please connect the PCAN device before starting transmission.");
        return;
    }

    if (!myApp.s.trcRunning)
    {
        // Check TRC path
        if (myApp.v.trcFilePath.isEmpty()) {
            ui->pushButtonTraceRePlayStartStop->setEnabled(false);
            QMessageBox::warning(this, tr("Error"), tr("No TRC file selected. Please upload a TRC file first."));
            return;
        }

        myApp.DeviceBufferReset();
        resetPlotChart();

        myApp.s.trcRunning = true;
        myApp.s.rxRunning = false;
        myApp.s.txRunning = false;
        myApp.s.trcPaused = false;

        ui->pushButtonRxStartStop->setEnabled(false);
        ui->pushButtonRxPauseResume->setEnabled(false);
        ui->pushButtonTxStartStop->setEnabled(false);
        ui->pushButtonTxPauseResume->setEnabled(false);
        ui->pushButtonTraceFileBrowse->setEnabled(false);
        ui->pushButtonTraceFileLoad->setEnabled(false);

        //ui->pushButtonTraceRePlayStartStop->setText("TRC Stop");
        ui->pushButtonTraceRePlayStartStop->setStyleSheet("background-color: red; color: white; border-radius: 6px; padding: 8px 16px;");
        ui->pushButtonTraceRePlayStartStop->setIcon(QIcon::fromTheme("media-playback-stop"));

        ui->pushButtonTraceRePlayPauseResume->setEnabled(true);
        //ui->pushButtonTraceRePlayPauseResume->setText("TRC Pause");
        ui->pushButtonTraceRePlayPauseResume->setStyleSheet("background-color: orange; color: white; border-radius: 6px; padding: 8px 16px;");
        ui->pushButtonTraceRePlayPauseResume->setIcon(QIcon::fromTheme("media-playback-pause"));

        myApp.v.chartUpdateTimer->start(200);

        // === Reader Thread ===
        myApp.v.trcRePlayProducerFuture = QtConcurrent::run([this]() {
            const QList<TraceFileData> &traceList = myApp.v.traceFileDataList;
            if (traceList.isEmpty()) {
                qWarning() << "Trace file is empty OR not applied!";
                return;
            }

            const qint64 startWallClockMs = QDateTime::currentMSecsSinceEpoch();
            const qint64 startTraceTimeMs = static_cast<qint64>(traceList.first().traceTimeMs);
            qint64 totalPauseTimeMs = 0;
            qint64 lastPauseStartMs = 0;

            for (int i = 0; i < traceList.size(); ++i)
            {
                if (!myApp.s.trcRunning) {
                    break;
                }

                while (myApp.s.trcPaused && myApp.s.trcRunning) {
                    if (lastPauseStartMs == 0)
                        lastPauseStartMs = QDateTime::currentMSecsSinceEpoch();
                    QThread::msleep(50);
                }

                if (lastPauseStartMs != 0) {
                    totalPauseTimeMs += QDateTime::currentMSecsSinceEpoch() - lastPauseStartMs;
                    lastPauseStartMs = 0;
                }

                const TraceFileData &data = traceList[i];
                qint64 currentTraceMs = static_cast<qint64>(data.traceTimeMs);
                qint64 relDelayMs = qMax(currentTraceMs - startTraceTimeMs, 0LL);
                qint64 targetWallTimeMs = startWallClockMs + relDelayMs + totalPauseTimeMs;

                while (QDateTime::currentMSecsSinceEpoch() < targetWallTimeMs) {
                    QThread::usleep(500);
                }

                TPCANMsg msg;
                TPCANTimestamp ts = {0};
                qint64 timestampInMs = currentTraceMs;

                ts.millis = static_cast<DWORD>(currentTraceMs & 0xFFFFFFFF);
                ts.millis_overflow = static_cast<WORD>((static_cast<quint64>(currentTraceMs) >> 32) & 0xFFFF);
                ts.micros = 0;

                msg.ID = static_cast<DWORD>(data.traceMsgId);
                msg.MSGTYPE = (msg.ID > 0x7FF) ? PCAN_MESSAGE_EXTENDED : PCAN_MESSAGE_STANDARD;
                msg.LEN = static_cast<BYTE>(data.traceMsgDlc);
                memset(msg.DATA, 0, sizeof(msg.DATA));
                for (int j = 0; j < msg.LEN && j < 8; ++j) {
                    msg.DATA[j] = static_cast<BYTE>(data.traceMsgData[j]);
                }

                TPCANStatus status = CAN_Write(myApp.h.handle, &msg);
                if (status != PCAN_ERROR_OK) {
                    qWarning() << "CAN_Write failed at index" << i << "Status:" << status;
                    break;
                }

                QByteArray canData(reinterpret_cast<char*>(msg.DATA), msg.LEN);
                QList<QPair<QString, double>> decodedSignals = myDBC.decodeFrame(msg.ID, canData);

                QMutexLocker locker(&queueMutex);
                canMessageQueue.enqueue(CANMessageData{msg, ts, decodedSignals, timestampInMs});
                queueNotEmpty.wakeOne();

                // Highlight current trace row
                QMetaObject::invokeMethod(this, [this, i]() {
                    auto *model = ui->tableViewTraceFile->model();
                    if (!model || i >= model->rowCount()) return;
                    QModelIndex idx = model->index(i, 0);
                    ui->tableViewTraceFile->selectionModel()->select(
                        idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
                    ui->tableViewTraceFile->scrollTo(idx);
                }, Qt::QueuedConnection);
            }

            myApp.s.trcRunning = false;
            myApp.s.trcPaused = false;
            consumerThreadRunning = false;
            queueNotEmpty.wakeAll();
        });

        // === Consumer Thread ===
        consumerThreadRunning = true;
        myApp.v.trcRePlayCosumerFuture = QtConcurrent::run([this]() {
            QMap<QString, qint64> lastTimestamps;
            QMap<QString, double> lastSignalValues;

            while (consumerThreadRunning) {
                queueMutex.lock();
                while (canMessageQueue.isEmpty() && consumerThreadRunning)
                    queueNotEmpty.wait(&queueMutex);

                if (!consumerThreadRunning) {
                    queueMutex.unlock();
                    break;
                }

                CANMessageData msgData = canMessageQueue.dequeue();
                queueMutex.unlock();

                QMetaObject::invokeMethod(this, [=, &lastTimestamps, &lastSignalValues]() mutable {
                    updateCanMessageUI(msgData, lastTimestamps, lastSignalValues);
                }, Qt::QueuedConnection);
            }

            QMetaObject::invokeMethod(this, [this]() {
                consumerThreadRunning = false;
                myApp.s.rxRunning = false;
                myApp.s.txRunning = false;
                myApp.s.trcRunning = false;
                myApp.s.trcPaused = false;

                ui->pushButtonRxStartStop->setEnabled(true);
                ui->pushButtonTxStartStop->setEnabled(true);
                ui->pushButtonTraceFileBrowse->setEnabled(true);
                ui->pushButtonTraceFileLoad->setEnabled(true);

                //ui->pushButtonTraceRePlayStartStop->setText("TRC Start");
                ui->pushButtonTraceRePlayStartStop->setStyleSheet("background-color: green; color: white; border-radius: 6px; padding: 8px 16px;");
                ui->pushButtonTraceRePlayStartStop->setIcon(QIcon::fromTheme("media-playback-start"));

                ui->pushButtonTraceRePlayPauseResume->setEnabled(false);
                //ui->pushButtonTraceRePlayPauseResume->setText("TRC Pause");
                //ui->pushButtonTraceRePlayPauseResume->setStyleSheet("");
                ui->pushButtonTraceRePlayPauseResume->setIcon(QIcon::fromTheme("media-playback-pause"));

            }, Qt::QueuedConnection);
        });

    } else {
        consumerThreadRunning = false;
        myApp.s.trcRunning = false;
        myApp.s.trcPaused = false;

        ui->pushButtonRxStartStop->setEnabled(true);
        ui->pushButtonTxStartStop->setEnabled(true);
        ui->pushButtonTraceFileBrowse->setEnabled(true);
        ui->pushButtonTraceFileLoad->setEnabled(true);

        //ui->pushButtonTraceRePlayStartStop->setText("TRC Start");
        ui->pushButtonTraceRePlayStartStop->setStyleSheet("background-color: green; color: white; border-radius: 6px; padding: 8px 16px;");
        ui->pushButtonTraceRePlayStartStop->setIcon(QIcon::fromTheme("media-playback-start"));

        ui->pushButtonTraceRePlayPauseResume->setEnabled(false);
        //ui->pushButtonTraceRePlayPauseResume->setText("TRC Pause");
        ui->pushButtonTraceRePlayPauseResume->setStyleSheet("background-color: gray; color: white; border-radius: 6px; padding: 8px 16px;");
        ui->pushButtonTraceRePlayPauseResume->setIcon(QIcon::fromTheme("media-playback-pause"));

        // Clear CAN values from UI
        for (int i = 0; i < ui->treeWidgetCanMessage->topLevelItemCount(); ++i) {
            QTreeWidgetItem *item = ui->treeWidgetCanMessage->topLevelItem(i);
            if (item) item->setText(4, QString::number(0));
        }

        myApp.v.bufferedPoints.clear();
        myApp.v.chartUpdateTimer->stop();

        if (myApp.v.axisX) {
            QDateTime now = QDateTime::currentDateTime();
            myApp.v.axisX->setRange(now.addSecs(-30), now);
        }

        myApp.s.rxRunning = false;
        myApp.s.txRunning = false;
    }
}

void MainWindow::on_pushButtonTraceRePlayPauseResume_clicked()
{
    if (!myApp.s.trcRunning)
        return;

    myApp.s.trcPaused = !myApp.s.trcPaused;

    if (myApp.s.trcPaused) {
        //ui->pushButtonTraceRePlayPauseResume->setText("TRC Resume");
        ui->pushButtonTraceRePlayPauseResume->setStyleSheet("background-color: gray; color: white; border-radius: 6px; padding: 8px 16px;");
        ui->pushButtonTraceRePlayPauseResume->setIcon(QIcon::fromTheme("media-playback-start"));
        qDebug() << "⏸️ TRC Paused";
    } else {
        //ui->pushButtonTraceRePlayPauseResume->setText("TRC Pause");
        ui->pushButtonTraceRePlayPauseResume->setStyleSheet("background-color: orange; color: white; border-radius: 6px; padding: 8px 16px;");
        ui->pushButtonTraceRePlayPauseResume->setIcon(QIcon::fromTheme("media-playback-pause"));
        qDebug() << "▶️ TRC Resumed";
    }
}

void MainWindow::on_pushButtonTraceRecordStartStop_clicked()
{
    if (!myApp.s.deviceConnected)
    {
        QMessageBox::warning(this, "Device Not Connected",
                             "Please connect the PCAN device before starting trace recording.");
        return;
    }

    myApp.s.trcRecording = !myApp.s.trcRecording;

    if (myApp.s.trcRecording) {
        myApp.v.traceFlickerTimer->start(250);
        //ui->pushButtonTraceRecordStartStop->setText("TRC Stop");
        ui->pushButtonTraceRecordStartStop->setIcon(QIcon::fromTheme("media-playback-stop"));
        qDebug() << "🔴 TRC started";

        if (!myApp.v.traceFile.isOpen()) {
            startTraceFile();
        }
    } else {
        myApp.s.trcRecording = false;
        myApp.v.traceFlickerTimer->stop();
        //ui->pushButtonTraceRecordStartStop->setText("TRC Start");
        ui->pushButtonTraceRecordStartStop->setIcon(QIcon::fromTheme("media-record"));
        qDebug() << "⏹️ TRC stopped";

        if (myApp.v.traceFile.isOpen()) {
            stopTraceFile();
        }
    }
}

void MainWindow::on_pushButtonDeviceConnectDisconnect_clicked()
{
    static QTimer *canStatusTimer = nullptr;  
    deviceDialog dlg(this);

    if (!myApp.s.deviceConnected)
    {
        // ---- CONNECT ----
        ui->pushButtonDeviceConfigure->setEnabled(false);
        qDebug() << "🔌 Attempting device connection...";
        myApp.DeviceDisconnect();

        int status = myApp.DeviceConnect();
        if (status == PCAN_ERROR_OK)
        {
            myApp.s.deviceConnected = true;

            ui->labelDeviceName->setText(dlg.getPcanDeviceName(myApp.h.handle));
            ui->labelDeviceBitRate->setText(dlg.getPcanBitrateString(myApp.h.bitRate));
            ui->labelConnectionStatus->setText("Connected");
            ui->labelConnectionStatus->setStyleSheet(
                R"(QLabel {background-color: #22C55E; color: #FFFFFF})");

            // Enable main operation buttons
            ui->pushButtonDeviceConnectDisconnect->setText("Disconnect");
            ui->pushButtonTxStartStop->setEnabled(true);
            ui->pushButtonRxStartStop->setEnabled(true);
            ui->pushButtonTraceRePlayStartStop->setEnabled(true);
            ui->pushButtonTraceRecordStartStop->setEnabled(true);

            qDebug() << "✅ PCAN connected successfully.";

            // Start CAN bus status monitor every 1s
            if (!canStatusTimer)
                canStatusTimer = new QTimer(this);

            connect(canStatusTimer, &QTimer::timeout, this, [this]() {
                TPCANStatus st = CAN_GetStatus(myApp.h.handle);
                if (st == PCAN_ERROR_OK) {
                    ui->labelDeviceBusErrorStatus->setText("Bus OK");
                    ui->labelDeviceBusErrorStatus->setStyleSheet(
                        R"(QLabel {background-color: #22C55E; color: #FFFFFF})");
                } else {
                    QString desc = myApp.DeviceGetErrorDescription(st);

                    // Optionally interpret bus-specific bits
                    if (st & PCAN_ERROR_BUSLIGHT) {
                        ui->labelDeviceBusErrorStatus->setText("Bus Light Warning");
                    } else if (st & PCAN_ERROR_BUSHEAVY) {
                        ui->labelDeviceBusErrorStatus->setText("Bus Heavy");
                    } else if (st & PCAN_ERROR_BUSOFF) {
                        ui->labelDeviceBusErrorStatus->setText("Bus Off");
                    } else {
                        //ui->labelDeviceBusErrorStatus->setText(QString("Error: 0x%1 (%2)").arg(st, 0, 16).toUpper().arg(desc));
                    }

                    ui->labelDeviceBusErrorStatus->setStyleSheet(
                        R"(QLabel {background-color: #EF4444; color: #FFFFFF})");
                    qDebug() << "⚠️ Bus error detected:" << QString("0x%1").arg(st, 0, 16)
                             << "-" << desc;
                }
            });

            canStatusTimer->start(1000); // check every 1 second
        }
        else
        {
            myApp.s.deviceConnected = false;
            ui->labelConnectionStatus->setText("Connection Failed");
            ui->labelConnectionStatus->setStyleSheet(
                R"(QLabel {background-color: #EF4444; color: #FFFFFF})");

            QString desc = myApp.DeviceGetErrorDescription(status);
            QMessageBox::critical(
                this,
                "PCAN Connection Error",
                QString("Failed to connect to PCAN device.\nError Code: 0x%1\nDescription: %2")
                    .arg(status, 0, 16).toUpper()
                    .arg(desc)
                );
            qDebug() << "❌ PCAN connection failed with error code:"
                     << QString("0x%1").arg(status, 0, 16) << "-" << desc;
        }
    }
    else
    {
        // ---- DISCONNECT ----
        ui->pushButtonDeviceConfigure->setEnabled(true);
        qDebug() << "🔌 Disconnecting device and resetting all running threads...";

        // Stop background CAN monitor
        if (canStatusTimer && canStatusTimer->isActive())
            canStatusTimer->stop();

        ui->labelDeviceBusErrorStatus->setText("NO ERROR");
        ui->labelDeviceBusErrorStatus->setStyleSheet(
            R"(QLabel {background-color: #9CA3AF; color: #FFFFFF})");

        // === Stop all threads and background operations ===
        myApp.s.txRunning = myApp.s.rxRunning = myApp.s.trcRunning = false;
        myApp.s.txPaused = myApp.s.rxPaused = myApp.s.trcPaused = false;
        consumerThreadRunning = false;

        queueNotEmpty.wakeAll();
        myApp.v.RxProducerFuture.waitForFinished();
        myApp.v.RxConsumerFuture.waitForFinished();

        // === Stop charts and clear data ===
        myApp.v.bufferedPoints.clear();
        if (myApp.v.chartUpdateTimer) {
            myApp.v.chartUpdateTimer->stop();
        }

        if (myApp.v.axisX) {
            QDateTime now = QDateTime::currentDateTime();
            myApp.v.axisX->setRange(now.addSecs(-30), now);
        }

        // === Clear CAN data in UI ===
        for (int i = 0; i < ui->treeWidgetCanMessage->topLevelItemCount(); ++i) {
            QTreeWidgetItem *item = ui->treeWidgetCanMessage->topLevelItem(i);
            if (item) item->setText(4, QString::number(0));
        }

        // === Default button styles ===
        QString defaultButtonStyle = R"(
            QPushButton {
                background-color: #14B8A6; color: #FFFFFF;
                border-radius: 6px; padding: 8px 16px;
            }
            QPushButton:hover { background-color: #0D9488; }
            QPushButton:pressed { background-color: #0F766E; }
            QPushButton:disabled { background-color: #4B5563; color: #A1A1AA; }
        )";

        QString defaultRcdButtonStyle = R"(
            QPushButton {
                background-color: #EF4444; color: #FFFFFF;
                border-radius: 6px; padding: 8px 16px;
            }
            QPushButton:hover { background-color: #DC2626; }
            QPushButton:pressed { background-color: #B91C1C; }
            QPushButton:disabled { background-color: #4B5563; color: #A1A1AA; }
        )";

        // === Reset all operation buttons ===
        ui->pushButtonTxStartStop->setStyleSheet(defaultButtonStyle);
        ui->pushButtonTxStartStop->setIcon(QIcon::fromTheme("media-playback-start"));
        ui->pushButtonTxPauseResume->setEnabled(false);
        ui->pushButtonTxPauseResume->setStyleSheet(defaultButtonStyle);

        ui->pushButtonRxStartStop->setStyleSheet(defaultButtonStyle);
        ui->pushButtonRxStartStop->setIcon(QIcon::fromTheme("media-playback-start"));
        ui->pushButtonRxPauseResume->setEnabled(false);
        ui->pushButtonRxPauseResume->setStyleSheet(defaultButtonStyle);

        ui->pushButtonTraceRePlayStartStop->setStyleSheet(defaultButtonStyle);
        ui->pushButtonTraceRePlayStartStop->setIcon(QIcon::fromTheme("media-playback-start"));
        ui->pushButtonTraceRePlayPauseResume->setEnabled(false);
        ui->pushButtonTraceRePlayPauseResume->setStyleSheet(defaultButtonStyle);

        // === Stop Trace Recording ===
        if (myApp.s.trcRecording) {
            myApp.s.trcRecording = false;
            if (myApp.v.traceFlickerTimer)
                myApp.v.traceFlickerTimer->stop();
            if (myApp.v.traceFile.isOpen())
                stopTraceFile();
            qDebug() << "⏹️ Trace recording stopped due to disconnect.";
        }

        ui->pushButtonTraceRecordStartStop->setStyleSheet(defaultRcdButtonStyle);
        ui->pushButtonTraceRecordStartStop->setIcon(QIcon::fromTheme("media-record"));
        ui->pushButtonTraceRecordStartStop->setEnabled(false);

        // === Disconnect PCAN ===
        int status = myApp.DeviceDisconnect();
        if (status == PCAN_ERROR_OK)
        {
            myApp.s.deviceConnected = false;
            //ui->labelDeviceName->setText(dlg.getPcanDeviceName(0x0));
            //ui->labelDeviceBitRate->setText(dlg.getPcanBitrateString(0x0));
            ui->labelConnectionStatus->setText("Disconnected");
            ui->labelConnectionStatus->setStyleSheet(
                R"(QLabel {background-color: #EF4444; color: #FFFFFF})");

            qDebug() << "✅ PCAN disconnected successfully.";
        }
        else
        {
            myApp.s.deviceConnected = true;
            QString desc = myApp.DeviceGetErrorDescription(status);
            QMessageBox::critical(
                this,
                "PCAN Disconnection Error",
                QString("Failed to disconnect from PCAN device.\nError Code: 0x%1\nDescription: %2")
                    .arg(status, 0, 16).toUpper()
                    .arg(desc)
                );
            qDebug() << "❌ PCAN disconnection failed with error code:"
                     << QString("0x%1").arg(status, 0, 16) << "-" << desc;
        }

        // === Disable operational buttons until reconnect ===
        ui->pushButtonTxStartStop->setEnabled(false);
        ui->pushButtonRxStartStop->setEnabled(false);
        ui->pushButtonTraceRePlayStartStop->setEnabled(false);
        ui->pushButtonTraceRecordStartStop->setEnabled(false);

        // === Restore connect button text ===
        ui->pushButtonDeviceConnectDisconnect->setText("Connect");
    }
}

void MainWindow::on_pushButtonDeviceConfigure_clicked()
{
    deviceDialog dlg(this);

    // Load existing configuration before showing dialog
    loadAppData(workspacePath+ "/" + APP_WORKSPACE_FILE_NAME);

    // Preload values into dialog (from myApp)
    dlg.loadConfiguration(myApp.h.handle, myApp.h.bitRate);

    // Connect signal from dialog to saveAppData() in MainWindow
    connect(&dlg, &deviceDialog::configurationUpdated, this, [this](TPCANHandle handle, TPCANBaudrate bitrate) {
        myApp.h.handle = handle;
        myApp.h.bitRate = bitrate;

        saveAppData(workspacePath + "/" + APP_WORKSPACE_FILE_NAME);

    });

    dlg.setModal(true);
    dlg.exec();
}

