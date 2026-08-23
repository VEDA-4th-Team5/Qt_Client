#include "services/camerasettings.h"

#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>

#include <iostream>
#include <optional>

namespace {
bool require(bool condition, const char *message)
{
    if (condition) return true;
    std::cerr << "FAIL: " << message << '\n';
    return false;
}
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTemporaryDir temporaryDir;
    if (!require(temporaryDir.isValid(), "temporary config directory must be created")) return 1;

    const QString configPath = temporaryDir.filePath(QStringLiteral("camera_config.ini"));
    {
        QSettings initial(configPath, QSettings::IniFormat);
        initial.setValue(QStringLiteral("camera/camera_ip"), QStringLiteral("192.168.1.20"));
        initial.setValue(QStringLiteral("camera/username"), QStringLiteral("local-user"));
        initial.setValue(QStringLiteral("camera/password"), QStringLiteral("local-password"));
        initial.setValue(QStringLiteral("camera/rtsp_port"), 554);
        initial.setValue(QStringLiteral("camera/channel_ch1"), 0);
        initial.setValue(QStringLiteral("camera/high_profile"), QStringLiteral("profile2"));
        initial.setValue(QStringLiteral("camera/low_profile"), QStringLiteral("profile3"));
        initial.setValue(QStringLiteral("camera/https_certificate_sha256"),
                         QStringLiteral("AA:BB"));
        initial.sync();
    }

    CameraSettings settings(configPath);
    QString savedIp;
    QString error;
    if (!require(settings.saveCameraCredentials(
                     QStringLiteral("10.40.2.77"), QStringLiteral("saved-user"),
                     QStringLiteral("saved-password"), savedIp, error),
                 "camera IP and credentials must be saved together")) return 1;
    if (!require(savedIp == QStringLiteral("10.40.2.77"),
                 "saved address must not preserve an old subnet prefix")) return 1;
    if (!require(settings.cameraIp() == QStringLiteral("10.40.2.77"),
                 "the complete IPv4 address must be persisted")) return 1;
    if (!require(settings.cameraUsername() == QStringLiteral("saved-user")
                     && settings.cameraPassword() == QStringLiteral("saved-password"),
                 "saved camera credentials must be reloaded from the local config")) return 1;
    const QStringList savedRtspUrls = settings.rtspUrls(QStringLiteral("profile2"));
    if (!require(savedRtspUrls.value(0)
                     == QStringLiteral("rtsp://saved-user:saved-password@10.40.2.77:554/0/profile2/media.smp"),
                 "RTSP URL generation must use the newly saved camera credentials")) return 1;
    if (!require(settings.httpsCertificateSha256() == QStringLiteral("AA:BB"),
                 "camera certificate pin must be read from the local-only config")) return 1;

    error.clear();
    if (!require(!settings.saveCameraCredentials(
                     QStringLiteral("10.40.2.77"), QString(),
                     QStringLiteral("saved-password"), savedIp, error),
                 "an empty camera username must be rejected")) return 1;
    error.clear();
    if (!require(settings.saveCameraCredentials(
                     QStringLiteral("10.40.2.77"), QStringLiteral("saved-user"),
                     QString(), savedIp, error),
                 "an empty password input must preserve the configured password")) return 1;
    if (!require(settings.cameraPassword() == QStringLiteral("saved-password"),
                 "blank password input must not clear the runtime password")) return 1;
    error.clear();
    if (!require(!settings.saveCameraIp(QStringLiteral("172.20.35"), savedIp, error),
                 "an incomplete address must be rejected")) return 1;
    if (!require(settings.cameraIp() == QStringLiteral("10.40.2.77"),
                 "invalid input must not overwrite the saved address")) return 1;

    error.clear();
    if (!require(!settings.saveCameraIp(QStringLiteral("999.1.1.1"), savedIp, error),
                 "an out-of-range IPv4 address must be rejected")) return 1;

    const QByteArray previousEnvironmentPassword =
        qgetenv("HANWHA_CAMERA_PASSWORD");
    const bool environmentPasswordWasSet =
        qEnvironmentVariableIsSet("HANWHA_CAMERA_PASSWORD");
    qunsetenv("HANWHA_CAMERA_PASSWORD");

