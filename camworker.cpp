#include "camworker.h"
#include "config.h"

CamWorker::CamWorker(QObject* parent) : QObject(parent), cam1Num(0), cam2Num(0), cam3Num(0)
{
    QMap<QString, QString> params = Config::getParmas();

    timeInteval = params["timeInterval"].toInt();
    videoLength = params["videoLength"].toInt() * 10;
    filePath = params["fileDir"];

    mode = Config::getModeType();

    loadSensor();

    if(mode)
    {
        runtime = &PoshRuntime::initRuntime(APP_NAME);

        qDebug() << "[AutoSaveWorker] shared memory(iceoryx) - Create Runtime instance: " << APP_NAME;
        iox::popo::SubscriberOptions subscriberOptions;
        subscriberOptions.queueCapacity = 0U;
        subscriberOptions.queueFullPolicy = iox::popo::QueueFullPolicy::DISCARD_OLDEST_DATA;

        camSubscribers = std::vector<Subscriber<CustomCamDataType>*>(camNameList.size());

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
    else
    {
        QStringList filters;
        filters << "*.raw";

        camsData.resize(3);

        for(int i = 0; i < 3; i++)
        {
            QString camDir = filePath + QString("/cam%1").arg(i+1);
            QDir dir(camDir);

            if(!dir.exists())
            {
                qCritical() << "Cam Folder does not exist: " << camDir;
                break;
            }

            QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files | QDir::NoSymLinks, QDir::Name);

            if(fileList.isEmpty())
            {
                qWarning() << "No raw fies found in folder: " << camDir;
                continue;
            }

            for(QFileInfo f : fileList)
            {
                QString filePath = f.absoluteFilePath();
                camsData[i].push(filePath);
            }
        }

    }


    initFFmpeg();
}

CamWorker::~CamWorker()
{
    if (ffmpeg[0].state() == QProcess::Running)
        stopFFmpeg(0);

    if (ffmpeg[1].state() == QProcess::Running)
        stopFFmpeg(1);

    if (ffmpeg[2].state() == QProcess::Running)
        stopFFmpeg(2);
}

void CamWorker::receiveGrabFrame()
{
    if(mode)
    {
        for(unsigned long i = 0; i < camSubscribers.size(); i++)
        {
            if(cam1Num >= videoLength && cam2Num >= videoLength && cam3Num >= videoLength)
            {
                cam1Num = 0;
                cam2Num = 0;
                cam3Num = 0;
                emit done();
                return;
            }
            Subscriber<CustomCamDataType> *camSubscriber = camSubscribers[i];

            if(cam1Num == 50)
                std::cout << "";

            if(cam1Num >= videoLength && i == 0)
            {
                qDebug() << "Received CAM1 frame :" << videoLength;
                continue;
            }

            if(cam2Num >= videoLength && i == 1)
            {
                qDebug() << "Received CAM2 frame :" << videoLength;
                continue;
            }

            if(cam3Num >= videoLength && i == 2)
            {
                qDebug() << "Received CAM3 frame :" << videoLength;
                continue;
            }


            auto camTakeResult = camSubscriber->take();
            if (!camTakeResult.has_error())
            {
                CamData inputData;

                // Handle received data
                inputData.timestamp = camTakeResult.value()->timestamp;
                inputData.height = camTakeResult.value()->height;
                inputData.width = camTakeResult.value()->width;

                int dataSize = static_cast<int>(inputData.height * inputData.width * 3);

                inputData.data = QByteArray(reinterpret_cast<const char*>(camTakeResult.value()->data), dataSize);

                pushFrameToFFmpegLive(inputData, i);

            }
            else
            {
                if (camTakeResult.get_error() == iox::popo::ChunkReceiveResult::NO_CHUNK_AVAILABLE)
                {
                    qDebug() << "CAM" << i+1 << "] No chunk available.";
                    return;
                }
                else
                {
                    qDebug() << "CAM" << i+1 << "] Error receiving chunk.";
                    return;
                }
            }

        }
    }
    else
    {
        for(unsigned long i = 0; i < camsData.size(); i++)
        {
            if(cam1Num > videoLength && cam2Num > videoLength && cam3Num > videoLength)
            {
                qDebug() << "videoLength" << videoLength;
                cam1Num = 0;
                cam2Num = 0;
                cam3Num = 0;
                emit done();
                return;
            }

            if(cam1Num == 50)
                std::cout << "";

            if(cam1Num > videoLength && i == 0)
            {
                qDebug() << "Received CAM1 frame :" << videoLength;
                continue;
            }

            if(cam2Num > videoLength && i == 1)
            {
                qDebug() << "Received CAM2 frame :" << videoLength;
                continue;
            }

            if(cam3Num > videoLength && i == 2)
            {
                qDebug() << "Received CAM3 frame :" << videoLength;
                continue;
            }

            QString rawPath = camsData[i].front();
            camsData[i].pop();

            pushFrameToFFmpegFile(rawPath, i);
        }
    }

}

void CamWorker::loadSensor()
{
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
}

