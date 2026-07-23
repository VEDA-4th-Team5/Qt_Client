#include "controllers/parkingcontroller.h"
#include "services/notificationcenter.h"

#include <QCoreApplication>
#include <QList>
#include <QTemporaryDir>

namespace {
struct CapturedEvent {
    QString time;
    QString sourceId;
    QString eventType;
    QString message;
    QString status;
};
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid()) return 2;

    ParkingController controller(
        directory.filePath(QStringLiteral("client_config.ini")),
        directory.filePath(QStringLiteral("client_config.local.ini")));
    NotificationCenter notifications;
    QList<CapturedEvent> events;

    QObject::connect(
        &controller, &ParkingController::eventLogged, &app,
        [&](const QString &time, const QString &sourceId,
            const QString &eventType, const QString &message,
            const QString &status) {
            events.append({time, sourceId, eventType, message, status});
            notifications.ingestEvent(time, sourceId, eventType, message, status);
        });

    controller.processIncomingMessage(QStringLiteral("INVALID"));
    if (events.size() != 1) return 3;
    if (events.constLast().eventType != QStringLiteral("RX_ERROR")) return 4;
    if (events.constLast().status != QStringLiteral("REJECTED")) return 5;
    if (notifications.hasNotifications()) return 6;

    controller.processIncomingMessage(QStringLiteral("PARKING_SLOT, P04, OCCUPIED"));
    if (controller.slotState(QStringLiteral("P-04")) != SlotState::Occupied) return 7;
    if (events.constLast().sourceId != QStringLiteral("P-04")) return 8;
    if (events.constLast().eventType != QStringLiteral("OCCUPIED")) return 9;

    controller.processIncomingMessage(
        QStringLiteral("EVENT, P03, HALL_SENSOR_ERROR, OPEN, sensor, disconnected"));
    if (events.constLast().sourceId != QStringLiteral("P-03")) return 10;
    if (events.constLast().message != QStringLiteral("sensor, disconnected")) return 11;
    if (notifications.notifications().size() != 1) return 12;
    if (notifications.notifications().constFirst().severity != QStringLiteral("WARNING")) return 13;

    controller.processIncomingMessage(
        QStringLiteral("EVENT, P03, HALL_SENSOR_CLEAR, CLEARED, sensor recovered"));
    if (notifications.hasNotifications()) return 14;

    controller.processIncomingMessage(QStringLiteral("FIRE_ALARM, CH2, DETECTED"));
    if (events.constLast().eventType != QStringLiteral("FIRE_ALARM")) return 15;
    if (events.constLast().status != QStringLiteral("OPEN")) return 16;
    if (notifications.notifications().size() != 1) return 17;
    if (notifications.notifications().constFirst().severity != QStringLiteral("CRITICAL")) return 18;

    controller.processIncomingMessage(QStringLiteral("FIRE_ALARM, CH2, CLEAR"));
    if (events.constLast().eventType != QStringLiteral("FIRE_ALARM_ACK")) return 19;
    if (events.constLast().status != QStringLiteral("ACKED")) return 20;
    if (notifications.hasNotifications()) return 21;

    return 0;
}
