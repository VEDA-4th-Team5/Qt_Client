#ifndef PARKINGMAPPAGE_H
#define PARKINGMAPPAGE_H

#include "models/parkingstate.h"

#include <QHash>
#include <QWidget>

class QEvent;
class QFrame;
class QLabel;
class QTableWidget;

class ParkingMapPage : public QWidget
{
    Q_OBJECT

public:
    explicit ParkingMapPage(QWidget *parent = nullptr);
    void render(const ParkingViewState &state);

signals:
    void slotClicked(const QString &slotId);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QFrame *createMapSlot(const QString &slotId);
    QWidget *createLegendItem(const QString &label, SlotState state);

    QHash<QString, QFrame *> m_slotFrames;
    QHash<QString, QLabel *> m_slotStateLabels;
    QHash<QString, QLabel *> m_slotPhotoLabels;
    QTableWidget *m_evStatusTable = nullptr;
};

#endif
