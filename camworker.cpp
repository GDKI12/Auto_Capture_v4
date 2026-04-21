#include "camworker.h"

#include <QElapsedTimer>
#include <QThread>
#include <QAbstractSocket>

CamWorker::CamWorker(const QString& camId, const Config& config, QObject* parent)
    : QObject(parent), id(camId)
{
    socket = new QTcpSocket(this);
    ffmpeg = new QProcess(this);

    connect(socket, &QTcpSocket::readyRead, this, [=]()
    {
        QByteArray data = socket->readAll();
        qDebug() << "Received from server:" << data;
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

    frames = timeInterval / 100;


    connect(&watcher, &QFileSystemWatcher::directoryChanged, this, &CamWorker::onFileSystemChanged);

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
      qCritical() << "ffmpeg start fail";
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
            qDebug() << "added folder: " << camDir;
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
    qDebug() << "Start to vss";
    if(sensorDirs.isEmpty() && rawFiles.isEmpty())
    {
        qDebug() << "No sensorDirs";
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
    qDebug() << "Sensor Dir list size " << sensorDirs.size();
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
        qCritical() << QString("fail connect to ip : %1 port : %2").arg(dstIp).arg(metaPort);
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
        qCritical() << "write fail:" << socket->errorString();
        return;
    }

    socket->flush();

    qDebug() << "Success to send meta: " << header;

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
            qCritical() << "Fail to read raw file : "
                      << in.fileName()
                      << "read = " << readBytes
                      << "expected = " << rawBytes;

            stopFfmpeg();
            return;
        }


      qint64 written = 0;
      while(written < frame.size())
      {
          qint64 chunk = ffmpeg->write(frame.constData() + written, frame.size() - written);
          if(chunk < 0)
          {
              qCritical() << "fail to write to ffmpeg stdin" << in.fileName();
              stopFfmpeg();
              return;
          }

          written += chunk;
          if(!ffmpeg->waitForBytesWritten(-1))
          {
              qCritical() << "ffmpeg stdin flush 실패:" << in.fileName();
              qCritical() << "state =" << ffmpeg->state();
              qCritical() << "exitCode =" << ffmpeg->exitCode();
              qCritical() << "error =" << ffmpeg->errorString();
              qCritical().noquote() << "stderr:\n" << ffmpeg->readAllStandardError();
              stopFfmpeg();
              return;
          }
      }
    }

      qint64 elapsedMs = timer.elapsed();
      qDebug() << "ffmpeg encoding time(ms):" << elapsedMs;

      if (ffmpeg->state() != QProcess::Running) {
          qCritical() << "ffmpeg stopped unexpectedly. exitCode =" << ffmpeg->exitCode();
          return;
      }

      qDebug() << "cam" << id << " complete ffmpeg encoding";
      qDebug() << "queue size : " << rawFiles.size();
      qDebug() << "current dir : " << currDir;
      return;
}

void CamWorker::getAnswer(QByteArray data)
{
    QJsonParseError parseError;

    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if(parseError.error != QJsonParseError::NoError)
    {
        qCritical() << "JSON parsing error: " << parseError.errorString();
    }

    QJsonObject root = doc.object();

    QString folderName = root.value("fileName").toString();
    QString answer = root.value("answer").toString();

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
                qDebug() << "Success to remove folder : " << p;
                trashList.remove(p);
            }
            else
            {
                qDebug() << "Failed to remove folder : " << p;
            }
        }
    }
}
