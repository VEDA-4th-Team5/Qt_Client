#ifndef EVENTSPAGE_H
#define EVENTSPAGE_H

#include "models/monitoringevent.h"

#include <QWidget>

class QTableWidget;

class EventsPage : public QWidget
{
    Q_OBJECT

public:
    explicit EventsPage(QWidget *parent = nullptr);
    void appendEvent(const MonitoringEvent &event);

signals:
    void exportResult(bool success, const QString &message);

private slots:
    void exportCsv();

private:
    QTableWidget *m_eventTable = nullptr;
};

#endif
