#include "iva/ivaareaoptionsparser.h"
#include "iva/ivaareaupdatebuilder.h"

#include <QCoreApplication>
#include <QJsonArray>

#include <iostream>

namespace {
bool require(bool condition, const char *message)
{
    if (condition) return true;
    std::cerr << "FAIL: " << message << '\n';
    return false;
}

QJsonDocument optionsDocument()
{
    const QJsonObject defined{
        {QStringLiteral("appearanceDuration"),
         QJsonObject{{QStringLiteral("min"), 1}, {QStringLiteral("max"), 60}}},
        {QStringLiteral("areaCoordinates"),
         QJsonObject{{QStringLiteral("min"), 4}, {QStringLiteral("max"), 8}}},
        {QStringLiteral("detectionModes"),
         QJsonArray{QStringLiteral("Intrusion"), QStringLiteral("Loitering")}},
        {QStringLiteral("index"),
         QJsonObject{{QStringLiteral("min"), 1}, {QStringLiteral("max"), 4}}},
        {QStringLiteral("intrusionDuration"),
         QJsonObject{{QStringLiteral("min"), 0}, {QStringLiteral("max"), 5}}},
        {QStringLiteral("loiteringDuration"),
         QJsonObject{{QStringLiteral("min"), 1}, {QStringLiteral("max"), 600}}},
        {QStringLiteral("objectTypeFilter"),
         QJsonArray{QStringLiteral("Person"), QStringLiteral("Vehicle.Car")}}};
    return QJsonDocument(QJsonObject{
        {QStringLiteral("ivaAreaOptions"),
         QJsonArray{QJsonObject{{QStringLiteral("channel"), 2},
                                {QStringLiteral("definedAreaOptions"), defined}}}}});
}

IvaAreaDefinition validArea()
{
    IvaAreaDefinition area;
    area.channel = 2;
    area.areaIndex = 1;
    area.name = QStringLiteral("EV-01");
    area.detectionModes = {QStringLiteral("Intrusion"), QStringLiteral("Loitering")};
    area.objectTypeFilter = {QStringLiteral("Vehicle.Car")};
    area.areaCoordinates = {QPointF(90, 533), QPointF(782, 533),
                            QPointF(782, 1344), QPointF(90, 1344)};
    area.appearanceDuration = 1;
    area.intrusionDuration = 5;
    area.loiteringDuration = 30;
    area.rawDefinition = QJsonObject{
        {QStringLiteral("handoverIndex"), 3},
        {QStringLiteral("vendorExtension"), QStringLiteral("do-not-send")}};
    return area;
}
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    IvaAreaOptions options;
    QString error;
    if (!require(IvaAreaOptionsParser::parse(optionsDocument(), options, error),
                 "actual WiseAI ivaAreaOptions schema must parse")) return 1;
    const IvaChannelOptions *channelOptions = options.forChannel(2);
    if (!require(channelOptions && channelOptions->areaIndex.minimum == 1
                     && channelOptions->areaIndex.maximum == 4
                     && channelOptions->areaCoordinateCount.minimum == 4
                     && channelOptions->areaCoordinateCount.maximum == 8,
                 "camera-provided rule and polygon ranges must normalize")) return 1;

    IvaAreaDefinition area = validArea();
    QJsonObject payload;
    if (!require(IvaAreaUpdateBuilder::buildChannelPayload(
                     2, true, {area}, *channelOptions, payload, error),
                 "valid channel edits must produce a PUT payload")) return 1;
    if (!require(payload.keys()
                     == QStringList{QStringLiteral("channel"),
                                    QStringLiteral("definedArea"),
                                    QStringLiteral("enable")},
                 "PUT root must match the WebViewer channel contract")) return 1;
    const QJsonObject definition = payload.value(QStringLiteral("definedArea"))
                                       .toArray().at(0).toObject();
    if (!require(definition.value(QStringLiteral("handoverIndex")).toInt() == 3
                     && !definition.contains(QStringLiteral("vendorExtension")),
                 "WebViewer whitelist must preserve handoverIndex and omit unknown fields")) return 1;

    area.areaCoordinates.removeLast();
    if (!require(!IvaAreaUpdateBuilder::buildChannelPayload(
                     2, true, {area}, *channelOptions, payload, error)
                     && error.contains(QStringLiteral("point count"),
                                       Qt::CaseInsensitive),
                 "camera-provided polygon limits must reject three points")) return 1;

    area = validArea();
    area.detectionModes = {QStringLiteral("UnknownMode")};
    if (!require(!IvaAreaUpdateBuilder::buildChannelPayload(
                     2, true, {area}, *channelOptions, payload, error)
                     && error.contains(QStringLiteral("unsupported"),
                                       Qt::CaseInsensitive),
                 "unsupported camera detection modes must be rejected")) return 1;

    area = validArea();
    area.areaCoordinates[0].setX(90.5);
    if (!require(!IvaAreaUpdateBuilder::buildChannelPayload(
                     2, true, {area}, *channelOptions, payload, error)
                     && error.contains(QStringLiteral("integer pixels"),
                                       Qt::CaseInsensitive),
                 "fractional coordinates must be rejected before the camera can corrupt them")) return 1;

    area = validArea();
    area.areaCoordinates[0].setX(2593);
    if (!require(!IvaAreaUpdateBuilder::buildChannelPayload(
                     2, true, {area}, *channelOptions, payload, error,
                     QSize(2592, 1520))
                     && error.contains(QStringLiteral("resolution"),
                                       Qt::CaseInsensitive),
                 "coordinates outside the capability resolution must be rejected")) return 1;

    area = validArea();
    IvaAreaDefinition duplicate = area;
    duplicate.name = QStringLiteral("EV-02");
    if (!require(!IvaAreaUpdateBuilder::buildChannelPayload(
                     2, true, {area, duplicate}, *channelOptions, payload, error)
                     && error.contains(QStringLiteral("duplicated")),
                 "duplicate rule indexes must be rejected before PUT")) return 1;

    std::cout << "PASS: camera options drive the exact validated WiseAI PUT payload\n";
    return 0;
}
