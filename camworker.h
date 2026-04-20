#ifndef CAMWORKER_H
#define CAMWORKER_H

#include <QObject>
#include <QTimer>
#include <QSet>
#include <QQueue>
#include <QVector>
#include <QFileInfoList>
#include <QFileSystemWatcher>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QTcpSocket>
#include <QDateTime>

#include "config.h"

class CamWorker : public QObject
{
    Q_OBJECT
public:
    explicit CamWorker(const QString& camId, const Config& config, QObject* parent = nullptr);
    ~CamWorker();
public slots:
    void onFileSystemChanged(const QString& path);
    void getAnswer(QByteArray data);
private:
    void loadSensor();
    void getConfig();
    void start();
    void rootScan();
    void sensorMode();
    void requestCreateClip();
    void init();
    void sendClip(const QVector<QString>&);

private:
    QString id;
    QFileSystemWatcher watcher;

    QTimer timer;
    int frames;
    // setting params
    QString rootPath;
    QString dstIp;
    int dstPort;
    int timeInterval;
    int videoLength;
    bool mode;
    int width;
    int height;

    int metaPort;

    QQueue<QString> sensorDirs;
    QQueue<QString> rawFiles;

    QSet<QString> preSensors;

    QSet<QString> trashList;
    QString currDir;
    int frameIndex;

    QTcpSocket* socket;
};

#endif // CAMWORKER_H