void CamWorker::startFFmpeg(int index)
{
    QString frameSize = "2048x1536";
    double qTimestamp = QDateTime::currentMSecsSinceEpoch();
    QString strTimestamp = QString::number(qTimestamp, 'f', 0);

    QStringList args;

    args << "-y"
         << "-f" << "rawvideo"
         << "-pix_fmt" << "bgr24"
         << "-s" << frameSize
         << "-r" << "10"
         << "-i" << "-"
         << "-an"
         << "-vf" << "format=yuv420p"
         << "-c:v" << "libx265"
         << "-preset" << "ultrafast"
         << "-tune" << "zerolatency"
         << "-crf" << "28"
         << "-f" << "mpegts"
         << QString("tcp://1.223.191.187:%1").arg(5000 + index);


    ffmpeg[index].setProgram("ffmpeg");
    ffmpeg[index].setArguments(args);
    ffmpeg[index].start();

    if (!ffmpeg[index].waitForStarted()) {
        qCritical() << "ffmpeg start 실패";
    }

}

void CamWorker::stopFFmpeg(int index)
{
    ffmpeg[index].closeWriteChannel();

    if (!ffmpeg[index].waitForFinished(-1)) {
        qCritical() << " Fail to terminate ffmpeg";
    }

    if (ffmpeg[index].exitCode() != 0) {
        qCritical() << "Fail to ffmpeg";
    }

    qDebug() << "[Client] Success to Send CAM" << index << "Video";
}

void CamWorker::initFFmpeg()
{

    if (ffmpeg[0].state() == QProcess::Running)
        stopFFmpeg(0);

    if (ffmpeg[1].state() == QProcess::Running)
        stopFFmpeg(1);

    if (ffmpeg[2].state() == QProcess::Running)
        stopFFmpeg(2);

    startFFmpeg(0);
    startFFmpeg(1);
    startFFmpeg(2);
    pTimer.start();
}

void CamWorker::pushFrameToFFmpegLive(CamData data, int index)
{

    if (index < 0 || index >= 3)
    {
        qCritical() << "Wrong ffmpeg index:" << index;
        return;
    }

    if (ffmpeg[index].state() != QProcess::Running)
    {
        qCritical() << "ffmpeg process not running:" << index;
        return;
    }

    const int width = data.width;
    const int height = data.height;

    if (width <= 0 || height <= 0)
    {
        qCritical() << "Wrong frame size:" << width << height;
        return;
    }


    const qint64 expectedBytes = static_cast<qint64>(width) * height * 3;
    if (data.data.size() < expectedBytes)
    {
        qCritical() << "Insufficient input data size:"
                    << "expected =" << expectedBytes
                    << ", actual =" << data.data.size();
        return;
    }

    const char* ptr = data.data.constData();
    const qint64 totalBytes = expectedBytes;

    qint64 written = 0;
    while (written < totalBytes)
    {
       const qint64 n = ffmpeg[index].write(ptr + written, totalBytes - written);
       if (n <= 0)
       {
           qCritical() << "Failt to write ffmpeg :"
                       << ffmpeg[index].errorString();
           return;
       }

       written += n;

       if (!ffmpeg[index].waitForBytesWritten(3000))
       {
           qCritical() << "Fail to flush ffmpeg flush :"
                       << ffmpeg[index].errorString();
           return;
       }
    }

    if (index == 0) ++cam1Num;
    else if (index == 1) ++cam2Num;
    else if (index == 2) ++cam3Num;
}

void CamWorker::pushFrameToFFmpegFile(const QString& rawPath, int index)
{
    if (index < 0 || index >= 3)
    {
        qCritical() << "Wrong ffmpeg index:" << index;
        return;
    }

    if (ffmpeg[index].state() != QProcess::Running)
    {
        qCritical() << "ffmpeg process not running:" << index;
        return;
    }

    const int width = 2048;
    const int height = 1536;

    if (width <= 0 || height <= 0)
    {
        qCritical() << "Wrong frame size:" << width << height;
        return;
    }

    QFile rawFile(rawPath);
    if(!rawFile.open(QIODevice::ReadOnly))
    {
        qCritical() << "Cannot open file : " << rawPath;
        return;
    }


    const QByteArray frameData = rawFile.readAll();
    rawFile.close();


    const qint64 expectedBytes = static_cast<qint64>(width) * height;


    if (frameData.size() < expectedBytes)
    {
        qCritical() << "Insufficient raw file size:"
                    << "expected =" << expectedBytes
                    << ", actual =" << frameData.size()
                    << ", file =" << rawPath;
        return;
    }

    cv::Mat gray(height, width, CV_8UC1, const_cast<char*>(frameData.constData()));
    cv::Mat bgr;
    cv::cvtColor(gray, bgr, cv::COLOR_BayerBG2BGR);

    const char* ptr = reinterpret_cast<const char*>(bgr.data);
    const qint64 totalBytes = static_cast<qint64>(bgr.total() * bgr.elemSize());

    if (frameData.size() > totalBytes)
    {
        qWarning() << "Raw file bigger than expected. Only first frame will be used:"
                   << "expected =" << totalBytes
                   << ", actual =" << frameData.size()
                   << ", file =" << rawPath;
    }

    qint64 written = 0;

    while (written < totalBytes)
    {
        const qint64 n = ffmpeg[index].write(ptr + written, totalBytes - written);
        if (n <= 0)
        {
            qCritical() << "Fail to write ffmpeg:" << ffmpeg[index].errorString();
            return;
        }

        written += n;

        if (!ffmpeg[index].waitForBytesWritten(3000))
        {
            qCritical() << "Fail to flush ffmpeg:" << ffmpeg[index].errorString();
            return;
        }
    }

    if (index == 0) ++cam1Num;
    else if (index == 1) ++cam2Num;
    else if (index == 2) ++cam3Num;
}



