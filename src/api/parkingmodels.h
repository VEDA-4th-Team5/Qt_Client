#pragma once

#include <QDateTime>
#include <QList>
#include <QString>
#include <QUrl>

struct ParkingImageResource {
    QUrl url;
    QDateTime timestamp;
    QString role;
    QString processing;
};

struct ParkingSlotSnapshot {
    QString slotId;
    QString state;
    QString plateNumber;
    bool isEv = false;
    QDateTime occupiedSince;
    int elapsedSeconds = 0;
    QString alarm;
    QList<ParkingImageResource> images;
};

struct ParkingSnapshot {
    QDateTime generatedAt;
    QList<ParkingSlotSnapshot> parkingSlots;
};