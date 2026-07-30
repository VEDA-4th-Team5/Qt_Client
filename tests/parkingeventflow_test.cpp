#include "controllers/parkingcontroller.h"
#include "services/notificationcenter.h"

#include <QCoreApplication>
#include <QList>
#include <QMetaObject>
#include <QTemporaryDir>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid()) return 2;

    ParkingController controller(
        directory.filePath(QStringLiteral("client_config.ini")),
        directory.filePath(QStringLiteral("client_config.local.ini")));
    NotificationCenter notifications;
    QList<MonitoringEvent> events;

    QObject::connect(
        &controller, &ParkingController::eventLogged, &app,
        [&](const MonitoringEvent &event) {
            events.append(event);
            notifications.ingestEvent(event);
        });

    controller.processIncomingMessage(QStringLiteral("INVALID"));
    if (events.size() != 1) return 3;
    if (events.constLast().id.isEmpty() || !events.constLast().occurredAt.isValid()) return 4;
    if (events.constLast().eventType != QStringLiteral("RX_ERROR")) return 5;
    if (events.constLast().status != QStringLiteral("REJECTED")) return 6;
    if (events.constLast().ackState != EventAckState::None) return 7;
    if (notifications.hasNotifications()) return 8;

    controller.processIncomingMessage(QStringLiteral("PARKING_SLOT, P04, OCCUPIED"));
    if (controller.slotState(QStringLiteral("P-04")) != SlotState::Occupied) return 9;
    if (events.constLast().sourceId != QStringLiteral("P-04")) return 10;
    if (events.constLast().eventType != QStringLiteral("OCCUPIED")) return 11;

    controller.processIncomingMessage(
        QStringLiteral("EVENT, P03, HALL_SENSOR_ERROR, OPEN, sensor, disconnected"));
    if (events.constLast().sourceId != QStringLiteral("P-03")) return 12;
    if (events.constLast().message != QStringLiteral("sensor, disconnected")) return 13;
    if (events.constLast().ackState != EventAckState::Open) return 14;
    if (notifications.notifications().size() != 1) return 15;
    if (notifications.notifications().constFirst().severity != QStringLiteral("WARNING")) return 16;

    controller.processIncomingMessage(
        QStringLiteral("EVENT, P03, HALL_SENSOR_CLEAR, CLEARED, sensor recovered"));
    if (events.constLast().ackState != EventAckState::Cleared) return 17;
    if (notifications.hasNotifications()) return 18;

    controller.processIncomingMessage(QStringLiteral("FIRE_ALARM, CH2, DETECTED"));
    if (events.constLast().eventType != QStringLiteral("FIRE_ALARM")) return 19;
    if (events.constLast().status != QStringLiteral("OPEN")) return 20;
    if (notifications.notifications().size() != 1) return 21;
    if (notifications.notifications().constFirst().severity != QStringLiteral("CRITICAL")) return 22;

    controller.processIncomingMessage(QStringLiteral("FIRE_ALARM, CH2, CLEAR"));
    if (events.constLast().eventType != QStringLiteral("FIRE_ALARM_ACK")) return 23;
    if (events.constLast().status != QStringLiteral("ACKED")) return 24;
    if (events.constLast().ackState != EventAckState::Acknowledged) return 25;
    if (notifications.hasNotifications()) return 26;

    ParkingViewState liveState;
    EvSlotInfo fireSlot;
    fireSlot.slotId = QStringLiteral("EV-01");
    fireSlot.state = SlotState::Occupied;
    fireSlot.alarmText = QStringLiteral("NORMAL");
    fireSlot.visual.occupancy = SlotOccupancy::Occupied;
    fireSlot.visual.vehicleClass = VehicleClass::Electric;
    liveState.evSlots.insert(fireSlot.slotId, fireSlot);
    controller.replaceViewState(liveState);

    const QByteArray suspectedPayload = R"JSON({
        "event_type": "sensor_fire_suspected",
        "source_id": "fire_sensor_01",
        "slot_id": "EV-01",
        "raw_payload": "FIRE:1"
    })JSON";
    if (!QMetaObject::invokeMethod(
            &controller, "handleMqttMessage", Qt::DirectConnection,
            Q_ARG(QString, QStringLiteral("parking/fire/fire_sensor_01")),
            Q_ARG(QByteArray, suspectedPayload))) return 27;
    if (events.constLast().eventType != QStringLiteral("FIRE_SUSPECTED")) return 28;
    if (events.constLast().status != QStringLiteral("OPEN")) return 29;
    if (controller.state().evSlots.value(QStringLiteral("EV-01")).visual.alarm
        != SlotAlarmKind::FireSuspected) return 30;
    if (notifications.notifications().size() != 1) return 31;
    const NotificationRecord mqttFire = notifications.notifications().constFirst();
    if (mqttFire.sourceId != QStringLiteral("EV-01")) return 32;
    if (mqttFire.eventType != QStringLiteral("FIRE_SUSPECTED")) return 33;
    if (mqttFire.severity != QStringLiteral("CRITICAL")) return 34;
    if (mqttFire.title != QStringLiteral("Fire suspected")) return 35;

    const QByteArray clearedPayload = R"JSON({
        "event_type": "sensor_fire_cleared",
        "source_id": "fire_sensor_01",
        "slot_id": "EV-01",
        "raw_payload": "FIRE:0"
    })JSON";
    if (!QMetaObject::invokeMethod(
            &controller, "handleMqttMessage", Qt::DirectConnection,
            Q_ARG(QString, QStringLiteral("parking/fire/fire_sensor_01")),
            Q_ARG(QByteArray, clearedPayload))) return 36;
    if (events.constLast().eventType != QStringLiteral("FIRE_CLEARED")) return 37;
    if (events.constLast().status != QStringLiteral("CLOSED")) return 38;
    if (notifications.hasNotifications()) return 39;
    const EvSlotInfo restoredSlot =
        controller.state().evSlots.value(QStringLiteral("EV-01"));
    if (restoredSlot.visual.alarm != SlotAlarmKind::None) return 40;
    if (restoredSlot.visual.occupancy != SlotOccupancy::Occupied) return 41;

    return 0;
}
