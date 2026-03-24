#include "camworker.h"
#include "config.h"

CamWorker::CamWorker(QObject* parent) : QObject(parent), index1(0), index2(0), index3(0)
{
    QMap<QString, QString> params = Config::getParmas();

    timeInteval = params["timeInterval"].toInt();
    videoLength = params["videoLength"].toInt() * 10;
    savePath = params["saveDir"];

    runtime = &PoshRuntime::initRuntime(APP_NAME);

    qDebug() << "[AutoSaveWorker] shared memory(iceoryx) - Create Runtime instance: " << APP_NAME;
    iox::popo::SubscriberOptions subscriberOptions;
    subscriberOptions.queueCapacity = 0U;
    subscriberOptions.queueFullPolicy = iox::popo::QueueFullPolicy::DISCARD_OLDEST_DATA;


    QString sensorListFileName = Config::getSensorListFile();
    QFile sensorListFile(sensorListFileName);

    sensorListFile.open(QIODevice::ReadOnly);

    QJsonParseError jsonParserError;

    QJsonDocument jsonDoc = QJsonDocument::fromJson(sensorListFile.readAll(), &jsonParserError);

    sensorListFile.close();

    QJsonObject rootObj = jsonDoc.object();
    QJsonArray sensorArray = rootObj.value("Sensors").toArray();

    for(int i = 0; i < sensorArray.size(); i++)
    {
        QJsonObject sensor = sensorArray[i].toObject();

        int sensorType = sensor.value("type").toInt();
        int sensorId = sensor.value("id").toInt();
        QString sensorName = sensor.value("name").toString();

        camNameList.push_back(sensorName);
    }

    camSubscribers = std::vector<Subscriber<CustomCamDataType>*>(camNameList.size());
    inputData = std::vector<CustomCamDataType *>(camNameList.size());

    for (unsigned long i = 0; i < camNameList.size(); i++)
    {
        inputData[i] = new CustomCamDataType();
    }

    for(unsigned long i = 0; i < camNameList.size(); i++)
    {
        std::string tmp_cam = "cam";
        std::string tmp_num = std::to_string(i+1);
        tmp_cam += tmp_num;
        char result[100];
        strcpy (result, tmp_cam.c_str());
        iox::capro::IdString_t service{result};

        Subscriber<CustomCamDataType> *camSubscriber = new Subscriber<CustomCamDataType>({service, "Data", "Image"}, subscriberOptions);
        camSubscribers.at(i) = camSubscriber;
    }

}


void CamWorker::receiveGrabFrame()
{
    double qTimestamp = QDateTime::currentMSecsSinceEpoch();
    QString strTimestamp = QString::number(qTimestamp, 'f', 0);

    for(int i = 0; i < camSubscribers.size(); i++)
    {
        if(index1 >= videoLength && index2 >= videoLength && index3 >= videoLength)
        {
            index1 = 0;
            index2 = 0;
            index3 = 0;
            emit done();
        }
        Subscriber<CustomCamDataType> *camSubscriber = camSubscribers[i];

        if(index1 == 50)
            std::cout << "" <<std::endl;

        if(index1 >= videoLength && i == 0)
            continue;

        if(index2 >= videoLength && i == 1)
            continue;

        if(index3 >= videoLength && i == 2)
            continue;


        auto camTakeResult = camSubscriber->take();
        if (!camTakeResult.has_error())
        {
            // 수신한 이미지 데이터를 처리합니다.
            inputData[i]->timestamp = camTakeResult.value()->timestamp;
            inputData[i]->height = camTakeResult.value()->height;
            inputData[i]->width = camTakeResult.value()->width;
            std::memcpy(inputData[i]->data, camTakeResult.value()->data, sizeof(inputData[i]->data));

            std::cout << "=== CAM" << i+1 << "]" << APP_NAME << "[" << strTimestamp.toStdString() << "] - get value1: "
                      << QString::number(camTakeResult.value()->timestamp, 'f', 0).toStdString() << std::endl;

            QFuture<int> cycle = QtConcurrent::run(this, &CamWorker::saveRawFile, inputData[i], i+1);
        }
        else
        {
            //! [error]
            if (camTakeResult.get_error() == iox::popo::ChunkReceiveResult::NO_CHUNK_AVAILABLE)
            {
//                    std::cout << "CAM" << i+1 << "] No chunk available." << std::endl;
            }
            else
            {
                std::cout << "CAM" << i+1 << "] Error receiving chunk." << std::endl;
            }
            //! [error]
        }

    }
}


// FLIR_BFS_PGE_32S4C_C_camera_data
int CamWorker::saveRawFile(CustomCamDataType* data, int index)
{
    switch (index) {
    case 1:
        index1++;
        break;
    case 2:
        index2++;
        break;
    case 3:
        index3++;
        break;
    default:
        break;
    }

    QString timeStamp = QString::number(data->timestamp, 'f', 0);

    QString rootDir = savePath + "/cam" + QString::number(index) + "/";

    QDir().mkpath(rootDir);

    QString rawFileName = rootDir + timeStamp + ".raw";

    QFile file(rawFileName);

    if(!file.open(QIODevice::WriteOnly))
    {
        // TODO
        // Handle Error
        std::cout << "Can not open file: " << rawFileName.toStdString() << std::endl;
    }

    file.write(reinterpret_cast<const char*>(data->data), data->width * data->height);
    std::cout << "Success to save file : " << rawFileName.toStdString() << std::endl;
    file.close();

    return 0;
}


