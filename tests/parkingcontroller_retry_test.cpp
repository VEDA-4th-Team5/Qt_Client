#include "controllers/parkingcontroller.h"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid()) return 2;

    const QString sharedPath = directory.filePath(QStringLiteral("client_config.ini"));
    QFile config(sharedPath);
    if (!config.open(QIODevice::WriteOnly | QIODevice::Text)) return 3;
    QTextStream stream(&config);
    stream << "[api]\n"
           << "enabled=true\n"
           << "base_url=http://127.0.0.1:1\n"
           << "slots_path=/api/v1/parking-slots\n"
           << "slot_detail_path=/api/v1/parking-slots/{slot_id}\n"
           << "timeout_ms=200\n"
           << "reconnect_interval_ms=1000\n"
           << "max_reconnect_interval_ms=1000\n"
           << "allow_insecure_http=true\n";
    config.close();

    ParkingController controller(
        sharedPath, directory.filePath(QStringLiteral("client_config.local.ini")));
    int connectingCount = 0;
    bool retryScheduled = false;
    QObject::connect(&controller, &ParkingController::serverConnectionChanged,
                     &app, [&](const QString &status, bool) {
        if (status == QStringLiteral("Connecting...")) ++connectingCount;
        if (status.contains(QStringLiteral("retry in"))) retryScheduled = true;
        if (retryScheduled && connectingCount >= 2) app.exit(0);
    });
    QTimer::singleShot(4000, &app, [&]() {
        app.exit(retryScheduled && connectingCount >= 2 ? 0 : 1);
    });
    controller.start();
    return app.exec();
}
