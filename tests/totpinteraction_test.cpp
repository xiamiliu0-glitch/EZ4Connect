#include <QCoreApplication>
#include <QDebug>
#include <QEventLoop>
#include <QStandardPaths>
#include <QTimer>

#include <cstdio>
#include <iostream>
#include <string>

#include "infrastructure/coreprocess/zjuconnectprocess.h"

namespace
{
int runFakeCore(const QByteArray &mode)
{
    std::fputs("CAS login completed\n", stdout);
    std::fflush(stdout);
    const int attempts = mode == "retry" ? 2 : 1;
    for (int attempt = 0; attempt < attempts; ++attempt)
    {
        // Deliberately split the prompt and omit its final newline.
        std::fputs("Please enter the TOTP ", stdout);
        std::fflush(stdout);
        std::fputs("token: ", stdout);
        std::fflush(stdout);
        if (mode == "exit")
        {
            return 1;
        }
        std::string input;
        if (!std::getline(std::cin, input))
        {
            return 2;
        }
        if (mode == "reject")
        {
            std::fputs("token authentication failed with code 100\n", stdout);
            return 1;
        }
        if (input == "000000")
        {
            std::fputs("VPN client started\n", stdout);
            return 0;
        }
        std::fputs("Try again\n", stdout);
    }
    return 1;
}

bool runScenario(const QByteArray &mode, int expectedPrompts, bool expectConnected,
                 bool expectRejection)
{
    qputenv("EZ4CONNECT_TEST_CORE_MODE", mode);
    ZjuConnectProcess process;
    ConnectionProfile profile;
    profile.program = QCoreApplication::applicationFilePath();
    profile.endpoint.protocol = "easyconnect";
    QEventLoop loop;
    bool timedOut = false;
    bool finished = false;
    bool connected = false;
    bool rejected = false;
    int prompts = 0;
    QObject::connect(&process, &CoreProcess::totpCode, &loop, [&]()
    {
        ++prompts;
        if (mode == "cancel")
        {
            process.stop();
        }
        else if (mode != "exit")
        {
            process.writeInput((mode == "retry" && prompts == 1)
                               ? QByteArray("111111\n") : QByteArray("000000\n"));
        }
    });
    QObject::connect(&process, &CoreProcess::connectionEstablished, &loop,
                     [&]() { connected = true; });
    QObject::connect(&process, &CoreProcess::error, &loop,
                     [&](ZJU_ERROR error)
                     {
                         if (error == ZJU_ERROR::SECONDARY_AUTH_FAILED)
                         {
                             rejected = true;
                         }
                     });
    QObject::connect(&process, &CoreProcess::finished, &loop, [&]()
    {
        finished = true;
        loop.quit();
    });
    QTimer::singleShot(5000, &loop, [&]()
    {
        timedOut = true;
        process.stop();
        loop.quit();
    });
    process.start(profile);
    qunsetenv("EZ4CONNECT_TEST_CORE_MODE");
    if (!finished)
    {
        loop.exec();
    }
    if (timedOut || prompts != expectedPrompts || connected != expectConnected
        || rejected != expectRejection)
    {
        std::fprintf(stderr,
                     "scenario=%s timeout=%d finished=%d prompts=%d connected=%d rejected=%d\n",
                     mode.constData(), timedOut, finished, prompts, connected, rejected);
        qCritical() << "scenario failed" << mode << "timeout" << timedOut
                    << "prompts" << prompts << "connected" << connected
                    << "rejected" << rejected;
        return false;
    }
    return true;
}
}

int main(int argc, char *argv[])
{
    const QByteArray mode = qgetenv("EZ4CONNECT_TEST_CORE_MODE");
    if (!mode.isEmpty())
    {
        return runFakeCore(mode);
    }
    QCoreApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    return runScenario("success", 1, true, false)
        && runScenario("retry", 2, true, false)
        && runScenario("reject", 1, false, true)
        && runScenario("cancel", 1, false, false)
        && runScenario("exit", 1, false, false) ? 0 : 1;
}
