#include "camworker.h"

#include <QElapsedTimer>
#include <QThread>
#include <QAbstractSocket>
#include <QRegularExpression>

CamWorker::CamWorker(const QString& camId, const Config& config, QObject* parent)
    : QObject(parent), id(camId)
{
    outInfo("Start to vss");

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



    rootPath = config.rootPath;
    dstIp = config.ip;

    dstPort = config.port;
    metaPort = dstPort + 100;
    timeInterval = config.timeInterval;

    videoLength = config.videoLength;

    mode = config.mode;    width = config.width;
    height = config.height;

    frames = timeInterval / 100;    connect(&watcher, &QFileSystemWatcher::directoryChanged, this, &CamWorker::onFileSystemChanged);

    init();

    timer.setInterval(timeInterval);
    timer.setTimerType(Qt::PreciseTimer);

    connect(&timer, &QTimer::timeout, this, &CamWorker::start);

    timer.start();

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
    timer.stop();

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

    if(!ffmpeg->waitForStarted(3000))
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
    if(ffmpeg->waitForFinished(3000))
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
        outInfo("No sensorDirs");
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
    outInfo(QString("Sensor Dir list size %1").arg(sensorDirs.size()));
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
        outError(QString("Fail connect to ip : %1 port : %2").arg(dstIp).arg(metaPort));
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
        outError(QString("Fail to write meta info - %1").arg(socket->errorString()));
        return;
    }

    socket->flush();
    QString log = QString("Success to send meta: %1").arg(QString::fromUtf8(header));
    outInfo(log.remove(QRegularExpression("[\r\n]+$")));

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

            outError(log);

            stopFfmpeg();
            return;
        }


      qint64 written = 0;
      while(written < frame.size())
      {
          qint64 chunk = ffmpeg->write(frame.constData() + written, frame.size() - written);
          if(chunk < 0)
          {
              outError(QString("[ERROR] Fail to write to ffmpeg stdin %1").arg(in.fileName()));
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

              outError(QString("Failt to flush ffmpeg stdin flush : %1").arg(in.fileName()));
              outError(QString("state = %1").arg(ffmpeg->state()));
              outError(QString("exitCode = %1").arg(ffmpeg->exitCode()));
              outError(QString("error = %1").arg(ffmpeg->errorString()));

              stopFfmpeg();
              return;
          }
      }
    }

      qint64 elapsedMs = timer.elapsed();
      outInfo(QString("ffmpeg encoding time(ms) : %1").arg(elapsedMs));

      if (ffmpeg->state() != QProcess::Running) {
          outError(QString("ffmpeg stopped unexpectedly. exitCode = %1").arg(ffmpeg->exitCode()));
          return;
      }

      stopFfmpeg();

      outInfo(QString("%1 complete ffmpeg encoding").arg(id));
      outInfo(QString("Raw queue size : %1").arg(rawFiles.size()));
      outInfo(QString("Current working dir : %1").arg(currDir));

      return;
}

void CamWorker::getAnswer(QByteArray data)
{
    QJsonParseError parseError;

    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if(parseError.error != QJsonParseError::NoError)
    {
        outError(QString("JSON parsing error : %1").arg(parseError.errorString()));
    }

    QJsonObject root = doc.object();

    QString folderName = root.value("fileName").toString();
    QString answer = root.value("answer").toString();

    if(answer.isEmpty())
    {
        outError("VSS Server is not Running");
        stop();
        return;
    }

    QString path = rootPath + "/" + folderName;
    if(answer.contains("yes", Qt::CaseInsensitive) || answer.contains("snow", Qt::CaseInsensitive) || answer.contains("rain", Qt::CaseInsensitive))
    {
        trashList.remove(path);
    }
    else
    {
        QString path = rootPath + "/" + folderName;
        trashList.insert(path);
    }

    QFileInfo fi(currDir);
    QDir parentDir = fi.dir();


    for(const QString& p : trashList)
    {
        if(p == parentDir.absolutePath())
            continue;

        QDir removeDir(p);
        if(removeDir.exists())
        {
            if(removeDir.removeRecursively())
            {
                outInfo(QString("Success to remove folder : %1").arg(p));
                trashList.remove(p);
            }
            else
            {
                outWarn(QString("Failed to remove folder : ").arg(p));
            }
        }
    }
}

void CamWorker::onWrite(const QString& content, LogLevel level)
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss.zzz");

    if(level == LogLevel::INFO)
        qDebug().noquote() << "\033[32m" << timestamp << "[INFO] " << content;
    else if(level == LogLevel::WARN)
        qWarning().noquote() << "\033[33m" << timestamp << "[WARN] " << content;
    else if(level == LogLevel::ERROR)
        qCritical().noquote() << "\033[31m" << timestamp << "[ERROR] " << content;

}
