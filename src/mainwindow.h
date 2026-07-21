#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "services/camerasettings.h"

#include <QMainWindow>
#include <QString>

class DashboardPage;
class DebugPage;
class EventsPage;
class QFrame;
class QLabel;
class NotificationCenter;
class ParkingController;
class ParkingMapPage;
class QPushButton;
class SettingsPage;
class QStackedWidget;
class QToolButton;

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
    void updateNotificationIndicator();
    void showNotificationPopup();
    void showEventsPage();
    QString cameraConfigPath() const;
    QString clientConfigPath() const;
    QString clientLocalConfigPath() const;
    QString parkingMapLayoutPath() const;

    CameraSettings m_cameraSettings;
    NotificationCenter *m_notificationCenter = nullptr;
    QLabel *m_alertBanner = nullptr;
    QToolButton *m_notificationButton = nullptr;
    QLabel *m_notificationBadge = nullptr;
    QFrame *m_notificationPopup = nullptr;
    QStackedWidget *m_pages = nullptr;
    QPushButton *m_eventsNavButton = nullptr;
    DashboardPage *m_dashboardPage = nullptr;
    ParkingMapPage *m_parkingMapPage = nullptr;
    EventsPage *m_eventsPage = nullptr;
    SettingsPage *m_settingsPage = nullptr;
    DebugPage *m_debugPage = nullptr;
    ParkingController *m_parkingController = nullptr;
};

#endif
