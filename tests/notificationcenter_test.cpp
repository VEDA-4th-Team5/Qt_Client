#include "services/notificationcenter.h"

#include <QCoreApplication>
#include <QDateTime>

namespace {
MonitoringEvent makeEvent(const QString &sourceId,
                          const QString &eventType,
                          const QString &status,
                          const QString &message,
                          EventSeverity severity = EventSeverity::Unknown)
{
    MonitoringEvent event;
    event.id = sourceId + QLatin1Char('-') + eventType;
    event.occurredAt = QDateTime::currentDateTime();
    event.sourceId = sourceId;
    event.eventType = eventType;
    event.severity = severity;
    event.ackState = eventAckStateFromStatus(status);
    event.message = message;
    event.status = status;
    return event;
}
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    NotificationCenter center;
    int changeCount = 0;

    QObject::connect(&center, &NotificationCenter::notificationsChanged,
                     &app, [&changeCount]() { ++changeCount; });

    center.ingestEvent(makeEvent(QStringLiteral("SYSTEM"),
                                 QStringLiteral("RX_ERROR"),
                                 QStringLiteral("REJECTED"),
                                 QStringLiteral("invalid input")));
    if (center.hasNotifications() || changeCount != 0) return 1;

    center.ingestEvent(makeEvent(QStringLiteral("P-03"),
                                 QStringLiteral("HALL_SENSOR_ERROR"),
                                 QStringLiteral("OPEN"),
                                 QStringLiteral("sensor disconnected")));
    if (center.notifications().size() != 1 || changeCount != 1) return 2;
    if (center.notifications().constFirst().severity != QStringLiteral("WARNING")) return 3;
    if (center.notifications().constFirst().title != QStringLiteral("Hall sensor error")) return 4;
    if (center.unreadCount() != 1) return 5;

    center.ingestEvent(makeEvent(QStringLiteral("P-03"),
                                 QStringLiteral("HALL_SENSOR_ERROR"),
                                 QStringLiteral("OPEN"),
                                 QStringLiteral("sensor still disconnected")));
    if (center.notifications().size() != 1 || changeCount != 2) return 6;
    if (center.notifications().constFirst().message
        != QStringLiteral("sensor still disconnected")) return 7;

    center.ingestEvent(makeEvent(QStringLiteral("P-04"),
                                 QStringLiteral("HALL_SENSOR_ERROR"),
                                 QStringLiteral("OPEN"),
                                 QStringLiteral("other sensor disconnected")));
    if (center.notifications().size() != 2) return 8;

    center.ingestEvent(makeEvent(QStringLiteral("P-03"),
                                 QStringLiteral("HALL_SENSOR_CLEAR"),
                                 QStringLiteral("CLEARED"),
                                 QStringLiteral("sensor recovered")));
    if (center.notifications().size() != 1) return 9;
    if (center.notifications().constFirst().sourceId != QStringLiteral("P-04")) return 10;

    center.ingestEvent(makeEvent(QStringLiteral("CH2"),
                                 QStringLiteral("FIRE_EVENT"),
                                 QStringLiteral("OPEN"),
                                 QStringLiteral("fire detected")));
    if (center.notifications().constFirst().eventType != QStringLiteral("FIRE_ALARM")) return 11;
    if (center.notifications().constFirst().severity != QStringLiteral("CRITICAL")) return 12;

    center.ingestEvent(makeEvent(QStringLiteral("CH2"),
                                 QStringLiteral("FLAME"),
                                 QStringLiteral("OPEN"),
                                 QStringLiteral("flame detected")));
    if (center.notifications().size() != 3) return 13;

    center.ingestEvent(makeEvent(QStringLiteral("CH2"),
                                 QStringLiteral("FIRE_ALARM_ACK"),
                                 QStringLiteral("ACKED"),
                                 QStringLiteral("operator acknowledged")));
    if (center.notifications().size() != 1) return 14;
    if (center.notifications().constFirst().sourceId != QStringLiteral("P-04")) return 15;

    center.markAllRead();
    if (center.unreadCount() != 0) return 16;
    const int countAfterRead = changeCount;
    center.markAllRead();
    if (changeCount != countAfterRead) return 17;

    center.clearAll();
    if (center.hasNotifications()) return 18;

    for (int i = 0; i < 12; ++i) {
        center.ingestEvent(makeEvent(QStringLiteral("SOURCE-%1").arg(i),
                                     QStringLiteral("API_ERROR"),
                                     QStringLiteral("FAILED"),
                                     QStringLiteral("API failure")));
    }
    if (center.notifications().size() != 10) return 19;
    if (center.notifications().constFirst().sourceId != QStringLiteral("SOURCE-11")) return 20;
    if (center.notifications().constLast().sourceId != QStringLiteral("SOURCE-2")) return 21;

    center.ingestEvent(makeEvent(QStringLiteral("ALL"),
                                 QStringLiteral("ALARM_ACK"),
                                 QStringLiteral("ACKED"),
                                 QStringLiteral("all alarms acknowledged")));
    if (center.hasNotifications()) return 22;

    center.ingestEvent(makeEvent(QStringLiteral("CH1"),
                                 QStringLiteral("CAMERA_DISCONNECTED"),
                                 QStringLiteral("FAILED"),
                                 QStringLiteral("camera disconnected")));
    if (center.notifications().size() != 1) return 23;
    center.ingestEvent(makeEvent(QStringLiteral("CH1"),
                                 QStringLiteral("CAMERA_RECONNECTED"),
                                 QStringLiteral("DONE"),
                                 QStringLiteral("camera reconnected")));
    if (center.hasNotifications()) return 24;

    return 0;
}
