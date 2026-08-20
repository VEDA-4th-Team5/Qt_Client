#include "pages/parkingmappage.h"

#include <QApplication>
#include <QGraphicsItem>
#include <QGraphicsView>
#include <QGroupBox>
#include <QPushButton>
#include <QTemporaryDir>
#include <QWidget>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid()) return 2;

    ParkingMapPage page(directory.filePath(QStringLiteral("parking_map_layout.json")));
    QGraphicsView *mapView = page.findChild<QGraphicsView *>();
    QWidget *adminToolbar = page.findChild<QWidget *>(QStringLiteral("parkingMapAdminToolbar"));
    QGroupBox *searchGroup = page.findChild<QGroupBox *>(QStringLiteral("parkingZoneSearchGroup"));
    QGroupBox *editorGroup = page.findChild<QGroupBox *>(QStringLiteral("parkingMapLayoutEditor"));
    QPushButton *cameraButton = page.findChild<QPushButton *>(QStringLiteral("parkingOpenCameraButton"));
    QPushButton *evidenceButton = page.findChild<QPushButton *>(QStringLiteral("parkingOpenEvidenceButton"));

    if (!mapView || !mapView->scene() || !adminToolbar || !searchGroup || !editorGroup
        || !cameraButton) return 3;
    if (mapView->dragMode() != QGraphicsView::NoDrag) return 4;
    if (!adminToolbar->isHidden() || !searchGroup->isHidden() || !editorGroup->isHidden()) return 5;
    if (evidenceButton) return 6;

    int slotCount = 0;
    for (QGraphicsItem *item : mapView->scene()->items()) {
        if (item->data(0).toString().isEmpty()) continue;
        ++slotCount;
        if (item->flags().testFlag(QGraphicsItem::ItemIsMovable)) return 7;
    }
    if (slotCount != 8) return 8;
    return 0;
}
