#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QWidget>
#include <QDebug>

#include "presentation/coordinators/authdialogcoordinator.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QTemporaryDir settingsDir;
    QSettings settings(settingsDir.filePath("settings.ini"), QSettings::IniFormat);
    QWidget parent;
    AuthDialogCoordinator coordinator(&parent, &settings);
    QByteArray submitted;
    int submissions = 0;
    int cancellations = 0;
    QObject::connect(&coordinator, &AuthDialogCoordinator::interactiveInputSubmitted,
                     [&](const QByteArray &input)
                     {
                         submitted = input;
                         ++submissions;
                     });
    QObject::connect(&coordinator, &AuthDialogCoordinator::interactiveInputCancelled,
                     [&]() { ++cancellations; });

    coordinator.requestTotpCode();
    coordinator.requestTotpCode();
    const QList<QDialog *> dialogs = parent.findChildren<QDialog *>();
    if (dialogs.size() != 1)
    {
        qCritical() << "duplicate TOTP dialog";
        return 1;
    }
    QDialog *dialog = dialogs.first();
    QLineEdit *edit = dialog->findChild<QLineEdit *>("totpCodeLineEdit");
    QDialogButtonBox *buttons = dialog->findChild<QDialogButtonBox *>();
    if (edit == nullptr || buttons == nullptr)
    {
        qCritical() << "TOTP controls missing";
        return 1;
    }
    QPushButton *ok = buttons->button(QDialogButtonBox::Ok);
    edit->setText("12345");
    if (ok->isEnabled())
    {
        qCritical() << "short code accepted";
        return 1;
    }
    edit->setText("000000");
    if (!ok->isEnabled())
    {
        qCritical() << "six digit code rejected";
        return 1;
    }
    ok->click();
    if (submissions != 1 || submitted != "000000\n" || cancellations != 0)
    {
        qCritical() << "TOTP submission failed";
        return 1;
    }
    submitted.clear();
    app.processEvents();

    coordinator.requestTotpCode();
    dialog = parent.findChildren<QDialog *>().last();
    dialog->reject();
    if (cancellations != 1 || submissions != 1)
    {
        qCritical() << "TOTP cancellation failed";
        return 1;
    }
    app.processEvents();

    coordinator.requestTotpCode();
    coordinator.dismissTotpDialog();
    if (cancellations != 1)
    {
        qCritical() << "process exit treated as user cancellation";
        return 1;
    }
    return 0;
}
