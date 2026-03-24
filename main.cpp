#include <QCoreApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <QFile>
#include <QDebug>
#include <QThread>
#include "videowatcher.h"
#include "config.h"
#include "camworker.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

//    QString inputFolder = "/home/tesla/cscho/video";
//    QString outputVideo = "/home/tesla/cscho/output.mp4";
//    VideoWatcher& watcher = VideoWatcher::getInstance();

//    watcher.createVideo(inputFolder, outputVideo);

    QThread* thread = new QThread();
    QTimer* timer = new QTimer();

    CamWorker* camWorker = new CamWorker();

    camWorker->moveToThread(thread);
    timer->moveToThread(thread);

    QObject::connect(thread, &QThread::started, timer, [timer]() {
            timer->setInterval(1);
            timer->setTimerType(Qt::PreciseTimer);
            timer->start();
        });

    QObject::connect(timer, &QTimer::timeout, camWorker, &CamWorker::receiveGrabFrame);
    QObject::connect(thread, &QThread::finished, camWorker, &CamWorker::deleteLater);
    QObject::connect(thread, &QThread::finished, timer, &QTimer::deleteLater);

    thread->start();

    return a.exec();
}
