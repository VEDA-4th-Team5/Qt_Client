#include "parkingzonelayout.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
constexpr double kOperatorSlotWidth = 124.0;
constexpr double kOperatorSlotHeight = 72.0;
constexpr double kOperatorSlotGap = 22.0;
constexpr double kOperatorPanelX = 30.0;
constexpr double kOperatorPanelWidth = 860.0;
constexpr double kOperatorSlotTopOffset = 58.0;

QRectF fixedOperatorSlotRect(const QString &channel, int slotOrder)
{
    if ((channel != QStringLiteral("CH1") && channel != QStringLiteral("CH3"))
        || slotOrder < 1 || slotOrder > 4) return QRectF();

    const double panelY = channel == QStringLiteral("CH3") ? 282.0 : 32.0;
    const double slotBlockWidth = (kOperatorSlotWidth * 4.0) + (kOperatorSlotGap * 3.0);
    const double slotOriginX = kOperatorPanelX + ((kOperatorPanelWidth - slotBlockWidth) / 2.0);
    return QRectF(slotOriginX + ((slotOrder - 1) * (kOperatorSlotWidth + kOperatorSlotGap)),
                  panelY + kOperatorSlotTopOffset,
                  kOperatorSlotWidth, kOperatorSlotHeight);
}

ParkingZoneLayout makeZone(const QString &zoneId, const QString &zoneType, int slotOrder,
                           const QString &cameraChannel, const QString &ivaAreaId,
                           const QString &hallSensorId)
{
    ParkingZoneLayout zone;
    zone.zoneId = zoneId;
    zone.zoneType = zoneType;
    zone.displayName = zoneId;
    zone.slotOrder = slotOrder;
    zone.rect = fixedOperatorSlotRect(cameraChannel, slotOrder);
    zone.cameraChannel = cameraChannel;
    zone.ivaAreaId = ivaAreaId;
    zone.hallSensorId = hallSensorId;
    return zone;
}

QString numberedZoneId(const QString &prefix, int number)
{
    return QStringLiteral("%1-%2").arg(prefix).arg(number, 2, 10, QLatin1Char('0'));
}

QString hallSensorIdFor(const QString &prefix, int number)
{
    return QStringLiteral("HALL_%1_%2").arg(prefix).arg(number, 2, 10, QLatin1Char('0'));
}

void appendChannelTestLayout(QList<ParkingZoneLayout> *zones, int channelNumber,
                             const QString &zonePrefix, const QString &zoneType,
                             int firstZoneNumber,
                             double panelX, double panelY)
{
    if (!zones) return;
    const QString channel = QStringLiteral("CH%1").arg(channelNumber);
    Q_UNUSED(panelX)
    Q_UNUSED(panelY)
    const bool isEv = zoneType == QStringLiteral("EV");
    for (int index = 0; index < 4; ++index) {
        const int zoneNumber = firstZoneNumber + index;
        zones->append(makeZone(
            numberedZoneId(zonePrefix, zoneNumber),
            zoneType,
            index + 1,
            channel,
            isEv ? QStringLiteral("IVA%1").arg(index + 1) : QString(),
            hallSensorIdFor(zonePrefix, zoneNumber)));
    }
}

QJsonObject zoneToJson(const ParkingZoneLayout &zone)
{
    QJsonObject object;
    object.insert(QStringLiteral("zone_id"), zone.zoneId);
    object.insert(QStringLiteral("zone_type"), zone.zoneType);
    object.insert(QStringLiteral("display_name"), zone.displayName);
    object.insert(QStringLiteral("slot_order"), zone.slotOrder);
    object.insert(QStringLiteral("camera_channel"), zone.cameraChannel);
    object.insert(QStringLiteral("iva_area_id"), zone.ivaAreaId);
    object.insert(QStringLiteral("hall_sensor_id"), zone.hallSensorId);
    object.insert(QStringLiteral("enabled"), zone.enabled);
    return object;
}

ParkingZoneLayout zoneFromJson(const QJsonObject &object)
{
    ParkingZoneLayout zone;
    zone.zoneId = object.value(QStringLiteral("zone_id")).toString().trimmed().toUpper();
    zone.zoneType = object.value(QStringLiteral("zone_type")).toString(QStringLiteral("GENERAL")).trimmed().toUpper();
    zone.displayName = object.value(QStringLiteral("display_name")).toString(zone.zoneId).trimmed();
    zone.cameraChannel = object.value(QStringLiteral("camera_channel")).toString().trimmed().toUpper();
    zone.slotOrder = object.value(QStringLiteral("slot_order")).toInt();
    zone.rect = fixedOperatorSlotRect(zone.cameraChannel, zone.slotOrder);
    if (zone.rect.isEmpty()) {
        // Version 1/2 local files used absolute coordinates. Retain them only
        // while deriving a stable order during load; version 3 writes no coordinates.
        zone.rect = QRectF(
            object.value(QStringLiteral("x")).toDouble(),
            object.value(QStringLiteral("y")).toDouble(),
            object.value(QStringLiteral("width")).toDouble(kOperatorSlotWidth),
            object.value(QStringLiteral("height")).toDouble(kOperatorSlotHeight));
    }
    zone.ivaAreaId = object.value(QStringLiteral("iva_area_id")).toString().trimmed().toUpper();
    zone.hallSensorId = object.value(QStringLiteral("hall_sensor_id")).toString().trimmed().toUpper();
    zone.enabled = object.value(QStringLiteral("enabled")).toBool(true);
    if (zone.displayName.isEmpty()) {
        zone.displayName = zone.zoneId;
    }
    if (zone.zoneType != QStringLiteral("EV")) {
        zone.zoneType = QStringLiteral("GENERAL");
        zone.ivaAreaId.clear();
    } else if (zone.ivaAreaId != QStringLiteral("IVA1")
               && zone.ivaAreaId != QStringLiteral("IVA2")
               && zone.ivaAreaId != QStringLiteral("IVA3")
               && zone.ivaAreaId != QStringLiteral("IVA4")) {
        zone.ivaAreaId = QStringLiteral("IVA1");
    }
    return zone;
}

