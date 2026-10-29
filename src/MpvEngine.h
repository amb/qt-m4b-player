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
    bool setEqualizer(bool enabled, double tone, double frequency, double boost = 0);
    QString version() const;

signals:
    void propertyChanged(const QString &name, const QVariant &value);
    void fileLoaded(const QString &path);
    void failed(const QString &message);
    void equalizerFailed(const QString &message);

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
    quint64 m_equalizerRequest = 0;
};
