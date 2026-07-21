#include "mainwindow.h"

#include "controllers/parkingcontroller.h"
#include "dialogs/slotevidencedialog.h"
#include "pages/dashboardpage.h"
#include "pages/debugpage.h"
#include "pages/eventspage.h"
#include "pages/parkingmappage.h"
#include "pages/settingspage.h"
#include "services/camerasettings.h"
#include "services/notificationcenter.h"


#include <QButtonGroup>
#include <QCoreApplication>
#include <QDir>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QStringList>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
QString formatNotificationBrief(const NotificationRecord &notification)
{
    return QStringLiteral("[%1] %2 | %3 | %4")
        .arg(notification.severity,
             notification.sourceId,
             notification.eventType,
             notification.message);
}

QString formatNotificationDetail(const NotificationRecord &notification)
{
    return QStringLiteral(
        "Time: %1\nSource: %2\nSeverity: %3\nEvent: %4\nStatus: %5\nMessage: %6")
        .arg(notification.time,
             notification.sourceId,
             notification.severity,
             notification.eventType,
             notification.status,
             notification.message);
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_cameraSettings(cameraConfigPath())
    , m_notificationCenter(new NotificationCenter(this))
{
    buildUi();
    m_parkingController = new ParkingController(clientConfigPath(), clientLocalConfigPath(), this);
    connectPages();
    m_parkingController->start();
}

void MainWindow::buildUi()
{
    setWindowTitle(QStringLiteral("Smart Parking Integrated Monitoring System"));
    resize(1360, 860);
    auto *central = new QWidget(this);
    auto *rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    auto *sidebar = new QFrame(central);
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setFixedWidth(190);
    auto *sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(14, 18, 14, 18);
    sideLayout->setSpacing(8);
    auto *brand = new QLabel(QStringLiteral("Smart Parking"), sidebar);
    brand->setObjectName(QStringLiteral("brandLabel"));
    sideLayout->addWidget(brand);
    sideLayout->addSpacing(16);

    m_pages = new QStackedWidget(central);
    auto *navGroup = new QButtonGroup(this);
    navGroup->setExclusive(true);
    auto addNavButton = [&](const QString &text, int index) {
        auto *button = new QPushButton(text, sidebar);
        button->setCheckable(true);
        button->setProperty("nav", true);
        navGroup->addButton(button, index);
        sideLayout->addWidget(button);
        connect(button, &QPushButton::clicked, m_pages,
                [this, index]() { m_pages->setCurrentIndex(index); });
        return button;
    };
    auto *dashboardButton = addNavButton(QStringLiteral("Dashboard"), 0);
    addNavButton(QStringLiteral("Parking Map"), 1);
    m_eventsNavButton = addNavButton(QStringLiteral("Events"), 2);
    addNavButton(QStringLiteral("Settings"), 3);
    addNavButton(QStringLiteral("Debug"), 4);
    sideLayout->addStretch();

    auto *contentWidget = new QWidget(central);
    auto *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(16, 14, 16, 14);
    contentLayout->setSpacing(10);
    auto *headerLayout = new QHBoxLayout;
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(10);
    auto *title = new QLabel(QStringLiteral("Smart Parking Integrated Monitoring System"), contentWidget);
    title->setObjectName(QStringLiteral("titleLabel"));
    headerLayout->addWidget(title);
    headerLayout->addStretch();

    auto *alarmWidget = new QWidget(contentWidget);
    auto *alarmLayout = new QHBoxLayout(alarmWidget);
    alarmLayout->setContentsMargins(0, 0, 0, 0);
    alarmLayout->setSpacing(4);
    m_notificationButton = new QToolButton(alarmWidget);
    m_notificationButton->setText(QString::fromUtf8("\xF0\x9F\x94\x94"));
    m_notificationButton->setToolTip(QStringLiteral("Event notifications"));
    m_notificationButton->setFixedSize(36, 34);
    m_notificationButton->setStyleSheet(QStringLiteral(
        "QToolButton { background: #eceff1; border: 1px solid #b0bec5; border-radius: 6px; font-size: 18px; }"
        "QToolButton:hover { background: #fff3e0; border-color: #ff9800; }"));
    m_notificationBadge = new QLabel(QStringLiteral("0"), alarmWidget);
    m_notificationBadge->setAlignment(Qt::AlignCenter);
    m_notificationBadge->setFixedSize(18, 18);
    m_notificationBadge->setStyleSheet(QStringLiteral(
        "QLabel { background: #d32f2f; color: white; border-radius: 9px; font-size: 12px; font-weight: 900; }"));
    alarmLayout->addWidget(m_notificationButton);
    alarmLayout->addWidget(m_notificationBadge);
    headerLayout->addWidget(alarmWidget);
    contentLayout->addLayout(headerLayout);
    connect(m_notificationButton, &QToolButton::clicked,
            this, &MainWindow::showNotificationPopup);
    connect(m_notificationCenter, &NotificationCenter::notificationsChanged,
            this, &MainWindow::updateNotificationIndicator);
    updateNotificationIndicator();

    m_alertBanner = new QLabel(QStringLiteral("System ready | Mock data displayed"), contentWidget);
    m_alertBanner->setAlignment(Qt::AlignCenter);
    m_alertBanner->setMinimumHeight(34);
    contentLayout->addWidget(m_alertBanner);

    m_dashboardPage = new DashboardPage(m_cameraSettings.rtspUrls(QStringLiteral("profile3")),
                                        m_cameraSettings.rtspUrls(QStringLiteral("profile2")), m_pages);
    m_parkingMapPage = new ParkingMapPage(m_pages);
    m_eventsPage = new EventsPage(m_pages);
    m_settingsPage = new SettingsPage(m_cameraSettings.configPath(), m_cameraSettings.cameraIp(), m_pages);
    m_debugPage = new DebugPage(m_pages);
    m_pages->addWidget(m_dashboardPage);
    m_pages->addWidget(m_parkingMapPage);
    m_pages->addWidget(m_eventsPage);
    m_pages->addWidget(m_settingsPage);
    m_pages->addWidget(m_debugPage);
    contentLayout->addWidget(m_pages, 1);
    rootLayout->addWidget(sidebar);
    rootLayout->addWidget(contentWidget, 1);
    setCentralWidget(central);
    dashboardButton->setChecked(true);
    m_pages->setCurrentIndex(0);

    setStyleSheet(QStringLiteral(
        "QMainWindow { background: #f5f7f9; }"
        "QFrame#sidebar { background: #1f2a33; }"
        "QLabel#brandLabel { color: white; font-size: 18px; font-weight: 800; }"
        "QLabel#titleLabel { font-size: 22px; font-weight: 700; color: #202124; }"
        "QGroupBox { font-weight: 700; border: 1px solid #c7cdd4; border-radius: 6px; margin-top: 8px; padding-top: 10px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }"
        "QTableWidget { background: white; gridline-color: #d8dde3; }"
        "QHeaderView::section { background: #263238; color: white; padding: 6px; border: none; }"
        "QPushButton { background: #263238; color: white; border: none; border-radius: 4px; padding: 9px 14px; font-weight: 600; }"
        "QPushButton:hover { background: #37474f; }"
        "QPushButton[nav=\"true\"] { text-align: left; background: transparent; color: #dce5ea; padding: 11px 12px; }"
        "QPushButton[nav=\"true\"]:hover { background: #2e3c46; }"
        "QPushButton[nav=\"true\"]:checked { background: #406274; color: white; }"));
}

void MainWindow::connectPages()
{
    connect(m_parkingController, &ParkingController::stateChanged,
            this, &MainWindow::renderParkingState);
    connect(m_parkingController, &ParkingController::bannerChanged, this,
            [this](const QString &message, bool hasAlert) {
                m_alertBanner->setText(message);
                m_alertBanner->setStyleSheet(hasAlert
                    ? QStringLiteral("background: #ffebee; color: #b71c1c; border: 1px solid #ef9a9a; border-radius: 4px; font-weight: 800;")
                    : QStringLiteral("background: #e8f5e9; color: #1b5e20; border: 1px solid #a5d6a7; border-radius: 4px; font-weight: 700;"));
            });
    connect(m_parkingController, &ParkingController::eventLogged, this,
            [this](const QString &time, const QString &zone, const QString &eventType,
                   const QString &message, const QString &status) {
                m_dashboardPage->prependEvent(time, zone, eventType, message, status);
                m_eventsPage->appendEvent(time, zone, eventType, message, status);
                m_notificationCenter->ingestEvent(time, zone, eventType, message, status);
            });
    connect(m_parkingController, &ParkingController::statusMessageChanged,
            m_debugPage, &DebugPage::setLastMessage);
    connect(m_parkingController, &ParkingController::slotDetailReady,
            this, &MainWindow::showSlotEvidence);
    connect(m_parkingController, &ParkingController::detailError, this,
            [this](const QString &message) {
                QMessageBox::warning(this, QStringLiteral("Parking detail"), message);
            });
    connect(m_parkingMapPage, &ParkingMapPage::slotClicked,
            m_parkingController, &ParkingController::requestSlotDetail);
    connect(m_debugPage, &DebugPage::clearAlarmsRequested,
            m_parkingController, &ParkingController::clearAlarms);
    connect(m_debugPage, &DebugPage::toggleMockEvRequested,
            m_parkingController, &ParkingController::toggleMockEv);
    connect(m_debugPage, &DebugPage::nonEvAlertRequested,
            m_parkingController, &ParkingController::triggerNonEvAlert);
    connect(m_debugPage, &DebugPage::overtimeAlertRequested,
            m_parkingController, &ParkingController::triggerOvertimeAlert);
    connect(m_debugPage, &DebugPage::sensorErrorRequested,
            m_parkingController, &ParkingController::triggerSensorError);
    connect(m_debugPage, &DebugPage::randomizeParkingRequested,
            m_parkingController, &ParkingController::randomizeParkingSlots);
    connect(m_debugPage, &DebugPage::sampleMessagesRequested,
            m_parkingController, &ParkingController::simulateIncomingMessages);
    connect(m_debugPage, &DebugPage::manualMessageRequested,
            m_parkingController, &ParkingController::processIncomingMessage);
    connect(m_eventsPage, &EventsPage::exportResult, this,
            [this](bool success, const QString &message) {
                m_parkingController->recordEvent(
                    QStringLiteral("SYSTEM"),
                    success ? QStringLiteral("EXPORT_CSV") : QStringLiteral("EXPORT_ERROR"),
                    message, success ? QStringLiteral("DONE") : QStringLiteral("FAILED"));
            });
    connect(m_settingsPage, &SettingsPage::saveCameraIpRequested,
            this, &MainWindow::saveCameraIpLastOctet);
    connect(m_settingsPage, &SettingsPage::saveServerBaseUrlRequested,
            m_parkingController, &ParkingController::updateServerBaseUrl);
    connect(m_settingsPage, &SettingsPage::reconnectServerRequested,
            m_parkingController, &ParkingController::reconnectNow);
    connect(m_parkingController, &ParkingController::serverBaseUrlChanged,
            m_settingsPage, &SettingsPage::setServerBaseUrl);
    connect(m_parkingController, &ParkingController::serverConnectionChanged,
            m_settingsPage, &SettingsPage::setServerConnectionStatus);
    connect(m_parkingController, &ParkingController::serverConfigurationError, this,
            [this](const QString &message) {
                QMessageBox::warning(this, QStringLiteral("Server API"), message);
            });
}

void MainWindow::renderParkingState()
{
    const ParkingViewState &state = m_parkingController->state();
    m_parkingMapPage->render(state);
    int occupied = 0;
    int vacant = 0;
    int sensorErrors = 0;
    for (const ParkingSlotInfo &slot : state.parkingSlots) {
        if (slot.state == SlotState::Occupied) ++occupied;
        else if (slot.state == SlotState::SensorError) ++sensorErrors;
        else ++vacant;
    }
    m_dashboardPage->setSummary(
        state.parkingSlots.size(), occupied, vacant, sensorErrors);
}

void MainWindow::showSlotEvidence(const QString &slotId)
{
    const QList<ParkingImageResource> images = m_parkingController->images(slotId);
    if (images.isEmpty()) {
        QMessageBox::information(
            this, QStringLiteral("Parking detail"),
            QStringLiteral("%1 has no image data.").arg(slotId));
        return;
    }
    auto *dialog = new SlotEvidenceDialog(
        slotId, m_parkingController->slotState(slotId),
        m_parkingController->plateNumber(slotId), images,
        m_parkingController->imageLoader(), this);
    dialog->show();
}

void MainWindow::updateNotificationIndicator()
{
    const int unreadCount = m_notificationCenter ? m_notificationCenter->unreadCount() : 0;
    const bool hasNotifications = m_notificationCenter && m_notificationCenter->hasNotifications();
    if (m_notificationBadge) {
        m_notificationBadge->setVisible(unreadCount > 0);
        m_notificationBadge->setText(unreadCount > 9 ? QStringLiteral("9+") : QString::number(unreadCount));
    }
    if (m_notificationButton) {
        m_notificationButton->setToolTip(
            unreadCount > 0
                ? QStringLiteral("Event notifications (%1 unread)").arg(unreadCount)
                : QStringLiteral("Event notifications"));
        m_notificationButton->setStyleSheet(hasNotifications
            ? QStringLiteral(
                "QToolButton { background: #fff3e0; color: #b71c1c; border: 1px solid #fb8c00; border-radius: 6px; font-size: 18px; font-weight: 900; }"
                "QToolButton:hover { background: #ffe0b2; border-color: #ef6c00; }")
            : QStringLiteral(
                "QToolButton { background: #eceff1; border: 1px solid #b0bec5; border-radius: 6px; font-size: 18px; }"
                "QToolButton:hover { background: #fff3e0; border-color: #ff9800; }"));
    }
}

void MainWindow::showNotificationPopup()
{
    if (m_notificationPopup) {
        m_notificationPopup->close();
        return;
    }

    const QList<NotificationRecord> notifications =
        m_notificationCenter ? m_notificationCenter->notifications() : QList<NotificationRecord>();

    auto *popup = new QFrame(nullptr, Qt::Popup | Qt::FramelessWindowHint);
    m_notificationPopup = popup;
    popup->setAttribute(Qt::WA_DeleteOnClose);
    popup->setObjectName(QStringLiteral("notificationPopup"));
    popup->setFixedWidth(390);
    popup->setStyleSheet(QStringLiteral(
        "QFrame#notificationPopup { background: #ffffff; border: 1px solid #b0bec5; border-radius: 8px; }"
        "QLabel#notificationTitle { color: #17212b; font-size: 14px; font-weight: 800; }"
        "QLabel#notificationHint { color: #607d8b; font-size: 11px; }"
        "QLabel#notificationDetail { background: #f5f7f9; border: 1px solid #d8dde3; border-radius: 6px; color: #263238; padding: 8px; }"
        "QListWidget { background: #ffffff; border: 1px solid #d8dde3; border-radius: 6px; outline: 0; }"
        "QListWidget::item { border-bottom: 1px solid #eceff1; padding: 7px; }"
        "QListWidget::item:selected { background: #fff3e0; color: #17212b; }"
        "QPushButton { background: #263238; color: white; border: none; border-radius: 4px; padding: 7px 10px; font-weight: 700; }"
        "QPushButton:hover { background: #37474f; }"));
    connect(popup, &QObject::destroyed, this, [this]() {
        m_notificationPopup = nullptr;
    });

    auto *layout = new QVBoxLayout(popup);
    layout->setContentsMargins(12, 10, 12, 12);
    layout->setSpacing(8);

    auto *headerLayout = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("Event notifications"), popup);
    title->setObjectName(QStringLiteral("notificationTitle"));
    auto *countLabel = new QLabel(
        notifications.isEmpty()
            ? QStringLiteral("No open alerts")
            : QStringLiteral("%1 open").arg(notifications.size()),
        popup);
    countLabel->setObjectName(QStringLiteral("notificationHint"));
    countLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    headerLayout->addWidget(title);
    headerLayout->addStretch();
    headerLayout->addWidget(countLabel);
    layout->addLayout(headerLayout);

    auto *list = new QListWidget(popup);
    list->setFixedHeight(notifications.isEmpty() ? 72 : qMin(250, qMax(88, notifications.size() * 58)));
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(list);

    auto *detailLabel = new QLabel(popup);
    detailLabel->setObjectName(QStringLiteral("notificationDetail"));
    detailLabel->setWordWrap(true);
    detailLabel->setMinimumHeight(118);
    detailLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    layout->addWidget(detailLabel);

    if (notifications.isEmpty()) {
        auto *item = new QListWidgetItem(QStringLiteral("No important notifications are currently open."), list);
        item->setFlags(Qt::NoItemFlags);
        detailLabel->setText(QStringLiteral("All important events are clear. Use Events for the complete event log."));
    } else {
        for (int i = 0; i < notifications.size(); ++i) {
            const NotificationRecord &notification = notifications.at(i);
            const QString newPrefix = notification.unread ? QStringLiteral("[NEW] ") : QString();
            auto *item = new QListWidgetItem(
                newPrefix + QStringLiteral("%1 | %2\n%3")
                    .arg(notification.severity,
                         notification.sourceId,
                         notification.title.isEmpty() ? notification.eventType : notification.title),
                list);
            item->setData(Qt::UserRole, i);
            item->setSizeHint(QSize(0, 56));
        }
        list->setCurrentRow(0);
        detailLabel->setText(formatNotificationDetail(notifications.first()));
        connect(list, &QListWidget::itemClicked, popup,
                [detailLabel, notifications](QListWidgetItem *item) {
                    if (!item) return;
                    const int index = item->data(Qt::UserRole).toInt();
                    if (index < 0 || index >= notifications.size()) return;
                    detailLabel->setText(formatNotificationDetail(notifications.at(index)));
                });
    }

    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();
    auto *eventsButton = new QPushButton(QStringLiteral("View all events"), popup);
    auto *closeButton = new QPushButton(QStringLiteral("Close"), popup);
    buttonLayout->addWidget(eventsButton);
    buttonLayout->addWidget(closeButton);
    layout->addLayout(buttonLayout);

    connect(eventsButton, &QPushButton::clicked, this, [this, popup]() {
        popup->close();
        showEventsPage();
    });
    connect(closeButton, &QPushButton::clicked, popup, &QFrame::close);

    popup->adjustSize();
    const QPoint buttonBottomRight =
        m_notificationButton ? m_notificationButton->mapToGlobal(m_notificationButton->rect().bottomRight())
                             : mapToGlobal(rect().topRight());
    popup->move(buttonBottomRight.x() - popup->width() + 1,
                buttonBottomRight.y() + 8);
    popup->show();
    if (m_notificationCenter) {
        m_notificationCenter->markAllRead();
    }
}

