#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "services/camerasettings.h"

#include <QMainWindow>

class DashboardPage;
class DebugPage;
class EventsPage;
class QLabel;
class ParkingController;
class ParkingMapPage;
class SettingsPage;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void buildUi();
    void connectPages();
    void renderParkingState();
    void showSlotEvidence(const QString &slotId);
    void saveCameraIpLastOctet(const QString &lastOctetText);
    QString cameraConfigPath() const;
    QString clientConfigPath() const;
    QString clientLocalConfigPath() const;

    CameraSettings m_cameraSettings;
    QLabel *m_alertBanner = nullptr;
    DashboardPage *m_dashboardPage = nullptr;
    ParkingMapPage *m_parkingMapPage = nullptr;
    EventsPage *m_eventsPage = nullptr;
    SettingsPage *m_settingsPage = nullptr;
    DebugPage *m_debugPage = nullptr;
    ParkingController *m_parkingController = nullptr;
};

#endif