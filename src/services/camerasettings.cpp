#include "camerasettings.h"

#include <algorithm>
#include <utility>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>

CameraSettings::CameraSettings(QString configPath)
    : m_configPath(std::move(configPath))
{
}

QString CameraSettings::cameraIp() const
{
    QSettings settings(m_configPath, QSettings::IniFormat);
    return settings.value(QStringLiteral("camera/camera_ip")).toString();
}

bool CameraSettings::saveLastOctet(const QString &lastOctetText, QString &newIp,
                                   QString &errorMessage) const
{
    bool ok = false;
    const int lastOctet = lastOctetText.toInt(&ok);
    if (!ok || lastOctet < 0 || lastOctet > 255) {
        errorMessage = QStringLiteral("Enter a valid last octet from 0 to 255.");
        return false;
    }
    QStringList parts = cameraIp().split(QLatin1Char('.'));
    if (parts.size() != 4) {
        parts = {QStringLiteral("172"), QStringLiteral("20"),
                 QStringLiteral("35"), QStringLiteral("0")};
    }
    parts[3] = QString::number(lastOctet);
    newIp = parts.join(QLatin1Char('.'));
    QSettings settings(m_configPath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("camera/camera_ip"), newIp);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        errorMessage = QStringLiteral("Failed to save camera_config.ini.");
        return false;
    }
    return true;
}

QStringList CameraSettings::rtspUrls(const QString &profileOverride) const
{
    QStringList urls(4);
    const QString commonUrl = qEnvironmentVariable("RTSP_URL", qEnvironmentVariable("HANWHA_RTSP_URL"));
    const QString highUrl = qEnvironmentVariable("RTSP_HIGH_URL");
    const QString lowUrl = qEnvironmentVariable("RTSP_LOW_URL");
    const QStringList channelEnvNames = {QStringLiteral("RTSP_CH1_URL"), QStringLiteral("RTSP_CH2_URL"),
                                         QStringLiteral("RTSP_CH3_URL"), QStringLiteral("RTSP_CH4_URL")};
    for (int i = 0; i < channelEnvNames.size(); ++i) {
        QString url = qEnvironmentVariable(channelEnvNames.at(i).toUtf8().constData());
        if (url.isEmpty() && i == 0) url = highUrl;
        if (url.isEmpty() && i == 1) url = lowUrl;
        if (url.isEmpty()) url = commonUrl;
        urls[i] = url;
    }
    if (std::any_of(urls.cbegin(), urls.cend(), [](const QString &url) { return !url.isEmpty(); })) {
        return urls;
    }

    const QString exampleConfig = QDir(QFileInfo(m_configPath).absolutePath())
        .absoluteFilePath(QStringLiteral("camera_config.example.ini"));
    const QString sourcePath = QFile::exists(m_configPath) ? m_configPath : exampleConfig;
    QSettings settings(sourcePath, QSettings::IniFormat);
    const QString configuredUrl = settings.value(QStringLiteral("camera/rtsp_url")).toString();
    if (!configuredUrl.isEmpty() && profileOverride.isEmpty()) {
        urls.fill(configuredUrl);
        return urls;
    }

    const QString cameraIpValue = settings.value(QStringLiteral("camera/camera_ip")).toString();
    const QString user = settings.value(QStringLiteral("camera/username")).toString();
    const QString password = settings.value(QStringLiteral("camera/password")).toString();
    const int port = settings.value(QStringLiteral("camera/rtsp_port"), 554).toInt();
    QString defaultProfile;
    if (profileOverride == QStringLiteral("profile2")) {
        defaultProfile = settings.value(QStringLiteral("camera/high_profile"), profileOverride).toString();
    } else if (profileOverride == QStringLiteral("profile3")) {
        defaultProfile = settings.value(QStringLiteral("camera/low_profile"), profileOverride).toString();
    } else {
        defaultProfile = settings.value(QStringLiteral("camera/low_profile"), QStringLiteral("profile3")).toString();
    }

    for (int i = 0; i < urls.size(); ++i) {
        QString channel = settings.value(QStringLiteral("camera/channel_ch%1").arg(i + 1), QString::number(i)).toString();
        QString profile = defaultProfile;
        if (profileOverride.isEmpty()) {
            profile = settings.value(QStringLiteral("camera/profile_ch%1").arg(i + 1), profile).toString();
        } else {
            const QString legacy = settings.value(QStringLiteral("camera/profile_ch%1").arg(i + 1)).toString();
            const int slash = legacy.indexOf(QLatin1Char('/'));
            if (slash > 0) channel = legacy.left(slash);
        }
        const int slash = profile.indexOf(QLatin1Char('/'));
        if (slash > 0) {
            channel = profile.left(slash);
            profile = profile.mid(slash + 1);
        }
        if (cameraIpValue.isEmpty() || channel.isEmpty() || profile.isEmpty()) {
            urls[i].clear();
            continue;
        }
        QString auth;
        if (!user.isEmpty()) {
            auth = user;
            if (!password.isEmpty()) auth += QLatin1Char(':') + password;
            auth += QLatin1Char('@');
        }
        urls[i] = QStringLiteral("rtsp://%1%2:%3/%4/%5/media.smp")
            .arg(auth, cameraIpValue).arg(port).arg(channel, profile);
    }
    return urls;
}
