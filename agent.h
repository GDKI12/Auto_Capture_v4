#ifndef AGENT_H
#define AGENT_H

#include <QObject>
#include <QWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QFile>
#include <QDebug>
#include <QThread>
#include "videowatcher.h"
#include "config.h"
#include "camworker.h"

class Agent : public QObject
{
    Q_OBJECT
public:
    explicit Agent(QObject* parent = nullptr);
    ~Agent();

private:
    CamWorker* camWorker;
    QTimer* timer;
};

#endif // AGENT_H
