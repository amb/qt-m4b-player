#pragma once

#include <QObject>
#include <QSettings>
#include <QtQml/qqmlregistration.h>

class AppPreferences : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(AppPreferences)
    QML_UNCREATABLE("The application owns its preferences")
    Q_PROPERTY(int captionSize READ captionSize WRITE setCaptionSize NOTIFY captionSizeChanged)
    Q_PROPERTY(bool fillCover READ fillCover WRITE setFillCover NOTIFY fillCoverChanged)
public:
    explicit AppPreferences(const QString &settingsPath, QObject *parent = nullptr);
    int captionSize() const { return m_captionSize; }
    bool fillCover() const { return m_fillCover; }
    void setCaptionSize(int size);
    void setFillCover(bool fill);
signals:
    void captionSizeChanged();
    void fillCoverChanged();
private:
    QSettings m_settings;
    int m_captionSize = 26;
    bool m_fillCover = true;
};
