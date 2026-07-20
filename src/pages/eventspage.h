#ifndef EVENTSPAGE_H
#define EVENTSPAGE_H

#include <QWidget>

class QTableWidget;

class EventsPage : public QWidget
{
    Q_OBJECT

public:
    explicit EventsPage(QWidget *parent = nullptr);
    void appendEvent(const QString &time, const QString &zone, const QString &eventType,
                     const QString &message, const QString &status);

signals:
    void exportResult(bool success, const QString &message);

private slots:
    void exportCsv();

private:
    QTableWidget *m_eventTable = nullptr;
};

#endif
