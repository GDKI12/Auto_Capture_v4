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

        std::string rootDir = toml::find<std::string>(data, "setting", "root_dir");
        std::string saveDir = toml::find<std::string>(data, "setting", "save_path");
        std::string dstIP = toml::find<std::string>(data,"setting","dst_ip");
        int timeInteval = toml::find<int>(data, "setting", "time_inteval");
        int videoLength = toml::find<int>(data, "setting", "video_length");
        int dstPort = toml::find<int>(data, "setting", "dst_port");

        params.insert("rootDir", QString::fromStdString(rootDir));
        params.insert("saveDir", QString::fromStdString(saveDir));
        params.insert("dstIP", QString::fromStdString(dstIP));
        params.insert("videoLength", QString::number(timeInteval));
        params.insert("timeInterval", QString::number(videoLength));
        params.insert("dstPort", QString::number(dstPort));

        return params;
    } catch (const std::exception& e) {
        std::cout << "TOML error:" << e.what();
        return params;
    }
}


