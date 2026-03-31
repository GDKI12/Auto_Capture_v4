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
#include <QElapsedTimer>
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
    void pushFrameToFFmpegLive(CamData data, int index);
    void pushFrameToFFmpegFile(const QString& rawPath, int index);

    void startFFmpeg(int index);
    void stopFFmpeg(int idex);
    void initFFmpeg();
private:
    void loadSensor();
signals:
    void done();

public slots:
    void receiveGrabFrame();

private:
    QElapsedTimer pTimer;
    PoshRuntime* runtime;
    std::vector<Subscriber<CustomCamDataType> *> camSubscribers;
    std::vector<QString> camNameList;
    std::vector<CustomCamDataType *> inputData;

    std::vector<std::queue<QString>> camsData;

    std::queue<int> images;
    int timeInteval;
    int videoLength;
    QString filePath;

    int cam1Num;
    int cam2Num;
    int cam3Num;

    bool mode;

    QProcess ffmpeg[3];
};

#endif // CAMWORKER_H
