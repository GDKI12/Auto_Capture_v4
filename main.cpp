#include <QCoreApplication>
#include "agent.h"
#include "videowatcher.h"
int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

//    QString inputFolder = "/home/tesla/cscho/result/cam0";
//    QString outputVideo = "/home/tesla/cscho/output.mp4";
//    VideoWatcher watcher;

//    watcher.createVideo(inputFolder, outputVideo);
    Agent agent;
    return a.exec();
}
