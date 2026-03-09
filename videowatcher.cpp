#include "videowatcher.h"
#include "config.h"

#include <QDebug>
#include <QDir>
#include <QProcess>
#include <opencv2/opencv.hpp>
VideoWatcher::VideoWatcher(QObject* parent) : QObject(parent)
{

}

VideoWatcher& VideoWatcher::getInstance()
{
    static VideoWatcher instance(nullptr);
    return instance;

}

void VideoWatcher::setWatcher(QString path)
{

    client = new TCPHandler();

    QMap<QString, QString> params = Config::getParmas();
    convertProgram = params["convScript"];
    encodeProgram = params["encodeScript"];
    savePath = params["saveDir"];
    trashPath = params["rootDir"];

    watcher = new QFileSystemWatcher(this);
    rootPath = path;
    watcher->addPath(path);
    QDir dir(rootPath);

    prevFolders = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    connect(watcher, &QFileSystemWatcher::directoryChanged, this, &VideoWatcher::dirChanged);
    connect(this, &VideoWatcher::requestSend, client, &TCPHandler::sendVideo);
}

bool VideoWatcher::isReady(QString rootPath)
{

}

void VideoWatcher::dirChanged(const QString& path)
{
    QDir changedDir(path);
    QStringList l = changedDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    QStringList rawFilter;
    rawFilter << "*.raw" << "*.enc";

    if(l.size() <= prevFolders.size())
    {
        prevFolders = l;
        return;
    }

    for(QString d: l)
    {
        if(!prevFolders.contains(d))
        {
            QString addedPath = path + "/" + d;
            QDir addedDir(addedPath);

            QStringList subDirs = addedDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

            qDebug() << "add Dir : " << addedPath;
            qDebug() << "sub Dir";
            for(QString subDir : subDirs)
            {

            }
            process();
        }
    }
}


void VideoWatcher::process()
{
    convertRAWtoPNG();
//    qDebug() << "changed!";
}

void VideoWatcher::convertRAWtoPNG()
{
    QProcess* process = new QProcess();
    QString program = "/home/tesla/miniconda3/envs/cscho/bin/python";
    QStringList arguments;
    arguments << "/home/tesla/cho/transform_source_data_and_validate_and_fix_sorce_data.py"
              << "--source"
              << "/home/tesla/cho/test";

    process->start(program, arguments);

    if(!process->waitForStarted())
    {
        qDebug() << "Python process start failed";
        return;
    }

    process->waitForFinished();

    QString stdoutData = process->readAllStandardOutput();
    QString stderrData = process->readAllStandardError();

    QString jsonStr = stdoutData.trimmed();
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8(), &err);

    QStringList pathList;

    if(err.error == QJsonParseError::NoError && doc.isObject())
    {
        QJsonObject obj = doc.object();

        for(const QString& key : obj.keys())
            pathList << obj.value(key).toString();
    }


    qDebug() << "stdout: " << stdoutData;
    qDebug() << "stderr: " << stderrData;

    for(QString s : pathList)
    {
        createVideo(s);
    }



}

// Slot : After convert raw to png
void VideoWatcher::createVideo(QString path)
{
    QString cameraRootPath = path + "/camera";
    QDir rootDir(cameraRootPath);
    QStringList camFilter;

    QString sceneName;
    QStringList k  = path.split("/");

    for(auto itr = k.begin(); itr != k.end(); itr++)
    {
        if((itr+1) == k.end())
             sceneName = *itr;
    }

    camFilter << "cam*";
    QStringList camFolderList = rootDir.entryList(camFilter, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    for(QString cam : camFolderList)
    {
        QString pngPath = cameraRootPath + "/" + cam;
        QDir dir(pngPath);
        QStringList filters;
        QString outputPath = savePath + "/" + sceneName + "_" + QString("%1Video.mp4").arg(cam);
        filters << "*.png";

        QStringList files = dir.entryList(filters, QDir::Files, QDir::Name);

        if(files.isEmpty())
        {
            qDebug() << "No Png files found";
            return;
        }

        QString firstImagePath = dir.filePath(files[0]);
        cv::Mat frame = cv::imread(firstImagePath.toStdString());

        int width = frame.cols;
        int height = frame.rows;

        cv::VideoWriter video(
                    outputPath.toStdString(),
                    cv::VideoWriter::fourcc('m', 'p', '4', 'v'),
                    10,
                    cv::Size(width, height)
                    );

        if (!video.isOpened())
        {
            qDebug() << "Failed to open VideoWriter:" << outputPath;
            return;
        }

        for(const QString& file : files)
        {
            QString path = dir.filePath(file);
            cv::Mat img = cv::imread(path.toStdString());

            if(img.empty())
                continue;

            video.write(img);
        }

        video.release();

        qDebug() << "Complete Create Video!";
        encode(outputPath);
    }


}


// encoding file
void VideoWatcher::encode(QString videoPath)
{
    QProcess* process = new QProcess();
    QString program = "/bin/bash";
    QStringList arguments;
    arguments << "/home/tesla/cho/encode.sh"
              << videoPath;

    process->start(program, arguments);
    if(!process->waitForStarted())
    {
        qDebug() << "Encode process start failed";
        return;
    }

    process->waitForFinished();
    qDebug() << "Complete Encode Successfully!!";
}









