#include "videohandler.h"
VideoHandler::VideoHandler(QObject* parent) : QObject(parent)
  , resultPath("/home/tesla/cscho/result"), pFrameNum(0)
{

}

void VideoHandler::createVideo()
{
    const int width = 2048;
    const int height = 1536;

    if(!rootDir.exists())
    {
        //TODO
        qDebug() << "Don't eixt folder = " << rootPath;
    }

    QStringList filter;
    filter << "*.raw";

    QFileInfoList rawFiles = rootDir.entryInfoList(filter, QDir::Files, QDir::Name);

    if(rawFiles.isEmpty())
    {
        //TODO
        qDebug() << "No raw files";
        return;
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
         << resultPath;

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

    const qint64 rawBytes = static_cast<qint64>(width)*height;
    std::vector<uchar> rawBuffer(static_cast<size_t>(rawBytes));

    for(int i = 0; i < videoLength; i++)
    {
        const QFileInfo& fi = rawFiles[pFrameNum];
        QFile file(fi.absoluteFilePath());

        if(!file.open(QIODevice::ReadOnly))
        {
            qCritical() << "Failt to open: " << fi.absoluteFilePath();
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

        cv::Mat bayer(height, width, CV_8UC1, rawBuffer.data());
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
        while(written < bytesToWrite)
        {
            qint64 chunk = ffmpeg.write(dataPtr + written, bytesToWrite - written);
            if(chunk < 0)
            {
                qCritical() << "fail to write to ffmpeg stdin" << fi.fileName();
                ffmpeg.kill();
                ffmpeg.waitForFinished();
                return;
            }

            written += chunk;
            if(!ffmpeg.waitForBytesWritten(-1))
            {
                qCritical() << "ffmpeg stdin flush 실패:" << fi.fileName();
                qCritical() << "state =" << ffmpeg.state();
                qCritical() << "exitCode =" << ffmpeg.exitCode();
                qCritical() << "error =" << ffmpeg.errorString();
                qCritical().noquote() << "stderr:\n" << ffmpeg.readAllStandardError();
                ffmpeg.kill();
                ffmpeg.waitForFinished();
                return;
            }
        }

        qInfo() << QString("프레임 %1/%2 처리 완료: %3")
                       .arg(i + 1)
                       .arg(rawFiles.size())
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

    qInfo() << "완료: /home/tesla/cscho/result/output.mp4";
    return;
}

int VideoHandler::extractNumber(const QString& fileName)
{
    QRegularExpression re("(\\d+)");
    QRegularExpressionMatch match = re.match(fileName);

    if(match.hasMatch())
        return match.captured(-1).toInt();

    return -1;
}