void MainWindow::showEventsPage()
{
    if (m_pages) {
        m_pages->setCurrentIndex(2);
    }
    if (m_eventsNavButton) {
        m_eventsNavButton->setChecked(true);
    }
}

QString MainWindow::cameraConfigPath() const
{
#ifdef SMART_PARKING_CONFIG_DIR
    return QDir(QStringLiteral(SMART_PARKING_CONFIG_DIR))
        .absoluteFilePath(QStringLiteral("camera_config.ini"));
#else
    return QDir(QCoreApplication::applicationDirPath())
        .absoluteFilePath(QStringLiteral("../config/camera_config.ini"));
#endif
}

QString MainWindow::clientConfigPath() const
{
#ifdef SMART_PARKING_CONFIG_DIR
    return QDir(QStringLiteral(SMART_PARKING_CONFIG_DIR))
        .absoluteFilePath(QStringLiteral("client_config.ini"));
#else
    return QDir(QCoreApplication::applicationDirPath())
        .absoluteFilePath(QStringLiteral("../config/client_config.ini"));
#endif
}

QString MainWindow::clientLocalConfigPath() const
{
#ifdef SMART_PARKING_CONFIG_DIR
    return QDir(QStringLiteral(SMART_PARKING_CONFIG_DIR))
        .absoluteFilePath(QStringLiteral("client_config.local.ini"));
#else
    return QDir(QCoreApplication::applicationDirPath())
        .absoluteFilePath(QStringLiteral("../config/client_config.local.ini"));
#endif
}

void MainWindow::saveCameraIpLastOctet(const QString &lastOctetText)
{
    QString newIp;
    QString errorMessage;
    if (!m_cameraSettings.saveLastOctet(lastOctetText, newIp, errorMessage)) {
        QMessageBox::warning(this, QStringLiteral("Camera IP"), errorMessage);
        return;
    }
    m_settingsPage->setCameraIp(newIp);
    m_dashboardPage->setRtspUrls(
        m_cameraSettings.rtspUrls(QStringLiteral("profile3")),
        m_cameraSettings.rtspUrls(QStringLiteral("profile2")));
    m_parkingController->recordEvent(
        QStringLiteral("SYSTEM"), QStringLiteral("CAMERA_IP_UPDATED"),
        QStringLiteral("Camera IP changed to ") + newIp, QStringLiteral("DONE"));
}