bool isCameraChannelId(const QString &value)
{
    return value == QStringLiteral("CH1")
        || value == QStringLiteral("CH2")
        || value == QStringLiteral("CH3")
        || value == QStringLiteral("CH4");
}

ParkingChannelDisplayNames channelDisplayNamesFromJson(const QJsonObject &root)
{
    ParkingChannelDisplayNames names;
    const QJsonObject object = root.value(QStringLiteral("channel_display_names")).toObject();
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        const QString channel = it.key().trimmed().toUpper();
        const QString displayName = it.value().toString().trimmed();
        if (isCameraChannelId(channel)
            && !displayName.isEmpty()
            && displayName.compare(channel, Qt::CaseInsensitive) != 0) {
            names.insert(channel, displayName);
        }
    }
    return names;
}

QJsonObject channelDisplayNamesToJson(const ParkingChannelDisplayNames &names)
{
    QJsonObject object;
    for (int channelNumber = 1; channelNumber <= 4; ++channelNumber) {
        const QString channel = QStringLiteral("CH%1").arg(channelNumber);
        const QString displayName = names.value(channel).trimmed();
        if (!displayName.isEmpty()
            && displayName.compare(channel, Qt::CaseInsensitive) != 0) {
            object.insert(channel, displayName);
        }
    }
    return object;
}
}

QList<ParkingZoneLayout> defaultParkingZoneLayout()
{
    QList<ParkingZoneLayout> zones;
    appendChannelTestLayout(&zones, 1, QStringLiteral("EV"), QStringLiteral("EV"), 1, 30, 32);
    appendChannelTestLayout(&zones, 3, QStringLiteral("P"), QStringLiteral("GENERAL"), 1, 30, 282);
    return zones;
}

bool loadParkingZoneLayout(const QString &path, QList<ParkingZoneLayout> *zones, QString *errorMessage)
{
    return loadParkingZoneLayout(path, zones, nullptr, errorMessage);
}

bool loadParkingZoneLayout(const QString &path, QList<ParkingZoneLayout> *zones,
                           ParkingChannelDisplayNames *channelDisplayNames,
                           QString *errorMessage)
{
    if (!zones) {
        if (errorMessage) *errorMessage = QStringLiteral("Output zone list is null.");
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) *errorMessage = QStringLiteral("Could not open parking map layout: ") + path;
        return false;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        if (errorMessage) *errorMessage = QStringLiteral("Invalid parking map layout JSON: ") + path;
        return false;
    }

    const QJsonObject root = document.object();
    const QJsonArray zoneArray = root.value(QStringLiteral("zones")).toArray();
    QList<ParkingZoneLayout> loadedZones;
    QHash<QString, int> nextSlotOrder;
    for (const QJsonValue &value : zoneArray) {
        if (!value.isObject()) continue;
        ParkingZoneLayout zone = zoneFromJson(value.toObject());
        if (zone.slotOrder <= 0) {
            zone.slotOrder = nextSlotOrder.value(zone.cameraChannel, 0) + 1;
        }
        nextSlotOrder.insert(zone.cameraChannel,
                             qMax(nextSlotOrder.value(zone.cameraChannel), zone.slotOrder));
        const QRectF fixedRect = fixedOperatorSlotRect(zone.cameraChannel, zone.slotOrder);
        if (!fixedRect.isEmpty()) {
            zone.rect = fixedRect;
            zone.rotation = 0.0;
        }
        if (!zone.zoneId.isEmpty() && zone.rect.width() > 0.0 && zone.rect.height() > 0.0) {
            loadedZones.append(zone);
        }
    }

    if (loadedZones.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("Parking map layout has no valid zones: ") + path;
        return false;
    }

    *zones = loadedZones;
    if (channelDisplayNames) {
        *channelDisplayNames = channelDisplayNamesFromJson(root);
    }
    return true;
}

bool saveParkingZoneLayout(const QString &path, const QList<ParkingZoneLayout> &zones, QString *errorMessage)
{
    return saveParkingZoneLayout(path, zones, ParkingChannelDisplayNames(), errorMessage);
}

bool saveParkingZoneLayout(const QString &path, const QList<ParkingZoneLayout> &zones,
                           const ParkingChannelDisplayNames &channelDisplayNames,
                           QString *errorMessage)
{
    const QFileInfo fileInfo(path);
    QDir dir = fileInfo.dir();
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        if (errorMessage) *errorMessage = QStringLiteral("Could not create layout directory: ") + dir.absolutePath();
        return false;
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), 3);
    root.insert(QStringLiteral("canvas_width"), 920);
    root.insert(QStringLiteral("canvas_height"), 460);
    root.insert(QStringLiteral("channel_display_names"),
                channelDisplayNamesToJson(channelDisplayNames));
    QJsonArray zoneArray;
    for (const ParkingZoneLayout &zone : zones) {
        zoneArray.append(zoneToJson(zone));
    }
    root.insert(QStringLiteral("zones"), zoneArray);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (errorMessage) *errorMessage = QStringLiteral("Could not write parking map layout: ") + path;
        return false;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}
