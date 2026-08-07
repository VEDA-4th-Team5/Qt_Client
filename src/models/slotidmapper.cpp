#include "slotidmapper.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

namespace {
QString normalizeId(const QString &id)
{
    return id.trimmed().toUpper();
}
}

bool SlotIdMapper::loadFromFile(const QString &path, QString &errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errorMessage = QStringLiteral("Cannot open slot mapping file: %1").arg(path);
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        errorMessage = QStringLiteral("Slot mapping JSON parse error: %1")
                           .arg(parseError.errorString());
        return false;
    }

    if (!document.isObject()) {
        errorMessage = QStringLiteral("Slot mapping file must contain a JSON object");
        return false;
    }

    const QJsonArray mappings = document.object()
                                    .value(QStringLiteral("mappings"))
                                    .toArray();
    if (mappings.isEmpty()) {
        errorMessage = QStringLiteral("Slot mapping file has no \"mappings\" array or it is empty");
        return false;
    }

    QHash<QString, QString> serverToZone;
    QHash<QString, QString> zoneToServer;

    for (int i = 0; i < mappings.size(); ++i) {
        const QJsonValue entry = mappings.at(i);
        if (!entry.isObject()) {
            errorMessage = QStringLiteral("Slot mapping entry %1 is not an object").arg(i);
            return false;
        }
        const QJsonObject obj = entry.toObject();
        const QString serverSlotId = normalizeId(
            obj.value(QStringLiteral("server_slot_id")).toString());
        const QString zoneId = normalizeId(
            obj.value(QStringLiteral("zone_id")).toString());

        if (serverSlotId.isEmpty() || zoneId.isEmpty()) {
            errorMessage = QStringLiteral(
                "Slot mapping entry %1 is missing server_slot_id or zone_id").arg(i);
            return false;
        }
        if (serverToZone.contains(serverSlotId)) {
            errorMessage = QStringLiteral(
                "Duplicate server_slot_id \"%1\" in mapping entry %2")
                               .arg(serverSlotId)
                               .arg(i);
            return false;
        }
        if (zoneToServer.contains(zoneId)) {
            errorMessage = QStringLiteral(
                "Duplicate zone_id \"%1\" in mapping entry %2")
                               .arg(zoneId)
                               .arg(i);
            return false;
        }
        serverToZone.insert(serverSlotId, zoneId);
        zoneToServer.insert(zoneId, serverSlotId);
    }

    m_serverToZone = serverToZone;
    m_zoneToServer = zoneToServer;
    errorMessage.clear();
    return true;
}

QString SlotIdMapper::toZoneId(const QString &serverSlotId) const
{
    const QString normalized = normalizeId(serverSlotId);
    const auto it = m_serverToZone.constFind(normalized);
    if (it != m_serverToZone.constEnd()) {
        return it.value();
    }
    return m_passthroughEnabled ? normalized : QString();
}

QString SlotIdMapper::toServerSlotId(const QString &zoneId) const
{
    const QString normalized = normalizeId(zoneId);
    const auto it = m_zoneToServer.constFind(normalized);
    if (it != m_zoneToServer.constEnd()) {
        return it.value();
    }
    return m_passthroughEnabled ? normalized : QString();
}

void SlotIdMapper::setPassthroughEnabled(bool enabled)
{
    m_passthroughEnabled = enabled;
}

bool SlotIdMapper::isPassthroughEnabled() const
{
    return m_passthroughEnabled;
}

int SlotIdMapper::mappingCount() const
{
    return m_serverToZone.size();
}

QList<QPair<QString, QString>> SlotIdMapper::allMappings() const
{
    QList<QPair<QString, QString>> result;
    result.reserve(m_serverToZone.size());
    for (auto it = m_serverToZone.constBegin(); it != m_serverToZone.constEnd(); ++it) {
        result.append({it.key(), it.value()});
    }
    return result;
}

void SlotIdMapper::clear()
{
    m_serverToZone.clear();
    m_zoneToServer.clear();
}
