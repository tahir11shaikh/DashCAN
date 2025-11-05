#include "loginDialog.h"
#include "ui_loginDialog.h"
#include "mainWindow.h"

#include <QFile>
#include <QDir>
#include <QFileDialog>
#include <QStandardPaths>
#include <QRegularExpression>
#include <windows.h>

static QString userInfoFilePath()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + "/" + APP_USER_INFO_FILE_NAME;
}

static bool isNameValid(const QString &name)
{
    if(name.length() < 3) return false;

    // Only letters and spaces
    QRegularExpression re("^[A-Za-z ]+$");
    if(!re.match(name).hasMatch()) return false;

    // Check for repeated characters (e.g., "AAAA")
    QRegularExpression repeatRe("(.)\\1{2,}");
    if(repeatRe.match(name).hasMatch()) return false;

    // Check that at least one vowel exists
    QRegularExpression vowelRe("[AEIOUaeiou]");
    if(!vowelRe.match(name).hasMatch()) return false;

    // Check for sequential letters (e.g., abc, def, xyz)
    QString lowerName = name.toLower();
    for(int i = 0; i < lowerName.length() - 2; ++i)
    {
        QChar c1 = lowerName[i];
        QChar c2 = lowerName[i+1];
        QChar c3 = lowerName[i+2];

        if(c1.isLetter() && c2.isLetter() && c3.isLetter())
        {
            if(c2.unicode() == c1.unicode() + 1 &&
                c3.unicode() == c2.unicode() + 1)
            {
                return false; // sequential letters detected
            }
        }
    }

    return true;
}

static bool isEmailValid(const QString &email)
{
    // Basic strict pattern
    QRegularExpression re("^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,}$");
    if(!re.match(email).hasMatch())
        return false;

    // Split into local-part and domain
    QStringList parts = email.split('@');
    if(parts.size() != 2) return false;

    QString localPart = parts[0].toLower();
    QString domainPart = parts[1].toLower();

    auto hasSequentialLetters = [](const QString &str) -> bool {
        for(int i = 0; i < str.length() - 2; ++i)
        {
            QChar c1 = str[i];
            QChar c2 = str[i+1];
            QChar c3 = str[i+2];
            if(c1.isLetter() && c2.isLetter() && c3.isLetter())
            {
                if(c2.unicode() == c1.unicode() + 1 &&
                    c3.unicode() == c2.unicode() + 1)
                    return true;
            }
        }
        return false;
    };

    if(hasSequentialLetters(localPart) || hasSequentialLetters(domainPart))
        return false;

    return true;
}

static bool isCompanyValid(const QString &company)
{
    if(company.length() < 3) return false;

    // Only letters, numbers, space, dot, hyphen
    QRegularExpression re("^[A-Za-z0-9 .-]+$");
    if(!re.match(company).hasMatch()) return false;

    // Check for repeated characters (e.g., "AAA")
    QRegularExpression repeatRe("(.)\\1{2,}");
    if(repeatRe.match(company).hasMatch()) return false;

    // Check for sequential letters (e.g., abc, def, xyz)
    QString lowerCompany = company.toLower();
    for(int i = 0; i < lowerCompany.length() - 2; ++i)
    {
        QChar c1 = lowerCompany[i];
        QChar c2 = lowerCompany[i+1];
        QChar c3 = lowerCompany[i+2];

        if(c1.isLetter() && c2.isLetter() && c3.isLetter())
        {
            if(c2.unicode() == c1.unicode() + 1 &&
                c3.unicode() == c2.unicode() + 1)
            {
                return false; // sequential letters detected
            }
        }
    }

    return true;
}

bool MainDialog::validateUserInfo()
{
    QString name = ui->lineEditName->text().trimmed();
    QString email = ui->lineEditEmail->text().trimmed();
    QString company = ui->lineEditCompany->text().trimmed();

    if(!isNameValid(name))
    {
        QMessageBox::warning(this, "Error", "Please enter a valid name (letters only, at least 3 chars).");
        return false;
    }

    if(!isEmailValid(email))
    {
        QMessageBox::warning(this, "Error", "Please enter a valid email address.");
        return false;
    }

    if(!isCompanyValid(company))
    {
        QMessageBox::warning(this, "Error", "Please enter a valid company name (letters/numbers only, at least 3 chars).");
        return false;
    }

    return true;
}

MainDialog::MainDialog(QWidget *parent)
    : QDialog(parent),
    ui(new Ui::MainDialog)
{
    ui->setupUi(this);

    // Initial state: disable workspace buttons
    ui->btnBrowseWorkspace->setEnabled(false);
    ui->btnLaunch->setEnabled(false);
    ui->lineEditWorkspace->setReadOnly(true);

    // Load existing user data if available
    loadUserData();
}

MainDialog::~MainDialog()
{
    delete ui;
}

