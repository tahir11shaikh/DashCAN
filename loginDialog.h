#pragma once

#include <QDialog>
#include <QMessageBox>
#include <QJsonObject>
#include <QJsonDocument>
#include "ui_loginDialog.h"

class MainDialog : public QDialog {
    Q_OBJECT

public:
    explicit MainDialog(QWidget *parent = nullptr);
    ~MainDialog() override;

    bool loadUserData();
    bool saveUserData();

    QString userName() const                { return ui->lineEditName->text(); }
    QString email() const                   { return ui->lineEditEmail->text(); }
    QString company() const                 { return ui->lineEditCompany->text(); }
    QString workspacePath() const           { return ui->lineEditWorkspace->text(); }
    QString usrInfoFilePath() const         { return m_usrInfoFilePath; }
    QString workspaceDataFilePath() const   { return m_workspaceDataFilePath; }

private slots:
    void on_btnValidate_clicked();
    void on_btnBrowseWorkspace_clicked();
    void on_btnLaunch_clicked();
    void on_btnCancel_clicked();

private:
    Ui::MainDialog *ui;
    bool validateUserInfo();

    QString m_usrInfoFilePath;
    QString m_workspaceDataFilePath;
};
