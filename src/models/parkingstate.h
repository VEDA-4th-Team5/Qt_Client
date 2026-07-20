#ifndef PARKINGSTATE_H
#define PARKINGSTATE_H

#include "api/parkingmodels.h"

#include <QHash>
#include <QList>
#include <QString>

enum class SlotState {
    Vacant,
    Occupied,
    NonEvAlert,
    OvertimeAlert,
    SensorError,
    Acked
};

struct EvSlotInfo {
    QString slotId;
    QString plateNumber;
    bool isEv = false;
    QString occupiedTime;
    SlotState state = SlotState::Vacant;
    QString alarmText;
};

struct ParkingSlotInfo {
    QString slotId;
    SlotState state = SlotState::Vacant;
};

struct ParkingViewState {
    QHash<QString, EvSlotInfo> evSlots;
    QHash<QString, ParkingSlotInfo> parkingSlots;
    QHash<QString, QList<ParkingImageResource>> slotImages;
    QHash<QString, QString> slotPlateNumbers;
    bool apiEnabled = false;
};

QString slotStateText(SlotState state);
QString slotStateStyle(SlotState state);
SlotState slotStateFromText(const QString &text);
QString normalizeParkingSlotId(const QString &rawSlotId);

#endif
