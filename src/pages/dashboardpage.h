#ifndef DASHBOARDPAGE_H
#define DASHBOARDPAGE_H

#include "diagnostics/diagnostictypes.h"
#include "models/monitoringevent.h"

#include <QList>
#include <QStringList>
#include <QWidget>

class QGridLayout;
class QLabel;
class QQuickWidget;
class QTableWidget;
class QTimer;

class DashboardPage : public QWidget
{
    Q_OBJECT

public:
    explicit DashboardPage(const QStringList &lowRtspUrls,
                           const QStringList &highRtspUrls,
                           QWidget *parent = nullptr);

    void setSummary(int total, int occupied, int vacant, int sensorErrors);
    void prependEvent(const MonitoringEvent &event);
    void setRtspUrls(const QStringList &lowRtspUrls, const QStringList &highRtspUrls);

signals:
    void rtspDiagnosticsChanged(const QList<RtspChannelDiagnostic> &channels);

private slots:
    void handleVideoChannelClicked();

private:
    QWidget *createVideoChannel(int channelIndex, const QString &channel, const QString &title,
                                const QString &lowRtspUrl, const QString &highRtspUrl);
    void startDelayedVideoStreams();
    void toggleVideoChannel(int channelIndex);
    void publishRtspDiagnostics();

    QGridLayout *m_videoGrid = nullptr;
    QList<QWidget *> m_videoChannelWidgets;
    QList<QQuickWidget *> m_videoQuickWidgets;
    QLabel *m_totalSlotsLabel = nullptr;
    QLabel *m_occupiedSlotsLabel = nullptr;
    QLabel *m_vacantSlotsLabel = nullptr;
    QLabel *m_sensorErrorLabel = nullptr;
    QTableWidget *m_recentEventTable = nullptr;
    QTimer *m_diagnosticTimer = nullptr;
    int m_expandedVideoChannel = -1;
};

#endif
