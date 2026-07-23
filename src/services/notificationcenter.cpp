#include "notificationcenter.h"

#include <QStringList>

namespace {
constexpr int kMaxNotifications = 10;

bool isClosedStatus(const QString &status)
{
    return status == QStringLiteral("ACKED")
        || status == QStringLiteral("CLEAR")
        || status == QStringLiteral("CLEARED");
}
}

NotificationCenter::NotificationCenter(QObject *parent)
    : QObject(parent)
{
}

QList<NotificationRecord> NotificationCenter::notifications() const
{
    return m_notifications;
}

int NotificationCenter::unreadCount() const
{
    int count = 0;
    for (const NotificationRecord &notification : m_notifications) {
        if (notification.unread) {
            ++count;
        }
    }
    return count;
}

bool NotificationCenter::hasNotifications() const
{
    return !m_notifications.isEmpty();
}

void NotificationCenter::ingestEvent(const MonitoringEvent &event)
{
    const QString normalizedType = normalizeEventType(event.eventType);
    const QString normalizedStatus = monitoringEventStatusText(event);
    QString normalizedSource = event.sourceId.trimmed().toUpper();
    if (normalizedSource.isEmpty()) {
        normalizedSource = QStringLiteral("SYSTEM");
    }

    if (isAcknowledgementEvent(normalizedType, normalizedStatus)) {
        if (acknowledgeMatching(normalizedSource, normalizedType)) {
            emit notificationsChanged();
        }
        return;
    }

    if (!isNotifiableEvent(normalizedType, normalizedStatus)) {
        return;
    }

    NotificationRecord record;
    record.id = QStringLiteral("N%1").arg(m_nextNotificationId++);
    record.time = monitoringEventTimeText(event);
    record.sourceId = normalizedSource;
    record.eventType = normalizedType;
    record.severity = event.severity == EventSeverity::Unknown
        ? severityForEvent(normalizedType, normalizedStatus)
        : eventSeverityText(event.severity);
    record.title = titleForEvent(normalizedType);
    record.message = event.message;
    record.status = normalizedStatus;
    record.unread = true;
    addOrUpdateNotification(record);
    emit notificationsChanged();
}

void NotificationCenter::markAllRead()
{
    bool changed = false;
    for (NotificationRecord &notification : m_notifications) {
        if (notification.unread) {
            notification.unread = false;
            changed = true;
        }
    }
    if (changed) {
        emit notificationsChanged();
    }
}

void NotificationCenter::clearAll()
{
    if (m_notifications.isEmpty()) {
        return;
    }
    m_notifications.clear();
    emit notificationsChanged();
}

QString NotificationCenter::normalizeEventType(const QString &eventType) const
{
    const QString normalized = eventType.trimmed().toUpper();
    if (normalized == QStringLiteral("FIRE_EVENT")) {
        return QStringLiteral("FIRE_ALARM");
    }
    if (normalized == QStringLiteral("FLAME")) {
        return QStringLiteral("FLAME_DETECTED");
    }
    if (normalized == QStringLiteral("SENSOR_ERROR")) {
        return QStringLiteral("HALL_SENSOR_ERROR");
    }
    return normalized;
}

QString NotificationCenter::severityForEvent(const QString &eventType, const QString &status) const
{
    Q_UNUSED(status)
    if (eventType == QStringLiteral("FIRE_ALARM")
        || eventType == QStringLiteral("FLAME_DETECTED")) {
        return QStringLiteral("CRITICAL");
    }
    if (eventType == QStringLiteral("API_ERROR")
        || eventType == QStringLiteral("API_PARSE_ERROR")
        || eventType == QStringLiteral("CAMERA_DISCONNECTED")
        || eventType == QStringLiteral("DB_WRITE_FAILED")
        || eventType == QStringLiteral("LAYOUT_SAVE_FAILED")
        || eventType == QStringLiteral("HALL_SENSOR_ERROR")
        || eventType == QStringLiteral("NON_EV_ALERT")
        || eventType == QStringLiteral("OVERTIME_ALERT")) {
        return QStringLiteral("WARNING");
    }
    return QStringLiteral("INFO");
}

QString NotificationCenter::titleForEvent(const QString &eventType) const
{
    if (eventType == QStringLiteral("FIRE_ALARM")) {
        return QStringLiteral("Fire alarm");
    }
    if (eventType == QStringLiteral("FLAME_DETECTED")) {
        return QStringLiteral("Flame detected");
    }
    if (eventType == QStringLiteral("HALL_SENSOR_ERROR")) {
        return QStringLiteral("Hall sensor error");
    }
    if (eventType == QStringLiteral("HALL_SENSOR_CHANGED")) {
        return QStringLiteral("Hall sensor state changed");
    }
    if (eventType == QStringLiteral("CAMERA_DISCONNECTED")) {
        return QStringLiteral("Camera disconnected");
    }
    if (eventType == QStringLiteral("API_ERROR")
        || eventType == QStringLiteral("API_PARSE_ERROR")) {
        return QStringLiteral("Parking API error");
    }
    if (eventType == QStringLiteral("DB_WRITE_FAILED")) {
        return QStringLiteral("Database write failed");
    }
    if (eventType == QStringLiteral("LAYOUT_SAVE_FAILED")) {
        return QStringLiteral("Parking map layout save failed");
    }
    if (eventType == QStringLiteral("NON_EV_ALERT")) {
        return QStringLiteral("Non-EV alert");
    }
    if (eventType == QStringLiteral("OVERTIME_ALERT")) {
        return QStringLiteral("Overtime alert");
    }
    return eventType;
}

