#include "config.h"

#include <QDebug>
#include <toml.hpp>


QString Config::defaultFile()
{
    return QString("%1/%2").arg(DEFAULT_PATH).arg(FILE_NAME);
}

QMap<QString, QString> Config::getParmas()
{
    QMap<QString, QString> params;
    try {
        QString configPath = defaultFile();
        auto data = toml::parse(configPath.toStdString());

        std::string fileDir = toml::find<std::string>(data, "setting", "file_path");
        std::string dstIP = toml::find<std::string>(data,"setting","dst_ip");
        int timeInteval = toml::find<int>(data, "setting", "time_inteval");
        int videoLength = toml::find<int>(data, "setting", "video_length");
        int dstPort = toml::find<int>(data, "setting", "dst_port");

        params.insert("fileDir", QString::fromStdString(fileDir));
        params.insert("dstIP", QString::fromStdString(dstIP));
        params.insert("videoLength", QString::number(videoLength));
        params.insert("timeInterval", QString::number(timeInteval));
        params.insert("dstPort", QString::number(dstPort));

        return params;
    } catch (const std::exception& e) {
        std::cout << "TOML error:" << e.what();
        return params;
    }
}

QString Config::getSensorListFile()
{
    return "/home/tesla/EdgeInfravision/EdgeInfra_Capture_v4/config/sensor_list.json";
}

bool Config::getModeType()
{
    try {
        QString configPath = defaultFile();
        auto data = toml::parse(configPath.toStdString());

        bool mode = toml::find<bool>(data, "setting", "live_mode");
        return mode;
    } catch (const std::exception& e) {
        std::cout << "error: " << e.what();
    }
}

