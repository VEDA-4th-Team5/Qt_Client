#include "parkingresponseparser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QStringList>

namespace {
QString firstString(const QJsonObject &object, const QStringList &keys)
{
    for (const QString &key : keys) {
        const QJsonValue value = object.value(key);
        if (value.isString()) {
            return value.toString().trimmed();
        }
    }
    return {};
}

QJsonObject firstObject(const QJsonObject &object, const QStringList &keys)
{
    for (const QString &key : keys) {
        const QJsonValue value = object.value(key);
        if (value.isObject()) {
            return value.toObject();
        }
    }
    return {};
}

ParkingImageResource imageFromObject(const QJsonObject &image,
                                     const QString &defaultRole,
                                     const QString &defaultProcessing)
{
    ParkingImageResource resource;
    resource.url = QUrl(image.value(QStringLiteral("url")).toString());
    resource.role = firstString(image, {QStringLiteral("image_role"), QStringLiteral("role")}).toUpper();
    resource.processing = firstString(
        image, {QStringLiteral("processing_type"), QStringLiteral("processing")}).toUpper();
    if (resource.role.isEmpty()) {
        resource.role = defaultRole;
    }
    if (resource.processing.isEmpty()) {
        resource.processing = defaultProcessing;
    }

    const QString timestamp = firstString(
        image,
        {QStringLiteral("captured_at"), QStringLiteral("processed_at"), QStringLiteral("timestamp")});
    resource.timestamp = QDateTime::fromString(timestamp, Qt::ISODate);
    return resource;
}

bool parseSlotObject(const QJsonObject &item,
                     ParkingSlotSnapshot &slot,
                     QString &errorMessage)
{
    ParkingSlotSnapshot parsed;
    parsed.slotId = firstString(item, {QStringLiteral("slot_id"), QStringLiteral("slotId")});
    parsed.state = firstString(
        item,
        {QStringLiteral("parking_status"), QStringLiteral("status"), QStringLiteral("state")});

    if (parsed.slotId.isEmpty() || parsed.state.isEmpty()) {
        errorMessage = QStringLiteral("Parking slot requires an ID and state");
        return false;
    }

    const QJsonObject session = firstObject(
        item, {QStringLiteral("active_session"), QStringLiteral("activeSession")});
    const QJsonObject &details = session.isEmpty() ? item : session;

    parsed.plateNumber = firstString(
        details, {QStringLiteral("plate_number"), QStringLiteral("plateNumber")});
    const QString vehicleType = firstString(
        details, {QStringLiteral("vehicle_type"), QStringLiteral("vehicleType")}).toUpper();
    QJsonValue isEvValue = item.value(QStringLiteral("is_ev"));
    if (!isEvValue.isBool()) {
        isEvValue = details.value(QStringLiteral("is_ev"));
    }
    if (isEvValue.isBool()) {
        parsed.isEv = isEvValue.toBool();
        parsed.vehicleTypeKnown = true;
    } else if (vehicleType == QStringLiteral("EV")
               || vehicleType == QStringLiteral("ELECTRIC")
               || vehicleType == QStringLiteral("HYBRID")) {
        parsed.isEv = true;
        parsed.vehicleTypeKnown = true;
    } else if (vehicleType == QStringLiteral("GENERAL")
               || vehicleType == QStringLiteral("NON_EV")
               || vehicleType == QStringLiteral("ICE")
               || vehicleType == QStringLiteral("GASOLINE")
               || vehicleType == QStringLiteral("DIESEL")) {
        parsed.isEv = false;
        parsed.vehicleTypeKnown = true;
    }
    parsed.occupiedSince = QDateTime::fromString(
        firstString(details, {QStringLiteral("entry_time"), QStringLiteral("entryTime"),
                              QStringLiteral("occupied_since")}),
        Qt::ISODate);
    parsed.elapsedSeconds = details.value(QStringLiteral("elapsed_seconds")).toInt(
        details.value(QStringLiteral("elapsedSeconds")).toInt(0));
    parsed.alarm = firstString(details, {QStringLiteral("alert"), QStringLiteral("alarm")});

    const QJsonValue imageArrayValue = details.value(QStringLiteral("images"));
    if (imageArrayValue.isArray()) {
        for (const QJsonValue &value : imageArrayValue.toArray()) {
            if (!value.isObject()) {
                continue;
            }
            const ParkingImageResource image = imageFromObject(value.toObject(), {}, {});
            if (!image.url.isEmpty()) {
                parsed.images.append(image);
            }
        }
    } else if (imageArrayValue.isObject()) {
        const QJsonObject images = imageArrayValue.toObject();
        if (images.value(QStringLiteral("before")).isObject()) {
            const ParkingImageResource image = imageFromObject(
                images.value(QStringLiteral("before")).toObject(),
                QStringLiteral("VEHICLE"),
                QStringLiteral("ORIGINAL"));
            if (!image.url.isEmpty()) {
                parsed.images.append(image);
            }
        }
        if (images.value(QStringLiteral("after")).isObject()) {
            ParkingImageResource image = imageFromObject(
                images.value(QStringLiteral("after")).toObject(),
                QStringLiteral("VEHICLE"),
                QStringLiteral("ENHANCED"));
            if (image.processing.isEmpty()) {
                image.processing = images.value(QStringLiteral("method")).toString().toUpper();
            }
            if (!image.url.isEmpty()) {
                parsed.images.append(image);
            }
        }
    }

    slot = parsed;
    errorMessage.clear();
    return true;
}
}

bool ParkingResponseParser::parseSnapshot(const QJsonDocument &document,
                                          ParkingSnapshot &snapshot,
                                          QString &errorMessage)
{
    if (!document.isObject()) {
        errorMessage = QStringLiteral("Parking response must be a JSON object");
        return false;
    }

    const QJsonObject root = document.object();
    QJsonValue slotsValue = root.value(QStringLiteral("items"));
    if (!slotsValue.isArray()) {
        slotsValue = root.value(QStringLiteral("data"));
    }
    if (!slotsValue.isArray()) {
        slotsValue = root.value(QStringLiteral("slots"));
    }
    if (!slotsValue.isArray()) {
        errorMessage = QStringLiteral("Parking response is missing items, data, or slots array");
        return false;
    }

    ParkingSnapshot parsed;
    parsed.generatedAt = QDateTime::fromString(
        firstString(root, {QStringLiteral("generated_at"), QStringLiteral("generatedAt")}),
        Qt::ISODate);

    const QJsonArray items = slotsValue.toArray();
    for (int index = 0; index < items.size(); ++index) {
        if (!items.at(index).isObject()) {
            errorMessage = QStringLiteral("Parking item %1 must be an object").arg(index);
            return false;
        }

        ParkingSlotSnapshot slot;
        QString slotError;
        if (!parseSlotObject(items.at(index).toObject(), slot, slotError)) {
            errorMessage = QStringLiteral("Parking item %1: %2").arg(index).arg(slotError);
            return false;
        }
        parsed.parkingSlots.append(slot);
    }

    snapshot = parsed;
    errorMessage.clear();
    return true;
}

bool ParkingResponseParser::parseSlotDetail(const QJsonDocument &document,
                                            ParkingSlotSnapshot &slot,
                                            QString &errorMessage)
{
    if (!document.isObject()) {
        errorMessage = QStringLiteral("Parking slot detail must be a JSON object");
        return false;
    }
    return parseSlotObject(document.object(), slot, errorMessage);
}
