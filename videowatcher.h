#ifndef VIDEOWATCHER_H
#define VIDEOWATCHER_H

#include <QObject>
#include <QFileSystemWatcher>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>

#include "tcpHandler.h"

class VideoWatcher : public QObject
{
    Q_OBJECT
public:
    static VideoWatcher& getInstance();
    void setWatcher(QString);
    void encode(QString path);
private:
    explicit VideoWatcher(QObject* parent);

    VideoWatcher(const VideoWatcher&) = delete;
    VideoWatcher& operator=(const VideoWatcher&) = delete;

    void dirChanged(const QString& path);

    void process();
    void convertRAWtoPNG();

    bool isReady(QString);


signals:
    void completeConvert(QString);
    void requestSend(QString);

private slots:
    void createVideo(QString);

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
