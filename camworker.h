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

public slots:
    void receiveGrabFrame();
    void saveRawFile(CustomCamDataType* data);

private:
    PoshRuntime* runtime;
    std::vector<Subscriber<CustomCamDataType> *> camSubscribers;
    std::vector<QString> camNameList;
    std::vector<CustomCamDataType *> inputData;

    std::queue<int> images;
    int timeInteval;
    int videoLength;
    QString savePath;
};

#endif // CAMWORKER_H
