#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <CanApp.h>

// User information JSON file (stored in Roaming or user config)
#define APP_USER_INFO_FILE_NAME   "DashCAN-user-info.json"

// Application JSON data file (workspace-specific)
#define APP_WORKSPACE_FILE_NAME   "DashCAN-workspace-data.json"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const QString &workspacePath, QWidget *parent = nullptr);
    ~MainWindow();
    static QPlainTextEdit* logViewer();

    void setupPlotChart(void);
    void resetPlotChart();
    void repositionPanelWidgets();
    void onChartUpdateTimer();
    bool eventFilter(QObject *obj, QEvent *ev) override;

    void startTraceFile();
    void recordTraceFile(const TPCANMsg &msg, const TPCANTimestamp &ts);
    void stopTraceFile();
    void readTraceFileAndPopulate();
    void subTabRxTxReceive(const TPCANMsg &msg);
    void updateCanMessageUI(const CANMessageData &msgData, QMap<QString, qint64> &lastTimestamps, QMap<QString, double> &lastSignalValues);

private slots:
    void on_pushButtonClearLog_clicked();
    void on_pushButtonSaveLog_clicked();

    void on_pushButtonTxStartStop_clicked();
    void on_pushButtonTxPauseResume_clicked();
    void on_pushButtonRxStartStop_clicked();
    void on_pushButtonRxPauseResume_clicked();
    void on_pushButtonDbcFileBrowse_clicked();
    void on_pushButtonApplyDbc_clicked();
    void on_lineEditFilterItem_textChanged(const QString &arg1);
    void on_pushButtonAddItem_clicked();
    void on_pushButtonRemoveItem_clicked();
    void on_listWidgetCanSignals_itemDoubleClicked(QListWidgetItem *item);
    void on_tableWidgetTx_cellDoubleClicked(int row, int column);
    void on_pushButtonTraceFileBrowse_clicked();
    void on_pushButtonTraceFileLoad_clicked();
    void on_pushButtonTraceRePlayStartStop_clicked();
    void on_pushButtonTraceRePlayPauseResume_clicked();
    void on_pushButtonTraceRecordStartStop_clicked();

    void on_pushButtonDeviceConnectDisconnect_clicked();
    void on_pushButtonDeviceConfigure_clicked();

protected:
    void closeEvent(QCloseEvent *event) override;
    void saveAppData(const QString &filePath);
    void loadAppData(const QString &filePath);

private:
    Ui::MainWindow *ui;
    static MainWindow *instance;
    static QPlainTextEdit *s_logViewer;
    QString workspacePath;
    QString currentHoveredSignal;
    QMutex threadMutex;
    QMutex queueMutex;
    QWaitCondition queueNotEmpty;
    QQueue<CANMessageData> canMessageQueue;
    std::atomic<bool> consumerThreadRunning{false};
};

#endif // MAINWINDOW_H
