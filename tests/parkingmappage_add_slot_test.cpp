#include "pages/parkingmappage.h"

#include <QApplication>
#include <QGraphicsView>
#include <QMetaObject>
#include <QTableWidget>
#include <QTemporaryDir>

namespace {

QString slotId(const QString &prefix, int number)
{
    return QStringLiteral("%1-%2").arg(prefix).arg(number, 2, 10, QLatin1Char('0'));
}

QTableWidget *zoneTable(ParkingMapPage &page)
{
    const QList<QTableWidget *> tables = page.findChildren<QTableWidget *>();
    for (QTableWidget *table : tables) {
        if (table->columnCount() == 7
            && table->horizontalHeaderItem(0)
            && table->horizontalHeaderItem(0)->text() == QStringLiteral("Zone")) {
            return table;
        }
    }
    return nullptr;
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid()) return 2;

    ParkingMapPage page(directory.filePath(QStringLiteral("parking_map_layout.json")));

    ParkingViewState state;
    for (int number = 1; number <= 16; ++number) {
        EvSlotInfo evSlot;
        evSlot.slotId = slotId(QStringLiteral("EV"), number);
        evSlot.state = (number % 2 == 0) ? SlotState::Occupied : SlotState::Vacant;
        state.evSlots.insert(evSlot.slotId, evSlot);

        ParkingSlotInfo generalSlot;
        generalSlot.slotId = slotId(QStringLiteral("P"), number);
        generalSlot.state = (number % 2 == 0) ? SlotState::Occupied : SlotState::Vacant;
        state.parkingSlots.insert(generalSlot.slotId, generalSlot);
    }
    page.render(state);

    constexpr int additions = 12;
    for (int index = 0; index < additions / 2; ++index) {
        if (!QMetaObject::invokeMethod(&page, "addGeneralZone", Qt::DirectConnection)) return 3;
        if (!QMetaObject::invokeMethod(&page, "addEvZone", Qt::DirectConnection)) return 4;
    }

    QTableWidget *table = zoneTable(page);
    if (!table || table->rowCount() != 32 + additions) return 5;
    for (int row = 32; row < table->rowCount(); ++row) {
        const QTableWidgetItem *stateItem = table->item(row, 5);
        if (!stateItem || stateItem->text() != QStringLiteral("WAITING DATA")) return 6;
    }

    QGraphicsView *mapView = page.findChild<QGraphicsView *>();
    if (!mapView || !mapView->scene() || mapView->scene()->sceneRect().height() <= 560.0) return 7;

    for (int index = 0; index < 20; ++index) {
        if (!QMetaObject::invokeMethod(&page, "deleteSelectedZone", Qt::DirectConnection)) return 8;
        if (!QMetaObject::invokeMethod(&page, "addEvZone", Qt::DirectConnection)) return 9;
    }

    return table->rowCount() == 32 + additions ? 0 : 10;
}
