#ifndef DEFINE_H
#define DEFINE_H

#include <iostream>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <QByteArray>
#include <QString>
#include <iostream>
#include <toml.hpp>
#include <QDebug>
#include <QFile>
#include <QJsonParseError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

const QString DEFAULT_PATH = "/home/tesla/cscho/Auto_Capture_v4/config.toml";
const QString FILE_NAME = "config.toml";
const QString SENSOR_LIST_FILE = "/home/tesla/EdgeInfravision/EdgeInfra_Capture_v4/config/sensor_list.json";

enum class LogLevel{
    INFO, WARN, ERROR
};

class Config
{
public:
    Config() = default;
    ~Config() = default;
    void loadConfig()
    {
        try
        {
            auto data = toml::parse(DEFAULT_PATH.toStdString());

            std::string cRootPath;
            std::string cDstIp;

            cRootPath = toml::find<std::string>(data, "setting", "root_path");
            cDstIp = toml::find<std::string>(data, "setting","dst_ip");

            rootPath = QString::fromStdString(cRootPath);
            ip = QString::fromStdString(cDstIp);

            port = toml::find<int>(data, "setting","dst_port");

            timeInterval = toml::find<int>(data, "setting","time_interval");
            timeInterval *= 1000;

            videoLength = toml::find<int>(data, "setting","video_length");
            videoLength = videoLength * 10;

            mode = toml::find<bool>(data, "setting","live_mode");
            width = toml::find<int>(data, "setting", "width");
            height = toml::find<int>(data, "setting", "height");

            rawSize = toml::find<int>(data, "setting", "save_data_time");
            rawSize *= 1000;
            QFile snesorListFile(SENSOR_LIST_FILE);



        } catch (const std::exception& e)
        {
            qCritical() << "failed to load config settings";
        }
    }
public:
    QString rootPath;
    QString ip;
    int port;
    int videoLength;
    int timeInterval;
    int width;
    int height;
    bool mode;
    int rawSize;
};

#endif // DEFINE_H


