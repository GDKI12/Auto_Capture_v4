#ifndef DEFINE_H
#define DEFINE_H

#include <iostream>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <QByteArray>

const char APP_NAME[] = "iox-subscriber-AutoSave";

struct CustomCamDataType {
    double timestamp;
    uint32_t width;
    uint32_t height;
    uint8_t data[2048 * 1536 * 3];
};

struct CamData
{
    double timestamp;
    uint32_t width;
    uint32_t height;
    QByteArray data;
};


enum class Mode
{
    CONTINUOUS, SPLIT
};

#endif // DEFINE_H
