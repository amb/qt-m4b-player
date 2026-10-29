#include "MpvEngine.h"
#include "TiltEqualizer.h"

#include <QFile>
#include <stdexcept>

MpvEngine::MpvEngine(bool silent, QObject *parent) : QObject(parent), m_handle(mpv_create())
{
    if (!m_handle)
        throw std::runtime_error("Cannot create libmpv player");
    const auto option = [this](const char *name, const char *value, bool required = true) {
        const int status = mpv_set_option_string(m_handle, name, value);
        // A trimmed libmpv build may omit optional features, in which case their
        // option is unknown (MPV_ERROR_OPTION_NOT_FOUND). Only essentials are fatal.
        if (status < 0 && (required || status != MPV_ERROR_OPTION_NOT_FOUND))
            throw std::runtime_error(QString("libmpv option %1: %2").arg(name, mpv_error_string(status)).toStdString());
    };
    try {
        option("config", "no");
        // Scripting may be compiled out, so this is best-effort.
        option("load-scripts", "no", false);
        option("vo", "null");
        option("video", "no");
        option("sid", "no");
        option("audio-display", "no");
        option("idle", "yes");
        option("keep-open", "yes");
        option("pause", "yes");
        option("audio-pitch-correction", "yes");
        if (silent)
            option("ao", "null");
        const int status = mpv_initialize(m_handle);
        if (status < 0)
            throw std::runtime_error(mpv_error_string(status));
        mpv_observe_property(m_handle, 1, "time-pos", MPV_FORMAT_DOUBLE);
        mpv_observe_property(m_handle, 2, "duration", MPV_FORMAT_DOUBLE);
        mpv_observe_property(m_handle, 3, "pause", MPV_FORMAT_FLAG);
        mpv_observe_property(m_handle, 4, "speed", MPV_FORMAT_DOUBLE);
        mpv_observe_property(m_handle, 5, "volume", MPV_FORMAT_DOUBLE);
        mpv_observe_property(m_handle, 6, "eof-reached", MPV_FORMAT_FLAG);
        connect(&m_events, &QTimer::timeout, this, &MpvEngine::drainEvents);
        m_events.start(20);
    } catch (...) {
        mpv_terminate_destroy(m_handle);
        m_handle = nullptr;
        throw;
    }
}

MpvEngine::~MpvEngine()
{
    m_events.stop();
    if (m_handle)
        mpv_terminate_destroy(m_handle);
}

bool MpvEngine::check(int status)
{
    if (status >= 0)
        return true;
    emit failed(QString::fromUtf8(mpv_error_string(status)));
    return false;
}

bool MpvEngine::load(const QString &path)
{
    ++m_generation;
    setPaused(true);
    const auto filename = QFile::encodeName(path);
    const char *args[] = {"loadfile", filename.constData(), "replace", nullptr};
    return command(args);
}

bool MpvEngine::seek(double seconds)
{
    const auto time = QByteArray::number(seconds, 'f', 6);
    const char *args[] = {"seek", time.constData(), "absolute+exact", nullptr};
    return command(args);
}

bool MpvEngine::setPaused(bool paused)
{
    int value = paused;
    return property("pause", MPV_FORMAT_FLAG, &value);
}

bool MpvEngine::setSpeed(double speed)
{
    return property("speed", MPV_FORMAT_DOUBLE, &speed);
}

bool MpvEngine::setVolume(double volume)
{
    return property("volume", MPV_FORMAT_DOUBLE, &volume);
}

bool MpvEngine::command(const char **args)
{
    const quint64 request = ++m_nextRequest;
    // libmpv copies command arguments; caller buffers need not outlive submission.
    if (!check(mpv_command_async(m_handle, request, args)))
        return false;
    m_requests.insert(request, m_generation);
    return true;
}

bool MpvEngine::setEqualizer(bool enabled, double tone, double frequency, double boost)
{
    const auto graph = enabled && (tone != 0 || boost > 0) ? TiltEqualizer::filter(tone, frequency, boost).toUtf8() : QByteArray();
    const char *args[] = {"af", "set", graph.constData(), nullptr};
    const quint64 request = ++m_nextRequest;
    const int status = mpv_command_async(m_handle, request, args);
    if (status < 0) {
        emit equalizerFailed(QString::fromUtf8(mpv_error_string(status)));
        return false;
    }
    // EQ belongs to the session, so replies remain relevant across book loads.
    m_equalizerRequest = request;
    return true;
}

bool MpvEngine::property(const char *name, mpv_format format, void *value)
{
    const quint64 request = ++m_nextRequest;
    if (!check(mpv_set_property_async(m_handle, request, name, format, value)))
        return false;
    m_requests.insert(request, m_generation);
    return true;
}

QString MpvEngine::version() const
{
    char *value = mpv_get_property_string(m_handle, "mpv-version");
    const QString result = QString::fromUtf8(value ? value : "unknown");
    mpv_free(value);
    return result;
}

void MpvEngine::drainEvents()
{
    // Events and all application state are processed on the Qt owner thread.
    for (;;) {
        const auto *event = mpv_wait_event(m_handle, 0);
        if (event->event_id == MPV_EVENT_NONE)
            break;
        if (event->event_id == MPV_EVENT_COMMAND_REPLY || event->event_id == MPV_EVENT_SET_PROPERTY_REPLY) {
            if (event->reply_userdata == m_equalizerRequest) {
                m_equalizerRequest = 0;
                if (event->error < 0) {
                    setEqualizer(false, 0, 1000);
                    emit equalizerFailed(QString::fromUtf8(mpv_error_string(event->error)));
                }
                continue;
            }
            const auto request = m_requests.find(event->reply_userdata);
            if (request != m_requests.end()) {
                const bool current = request.value() == m_generation;
                m_requests.erase(request);
                if (current && event->error < 0)
                    check(event->error);
            }
        } else if (event->event_id == MPV_EVENT_FILE_LOADED) {
            char *path = mpv_get_property_string(m_handle, "path");
            emit fileLoaded(QFile::decodeName(path ? path : ""));
            mpv_free(path);
        } else if (event->event_id == MPV_EVENT_PROPERTY_CHANGE) {
            const auto *property = static_cast<mpv_event_property *>(event->data);
            if (!property->data)
                continue;
            QVariant value;
            if (property->format == MPV_FORMAT_DOUBLE)
                value = *static_cast<double *>(property->data);
            else if (property->format == MPV_FORMAT_FLAG)
                value = bool(*static_cast<int *>(property->data));
            if (value.isValid())
                emit propertyChanged(QString::fromUtf8(property->name), value);
        } else if (event->event_id == MPV_EVENT_END_FILE) {
            const auto *end = static_cast<mpv_event_end_file *>(event->data);
            if (end->reason == MPV_END_FILE_REASON_ERROR)
                emit failed(QString::fromUtf8(mpv_error_string(end->error)));
        }
    }
}
