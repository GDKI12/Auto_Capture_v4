#include "agent.h"

Agent::Agent(QObject* parent) : QObject(parent)
{
    camWorker = new CamWorker(this);
    timer = new QTimer();

    connect(timer, &QTimer::timeout, camWorker, &CamWorker::receiveGrabFrame);


    timer->setInterval(100);
    timer->setTimerType(Qt::PreciseTimer);
    timer->start();

    QObject::connect(camWorker, &CamWorker::done, timer, [this]() {
        timer->stop();
        qDebug() << "[Client] Start to Create Video";
        camWorker->initFFmpeg();

        qDebug() << "[Client] Wait for 6000ms to send video";
        QTimer::singleShot(60000, timer, [this]() {
            timer->start();
        });
    });

}

Agent::~Agent()
{
    camWorker->deleteLater();
    timer->deleteLater();
}

