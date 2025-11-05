#include "loginDialog.h"
#include "mainWindow.h"

#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QDateTime>

#ifdef Q_OS_WIN
#include <windows.h>
#include <tlhelp32.h>

// Kill previous instance of the application
void killPreviousRunningApp(const QString &processName)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(entry);

    if (Process32First(snap, &entry)) {
        do {
            if (QString::fromWCharArray(entry.szExeFile).compare(processName, Qt::CaseInsensitive) == 0) {
                DWORD currentPID = GetCurrentProcessId();
                if (entry.th32ProcessID != currentPID) {
                    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, entry.th32ProcessID);
                    if (hProcess) {
                        TerminateProcess(hProcess, 0);
                        CloseHandle(hProcess);
                    }
                }
            }
        } while (Process32Next(snap, &entry));
    }
    CloseHandle(snap);
}
#endif

// Qt message handler
void vLogHandler(QtMsgType, const QMessageLogContext&, const QString &msg)
{
    const QString timeStamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    const QString logLine = QString("[%1] %2").arg(timeStamp, msg);

    // Append to log viewer if exists
    if (auto e = MainWindow::logViewer()) {
        QMetaObject::invokeMethod(e, "appendPlainText", Qt::QueuedConnection, Q_ARG(QString, logLine));
    }
}

int main(int argc, char *argv[])
{
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

#ifdef Q_OS_WIN
    killPreviousRunningApp("DashCAN.exe");
#endif

    QApplication a(argc, argv);
    qInstallMessageHandler(vLogHandler);

    // Load translations
    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "DashCAN_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }

    QPalette lightPalette;
    lightPalette.setColor(QPalette::Window, Qt::white);
    lightPalette.setColor(QPalette::WindowText, Qt::black);
    lightPalette.setColor(QPalette::Base, Qt::white);
    lightPalette.setColor(QPalette::Text, Qt::black);
    lightPalette.setColor(QPalette::Button, QColor(240, 240, 240));
    lightPalette.setColor(QPalette::ButtonText, Qt::black);
    lightPalette.setColor(QPalette::Highlight, QColor(0, 120, 215)); // blue highlight
    lightPalette.setColor(QPalette::HighlightedText, Qt::white);
    a.setPalette(lightPalette);

    // First-time user setup dialog
    MainDialog setupDialog;
    if (setupDialog.exec() != QDialog::Accepted) {
        // User cancelled setup, exit application
        return 0;
    }

    // Access workspace and user info
    QString workspacePath = setupDialog.workspacePath();
    QString userName = setupDialog.userName();
    QString email = setupDialog.email();
    QString company = setupDialog.company();
    Q_UNUSED(workspacePath);
    Q_UNUSED(userName);
    Q_UNUSED(email);
    Q_UNUSED(company);

    // Launch main window with workspace-specific app file
    MainWindow w(setupDialog.workspacePath());
    w.show();

    return a.exec();
}
