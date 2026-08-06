#include "pages/ivasettingspage.h"
#include "iva/ivavideocanvas.h"

#include <QApplication>
#include <QJsonArray>
#include <QLabel>
#include <QImage>
#include <QMouseEvent>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>

#include <iostream>

namespace {
bool require(bool condition, const char *message)
{
    if (condition) return true;
    std::cerr << "FAIL: " << message << '\n';
    return false;
}
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    IvaSettingsPage page(QStringLiteral("192.0.2.10"));

    IvaAreaConfiguration configuration;
    IvaChannelDefinition channel0;
    channel0.channel = 0;
    channel0.enabled = true;
    channel0.rawContainer = QJsonObject{
        {QStringLiteral("definedArea"), QJsonArray{QJsonObject{}}}};
    configuration.channels.append(channel0);
    IvaChannelDefinition channel1;
    channel1.channel = 1;
    channel1.enabled = false;
    channel1.rawContainer = QJsonObject{
        {QStringLiteral("definedArea"), QJsonArray{}}};
    configuration.channels.append(channel1);

    IvaAreaDefinition area;
    area.channel = 0;
    area.channelEnabled = true;
    area.areaIndex = 1;
    area.name = QStringLiteral("parking-a");
    area.detectionModes = {QStringLiteral("Intrusion"), QStringLiteral("Loitering")};
    area.objectTypeFilter = {QStringLiteral("Vehicle.Car")};
    area.areaCoordinates = {QPointF(0, 0), QPointF(1, 0), QPointF(1, 1)};
    area.appearanceDuration = 10;
    area.intrusionDuration = 5;
    area.loiteringDuration = 15;
    configuration.areas.append(area);

    IvaChannelOptions channelOptions;
    channelOptions.channel = 0;
    channelOptions.areaIndex = {1, 4};
    channelOptions.areaCoordinateCount = {4, 8};
    channelOptions.appearanceDuration = {1, 60};
    channelOptions.intrusionDuration = {0, 5};
    channelOptions.loiteringDuration = {1, 600};
    channelOptions.detectionModes = {QStringLiteral("Intrusion"),
                                     QStringLiteral("Loitering")};
    channelOptions.objectTypeFilters = {QStringLiteral("Vehicle.Car")};
    IvaAreaOptions options;
    options.channels.append(channelOptions);
    page.setOptions(options);
    IvaChannelCapability capability;
    capability.channel = 0;
    capability.ivaAreaSupported = true;
    capability.maxResolution = QSize(2592, 1520);
    WiseAiCapabilities capabilities;
    capabilities.channels.append(capability);
    page.setCapabilities(capabilities);
    page.setConfiguration(configuration);
    page.setPreviewFrame(0, QImage(1296, 760, QImage::Format_RGB32));
    auto *table = page.findChild<QTableWidget *>(QStringLiteral("ivaAreaTable"));
    if (!require(table && table->rowCount() == 1,
                 "validated IVA areas must populate the table")) return 1;
    if (!require(table->item(0, 0)->text() == QStringLiteral("CH1")
                     && table->item(0, 3)->text() == QStringLiteral("parking-a"),
                 "table must show channel and rule identity")) return 1;
    auto *summary = page.findChild<QLabel *>(QStringLiteral("ivaChannelSummaryLabel"));
    if (!require(summary && summary->text().contains(QStringLiteral("CH2 OFF (0 areas)")),
                 "disabled empty camera channels must remain visible")) return 1;
    auto *coordinateTable = page.findChild<QTableWidget *>(
        QStringLiteral("ivaCoordinateTable"));
    auto *indexSpin = page.findChild<QSpinBox *>(QStringLiteral("ivaRuleIndexSpin"));
    auto *apply = page.findChild<QPushButton *>(
        QStringLiteral("applyIvaConfigurationButton"));
    if (!require(coordinateTable && coordinateTable->rowCount() == 3
                     && indexSpin && indexSpin->minimum() == 1
                     && indexSpin->maximum() == 4 && apply && apply->isEnabled(),
                 "selected rules must expose editable coordinates and camera option ranges")) return 1;

    auto *canvas = page.findChild<IvaVideoCanvas *>(QStringLiteral("ivaVideoCanvas"));
    auto *draw = page.findChild<QPushButton *>(QStringLiteral("ivaDrawRectangleButton"));
    auto *discard = page.findChild<QPushButton *>(QStringLiteral("ivaDiscardDraftButton"));
    page.resize(1400, 900);
    page.show();
    app.processEvents();
    if (!require(canvas && canvas->frameCompatible() && draw && draw->isEnabled(),
                 "shared matching RTSP frames must enable rectangle drawing")) return 1;
    draw->click();
    const QPoint start = canvas->mapFromScene(QPointF(1200, 500));
    const QPoint end = canvas->mapFromScene(QPointF(1600, 900));
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(start),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(canvas->viewport(), &press);
    QMouseEvent move(QEvent::MouseMove, QPointF(end),
                     Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(canvas->viewport(), &move);
    QMouseEvent release(QEvent::MouseButtonRelease, QPointF(end),
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(canvas->viewport(), &release);
    app.processEvents();
    if (!require(table->rowCount() == 2
                     && table->item(1, 6)->text() == QStringLiteral("4")
                     && discard && discard->isEnabled(),
                 "dragging on the video must create a four-point draft rule")) return 1;
    discard->click();
    if (!require(table->rowCount() == 1 && table->currentRow() == 0
                     && coordinateTable->rowCount() == 3 && apply->isEnabled(),
                 "discard draft must restore the last-good rule selection and editor")) return 1;

    page.setRequestError(QStringLiteral("temporary failure"));
    if (!require(table->rowCount() == 1,
                 "request errors must retain the last-good table")) return 1;

    int refreshRequests = 0;
    QObject::connect(&page, &IvaSettingsPage::refreshRequested,
                     [&]() { ++refreshRequests; });
    auto *refresh = page.findChild<QPushButton *>(
        QStringLiteral("refreshIvaConfigurationButton"));
    if (!require(refresh && refresh->isEnabled(),
                 "refresh control must be available after an error")) return 1;
    refresh->click();
    if (!require(refreshRequests == 1 && !refresh->isEnabled(),
                 "refresh must emit once and guard duplicate requests")) return 1;

    page.setRequestError(QStringLiteral("refresh stopped for test"));
    page.setCameraIp(QStringLiteral("192.0.2.11"));
    if (!require(table->rowCount() == 0 && !apply->isEnabled(),
                 "changing camera IP must clear values from the previous device")) return 1;

    std::cout << "PASS: IVA settings page edits option-bounded rules and isolates camera state\n";
    return 0;
}
