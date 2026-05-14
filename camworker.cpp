#include "camworker.h"

#include <QElapsedTimer>
#include <QThread>
#include <QAbstractSocket>
#include <QRegularExpression>
#include <QtGlobal>
#include <algorithm>
#include <QStringList>

namespace{
bool containsAny(const QString& text, const QStringList& patterns)
{
    for(const QString& pattern : patterns)
    {
        if(text.contains(pattern))
            return true;
    }

    return false;
}

bool hasEvent(const QString& answer)
{
    const QString text = answer.toLower();
//    const bool rainNegated = containsAny(text,
//                                         {
//                                             "no rain",
//                                             "no rainfall",
//                                             "no recent rain",
//                                             "no recent rainfall",
//                                             "no active rain",
//                                             "no active rainfall",
//                                             "without rain",
//                                             "without rainfall"
//                                         });


//    const bool snowNegated = containsAny(text, {
//                                             "no snow",
//                                             "no snowfall",
//                                             "without snow",
//                                             "without snowfall",
//                                         });

    const bool hasEvent = !text.contains("there is no");

    return hasEvent;
}

QString sensorPathFromCamPath(const QString& camPath)
{
    return QFileInfo(camPath).dir().absolutePath();
}

}

CamWorker::CamWorker(const QString& camId, const Config& config, QObject* parent)
    : QObject(parent), id(camId), ctn(0)
{
    socket = new QTcpSocket(this);
    ffmpeg = new QProcess(this);

    connect(this, &CamWorker::outInfo, this, &CamWorker::onWrite);
    connect(this, &CamWorker::outWarn, this, &CamWorker::onWrite);
    connect(this, &CamWorker::outError, this, &CamWorker::onWrite);

    connect(socket, &QTcpSocket::readyRead, this, [=]()
    {
        QByteArray data = socket->readAll();
        QString log = QString("Received from server: %1").arg(QString::fromUtf8(data));

        emit outInfo(log.remove(QRegularExpression("[\r\n]+$")));

        getAnswer(data);
    });

    emit outInfo("Start to vss");

    rootPath = config.rootPath;
    dstIp = config.ip;

    dstPort = config.port;
    metaPort = dstPort + 100;
    timeInterval = config.timeInterval;

    videoLength = config.videoLength;

    mode = config.mode;    width = config.width;
    height = config.height;

    frames = timeInterval / 100;
    connect(&watcher, &QFileSystemWatcher::directoryChanged, this, &CamWorker::onFileSystemChanged);
    qDebug() << "Video Lenggh : " << videoLength;
    init();
}

CamWorker::~CamWorker()
{
    socket->flush();
    socket->disconnectFromHost();
    socket->waitForDisconnected(3000);

    socket->deleteLater();
    ffmpeg->deleteLater();
}

void CamWorker::stop()
{
    if(!socket->isOpen())
    {
        socket->flush();
        socket->disconnectFromHost();
        socket->waitForDisconnected(-1);

        socket->deleteLater();
    }

    ffmpeg->deleteLater();
}
bool CamWorker::ensureFfmpegRunning()
{
    if(ffmpeg->state() == QProcess::Running)
        return true;
    const QString url = QString("tcp://%1:%2").arg(dstIp).arg(dstPort);
          const QStringList args = {
              "-f", "rawvideo",
              "-loglevel", "error",
              "-pixel_format", "bayer_rggb8",
              "-video_size", QString("%1x%2").arg(width).arg(height),
              "-framerate", "10",
              "-i", "pipe:0",
              "-vf", "format=bgr24",
              "-c:v", "libx264",
              "-preset", "veryfast",
              "-tune", "zerolatency",
              "-pix_fmt", "yuv420p",
              "-f", "mpegts",
              url
          };

    ffmpeg->setProgram("ffmpeg");
    ffmpeg->setArguments(args);
    ffmpeg->setProcessChannelMode(QProcess::SeparateChannels);
    ffmpeg->start();

    if(!ffmpeg->waitForStarted(-1))
    {
      emit outError("Fail to start ffmpeg (Check the connection with the server)");
      return false;
    }

    return true;

}

void CamWorker::stopFfmpeg()
{
    if(ffmpeg->state() == QProcess::NotRunning)
        return;

    ffmpeg->closeWriteChannel();
    if(ffmpeg->waitForFinished(60000))
        return;

    ffmpeg->kill();
    ffmpeg->waitForFinished();
}

