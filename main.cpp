#include <QCoreApplication>
#include "camworker.h"
#include "config.h"
#include <QTcpSocket>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    Config config;
    config.loadConfig();

    CamWorker camWorker("cam3", config);

    return a.exec();
}
