#include "camworker.h"

#include <QElapsedTimer>
#include <QThread>


CamWorker::CamWorker(const QString& camId, const Config& config, QObject* parent)
    : QObject(parent), id(camId)
{
    socket = new QTcpSocket(this);

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

    mode = config.mode;
    width = config.width;
    height = config.height;
    rawSize = config.rawSize;

    frames = timeInterval / 100;


    ffmpegUrl = QString("tcp://%1:%2").arg(dstIp).arg(dstPort);
    ffmpegArgs << "-f" << "rawvideo"
            << "-loglevel" << "debug"
            << "-pixel_format" << "bayer_rggb8"
            << "-video_size" << QString("%1x%2").arg(width).arg(height)
            << "-framerate" << "10"
            << "-i" << "pipe:0"
            << "-vf" << "format=bgr24"
            << "-c:v" << "libx264"
            << "-preset" << "veryfast"
            << "-tune" << "zerolatency"
            << "-pix_fmt" << "yuv420p"
            << "-f" << "mpegts"
            << ffmpegUrl;

    frameBuffer.resize(static_cast<int>(static_cast<qint64>(width) * height));
    ffmpeg.setProgram("ffmpeg");
    ffmpeg.setArguments(ffmpegArgs);
    ffmpeg.setProcessChannelMode(QProcess::SeparateChannels);

    connect(&watcher, &QFileSystemWatcher::directoryChanged, this, &CamWorker::onFileSystemChanged);

    init();

    timer.setInterval(timeInterval);
    timer.setTimerType(Qt::PreciseTimer);

    connect(&timer, &QTimer::timeout, this, &CamWorker::start);

    timer.start();
}

CamWorker::~CamWorker()
{
    stopFfmpeg(false);

    if(socket->state() == QAbstractSocket::ConnectedState)
    {
        socket->flush();
        socket->disconnectFromHost();
        socket->waitForDisconnected(3000);
    }

    socket->deleteLater();
    timer.deleteLater();
}

bool CamWorker::ensureMetaSocketConnected()
{
    if(socket->state() == QAbstractSocket::ConnectedState)
        return true;

    if(socket->state() != QAbstractSocket::UnconnectedState)
        socket->abort();

    socket->connectToHost(dstIp, metaPort);

    if(!socket->waitForConnected(3000))
    {
        QString errString = QString("fail connect to ip : %1 port : %2").arg(dstIp).arg(metaPort);
        qCritical() << errString;
        return false;
    }

    return true;
}

bool CamWorker::sendMetaHeader(const QByteArray& header)
{
    if(!ensureMetaSocketConnected())
        return false;

    socket->write(header);

    if(!socket->waitForBytesWritten(3000))
    {
        qCritical() << "write fail: " << socket->errorString();
        socket->abort();
        return false;
    }

    socket->flush();
    return true;
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

        if(rawFiles.size() == rawSize)
            sensorDirs.enqueue(path);
    }
}

bool CamWorker::startFfmpeg()
{
    if(ffmpeg.state() != QProcess::NotRunning)
        return true;

    ffmpeg.start();

    if(!ffmpeg.waitForStarted(3000))
    {
        qCritical() << "ffmpeg start fail: " << ffmpeg.errorString();
        return false;
    }

    return true;
}

void CamWorker::stopFfmpeg(bool forceKill)
{
    if(ffmpeg.state() == QProcess::NotRunning)
        return;

    if(forceKill)
    {
        ffmpeg.kill();
        ffmpeg.waitForFinished();
        return;
    }

    ffmpeg.closeWriteChannel();
    qCritical() << "ffmpeg failed to wait for termination";
    ffmpeg.kill();
    ffmpeg.waitForFinished();
}

bool CamWorker::writeFrameToFfmpeg(const QString& filePath)
{
    QFile in(filePath);
    if(!in.open(QIODevice::ReadOnly))
    {
        qCritical() << "Fail to open raw file: " << filePath;
        return false;
    }

    const qint64 rawBytes = frameBuffer.size();
    qint64 readBytes = in.read(frameBuffer.data(), rawBytes);
    in.close();

    if(readBytes != rawBytes)
    {
        qCritical() << "Fail to read raw file : "
                    << filePath
                    << "read = " << readBytes
                    << "expected = " << rawBytes;

        return false;
    }

    qint64 written = 0;

    while(written < frameBuffer.size())
    {
        qint64 chunk = ffmpeg.write(frameBuffer.constData() + written, frameBuffer.size() - written);
        if(chunk < 0)
        {
            qCritical() << "fail to write to ffmpeg stdin" << filePath;
            return false;
        }

        written += chunk;
        if(!ffmpeg.waitForBytesWritten(-1))
        {
            qCritical() << "ffmpeg stdin flush 실패:" << filePath;
            qCritical() << "state =" << ffmpeg.state();
            qCritical() << "exitCode =" << ffmpeg.exitCode();
            qCritical() << "error =" << ffmpeg.errorString();
            qCritical().noquote() << "stderr:\n" << ffmpeg.readAllStandardError();
            return false;
        }
    }

    return true;

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
    if(clips.isEmpty())
    {
        qDebug() << "No clips to send";
        return;
    }

    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");


    QJsonObject obj;

    QFileInfo fi(currDir);
    QDir parentDir = fi.dir();
    QString sensorName = parentDir.dirName();

    obj["videoName"] = QString("%1_%2_%3.mp4").arg(sensorName).arg(id).arg(timestamp);

    QByteArray header = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    header.append("\n");

    if(!sendMetaHeader(header))
        return;

    qDebug() << "Success to send meta: " << header;

    QElapsedTimer eTimer;
    eTimer.start();

    if(!startFfmpeg())
        return;

    for(const QString& file : clips)
    {
        if(!writeFrameToFfmpeg(file))
        {
            stopFfmpeg(true);
            return;
        }
    }

    stopFfmpeg(false);

    qint64 elapsedMs = eTimer.elapsed();
    qDebug() << "ffmpeg encoding time(ms): " << elapsedMs;

    if (ffmpeg.exitCode() != 0) {
        qCritical() << "ffmpeg fail to encoding. exitCode =" << ffmpeg.exitCode();
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
        qCritical() << "JSON parsing error: " << parseError.errorString();


    QJsonObject root = doc.object();

    QString folderName = root.value("fileName").toString();
    QString answer = root.value("answer").toString();

    QString path = rootPath + "/" + folderName;
    if(answer.contains("yes", Qt::CaseInsensitive))
    {
        if(trashList.contains(path))
        {
            qDebug() << "remove folder from trash queue" << path;
            trashList.remove(path);
        }
    }
    else
    {
        QString path = rootPath + "/" + folderName;
        qDebug() << "insert folder to trash queue" << path;
        if(!trashList.contains(path))
            trashList.insert(path);
    }

    QFileInfo fi(currDir);
    QDir parentDir = fi.dir();

    if(!trashList.isEmpty())
    {
        for(const QString& p : trashList)
        {
            if(p == parentDir.absolutePath())
                continue;

            QDir removeDir(p);
            if(removeDir.exists())
            {
                if(removeDir.removeRecursively())
                    qDebug() << "Success to remove folder : " << p;
                else
                    qDebug() << "Failed to remove folder : " << p;
            }
        }
    }

}
