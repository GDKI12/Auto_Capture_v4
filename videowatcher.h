#ifndef VIDEOWATCHER_H
#define VIDEOWATCHER_H

#include <QObject>
#include <QFileSystemWatcher>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>
#include <QProcess>
#include <QFileInfoList>


class VideoWatcher : public QObject
{
    Q_OBJECT
public:
    explicit VideoWatcher(QObject* parent = nullptr);

    bool mergeRawFiles(const QString& inputDir, const QString& mergedRawPath);
    bool encodeMergedRawToH265(const QString& ffmpegPath, const QString& mergedRawPath, const QString& outputVideoPath,
                               int width, int height, int fps, const QString& pixelFormat);
    void createVideo(const QString& inputDir, const QString& outputPath);
private:
    void deleteFolder(const QString&);

    int extractNumber(const QString&);



signals:
    void requestSend(QString);



};

#endif // VIDEOWATCHER_H
