#include "tcpHandler.h"
#include "config.h"

#include <QMap>
#include <QFileInfo>
#include <QIODevice>
#include <QDataStream>

TCPHandler::TCPHandler(QObject* parent) : QObject(parent)
{
    QMap<QString, QString> params = Config::getParmas();

    m_port = (quint16)(params["dstPort"].toInt());
    m_host = params["dstIP"];

    connect(&m_socket, &QTcpSocket::connected, this, &TCPHandler::onConnected);
    connect(&m_socket, &QTcpSocket::disconnected, this, &TCPHandler::onDisconnected);
    connect(&m_socket, &QTcpSocket::readyRead, this, &TCPHandler::OnReadyRead);

    connectToServer();
}

void TCPHandler::connectToServer()
{
    qDebug() << "Connecting to server..." << m_host << m_port;
    m_socket.connectToHost(m_host, m_port);
}


void TCPHandler::onConnected()
{
    qDebug() << "connected to server";
}

void TCPHandler::onDisconnected()
{
    qDebug() << "disconnected to server";
}

void TCPHandler::sendVideo(QString path)
{
    if (m_socket.state() != QAbstractSocket::ConnectedState) {
        qWarning() << "Socket is not connected. skip send.";
        return;
    }

    QString videoPath = "/home/tesla/cho/accident_2.mp4";

    QFile file(path);

    if(!file.open(QIODevice::ReadOnly))
    {
        qWarning() << "Cannot nopen video file: " << videoPath;
        return;
    }

    QByteArray fileName = QFileInfo(file.fileName()).fileName().toUtf8();
    quint32 fileNameSize = static_cast<quint32>(fileName.size());
    quint64 fileSize = static_cast<quint64>(file.size());

    QByteArray header;
    QDataStream out(&header, QIODevice::WriteOnly);
    out << fileNameSize;
    header.append(fileName);

    QByteArray sizeBlock;
    QDataStream out2(&sizeBlock, QIODevice::WriteOnly);
    out2 << fileSize;

    m_socket.write(header);
    m_socket.write(sizeBlock);

    while(!file.atEnd())
    {
        if (m_socket.state() != QAbstractSocket::ConnectedState) {
                qWarning() << "Disconnected while sending.";
                return;
            }
        QByteArray chunk = file.read(64 * 1024);
        qint64 written = m_socket.write(chunk);
        if (written == -1) {
            qWarning() << "Write failed:" << m_socket.errorString();
            return;
        }
    }

    m_socket.flush();
    qDebug() << "Video sent:" << videoPath << "size:" << fileSize;

}

void TCPHandler::OnReadyRead()
{

}

