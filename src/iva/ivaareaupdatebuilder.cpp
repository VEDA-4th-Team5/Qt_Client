#include "ivaareaupdatebuilder.h"

#include <QJsonArray>
#include <QSet>

#include <algorithm>
#include <cmath>

namespace {
bool validateSupportedValues(const QStringList &values,
                             const QStringList &supported,
                             const QString &fieldName,
                             QString &errorMessage)
{
    for (const QString &value : values) {
        if (!supported.contains(value)) {
            errorMessage = QStringLiteral("%1 contains unsupported value '%2'")
                               .arg(fieldName, value);
            return false;
        }
    }
    return true;
}

bool validateRange(int value,
                   const IvaIntegerRange &range,
                   const QString &fieldName,
                   QString &errorMessage)
{
    if (!range.contains(value)) {
        errorMessage = QStringLiteral("%1 must be between %2 and %3")
                           .arg(fieldName)
                           .arg(range.minimum)
                           .arg(range.maximum);
        return false;
    }
    return true;
}

QJsonArray stringArray(const QStringList &values)
{
    QJsonArray array;
    for (const QString &value : values) {
        array.append(value);
    }
    return array;
}
}

bool IvaAreaUpdateBuilder::buildChannelPayload(
    int channel,
    bool enabled,
    const QList<IvaAreaDefinition> &areas,
    const IvaChannelOptions &options,
    QJsonObject &payload,
    QString &errorMessage,
    const QSize &coordinateResolution)
{
    if (channel < 0 || options.channel != channel) {
        errorMessage = QStringLiteral("Channel options do not match the edited channel");
        return false;
    }

    QSet<int> indexes;
    QJsonArray definitions;
    for (const IvaAreaDefinition &area : areas) {
        if (area.channel != channel) {
            errorMessage = QStringLiteral("Edited areas must belong to one channel");
            return false;
        }
        if (area.name.trimmed().isEmpty()) {
            errorMessage = QStringLiteral("Every IVA rule requires a name");
            return false;
        }
        if (!validateRange(area.areaIndex, options.areaIndex,
                           QStringLiteral("Rule index"), errorMessage)
            || indexes.contains(area.areaIndex)) {
            if (indexes.contains(area.areaIndex)) {
                errorMessage = QStringLiteral("Rule index %1 is duplicated")
                                   .arg(area.areaIndex);
            }
            return false;
        }
        indexes.insert(area.areaIndex);
        if (!validateRange(area.areaCoordinates.size(),
                           options.areaCoordinateCount,
                           QStringLiteral("Polygon point count"), errorMessage)
            || !validateRange(area.appearanceDuration,
                              options.appearanceDuration,
                              QStringLiteral("Appearance duration"), errorMessage)
            || !validateRange(area.intrusionDuration,
                              options.intrusionDuration,
                              QStringLiteral("Intrusion duration"), errorMessage)
            || !validateRange(area.loiteringDuration,
                              options.loiteringDuration,
                              QStringLiteral("Loitering duration"), errorMessage)
            || !validateSupportedValues(area.detectionModes,
                                        options.detectionModes,
                                        QStringLiteral("Detection modes"),
                                        errorMessage)
            || !validateSupportedValues(area.objectTypeFilter,
                                        options.objectTypeFilters,
                                        QStringLiteral("Object filters"),
                                        errorMessage)) {
            return false;
        }

        QJsonArray coordinates;
        for (const QPointF &point : area.areaCoordinates) {
            if (!std::isfinite(point.x()) || !std::isfinite(point.y())
                || point.x() < 0.0 || point.y() < 0.0
                || std::round(point.x()) != point.x()
                || std::round(point.y()) != point.y()) {
                errorMessage = QStringLiteral(
                    "Polygon coordinates must be finite non-negative integer pixels");
                return false;
            }
            if (coordinateResolution.isValid()
                && (point.x() > coordinateResolution.width()
                    || point.y() > coordinateResolution.height())) {
                errorMessage = QStringLiteral(
                    "Polygon coordinates must stay within the IVA resolution %1x%2")
                                   .arg(coordinateResolution.width())
                                   .arg(coordinateResolution.height());
                return false;
            }
            coordinates.append(QJsonObject{{QStringLiteral("x"), point.x()},
                                           {QStringLiteral("y"), point.y()}});
        }

        QJsonObject definition{
            {QStringLiteral("appearanceDuration"), area.appearanceDuration},
            {QStringLiteral("areaCoordinates"), coordinates},
            {QStringLiteral("detectionModes"), stringArray(area.detectionModes)},
            {QStringLiteral("index"), area.areaIndex},
            {QStringLiteral("intrusionDuration"), area.intrusionDuration},
            {QStringLiteral("loiteringDuration"), area.loiteringDuration},
            {QStringLiteral("name"), area.name.trimmed()},
            {QStringLiteral("objectTypeFilter"), stringArray(area.objectTypeFilter)}};
        const QJsonValue handoverIndex = area.rawDefinition.value(
            QStringLiteral("handoverIndex"));
        if (!handoverIndex.isUndefined()) {
            definition.insert(QStringLiteral("handoverIndex"), handoverIndex);
        }
        definitions.append(definition);
    }

    payload = QJsonObject{{QStringLiteral("channel"), channel},
                          {QStringLiteral("enable"), enabled},
                          {QStringLiteral("definedArea"), definitions}};
    errorMessage.clear();
    return true;
}

bool IvaAreaUpdateBuilder::payloadMatchesChannel(
    const QJsonObject &expectedPayload,
    const IvaAreaConfiguration &configuration,
    const IvaChannelOptions &options,
    QString &errorMessage,
    const QSize &coordinateResolution)
{
    const int channel = expectedPayload.value(QStringLiteral("channel")).toInt(-1);
    const auto channelIt = std::find_if(
        configuration.channels.cbegin(), configuration.channels.cend(),
        [channel](const IvaChannelDefinition &item) {
            return item.channel == channel;
        });
    if (channelIt == configuration.channels.cend()) {
        errorMessage = QStringLiteral("The verification response is missing channel %1")
                           .arg(channel);
        return false;
    }
    QList<IvaAreaDefinition> channelAreas;
    for (const IvaAreaDefinition &area : configuration.areas) {
        if (area.channel == channel) {
            channelAreas.append(area);
        }
    }
    QJsonObject actualPayload;
    if (!buildChannelPayload(channel, channelIt->enabled, channelAreas, options,
                             actualPayload, errorMessage,
                             coordinateResolution)) {
        return false;
    }
    if (QJsonDocument(actualPayload).toJson(QJsonDocument::Compact)
        != QJsonDocument(expectedPayload).toJson(QJsonDocument::Compact)) {
        errorMessage = QStringLiteral("Camera verification did not match the requested IVA values");
        return false;
    }
    errorMessage.clear();
    return true;
}
