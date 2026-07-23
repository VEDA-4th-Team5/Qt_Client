#include "eventspage.h"

#include <QAbstractItemView>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

EventsPage::EventsPage(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    m_eventTable = new QTableWidget(0, 5, this);
    m_eventTable->setHorizontalHeaderLabels({QStringLiteral("Time"), QStringLiteral("Zone"), QStringLiteral("Event"), QStringLiteral("Message"), QStringLiteral("Status")});
    m_eventTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_eventTable->verticalHeader()->setVisible(false);
    m_eventTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_eventTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(m_eventTable, 1);
    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();
    auto *exportButton = new QPushButton(QStringLiteral("Export CSV"), this);
    buttonLayout->addWidget(exportButton);
    layout->addLayout(buttonLayout);
    connect(exportButton, &QPushButton::clicked, this, &EventsPage::exportCsv);
}

void EventsPage::appendEvent(const MonitoringEvent &event)
{
    const int row = m_eventTable->rowCount();
    m_eventTable->insertRow(row);
    const QStringList values = {
        monitoringEventTimeText(event), event.sourceId, event.eventType,
        event.message, monitoringEventStatusText(event)};
    for (int column = 0; column < values.size(); ++column) {
        m_eventTable->setItem(row, column, new QTableWidgetItem(values.at(column)));
    }
    m_eventTable->scrollToBottom();
}

void EventsPage::exportCsv()
{
    const QString defaultName = QStringLiteral("parking_event_log_")
        + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"))
        + QStringLiteral(".csv");
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Export Event Log CSV"), defaultName, QStringLiteral("CSV Files (*.csv)"));
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit exportResult(false, QStringLiteral("Could not open CSV file: ") + path);
        return;
    }
    auto escape = [](QString value) {
        value.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        return QLatin1Char('"') + value + QLatin1Char('"');
    };
    file.write("\xEF\xBB\xBF");
    QStringList headers;
    for (int column = 0; column < m_eventTable->columnCount(); ++column) {
        headers << escape(m_eventTable->horizontalHeaderItem(column)->text());
    }
    file.write(headers.join(QLatin1Char(',')).toUtf8());
    file.write("\n");
    for (int row = 0; row < m_eventTable->rowCount(); ++row) {
        QStringList values;
        for (int column = 0; column < m_eventTable->columnCount(); ++column) {
            const QTableWidgetItem *item = m_eventTable->item(row, column);
            values << escape(item ? item->text() : QString());
        }
        file.write(values.join(QLatin1Char(',')).toUtf8());
        file.write("\n");
    }
    emit exportResult(true, QStringLiteral("Event log CSV exported: ") + path);
}
