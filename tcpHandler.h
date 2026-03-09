#ifndef IMAGEHANDLER_H
#define IMAGEHANDLER_H

#include <QObject>
#include <QTcpSocket>
#include <QFile>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTimer>
#include "protocol.h"

class TCPHandler : public QObject
{
    Q_OBJECT
public:
    explicit TCPHandler(QObject* parent = nullptr);

public slots:
    void onConnected();
    void onDisconnected();
    void OnReadyRead();
    void sendVideo(QString path);

private:
    enum ReceiveState
    {
        ReadFileNameSize,
        ReadFileName,
        ReadFileSize,
        ReadFileData
    };

    void connectToServer();

    QTcpSocket m_socket;
    QTimer m_timer;
    QString m_host;
    quint16 m_port;

    ReceiveState m_state = ReadFileNameSize;
    quint32 m_fileNameSize = 0;
    QString m_fileName;
    quint64 m_fileSize = 0;
    quint64 m_receivedBytes = 0;
    QFile *m_outputFile = nullptr;
};


#endif // IMAGEHANDLER_H
