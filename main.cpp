#include <QCoreApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <QFile>
#include <QDebug>
#include <QThread>
#include "videowatcher.h"
#include "config.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    VideoWatcher& watcher = VideoWatcher::getInstance();
    watcher.setWatcher("/home/tesla/cho");

    return a.exec();
}

