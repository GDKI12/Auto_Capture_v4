#include "videowatcher.h"
#include "config.h"

#include <QDebug>
#include <QDir>
#include <QProcess>
#include <QThread>
#include <opencv2/opencv.hpp>
#include <QRegularExpression>

VideoWatcher::VideoWatcher(QObject* parent) : QObject(parent)
{
    client = new TCPHandler();

    connect(this, &VideoWatcher::requestSend, client, &TCPHandler::sendVideo);
}





// Slot : After convert raw to png
void VideoWatcher::createVideo(const QString& inputDir, const QString& outputPath)
{
    QDir dir(inputDir);

    QStringList filter;
    filter << "*.raw";

    QFileInfoList fileList = dir.entryInfoList(filter, QDir::Files | QDir::Readable, QDir::Name);

    const int width = 2048;
    const int height = 1536;

    // 숫자 기준 정렬
    std::sort(fileList.begin(), fileList.end(),
              [this](const QFileInfo& a, const QFileInfo& b) {

                  int na = extractNumber(a.fileName());
                  int nb = extractNumber(b.fileName());

                  if (na >= 0 && nb >= 0 && na != nb) {
                      return na < nb;
                  }
                  return a.fileName().toLower() < b.fileName().toLower();
              });

    if(fileList.isEmpty())
    {
        qCritical() << "raw files number: " << fileList.size();
    }

    QProcess ffmpeg;

    QString frameSize = QString("%1x%2").arg("2048").arg("1536");

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
         << "-preset" << "medium"
         << "-crf" << "28"
         << "-tag:v" << "hvc1"
         << "-movflags" << "+faststart"
         << outputPath;

    qInfo() << "execute ffmpeg >> ";

    ffmpeg.setProgram("ffmpeg");
    ffmpeg.setArguments(args);
    ffmpeg.setProcessChannelMode(QProcess::SeparateChannels);

    ffmpeg.start();

    if(!ffmpeg.waitForStarted())
    {
        qCritical() << "fail to start ffmpeg: " << ffmpeg.errorString();
        return;
    }

    const qint64 rawBytes = static_cast<qint64>(width) * height;
    std::vector<uchar> rawBuffer(static_cast<size_t>(rawBytes));

    for(int i = 0; i < fileList.size(); i++)
    {
        const QFileInfo& fi = fileList[i];
        QFile file(fi.absoluteFilePath());

        if(!file.open(QIODevice::ReadOnly))
        {
            qCritical() << "Fail to open: " << fi.absolutePath();
            // TODO
            // handle error
            ffmpeg.kill();
            ffmpeg.waitForFinished();
            return;
        }

        qint64 readBytes = file.read(reinterpret_cast<char*>(rawBuffer.data()), rawBytes);
        file.close();

        if(readBytes != rawBytes)
        {
            qCritical() << "Fail to read raw file : "
                        << fi.fileName()
                        << "read = " << readBytes
                        << "expected = " << rawBytes;

            // TODO
            // handle error
            ffmpeg.kill();
            ffmpeg.waitForFinished();
            return;
        }

        // 1채널 Bayer BGGR 8bit
        cv::Mat bayer(height, width, CV_8UC1, rawBuffer.data());

        // Bayer -> BGR
        cv::Mat bgr;
        cv::cvtColor(bayer, bgr, cv::COLOR_BayerBG2BGR);

        if (bgr.empty() || bgr.type() != CV_8UC3) {
            qCritical() << "BGR 변환 실패:" << fi.fileName();
            // TODO
            ffmpeg.kill();
            ffmpeg.waitForFinished();
            return;
        }

        if (!bgr.isContinuous()) {
            bgr = bgr.clone();
        }

        const char* dataPtr = reinterpret_cast<const char*>(bgr.data);
        const qint64 bytesToWrite = static_cast<qint64>(bgr.total() * bgr.elemSize());

        qint64 written = 0;
        while (written < bytesToWrite) {
            qint64 chunk = ffmpeg.write(dataPtr + written, bytesToWrite - written);
            if (chunk < 0) {
                qCritical() << "ffmpeg stdin 쓰기 실패:" << fi.fileName();
                // TODO
                // handle error
                ffmpeg.kill();
                ffmpeg.waitForFinished();
                return;
            }
            written += chunk;

            if (!ffmpeg.waitForBytesWritten(-1)) {
                qCritical() << "ffmpeg stdin flush 실패:" << fi.fileName();
                qCritical() << "state =" << ffmpeg.state();
                qCritical() << "exitCode =" << ffmpeg.exitCode();
                qCritical() << "error =" << ffmpeg.errorString();
                qCritical().noquote() << "stderr:\n" << ffmpeg.readAllStandardError();
                // TODO
                // handle error
                ffmpeg.kill();
                ffmpeg.waitForFinished();
                return;
            }
        }

        qInfo() << QString("프레임 %1/%2 처리 완료: %3")
                       .arg(i + 1)
                       .arg(fileList.size())
                       .arg(fi.fileName());
    }

    ffmpeg.closeWriteChannel();

    if(!ffmpeg.waitForFinished(-1))
    {
        qCritical() << "ffmpeg failed to wait for termination";
        return;
    }

    QByteArray stdOut = ffmpeg.readAllStandardOutput();
    QByteArray stdErr = ffmpeg.readAllStandardError();


    if (!stdOut.isEmpty()) {
        qInfo().noquote() << "ffmpeg stdout:\n" << QString::fromLocal8Bit(stdOut);
    }
    if (!stdErr.isEmpty()) {
        qInfo().noquote() << "ffmpeg stderr:\n" << QString::fromLocal8Bit(stdErr);
    }

    if (ffmpeg.exitCode() != 0) {
        qCritical() << "ffmpeg 인코딩 실패. exitCode =" << ffmpeg.exitCode();
        return;
    }

    qInfo() << "완료:" << outputPath;
    return;
}

int VideoWatcher::extractNumber(const QString& fileName)
{
    QRegularExpression re("(\\d+)");
    QRegularExpressionMatch match = re.match(fileName);

    if(match.hasMatch())
        return match.captured(-1).toInt();

    return -1;
}


void VideoWatcher::deleteFolder(const QString& folderPath)
{
    QDir dir(folderPath);

    if (!dir.exists()) {
        qDebug() << "Don't exit folder : " << dir.dirName();
        return;
    }

    bool result = dir.removeRecursively();

    if (result)
        qDebug() << "Succcess to delete folder";
    else
        qDebug() << "삭제 실패";
}









