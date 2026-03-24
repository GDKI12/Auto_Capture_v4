#ifndef VIDEOWATCHER_H
#define VIDEOWATCHER_H

#include <QObject>
#include <QFileSystemWatcher>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>
#include <QProcess>
#include <QFileInfoList>

#include "tcpHandler.h"

class VideoWatcher : public QObject
{
    Q_OBJECT
public:
    static VideoWatcher& getInstance();
    void setWatcher();
    bool mergeRawFiles(const QString& inputDir, const QString& mergedRawPath);
    bool encodeMergedRawToH265(const QString& ffmpegPath, const QString& mergedRawPath, const QString& outputVideoPath,
                               int width, int height, int fps, const QString& pixelFormat);
    void createVideo(const QString& inputDir, const QString& outputPath);
private:
    explicit VideoWatcher(QObject* parent);

    VideoWatcher(const VideoWatcher&) = delete;
    VideoWatcher& operator=(const VideoWatcher&) = delete;

    void dirChanged(const QString& path);

    void process();
    void deleteFolder(const QString&);
    bool isReady(QString);

    int extractNumber(const QString&);



signals:
    void completeConvert(QString);
    void requestSend(QString);


private:
    QFileSystemWatcher* watcher;
    QString rootPath;
    QString trashPath;
    QString savePath;
    QStringList prevFolders;
    QString convertProgram;
    QString encodeProgram;


    TCPHandler* client;

};

#endif // VIDEOWATCHER_H
