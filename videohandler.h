#ifndef VIDEOHANDLER_H
#define VIDEOHANDLER_H

#include <QObject>
#include <QProcess>
#include <QFileInfoList>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QRegularExpression>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <QByteArray>

class VideoHandler : public QObject
{
    Q_OBJECT
public:
    explicit VideoHandler(QObject* parent = nullptr);

public:
    void createVideo();
    int extractNumber(const QString& fileName);
private:
    QDir rootDir;
    QString resultPath;
    int timeInteval;
    int videoLength;
    QString rootPath;

    int pFrameNum;
};

#endif // VIDEOHANDLER_H