bool MainDialog::loadUserData()
{
    QFile file(userInfoFilePath());
    if(!file.exists() || !file.open(QIODevice::ReadOnly))
    {
        // File doesn't exist or cannot be opened → clear all fields
        ui->lineEditName->clear();
        ui->lineEditEmail->clear();
        ui->lineEditCompany->clear();
        ui->lineEditWorkspace->clear();

        // Disable workspace buttons
        ui->btnBrowseWorkspace->setEnabled(false);
        ui->btnLaunch->setEnabled(false);

        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if(!doc.isObject())
    {
        // Invalid JSON → clear fields
        ui->lineEditName->clear();
        ui->lineEditEmail->clear();
        ui->lineEditCompany->clear();
        ui->lineEditWorkspace->clear();

        ui->btnBrowseWorkspace->setEnabled(false);
        ui->btnLaunch->setEnabled(false);

        return false;
    }

    QJsonObject obj = doc.object();
    ui->lineEditName->setText(obj.value("userName").toString());
    ui->lineEditEmail->setText(obj.value("email").toString());
    ui->lineEditCompany->setText(obj.value("company").toString());
    ui->lineEditWorkspace->setText(obj.value("workspace").toString());

    return true;
}

bool MainDialog::saveUserData()
{
    QJsonObject obj;
    obj["userName"] = ui->lineEditName->text().trimmed();
    obj["email"] = ui->lineEditEmail->text().trimmed();
    obj["company"] = ui->lineEditCompany->text().trimmed();
    obj["workspace"] = ui->lineEditWorkspace->text().trimmed();

    QJsonDocument doc(obj);
    QFile file(userInfoFilePath());
    if(!file.open(QIODevice::WriteOnly)) return false;

    file.write(doc.toJson());
    file.close();
    return true;
}

void MainDialog::on_btnValidate_clicked()
{
    if(validateUserInfo())
    {
        QMessageBox::information(this, "Validation Check", "User data is valid!");

        // === Ensure Roaming\DashCAN folder exists ===
#ifdef Q_OS_WIN
        QString roamingPath = QDir::homePath() + "/AppData/Roaming/DashCAN";
#else
        QString roamingPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/DashCAN";
#endif

        QDir dir(roamingPath);
        if (!dir.exists())
        {
            if (!dir.mkpath(roamingPath))
            {
                QMessageBox::warning(this, "Error", "Failed to create DashCAN folder in Roaming!");
                return;
            }
        }

        // === Create DashCAN-user-info.json if not exists ===
        QString userInfoFilePath = dir.filePath(APP_USER_INFO_FILE_NAME);
        if (!QFile::exists(userInfoFilePath))
        {
            QFile file(userInfoFilePath);
            if (!file.open(QIODevice::WriteOnly))
            {
                QMessageBox::warning(this, "Error", "Failed to create DashCAN-user-info.json in Roaming!");
                return;
            }
            file.close();

#ifdef Q_OS_WIN
            SetFileAttributes((LPCWSTR)userInfoFilePath.utf16(), FILE_ATTRIBUTE_HIDDEN);
#endif
        }

        // === Save user data into the Roaming file ===
        if (!saveUserData())
        {
            QMessageBox::warning(this, "Error", "Failed to save user data into DashCAN-user-info.json!");
            return;
        }

        ui->btnBrowseWorkspace->setEnabled(true);
        if(!ui->lineEditWorkspace->text().isEmpty())
            ui->btnLaunch->setEnabled(true);
    }
    else
    {
        QMessageBox::warning(this, "Validation Failed", "Please fill all required fields correctly.");
    }
}

void MainDialog::on_btnBrowseWorkspace_clicked()
{
    QString initialDir = ui->lineEditWorkspace->text().trimmed();

    if (initialDir.isEmpty() || !QDir(initialDir).exists())
    {
#ifdef Q_OS_WIN
        initialDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
#else
        initialDir = QDir::homePath();
#endif
    }

    QString dir = QFileDialog::getExistingDirectory(this, "Select Workspace", initialDir);
    if(!dir.isEmpty())
    {
        ui->lineEditWorkspace->setText(dir);
        ui->btnLaunch->setEnabled(true);
    }
}

void MainDialog::on_btnLaunch_clicked()
{
    QString workspacePath = ui->lineEditWorkspace->text().trimmed();

    if(workspacePath.isEmpty())
    {
        QMessageBox::warning(this, "Warning", "Please select a workspace first!");
        return;
    }

    QDir dir(workspacePath);
    if(!dir.exists())
    {
        QMessageBox::warning(this, "Warning", "Workspace directory does not exist!");
        return;
    }

    // Create workspace data file if missing
    m_workspaceDataFilePath = dir.filePath(APP_WORKSPACE_FILE_NAME);
    if(!QFile::exists(m_workspaceDataFilePath))
    {
        QFile appFile(m_workspaceDataFilePath);
        if(!appFile.open(QIODevice::WriteOnly))
        {
            QMessageBox::warning(this, "Error", "Failed to create DashCAN-workspace-data.json!");
            return;
        }
        appFile.close();

#ifdef Q_OS_WIN
        SetFileAttributes((LPCWSTR)m_workspaceDataFilePath.utf16(), FILE_ATTRIBUTE_HIDDEN);
#endif
    }

    // Copy DashCAN-user-info.json from Roaming to workspace
#ifdef Q_OS_WIN
    QString userFileSrc = QDir::homePath() + "/AppData/Roaming/DashCAN/" + APP_USER_INFO_FILE_NAME;
#else
    QString userFileSrc = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/DashCAN/" + APP_USER_INFO_FILE_NAME;
#endif

    m_usrInfoFilePath = dir.filePath(APP_USER_INFO_FILE_NAME);

    if(QFile::exists(userFileSrc))
    {
        if(QFile::exists(m_usrInfoFilePath))
            QFile::remove(m_usrInfoFilePath);

        if(!QFile::copy(userFileSrc, m_usrInfoFilePath))
        {
            QMessageBox::warning(this, "Error", "Failed to copy DashCAN-user-info.json to workspace!");
            return;
        }

#ifdef Q_OS_WIN
        SetFileAttributes((LPCWSTR)m_usrInfoFilePath.utf16(), FILE_ATTRIBUTE_HIDDEN);
#endif
    }
    else
    {
        QMessageBox::warning(this, "Error", "Source DashCAN-user-info.json not found in Roaming!");
        return;
    }

    // Save user data before closing
    if(saveUserData())
    {
        accept();
    }
    else
    {
        QMessageBox::warning(this, "Error", "Failed to save user data!");
    }
}

void MainDialog::on_btnCancel_clicked()
{
    reject();
}
