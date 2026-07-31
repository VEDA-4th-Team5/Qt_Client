#include "serverfireeventadapter.h"

#include <QJsonValue>
#include <QRegularExpression>

namespace {
QString upperString(const QJsonObject &object, const QString &key)
{
    return object.value(key).toString().trimmed().toUpper();
}

QString normalizeChannelId(const QString &rawChannelId)
{
    static const QRegularExpression channelPattern(
        QStringLiteral("^CH0?([1-4])$"));
    const QRegularExpressionMatch match = channelPattern.match(
        rawChannelId.trimmed().toUpper());
    return match.hasMatch()
        ? QStringLiteral("CH%1").arg(match.captured(1)) : QString();
}

ServerFireEvent invalidEvent(ServerFireEvent event, const QString &message)
{
    event.action = ServerFireEventAction::Invalid;
    event.errorMessage = message;
    return event;
}
}

ServerFireEvent ServerFireEventAdapter::parse(const QJsonObject &object)
{
    ServerFireEvent event;
    event.eventId = object.value(QStringLiteral("event_id")).toString().trimmed();
    event.eventType = upperString(object, QStringLiteral("event_type"));
    event.channelId = normalizeChannelId(
        object.value(QStringLiteral("channel_id")).toString());
    event.sourceId = object.value(QStringLiteral("source_id")).toString().trimmed();
    event.alarmKind = upperString(object, QStringLiteral("alarm_kind"));
    const QString compatibilityAlarm = upperString(object, QStringLiteral("alarm"));
    event.alarmState = upperString(object, QStringLiteral("alarm_state"));
    event.scope = upperString(object, QStringLiteral("scope"));
    event.severity = upperString(object, QStringLiteral("severity"));
    event.message = object.value(QStringLiteral("message")).toString().trimmed();
    event.occurredAt = QDateTime::fromString(
        object.value(QStringLiteral("timestamp")).toString().trimmed(),
        Qt::ISODate);

    const QJsonValue activeValue = object.value(QStringLiteral("active"));
    event.activePresent = !activeValue.isUndefined() && !activeValue.isNull();
    if (event.activePresent) {
        if (!activeValue.isBool()) {
            return invalidEvent(event, QStringLiteral("active must be a boolean"));
        }
        event.active = activeValue.toBool();
    }

    const bool suspectedType = event.eventType == QStringLiteral("FIRE_SUSPECTED")
        || event.eventType == QStringLiteral("SENSOR_FIRE_SUSPECTED");
    const bool clearedType = event.eventType == QStringLiteral("FIRE_CLEARED")
        || event.eventType == QStringLiteral("SENSOR_FIRE_CLEARED");
    const bool alarmKindFire = event.alarmKind == QStringLiteral("FIRE_SUSPECTED");
    const bool compatibilityAlarmFire =
        compatibilityAlarm == QStringLiteral("FIRE_SUSPECTED");
    if (!suspectedType && !clearedType
        && !alarmKindFire && !compatibilityAlarmFire) {
        return event;
    }

    if (event.eventId.isEmpty()) {
        return invalidEvent(event, QStringLiteral("fire event requires event_id"));
    }
    if (event.channelId.isEmpty()) {
        return invalidEvent(event, QStringLiteral("unknown or missing channel_id"));
    }
    if (!event.scope.isEmpty()
        && event.scope != QStringLiteral("CAMERA_CHANNEL")) {
        return invalidEvent(event, QStringLiteral("fire scope must be CAMERA_CHANNEL"));
    }

    if (clearedType) {
        if (alarmKindFire || compatibilityAlarmFire) {
            return invalidEvent(event, QStringLiteral("FIRE_CLEARED contradicts fire alarm kind"));
        }
        if (!event.alarmKind.isEmpty()
            && event.alarmKind != QStringLiteral("NONE")) {
            return invalidEvent(event, QStringLiteral("cleared alarm_kind must be NONE"));
        }
        if (!compatibilityAlarm.isEmpty()
            && compatibilityAlarm != QStringLiteral("NONE")) {
            return invalidEvent(event, QStringLiteral("cleared alarm must be NONE"));
        }
        if (!event.alarmState.isEmpty()
            && event.alarmState != QStringLiteral("RESOLVED")) {
            return invalidEvent(event, QStringLiteral("cleared alarm_state must be RESOLVED"));
        }
        if (event.activePresent && event.active) {
            return invalidEvent(event, QStringLiteral("FIRE_CLEARED requires active=false"));
        }
        event.action = ServerFireEventAction::Clear;
        return event;
    }

    if (!event.eventType.isEmpty() && !suspectedType) {
        return invalidEvent(event, QStringLiteral("non-fire event_type contradicts fire alarm kind"));
    }
    if (!event.alarmKind.isEmpty() && !alarmKindFire) {
        return invalidEvent(event, QStringLiteral("suspected alarm_kind must be FIRE_SUSPECTED"));
    }
    if (!compatibilityAlarm.isEmpty() && !compatibilityAlarmFire) {
        return invalidEvent(event, QStringLiteral("suspected alarm must be FIRE_SUSPECTED"));
    }
    if (!event.alarmState.isEmpty()
        && event.alarmState != QStringLiteral("OPEN")) {
        return invalidEvent(event, QStringLiteral("suspected alarm_state must be OPEN"));
    }
    if (event.activePresent && !event.active) {
        return invalidEvent(event, QStringLiteral("FIRE_SUSPECTED requires active=true"));
    }

    event.action = ServerFireEventAction::Activate;
    return event;
}
