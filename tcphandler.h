#ifndef TCPHANDLER_H
#define TCPHANDLER_H

#include <QObject>

class TcpHandler : public QObject
{
    Q_OBJECT
public:
    explicit TcpHandler(QObject* parnet = nullptr);
};

#endif // TCPHANDLER_H
