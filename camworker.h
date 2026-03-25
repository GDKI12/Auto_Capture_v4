#ifndef CAMWORKER_H
#define CAMWORKER_H

#include <QObject>
#include <iostream>
#include <QDebug>
#include <QFile>
#include <QMap>
#include <QJsonParseError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QFuture>
#include <QtConcurrent>
#include "iceoryx_posh/popo/subscriber.hpp"
#include "iceoryx_posh/popo/untyped_subscriber.hpp"
#include "iceoryx_posh/runtime/posh_runtime.hpp"

#include <queue>
#include "define.h"
using namespace iox::runtime;
using namespace iox::popo;


class CamWorker : public QObject
{
    Q_OBJECT
public:
    explicit CamWorker(QObject* parent = nullptr);
    ~CamWorker();
    void pushFrameToFFmpeg(CamData data, int index);
    void startFFmpeg(int index);
    void stopFFmpeg(int idex);
    void initFFmpeg();
private:
    void loadSensor();
signals:
    void done();

public slots:
    void receiveGrabFrame();
    int saveRawFile(CamData data, int index);

private:
    PoshRuntime* runtime;
    std::vector<Subscriber<CustomCamDataType> *> camSubscribers;
    std::vector<QString> camNameList;
    std::vector<CustomCamDataType *> inputData;

    std::queue<int> images;
    int timeInteval;
    int videoLength;
    QString savePath;

    int cam1Num;
    int cam2Num;
    int cam3Num;

    QProcess ffmpeg[3];
};

#endif // CAMWORKER_H
