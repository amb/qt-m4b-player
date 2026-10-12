#pragma once

#include <QObject>
#include <QHash>
#include <QTimer>
#include <QVariant>
#include <mpv/client.h>

class MpvEngine : public QObject {
    Q_OBJECT
public:
    explicit MpvEngine(bool silent = false, QObject *parent = nullptr);
    ~MpvEngine() override;
    bool load(const QString &path);
    bool seek(double seconds);
    bool setPaused(bool paused);
    bool setSpeed(double speed);
    bool setVolume(double volume);
    QString version() const;

signals:
    void propertyChanged(const QString &name, const QVariant &value);
    void fileLoaded(const QString &path);
    void failed(const QString &message);

private:
    bool check(int status);
    bool command(const char **args);
    bool property(const char *name, mpv_format format, void *value);
    void drainEvents();
    mpv_handle *m_handle = nullptr;
    QTimer m_events;
    quint64 m_generation = 0;
    quint64 m_nextRequest = 0;
    QHash<quint64, quint64> m_requests;
};