void CamWorker::init()
{
    QDir d(rootPath);

    if(mode)
    {
        watcher.addPath(rootPath);

    }

    QFileInfoList dirs = d.entryInfoList({"Sensor_Data*"},
                                         QDir::Dirs | QDir::NoDotAndDotDot,
                                         QDir::Name);

    for(const QFileInfo& fi : dirs)
    {
        QString camDir = fi.absoluteFilePath() + "/" + id;
        sensorDirs.enqueue(camDir);
        preSensors.insert(fi.absoluteFilePath());
    }

    start();

}

void CamWorker::onFileSystemChanged(const QString& path)
{
    QFileInfo info(path);

    QString baseName = info.baseName();

    QDir dir(path);

    if(path == rootPath)
    {
        QFileInfoList sensorList = dir.entryInfoList({"Sensor_Data*"},
                                                     QDir::Dirs | QDir::NoDotAndDotDot,
                                                     QDir::Name);

        QSet<QString> curRawDirs;

        for(const QFileInfo& fi : sensorList)
            curRawDirs.insert(fi.absoluteFilePath());

        QSet<QString> added = curRawDirs - preSensors;

        for(const QString& f : added)
        {
            QString camDir = QString("%1/%2").arg(f).arg(id);

            emit outInfo(QString("Added folder : %1").arg(camDir));
            preSensors.insert(f);
            watcher.addPath(camDir);
        }
    }

    if(baseName.startsWith("cam"))
    {
        QStringList rawFiles = dir.entryList({"*raw"},
                                           QDir::Files | QDir::NoDotAndDotDot,
                                           QDir::Name);

        if(rawFiles.size() == 3000)
            sensorDirs.enqueue(path);
    }
}


void CamWorker::start()
{
    if(sensorDirs.isEmpty() && rawFiles.isEmpty())
    {
        emit outInfo("No sensorDirs");
        return;
    }
    if(rawFiles.isEmpty() || rawFiles.size() < videoLength)
    {
        currDir = sensorDirs.dequeue();

        QDir dir(currDir);

        QFileInfoList rawFileList = dir.entryInfoList({"*raw"},
                                                   QDir::Files | QDir::NoDotAndDotDot,
                                                   QDir::Name);
        while(rawFileList.isEmpty())
        {
            if(sensorDirs.isEmpty())
                return;

            currDir = sensorDirs.dequeue();
            QDir dir(currDir);
            rawFileList = dir.entryInfoList({"*raw"},
                                            QDir::Files | QDir::NoDotAndDotDot,
                                            QDir::Name);
        }
        for(const QFileInfo& fi : rawFileList)
            rawFiles.enqueue(fi.absoluteFilePath());
    }

    requestCreateClip();
}


void CamWorker::requestCreateClip()
{
    emit outInfo(QString("Sensor Dir list size %1").arg(sensorDirs.size()));
    QVector<QString> clips;

    for(int i = 0; i < frames; i++)
    {
        if(i < videoLength)
        {
            clips.push_back(rawFiles.dequeue());
        }
        else
        {
            if(rawFiles.isEmpty())
                continue;
            rawFiles.dequeue();
        }
    }

    sendClip(clips);
}

