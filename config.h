#ifndef CONFIG_H
#define CONFIG_H

#include <QString>
#include <QMap>
#include <toml.hpp>
#include <iostream>

constexpr const char* DEFAULT_PATH = "/home/tesla/cscho/Auto_Capture_v4";
constexpr const char* FILE_NAME = "config.toml";
constexpr const char* SENSOR_LIST_FILE = "sensor_list.json";

class Config
{
public:
    static QString defaultFile();
    static QMap<QString, QString> getParmas();
    static QString getSensorListFile();
};

#endif // CONFIG_H
