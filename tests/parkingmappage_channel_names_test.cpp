#include "models/parkingzonelayout.h"
#include "pages/parkingmappage.h"

#include <QApplication>
#include <QDialog>
#include <QFile>
#include <QFrame>
#include <QGraphicsRectItem>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsView>
#include <QGroupBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMetaObject>
#include <QPushButton>
#include <QStackedWidget>
#include <QTemporaryDir>

namespace {

bool sceneHasText(ParkingMapPage &page, const QString &expected)
{
    QGraphicsView *view = page.findChild<QGraphicsView *>(
        QStringLiteral("parkingMapDetailView"));
    if (!view || !view->scene()) return false;
    for (QGraphicsItem *item : view->scene()->items()) {
        auto *textItem = dynamic_cast<QGraphicsSimpleTextItem *>(item);
        if (textItem && textItem->text() == expected) return true;
    }
    return false;
}

bool sceneHasRect(ParkingMapPage &page, const QRectF &expected)
{
    QGraphicsView *view = page.findChild<QGraphicsView *>(
        QStringLiteral("parkingMapDetailView"));
    if (!view || !view->scene()) return false;
    for (QGraphicsItem *item : view->scene()->items()) {
        auto *rectItem = dynamic_cast<QGraphicsRectItem *>(item);
        if (rectItem && !rectItem->parentItem() && rectItem->rect() == expected) return true;
    }
    return false;
}

bool overviewSceneHasText(ParkingMapPage &page, const QString &expected)
{
    QGraphicsView *view = page.findChild<QGraphicsView *>(
        QStringLiteral("parkingOverviewMapView"));
    if (!view || !view->scene()) return false;
    for (QGraphicsItem *item : view->scene()->items()) {
        auto *textItem = dynamic_cast<QGraphicsSimpleTextItem *>(item);
        if (textItem && textItem->text() == expected) return true;
    }
    return false;
}

bool writeVersionOneLayout(const QString &path)
{
    QJsonObject zone;
    zone.insert(QStringLiteral("zone_id"), QStringLiteral("EV-01"));
    zone.insert(QStringLiteral("zone_type"), QStringLiteral("EV"));
    zone.insert(QStringLiteral("display_name"), QStringLiteral("EV-01"));
    zone.insert(QStringLiteral("x"), 64);
    zone.insert(QStringLiteral("y"), 96);
    zone.insert(QStringLiteral("width"), 84);
    zone.insert(QStringLiteral("height"), 58);
    zone.insert(QStringLiteral("rotation"), 0);
    zone.insert(QStringLiteral("camera_channel"), QStringLiteral("CH1"));
    zone.insert(QStringLiteral("iva_area_id"), QStringLiteral("IVA1"));
    zone.insert(QStringLiteral("hall_sensor_id"), QStringLiteral("HALL_EV_01"));
    zone.insert(QStringLiteral("enabled"), true);

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("canvas_width"), 920);
    root.insert(QStringLiteral("canvas_height"), 560);
    root.insert(QStringLiteral("zones"), QJsonArray{zone});

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) return false;
    return file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) > 0;
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid()) return 2;

    const QString layoutPath = directory.filePath(QStringLiteral("parking_map_layout.json"));
    ParkingMapPage page(layoutPath);
    if (page.hasUnsavedLayoutChanges()) return 3;
    if (page.channelDisplayName(QStringLiteral("CH1")) != QStringLiteral("CH1")) return 4;
    if (!page.channelDisplayName(QStringLiteral("EAST")).isEmpty()) return 5;
    if (!sceneHasText(page, QStringLiteral("CH1"))) return 6;
    if (!sceneHasText(page, QStringLiteral("CH2"))) return 48;
    if (!sceneHasText(page, QStringLiteral("CH3"))) return 49;
    if (!sceneHasText(page, QStringLiteral("CH4"))) return 50;
    if (!sceneHasText(page, QStringLiteral("EV-01"))
        || !sceneHasText(page, QStringLiteral("EV-04"))) return 59;
    if (!sceneHasText(page, QStringLiteral("P-01"))
        || !sceneHasText(page, QStringLiteral("P-04"))) return 60;
    if (sceneHasText(page, QStringLiteral("EV-05"))
        || sceneHasText(page, QStringLiteral("P-05"))) return 61;
    if (!sceneHasRect(page, QRectF(30, 32, 860, 150))) return 55;
    if (!sceneHasRect(page, QRectF(30, 204, 160, 64))) return 56;
    if (!sceneHasRect(page, QRectF(30, 282, 860, 150))) return 57;
    if (!sceneHasRect(page, QRectF(730, 204, 160, 64))) return 58;
    if (sceneHasRect(page, QRectF(190, 266, 540, 64))) return 62;
    if (sceneHasText(page, QStringLiteral("CH1 | UPPER PARKING | PARKING CONTROL"))) return 51;
    if (sceneHasText(page, QStringLiteral(
            "CORRIDOR MONITORING · no parking slots · event focus"))) return 54;
    if (!sceneHasText(page, QStringLiteral(
            "4 slots · vacant 0 · occupied 0 · waiting 4 · alert 0"))) return 52;
    if (sceneHasText(page, QStringLiteral("No recent events"))) return 53;

    QStackedWidget *operationStack = page.findChild<QStackedWidget *>(
        QStringLiteral("parkingMapOperationViewStack"));
    QWidget *overviewPage = page.findChild<QWidget *>(
        QStringLiteral("parkingMapOverviewPage"));
    QWidget *detailPage = page.findChild<QWidget *>(
        QStringLiteral("parkingMapZoneDetailPage"));
    QPushButton *openDetailButton = page.findChild<QPushButton *>(
        QStringLiteral("parkingOpenZoneDetailButton"));
    QPushButton *backToOverviewButton = page.findChild<QPushButton *>(
        QStringLiteral("parkingBackToOverviewButton"));
    if (!operationStack || !overviewPage || !detailPage || !openDetailButton
        || !backToOverviewButton || operationStack->currentWidget() != overviewPage) return 64;
    if (!overviewSceneHasText(page, QStringLiteral("CH1 · LIVE"))
        || !overviewSceneHasText(page, QStringLiteral("DEMO · L SHAPE"))
        || !overviewSceneHasText(page, QStringLiteral("DEMO · U SHAPE"))
        || !overviewSceneHasText(page, QStringLiteral("DEMO · STRAIGHT A"))
        || !overviewSceneHasText(page, QStringLiteral("DEMO · STRAIGHT B"))) return 67;
    openDetailButton->click();
    QApplication::processEvents();
    if (operationStack->currentWidget() != detailPage) return 65;
    backToOverviewButton->click();
    QApplication::processEvents();
    if (operationStack->currentWidget() != overviewPage) return 66;

    QPushButton *namesButton = page.findChild<QPushButton *>(QStringLiteral("editChannelNamesButton"));
    QWidget *adminToolbar = page.findChild<QWidget *>(QStringLiteral("parkingMapAdminToolbar"));
    QGroupBox *searchGroup = page.findChild<QGroupBox *>(QStringLiteral("parkingZoneSearchGroup"));
    QGroupBox *editorGroup = page.findChild<QGroupBox *>(QStringLiteral("parkingMapLayoutEditor"));
    if (!namesButton || !adminToolbar || !searchGroup || !editorGroup
        || !adminToolbar->isHidden()
        || !searchGroup->isHidden() || !editorGroup->isHidden()
        || namesButton->isEnabled()) return 7;
    if (!QMetaObject::invokeMethod(&page, "setEditMode", Qt::DirectConnection,
                                   Q_ARG(bool, true))) return 8;
    if (namesButton->isEnabled()) return 9;

    if (page.setChannelDisplayName(QStringLiteral("EAST"), QStringLiteral("Invalid"))) return 10;
    if (page.hasUnsavedLayoutChanges()) return 11;
    if (!page.setChannelDisplayName(QStringLiteral(" ch1 "),
                                    QStringLiteral("  East EV Zone  "))) return 12;
    if (!page.hasUnsavedLayoutChanges()) return 13;
    if (page.channelDisplayName(QStringLiteral("CH1")) != QStringLiteral("East EV Zone")) return 14;
    if (!sceneHasText(page, QStringLiteral("CH1"))) return 15;

    QString saveError;
    if (!page.saveLayoutNow(&saveError) || !saveError.isEmpty()) return 16;
    if (page.hasUnsavedLayoutChanges()) return 17;

    QFile savedFile(layoutPath);
    if (!savedFile.open(QIODevice::ReadOnly | QIODevice::Text)) return 18;
    const QJsonDocument savedDocument = QJsonDocument::fromJson(savedFile.readAll());
    if (!savedDocument.isObject()) return 19;
    const QJsonObject savedRoot = savedDocument.object();
    if (savedRoot.value(QStringLiteral("version")).toInt() != 3) return 20;
    const QJsonObject savedNames = savedRoot.value(QStringLiteral("channel_display_names")).toObject();
    if (savedNames.value(QStringLiteral("CH1")).toString() != QStringLiteral("East EV Zone")) return 21;
    if (savedNames.contains(QStringLiteral("EAST"))) return 22;
    const QJsonArray savedZones = savedRoot.value(QStringLiteral("zones")).toArray();
    if (savedZones.isEmpty()) return 23;
    const QJsonObject firstZone = savedZones.at(0).toObject();
    if (firstZone.value(QStringLiteral("camera_channel")).toString() != QStringLiteral("CH1")) return 24;
    if (firstZone.value(QStringLiteral("iva_area_id")).toString() != QStringLiteral("IVA1")) return 25;
    if (firstZone.value(QStringLiteral("slot_order")).toInt() != 1
        || firstZone.contains(QStringLiteral("x"))
        || firstZone.contains(QStringLiteral("rotation"))) return 63;

    ParkingMapPage reloadedPage(layoutPath);
    if (reloadedPage.hasUnsavedLayoutChanges()) return 26;
    if (reloadedPage.channelDisplayName(QStringLiteral("CH1")) != QStringLiteral("East EV Zone")) return 27;
    if (!sceneHasText(reloadedPage, QStringLiteral("CH1"))) return 28;
    if (!reloadedPage.setChannelDisplayName(QStringLiteral("CH1"), QStringLiteral("Unsaved Name"))) return 29;
    if (!QMetaObject::invokeMethod(&reloadedPage, "reloadLayout", Qt::DirectConnection)) return 30;
    if (reloadedPage.hasUnsavedLayoutChanges()) return 31;
    if (reloadedPage.channelDisplayName(QStringLiteral("CH1")) != QStringLiteral("East EV Zone")) return 32;
    if (!QMetaObject::invokeMethod(&reloadedPage, "resetDefaultLayout", Qt::DirectConnection)) return 33;
    if (!reloadedPage.hasUnsavedLayoutChanges()) return 34;
    if (reloadedPage.channelDisplayName(QStringLiteral("CH1")) != QStringLiteral("CH1")) return 35;
    if (!sceneHasText(reloadedPage, QStringLiteral("CH1"))) return 36;

    const QString versionOnePath = directory.filePath(QStringLiteral("parking_map_layout_v1.json"));
    if (!writeVersionOneLayout(versionOnePath)) return 37;
    QList<ParkingZoneLayout> versionOneZones;
    ParkingChannelDisplayNames versionOneNames;
    versionOneNames.insert(QStringLiteral("CH4"), QStringLiteral("Stale value"));
    QString loadError;
    if (!loadParkingZoneLayout(versionOnePath, &versionOneZones, &versionOneNames, &loadError)) return 38;
    if (!loadError.isEmpty() || versionOneZones.size() != 1) return 39;
    if (!versionOneNames.isEmpty()) return 40;
    if (versionOneZones.constFirst().cameraChannel != QStringLiteral("CH1")) return 41;
    if (versionOneZones.constFirst().ivaAreaId != QStringLiteral("IVA1")) return 42;

    ParkingMapPage versionOnePage(versionOnePath);
    if (versionOnePage.hasUnsavedLayoutChanges()) return 43;
    if (versionOnePage.channelDisplayName(QStringLiteral("CH1")) != QStringLiteral("CH1")) return 44;
    if (!sceneHasText(versionOnePage, QStringLiteral("CH1"))) return 45;

    QPushButton *helpButton = page.findChild<QPushButton *>(
        QStringLiteral("parkingMapHelpButton"));
    if (!helpButton || helpButton->icon().isNull()
        || helpButton->text() != QStringLiteral("Parking Map 안내")) return 46;
    helpButton->click();
    QApplication::processEvents();
    QDialog *helpDialog = page.findChild<QDialog *>(
        QStringLiteral("parkingMapHelpDialog"));
    QWidget *miniMap = helpDialog
        ? helpDialog->findChild<QWidget *>(QStringLiteral("parkingMapHelpMiniMap"))
        : nullptr;
    QWidget *stateLegend = helpDialog
        ? helpDialog->findChild<QWidget *>(QStringLiteral("parkingMapHelpStateLegend"))
        : nullptr;
    QLabel *safetyNotes = helpDialog
        ? helpDialog->findChild<QLabel *>(QStringLiteral("parkingMapHelpSafetyNotes"))
        : nullptr;
    if (!helpDialog || !miniMap || !stateLegend || !safetyNotes
        || miniMap->findChildren<QFrame *>().size() < 4
        || stateLegend->findChildren<QFrame *>().size() < 7
        || helpDialog->findChild<QWidget *>(QStringLiteral("parkingMapHelpEditFlow"))
        || helpDialog->findChild<QWidget *>(QStringLiteral("parkingMapHelpSlotTools"))
        || helpDialog->findChild<QGroupBox *>(QStringLiteral("parkingMapHelpSaveActions"))
        || !safetyNotes->text().contains(QStringLiteral("slot_id"))) return 47;
    helpDialog->close();

    return 0;
}
