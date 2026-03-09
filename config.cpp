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
        std::string convertScript = toml::find<std::string>(data, "setting", "convert_script");
        std::string encodeScript = toml::find<std::string>(data, "setting", "encode_script");
        int dstPort = toml::find<int>(data, "setting", "dst_port");

        params.insert("rootDir", QString::fromStdString(rootDir));
        params.insert("saveDir", QString::fromStdString(saveDir));
        params.insert("dstIP", QString::fromStdString(dstIP));
        params.insert("convScript", QString::fromStdString(convertScript));
        params.insert("encodeScript", QString::fromStdString(encodeScript));
        params.insert("dstPort", QString::number(dstPort));

        return params;
    } catch (const std::exception& e) {
        std::cout << "TOML error:" << e.what();
        return params;
    }
}
