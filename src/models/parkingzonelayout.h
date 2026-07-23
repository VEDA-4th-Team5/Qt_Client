#ifndef PARKINGZONELAYOUT_H
#define PARKINGZONELAYOUT_H

#include <QList>
#include <QRectF>
#include <QString>

struct ParkingZoneLayout {
    QString zoneId;
    QString zoneType;
    QString displayName;
    QRectF rect;
    double rotation = 0.0;
    QString cameraChannel;
    QString ivaAreaId;
    QString hallSensorId;
    bool enabled = true;
};

QList<ParkingZoneLayout> defaultParkingZoneLayout();
bool loadParkingZoneLayout(const QString &path, QList<ParkingZoneLayout> *zones, QString *errorMessage);
bool saveParkingZoneLayout(const QString &path, const QList<ParkingZoneLayout> &zones, QString *errorMessage);

#endif
