#include "mainwindow.h"

#include "controllers/parkingcontroller.h"
#include "dialogs/slotevidencedialog.h"
#include "pages/dashboardpage.h"
#include "pages/debugpage.h"
#include "pages/eventspage.h"
#include "pages/parkingmappage.h"
#include "pages/settingspage.h"
#include "services/camerasettings.h"


#include <QButtonGroup>
#include <QCoreApplication>
#include <QDir>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_cameraSettings(cameraConfigPath())
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

    auto *pages = new QStackedWidget(central);
    auto *navGroup = new QButtonGroup(this);
    navGroup->setExclusive(true);
    auto addNavButton = [&](const QString &text, int index) {
        auto *button = new QPushButton(text, sidebar);
        button->setCheckable(true);
        button->setProperty("nav", true);
        navGroup->addButton(button, index);
        sideLayout->addWidget(button);
        connect(button, &QPushButton::clicked, pages,
                [pages, index]() { pages->setCurrentIndex(index); });
        return button;
    };
    auto *dashboardButton = addNavButton(QStringLiteral("Dashboard"), 0);
    addNavButton(QStringLiteral("Parking Map"), 1);
    addNavButton(QStringLiteral("Events"), 2);
    addNavButton(QStringLiteral("Settings"), 3);
    addNavButton(QStringLiteral("Debug"), 4);
    sideLayout->addStretch();

    auto *contentWidget = new QWidget(central);
    auto *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(16, 14, 16, 14);
    contentLayout->setSpacing(10);
    auto *title = new QLabel(QStringLiteral("Smart Parking Integrated Monitoring System"), contentWidget);
    title->setObjectName(QStringLiteral("titleLabel"));
    contentLayout->addWidget(title);
    m_alertBanner = new QLabel(QStringLiteral("System ready | Mock data displayed"), contentWidget);
    m_alertBanner->setAlignment(Qt::AlignCenter);
    m_alertBanner->setMinimumHeight(34);
    contentLayout->addWidget(m_alertBanner);

    m_dashboardPage = new DashboardPage(m_cameraSettings.rtspUrls(QStringLiteral("profile3")),
                                        m_cameraSettings.rtspUrls(QStringLiteral("profile2")), pages);
    m_parkingMapPage = new ParkingMapPage(pages);
    m_eventsPage = new EventsPage(pages);
    m_settingsPage = new SettingsPage(m_cameraSettings.configPath(), m_cameraSettings.cameraIp(), pages);
    m_debugPage = new DebugPage(pages);
    pages->addWidget(m_dashboardPage);
    pages->addWidget(m_parkingMapPage);
    pages->addWidget(m_eventsPage);
    pages->addWidget(m_settingsPage);
    pages->addWidget(m_debugPage);
    contentLayout->addWidget(pages, 1);
    rootLayout->addWidget(sidebar);
    rootLayout->addWidget(contentWidget, 1);
    setCentralWidget(central);
    dashboardButton->setChecked(true);
    pages->setCurrentIndex(0);

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