bool NotificationCenter::isAcknowledgementEvent(const QString &eventType, const QString &status) const
{
    return eventType.contains(QStringLiteral("ACK"))
        || eventType.contains(QStringLiteral("CLEAR"))
        || eventType == QStringLiteral("CAMERA_RECONNECTED")
        || isClosedStatus(status);
}

bool NotificationCenter::isNotifiableEvent(const QString &eventType, const QString &status) const
{
    if (status == QStringLiteral("RECORDED")
        || status == QStringLiteral("DONE")
        || status == QStringLiteral("SKIPPED")
        || status == QStringLiteral("REJECTED")) {
        return false;
    }

    static const QStringList explicitTypes = {
        QStringLiteral("FIRE_ALARM"),
        QStringLiteral("FLAME_DETECTED"),
        QStringLiteral("HALL_SENSOR_ERROR"),
        QStringLiteral("HALL_SENSOR_CHANGED"),
        QStringLiteral("CAMERA_DISCONNECTED"),
        QStringLiteral("API_ERROR"),
        QStringLiteral("API_PARSE_ERROR"),
        QStringLiteral("DB_WRITE_FAILED"),
        QStringLiteral("NON_EV_ALERT"),
        QStringLiteral("OVERTIME_ALERT")
    };
    if (explicitTypes.contains(eventType)) {
        return true;
    }

    if (eventType.endsWith(QStringLiteral("_ERROR"))
        || eventType.contains(QStringLiteral("DISCONNECTED"))
        || eventType.contains(QStringLiteral("FAILED"))) {
        return true;
    }

    return (status == QStringLiteral("OPEN") || status == QStringLiteral("FAILED"))
        && (eventType.contains(QStringLiteral("ALARM"))
            || eventType.contains(QStringLiteral("ALERT"))
            || eventType.contains(QStringLiteral("ERROR")));
}

bool NotificationCenter::isSameAlertGroup(const QString &left, const QString &right) const
{
    const QString normalizedLeft = normalizeEventType(left);
    const QString normalizedRight = normalizeEventType(right);
    if (normalizedLeft == normalizedRight) {
        return true;
    }
    if ((normalizedLeft.contains(QStringLiteral("FIRE"))
         || normalizedLeft.contains(QStringLiteral("FLAME")))
        && (normalizedRight.contains(QStringLiteral("FIRE"))
            || normalizedRight.contains(QStringLiteral("FLAME")))) {
        return true;
    }
    if (normalizedLeft.startsWith(QStringLiteral("HALL_SENSOR"))
        && normalizedRight.startsWith(QStringLiteral("HALL_SENSOR"))) {
        return true;
    }
    if (normalizedRight == QStringLiteral("ALARM_ACK")) {
        return normalizedLeft.contains(QStringLiteral("ALARM"))
            || normalizedLeft.contains(QStringLiteral("ALERT"))
            || normalizedLeft.endsWith(QStringLiteral("_ERROR"));
    }
    if (normalizedRight == QStringLiteral("CAMERA_RECONNECTED")) {
        return normalizedLeft == QStringLiteral("CAMERA_DISCONNECTED");
    }
    return false;
}

bool NotificationCenter::acknowledgeMatching(const QString &sourceId, const QString &eventType)
{
    if (sourceId == QStringLiteral("ALL")) {
        const bool hadNotifications = !m_notifications.isEmpty();
        m_notifications.clear();
        return hadNotifications;
    }

    bool changed = false;
    for (int i = m_notifications.size() - 1; i >= 0; --i) {
        const NotificationRecord &notification = m_notifications.at(i);
        const bool sourceMatches = notification.sourceId == sourceId;
        if (sourceMatches && isSameAlertGroup(notification.eventType, eventType)) {
            m_notifications.removeAt(i);
            changed = true;
        }
    }
    return changed;
}

void NotificationCenter::addOrUpdateNotification(NotificationRecord record)
{
    for (int i = m_notifications.size() - 1; i >= 0; --i) {
        const NotificationRecord &existing = m_notifications.at(i);
        if (existing.sourceId == record.sourceId
            && normalizeEventType(existing.eventType) == normalizeEventType(record.eventType)) {
            m_notifications.removeAt(i);
        }
    }

    m_notifications.prepend(record);
    while (m_notifications.size() > kMaxNotifications) {
        m_notifications.removeLast();
    }
}