    const QString preservePath = temporaryDir.filePath(
        QStringLiteral("camera_preserve.ini"));
    {
        QSettings initial(preservePath, QSettings::IniFormat);
        initial.setValue(QStringLiteral("camera/camera_ip"),
                         QStringLiteral("192.168.1.30"));
        initial.setValue(QStringLiteral("camera/username"),
                         QStringLiteral("preserved-user"));
        initial.setValue(QStringLiteral("camera/password"),
                         QStringLiteral("preserved-password"));
        initial.setValue(QStringLiteral("camera/https_certificate_sha256"),
                         QStringLiteral("CC:DD"));
        initial.sync();
    }
    CameraSettings preserveSettings(preservePath);
    error.clear();
    if (!require(preserveSettings.hasCameraPassword(),
                 "configured camera password state must be queryable")) return 1;
    if (!require(preserveSettings.saveCameraCredentials(
                     QStringLiteral("10.40.2.88"),
                     QStringLiteral("preserved-user"), std::nullopt,
                     savedIp, error),
                 "omitted replacement must preserve the existing password")) return 1;
    {
        QSettings saved(preservePath, QSettings::IniFormat);
        if (!require(saved.value(QStringLiteral("camera/password")).toString()
                         == QStringLiteral("preserved-password"),
                     "preserve mode must not rewrite the password key")) return 1;
        if (!require(saved.value(
                         QStringLiteral("camera/https_certificate_sha256"))
                         .toString() == QStringLiteral("CC:DD"),
                     "camera save must retain the certificate pin")) return 1;
    }
    if (!require(
            preserveSettings.rtspUrls(QStringLiteral("profile2")).value(0)
                .contains(QStringLiteral("preserved-user:preserved-password@")),
            "preserved password must remain available to camera clients")) return 1;

    error.clear();
    if (!require(!preserveSettings.saveCameraCredentials(
                     QStringLiteral("10.40.2.180"),
                     QStringLiteral("preserved-user"),
                     std::optional<QString>(QString()), savedIp, error),
                 "an explicit empty replacement must be rejected")) return 1;
    if (!require(preserveSettings.cameraIp() == QStringLiteral("10.40.2.88"),
                 "rejected replacement must not partially change runtime state")) return 1;

    error.clear();
    if (!require(preserveSettings.saveCameraCredentials(
                     QStringLiteral("10.40.2.89"),
                     QStringLiteral("replacement-user"),
                     std::optional<QString>(
                         QStringLiteral("replacement-password")),
                     savedIp, error),
                 "explicit replacement must update the camera password")) return 1;
    {
        QSettings saved(preservePath, QSettings::IniFormat);
        if (!require(saved.value(QStringLiteral("camera/password")).toString()
                         == QStringLiteral("replacement-password"),
                     "replacement password must be persisted locally")) return 1;
    }

    const QString missingPasswordPath = temporaryDir.filePath(
        QStringLiteral("camera_missing_password.ini"));
    {
        QSettings initial(missingPasswordPath, QSettings::IniFormat);
        initial.setValue(QStringLiteral("camera/camera_ip"),
                         QStringLiteral("192.168.1.40"));
        initial.setValue(QStringLiteral("camera/username"),
                         QStringLiteral("no-password-user"));
        initial.sync();
    }
    CameraSettings missingPasswordSettings(missingPasswordPath);
    error.clear();
    if (!require(!missingPasswordSettings.saveCameraCredentials(
                     QStringLiteral("10.40.2.90"),
                     QStringLiteral("new-user"), QString(),
                     savedIp, error),
                 "preserve mode must fail when no password source exists")) return 1;
    {
        QSettings saved(missingPasswordPath, QSettings::IniFormat);
        if (!require(saved.value(QStringLiteral("camera/camera_ip")).toString()
                         == QStringLiteral("192.168.1.40")
                         && saved.value(QStringLiteral("camera/username")).toString()
                             == QStringLiteral("no-password-user"),
                     "failed preserve validation must not partially save fields")) return 1;
    }

    qputenv("HANWHA_CAMERA_PASSWORD", "environment-only-password");
    const QString environmentPath = temporaryDir.filePath(
        QStringLiteral("camera_environment.ini"));
    {
        QSettings initial(environmentPath, QSettings::IniFormat);
        initial.setValue(QStringLiteral("camera/camera_ip"),
                         QStringLiteral("192.168.1.50"));
        initial.setValue(QStringLiteral("camera/username"),
                         QStringLiteral("environment-user"));
        initial.sync();
    }
    CameraSettings environmentSettings(environmentPath);
    error.clear();
    const bool environmentSaveSucceeded =
        environmentSettings.saveCameraCredentials(
            QStringLiteral("10.40.2.91"),
            QStringLiteral("environment-user"), QString(),
            savedIp, error);
    QSettings environmentSaved(environmentPath, QSettings::IniFormat);
    const bool environmentPasswordCopied = environmentSaved.contains(
        QStringLiteral("camera/password"));
    if (environmentPasswordWasSet) {
        qputenv("HANWHA_CAMERA_PASSWORD", previousEnvironmentPassword);
    } else {
        qunsetenv("HANWHA_CAMERA_PASSWORD");
    }
    if (!require(environmentSaveSucceeded,
                 "environment password must satisfy preserve validation")) return 1;
    if (!require(!environmentPasswordCopied,
                 "environment password must never be copied into the INI file")) return 1;

    std::cout << "PASS: camera IP uses a complete validated IPv4 address without a fixed subnet\n";
    return 0;
}
