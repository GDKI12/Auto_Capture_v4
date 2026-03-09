#ifndef CONFIG_H
#define CONFIG_H

#include <QString>
#include <QMap>
#include <toml.hpp>
#include <iostream>

constexpr const char* DEFAULT_PATH = "/home/tesla/cho/Auto_Capture_v4";
constexpr const char* FILE_NAME = "config.toml";

class Config
{
public:
    static QString defaultFile();
    static QMap<QString, QString> getParmas();

};

#endif // CONFIG_H
