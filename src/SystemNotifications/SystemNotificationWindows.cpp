#include "SystemNotificationWindows.h"
#include <QDebug>
#include <QRegExp>
#include <QSysInfo>

#include "RequestLoginNameDialog.h"
#include "RequestDomainSelectionDialog.h"

const QString SystemNotificationWindows::SNORETOAST_FORMAT= "SnoreToast.exe -t \"%1\" -m \"%2\" %3 %4 -p icon.png %5";
const QString SystemNotificationWindows::WINDOWS10_VERSION = "10";

SystemNotificationWindows::SystemNotificationWindows(QObject *parent)
    : ISystemNotification(parent)
{
    process = new QProcess();
    notificationMap = new NotificationMap();
    messageMap = new MessageMap();
}

SystemNotificationWindows::~SystemNotificationWindows()
{
    delete process;
    for (auto &proc : notificationMap->values())
    {
        if (proc != nullptr)
        {
            delete proc;
        }
    }
    if (notificationMap != nullptr)
    {
        delete notificationMap;
    }

    if (messageMap != nullptr)
    {
        delete messageMap;
    }
}

void SystemNotificationWindows::createNotification(const QString &title, const QString text)
{
    QString notification = SNORETOAST_FORMAT.arg(title, text, "", "", "");
    qDebug() << notification;
    process->start(notification);
}

void SystemNotificationWindows::createButtonChoiceNotification(const QString &title, const QString text, const QStringList &buttons)
{
    if (buttons.empty())
    {
        qDebug() << "createButtonChoiceNotification called without button text";
        return;
    }
    QString buttonString = "-b \"";
    for (const auto &piece : buttons)
    {
        buttonString += piece + ";";
    }

    buttonString = buttonString.replace(buttonString.size()-1, 1, "\"");

    QProcess *proc = new QProcess();
    notificationMap->insert(notificationId, proc);
    QString notification = SNORETOAST_FORMAT.arg(title, text, buttonString, "-id " + QString::number(notificationId++), "-w");
    connect(proc, static_cast<CallbackType>(&QProcess::finished), this, &SystemNotificationWindows::callbackFunction);
    qDebug() << notification;
    proc->start(notification);
}

void SystemNotificationWindows::createTextBoxNotification(const QString &title, const QString text)
{
    QProcess *proc = new QProcess();
    notificationMap->insert(notificationId, proc);
    QString notification = SNORETOAST_FORMAT.arg(title, text, "-tb", "-id " + QString::number(notificationId++), "-w");
    connect(proc, static_cast<CallbackType>(&QProcess::finished), this, &SystemNotificationWindows::callbackFunction);
    proc->start(notification);
}

bool SystemNotificationWindows::displayLoginRequestNotification(const QString &service, QString &loginName, QString message)
{
    if (QSysInfo::productVersion() == WINDOWS10_VERSION)
    {
        // A text box notification is displayed on Win10
        messageMap->insert(notificationId, message);
        createTextBoxNotification(tr("A credential without a login has been detected."), tr("Login name for ") + service + ":");
        return false;
    }
    else
    {
        // A RequestLoginNameDialog is displayed on bellow Win10
        bool isSuccess = false;
        RequestLoginNameDialog dlg(service);
        isSuccess = (dlg.exec() != QDialog::Rejected);
        loginName = dlg.getLoginName();
        return isSuccess;
    }
}

bool SystemNotificationWindows::displayDomainSelectionNotification(const QString &domain, const QString &subdomain, QString &serviceName, QString message)
{
    if (QSysInfo::productVersion() == WINDOWS10_VERSION)
    {
        // A text box notification is displayed on Win10
        QStringList buttons;
        buttons.append({domain, subdomain});
        messageMap->insert(notificationId, message);
        createButtonChoiceNotification(tr("Subdomain Detected!"), tr("Choose the domain name:"), buttons);
        return false;
    }
    else
    {
        // A RequestLoginNameDialog is displayed on bellow Win10
        bool isSuccess = false;
        RequestDomainSelectionDialog dlg(domain, subdomain);
        isSuccess = (dlg.exec() != QDialog::Rejected);
        serviceName = dlg.getServiceName();
        return isSuccess;
    }
}

bool SystemNotificationWindows::processResult(const QString &toastResponse, QString &result, size_t &id) const
{
    constexpr int RESULT_REGEX_NUM = 2;
    constexpr int ID_REGEX_NUM = 4;
    const QRegExp rx("(Result:\\[)(.*)(\\]\\[)(.*)(\\])(\r\n)");

    if (rx.indexIn(toastResponse) != -1) {
        result = rx.cap(RESULT_REGEX_NUM).trimmed();
        id = rx.cap(ID_REGEX_NUM).toUInt();
        return true;
    }
    return false;
}

void SystemNotificationWindows::callbackFunction(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);
    QString output(static_cast<QProcess*>(QObject::sender())->readAllStandardOutput());
    QString result = "";
    size_t id = 0;
    if (this->processResult(output, result, id))
    {
        if (messageMap->contains(id))
        {
            QString message = (*messageMap)[id];
            if (message.contains("request_login"))
            {
                emit sendLoginMessage(message, result);
            }
            else if (message.contains("request_domain"))
            {
                emit sendDomainMessage(message, result);
            }
            messageMap->remove(id);
        }
    }
    else
    {
        qDebug() << "No result found";
    }
}