void CamWorker::sendClip(const QVector<QString>& clips)
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");


    if(!(socket->state() == QAbstractSocket::ConnectedState))
        socket->connectToHost(dstIp, metaPort);

    if(!socket->waitForConnected(3000))
    {
        emit outError(QString("Fail connect to VSS Sever ip : %1 port : %2").arg(dstIp).arg(metaPort));
        return;
    }

    QJsonObject obj;

    QFileInfo fi(currDir);
    QDir parentDir = fi.dir();
    QString sensorName = parentDir.dirName();

    obj["videoName"] = QString("%1_%2_%3.mp4").arg(sensorName).arg(id).arg(timestamp);

    QByteArray header = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    header.append("\n");

    socket->write(header);

    if (!socket->waitForBytesWritten(3000)) {
        emit outError(QString("Fail to write meta info - %1").arg(socket->errorString()));
        return;
    }

    socket->flush();
    QString log = QString("Success to send meta: %1").arg(QString::fromUtf8(header));
    emit outInfo(log.remove(QRegularExpression("[\r\n]+$")));

    if(!ensureFfmpegRunning())
        return;

    qint64 rawBytes = (qint64)width * height;
    QByteArray frame(rawBytes, Qt::Uninitialized);
    QElapsedTimer timer;
    timer.start();

    for (const QString& file : clips)
    {
        QFile in(file);
        if (!in.open(QIODevice::ReadOnly)) continue;

        qint64 readBytes = in.read(frame.data(), rawBytes);

        in.close();

        if(readBytes != rawBytes)
        {
            QString log = QString("Fail to read raw file : %1, read = %2, expected = %3")
                    .arg(in.fileName()).arg(readBytes).arg(rawBytes);

            emit outError(log);

            stopFfmpeg();
            return;
        }


      qint64 written = 0;
      while(written < frame.size())
      {
          qint64 chunk = ffmpeg->write(frame.constData() + written, frame.size() - written);
          if(chunk < 0)
          {
              emit outError(QString("[ERROR] Fail to write to ffmpeg stdin %1").arg(in.fileName()));
              stopFfmpeg();
              return;
          }

          written += chunk;
          if(!ffmpeg->waitForBytesWritten(-1))
          {
              qCritical() << "[ERROR] Failt to flush ffmpeg stdin flush:" << in.fileName();
              qCritical() << "[ERROR] state =" << ffmpeg->state();
              qCritical() << "[ERROR] exitCode =" << ffmpeg->exitCode();
              qCritical() << "[ERROR] error =" << ffmpeg->errorString();

              emit outError(QString("Failt to flush ffmpeg stdin flush : %1").arg(in.fileName()));
              emit outError(QString("state = %1").arg(ffmpeg->state()));
              emit outError(QString("exitCode = %1").arg(ffmpeg->exitCode()));
              emit outError(QString("error = %1").arg(ffmpeg->errorString()));

              stopFfmpeg();
              return;
          }
      }
    }

      qint64 elapsedMs = timer.elapsed();

      encodingTimes.push_back(elapsedMs);
      ctn++;

      emit outInfo(QString("ffmpeg encoding time(ms) : %1").arg(elapsedMs));

      if(ctn == 10)
      {
          showEncoding();
          ctn = 0;
          encodingTimes.clear();

      }

      if (ffmpeg->state() != QProcess::Running) {
          emit outError(QString("ffmpeg stopped unexpectedly. exitCode = %1").arg(ffmpeg->exitCode()));
          return;
      }

      stopFfmpeg();

      emit outInfo(QString("%1 complete ffmpeg encoding").arg(id));
      emit outInfo(QString("Raw queue size : %1").arg(rawFiles.size()));
      emit outInfo(QString("Current working dir : %1").arg(currDir));

      return;
}

void CamWorker::getAnswer(QByteArray data)
{
    QJsonParseError parseError;

    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if(parseError.error != QJsonParseError::NoError)
    {
        emit outError(QString("JSON parsing error : %1").arg(parseError.errorString()));
    }

    QJsonObject root = doc.object();

    QString folderName = root.value("fileName").toString();
    QString answer = root.value("answer").toString();

    if(answer.isEmpty())
    {
        emit outError("VSS Engine is not Running");
        stop();
        return;
    }

    QString path = rootPath + "/" + folderName;

    if(hasEvent(answer))
    {
        saveList.insert(path);
        trashList.remove(path);
    }
    else if(!saveList.contains(path))
    {
        trashList.insert(path);
    }

    const QString currentSensorPath = sensorPathFromCamPath(currDir);
    const QSet<QString> targets = trashList - saveList;

    for(const QString& p : targets)
    {
        if(p == currentSensorPath && !rawFiles.isEmpty())
          continue;

    QDir removeDir(p);
    if(removeDir.exists())
    {
        if(removeDir.removeRecursively())
        {
          emit outInfo(QString("Success to remove folder : %1").arg(p));
          trashList.remove(p);
        }
        else
        {
          emit outWarn(QString("Failed to remove folder : %1").arg(p));
        }
    }
    }

    start();
}

void CamWorker::onWrite(const QString& content, LogLevel level)
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss.zzz");

    if(level == LogLevel::INFO)
        qDebug().noquote() << timestamp << "[INFO] " << content;
    else if(level == LogLevel::WARN)
        qWarning().noquote() << "\033[33m" << timestamp << "[WARN] " << content;
    else if(level == LogLevel::ERROR)
        qCritical().noquote() << "\033[31m" << timestamp << "[ERROR] " << content;

}

void CamWorker::showEncoding()
{
    if(encodingTimes.isEmpty())
        return;

    auto result = std::minmax_element(encodingTimes.cbegin(), encodingTimes.cend());

    qint64 minValue = *result.first;
    qint64 maxValue = *result.second;

    qint64 sum = 0;

    for(auto itr = encodingTimes.cbegin(); itr != encodingTimes.cend(); itr++)
    {
        sum += *itr;
    }

    qint64 avg = sum / encodingTimes.size();

    QString log = QString("Encoding ( Max : %1, Min : %2, Avg : %3)").arg(maxValue).arg(minValue).arg(avg);

    emit outInfo(log);
}
