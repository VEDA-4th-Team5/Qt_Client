#include "parkingmappage.h"

#include <QAbstractItemView>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

ParkingMapPage::ParkingMapPage(QWidget *parent)
    : QWidget(parent)
{
    auto *pageLayout = new QHBoxLayout(this);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(10);
    auto *mapGroup = new QGroupBox(QStringLiteral("2D Parking Map"), this);
    auto *mapGrid = new QGridLayout(mapGroup);
    mapGrid->setSpacing(8);
    const QStringList slotIds = {QStringLiteral("EV-01"), QStringLiteral("EV-02"), QStringLiteral("EV-03"),
                                 QStringLiteral("P-01"), QStringLiteral("P-02"), QStringLiteral("P-03"), QStringLiteral("P-04")};
    mapGrid->addWidget(createMapSlot(slotIds.at(0)), 0, 0);
    mapGrid->addWidget(createMapSlot(slotIds.at(1)), 0, 1);
    mapGrid->addWidget(createMapSlot(slotIds.at(2)), 0, 2);
    mapGrid->addWidget(createMapSlot(slotIds.at(3)), 1, 0);
    mapGrid->addWidget(createMapSlot(slotIds.at(4)), 1, 1);
    mapGrid->addWidget(createMapSlot(slotIds.at(5)), 1, 2);
    mapGrid->addWidget(createMapSlot(slotIds.at(6)), 2, 1);
    auto *legendLayout = new QHBoxLayout;
    legendLayout->setSpacing(6);
    legendLayout->addWidget(createLegendItem(QStringLiteral("VACANT"), SlotState::Vacant));
    legendLayout->addWidget(createLegendItem(QStringLiteral("OCCUPIED"), SlotState::Occupied));
    legendLayout->addWidget(createLegendItem(QStringLiteral("NON_EV"), SlotState::NonEvAlert));
    legendLayout->addWidget(createLegendItem(QStringLiteral("OVERTIME"), SlotState::OvertimeAlert));
    legendLayout->addWidget(createLegendItem(QStringLiteral("ERROR"), SlotState::SensorError));
    legendLayout->addWidget(createLegendItem(QStringLiteral("FIRE?"), SlotState::FireSuspected));
    mapGrid->addLayout(legendLayout, 3, 0, 1, 3);
    pageLayout->addWidget(mapGroup, 2);

    auto *evGroup = new QGroupBox(QStringLiteral("EV Slot Status"), this);
    auto *evLayout = new QVBoxLayout(evGroup);
    m_evStatusTable = new QTableWidget(0, 5, evGroup);
    m_evStatusTable->setHorizontalHeaderLabels({QStringLiteral("Slot"), QStringLiteral("Plate"), QStringLiteral("Vehicle"), QStringLiteral("Duration"), QStringLiteral("Alarm")});
    m_evStatusTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_evStatusTable->verticalHeader()->setVisible(false);
    m_evStatusTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_evStatusTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    evLayout->addWidget(m_evStatusTable);
    pageLayout->addWidget(evGroup, 3);
}

QFrame *ParkingMapPage::createMapSlot(const QString &slotId)
{
    auto *frame = new QFrame(this);
    frame->setProperty("slotId", slotId);
    frame->installEventFilter(this);
    frame->setMinimumSize(145, 105);
    frame->setCursor(Qt::PointingHandCursor);
    auto *layout = new QVBoxLayout(frame);
    auto *idLabel = new QLabel(slotId, frame);
    idLabel->setAlignment(Qt::AlignCenter);
    idLabel->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: 800;"));
    auto *stateLabel = new QLabel(QStringLiteral("VACANT"), frame);
    stateLabel->setAlignment(Qt::AlignCenter);
    auto *photoLabel = new QLabel(QStringLiteral("NO PHOTO"), frame);
    photoLabel->setAlignment(Qt::AlignCenter);
    photoLabel->setStyleSheet(QStringLiteral("font-size: 10px;"));
    layout->addWidget(idLabel);
    layout->addWidget(stateLabel);
    layout->addWidget(photoLabel);
    m_slotFrames.insert(slotId, frame);
    m_slotStateLabels.insert(slotId, stateLabel);
    m_slotPhotoLabels.insert(slotId, photoLabel);
    return frame;
}

QWidget *ParkingMapPage::createLegendItem(const QString &label, SlotState state)
{
    auto *container = new QWidget(this);
    auto *layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *swatch = new QFrame(container);
    swatch->setFixedSize(14, 14);
    swatch->setStyleSheet(slotStateStyle(state));
    layout->addWidget(swatch);
    layout->addWidget(new QLabel(label, container));
    return container;
}

bool ParkingMapPage::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        const auto *mouseEvent = static_cast<QMouseEvent *>(event);
        const QString slotId = watched->property("slotId").toString();
        if (mouseEvent->button() == Qt::LeftButton && !slotId.isEmpty()) {
            emit slotClicked(slotId);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void ParkingMapPage::render(const ParkingViewState &state)
{
    for (auto it = m_slotFrames.cbegin(); it != m_slotFrames.cend(); ++it) {
        const QString slotId = it.key();
        SlotState slotState = SlotState::Vacant;
        if (state.evSlots.contains(slotId)) slotState = state.evSlots.value(slotId).state;
        else if (state.parkingSlots.contains(slotId)) slotState = state.parkingSlots.value(slotId).state;
        const int imageCount = state.slotImages.value(slotId).size();
        QString photoText = QStringLiteral("NO PHOTO");
        if (imageCount > 0) photoText = QStringLiteral("%1 PHOTO%2").arg(imageCount).arg(imageCount == 1 ? QString() : QStringLiteral("S"));
        else if (state.apiEnabled && slotState != SlotState::Vacant) photoText = QStringLiteral("CLICK DETAILS");
        it.value()->setStyleSheet(slotStateStyle(slotState));
        it.value()->setToolTip(QStringLiteral("Click to request current slot details and images"));
        m_slotStateLabels.value(slotId)->setText(slotStateText(slotState));
        m_slotPhotoLabels.value(slotId)->setText(photoText);
    }

    m_evStatusTable->setRowCount(0);
    const QStringList order = {QStringLiteral("EV-01"), QStringLiteral("EV-02"), QStringLiteral("EV-03")};
    for (const QString &slotId : order) {
        if (!state.evSlots.contains(slotId)) continue;
        const EvSlotInfo slot = state.evSlots.value(slotId);
        const int row = m_evStatusTable->rowCount();
        m_evStatusTable->insertRow(row);
        const QStringList values = {slot.slotId, slot.plateNumber, slot.isEv ? QStringLiteral("EV") : QStringLiteral("NON-EV"),
                                    slot.occupiedTime, slot.alarmText.isEmpty() ? slotStateText(slot.state) : slot.alarmText};
        for (int column = 0; column < values.size(); ++column) {
            m_evStatusTable->setItem(row, column, new QTableWidgetItem(values.at(column)));
        }
    }
}
