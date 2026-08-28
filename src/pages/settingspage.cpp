#include "settingspage.h"

#include "widgets/pagehelp.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QSpinBox>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>

SettingsPage::SettingsPage(const QString &configPath, const QString &cameraIp,
                           QWidget *parent,
                           const QString &cameraUsername,
                           const QString &cameraPassword)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    layout->addLayout(createPageHeader(
        this, QStringLiteral("System"),
        QStringLiteral("Configure connections, policies, diagnostics, and local test tools")));
    createPageHelpButton(
        this, this,
        {QStringLiteral("settings"), QStringLiteral("System"),
         QStringLiteral("System 사용 안내"),
         QStringLiteral("연결 설정, 운영 정책, 런타임 진단과 로컬 테스트 도구를 한 화면에서 관리합니다."),
         QStringLiteral(
             "<b>1. Connections</b><br>카메라 IPv4 주소·계정과 Pi API endpoint를 저장하고 연결을 다시 시도합니다.<br><br>"
             "<b>2. Parking Policy</b><br>서버의 초과주차 기준을 읽고 1분~24시간 범위에서 변경합니다.<br><br>"
             "<b>3. Diagnostics</b><br>Pi API, RTSP 채널과 주차 데이터 상태·지연·마지막 오류를 읽기 전용으로 확인합니다.<br><br>"
             "<b>4. Live Logs</b><br>Level, Module과 검색어로 로컬 Qt 진단 로그를 필터링합니다.<br><br>"
             "<b>5. Test Tools</b><br>Mock EV, 테스트 이벤트와 normalized RX 메시지를 로컬 Qt 프로세스에만 적용합니다."),
         QStringLiteral(
             "※ Pi 주소 변경 시 MQTT host도 같은 서버 host를 따릅니다. TLS 실패 시 평문으로 자동 전환하지 않습니다.\n"
             "   Test Tools는 Pi 서버로 전송되지 않는 로컬 simulation sandbox이며 실제 장비 통합 성공 증거가 아닙니다.")});

    auto *systemBanner = new QLabel(
        QStringLiteral("SYSTEM MANAGEMENT  ·  Configure endpoints and parking policy, inspect runtime health, and run local test scenarios."),
        this);
    systemBanner->setObjectName(QStringLiteral("systemScopeBanner"));
    systemBanner->setWordWrap(true);
    systemBanner->setStyleSheet(QStringLiteral(
        "background:#e8f1f5;color:#173b4d;border:1px solid #9fb9c6;"
        "border-radius:8px;padding:10px 12px;font-weight:800;"));
    layout->addWidget(systemBanner);

    m_systemTabs = new QTabWidget(this);
    m_systemTabs->setObjectName(QStringLiteral("systemTabWidget"));
    m_systemTabs->setDocumentMode(true);
    m_systemTabs->setStyleSheet(QStringLiteral(
        "QTabWidget::pane { border:1px solid #c7d0d8;border-radius:7px;"
        "background:#ffffff;top:-1px; }"
        "QTabBar::tab { background:#e8edf0;color:#455a64;padding:10px 18px;"
        "border:1px solid #c7d0d8;border-bottom:none;min-width:118px;font-weight:700; }"
        "QTabBar::tab:selected { background:#ffffff;color:#173b4d;"
        "border-top:3px solid #ef7d00;padding-top:8px; }"
        "QTabBar::tab:hover:!selected { background:#f3f6f8;color:#263238; }"));

    auto *configurationScroll = new QScrollArea(m_systemTabs);
    configurationScroll->setObjectName(QStringLiteral("systemConnectionsScrollArea"));
    configurationScroll->setWidgetResizable(true);
    configurationScroll->setFrameShape(QFrame::NoFrame);
    auto *configuration = new QWidget(configurationScroll);
    auto *configurationLayout = new QVBoxLayout(configuration);
    configurationLayout->setContentsMargins(12, 12, 12, 12);
    configurationLayout->setSpacing(12);
    configurationScroll->setWidget(configuration);

    auto *configurationNote = new QLabel(
        QStringLiteral("CONNECTION CHANGES · Applying a Pi endpoint updates this PC's local override and reconnects the API. MQTT follows the same host."),
        configuration);
    configurationNote->setWordWrap(true);
    configurationNote->setStyleSheet(QStringLiteral(
        "background:#fff8e1;color:#7a4f00;border:1px solid #ffe082;"
        "border-radius:6px;padding:8px;font-weight:700;"));
    configurationLayout->addWidget(configurationNote);

    auto *group = new QGroupBox(QStringLiteral("Camera & Pi Server"), configuration);
    auto *grid = new QGridLayout(group);
    m_cameraIpLabel = new QLabel(group);
    m_cameraIpInput = new QLineEdit(group);
    m_cameraIpInput->setPlaceholderText(QStringLiteral("e.g. 192.168.10.20"));
    m_cameraIpInput->setClearButtonEnabled(true);
    m_cameraUsernameInput = new QLineEdit(group);
    m_cameraUsernameInput->setObjectName(QStringLiteral("cameraUsernameInput"));
    m_cameraUsernameInput->setPlaceholderText(QStringLiteral("e.g. admin"));
    m_cameraUsernameInput->setClearButtonEnabled(true);
    m_cameraPasswordInput = new QLineEdit(group);
    m_cameraPasswordInput->setObjectName(QStringLiteral("cameraPasswordInput"));
    m_cameraPasswordInput->setEchoMode(QLineEdit::Password);
    m_cameraPasswordInput->setPlaceholderText(QStringLiteral("Camera password"));
    auto *saveCameraButton = new QPushButton(QStringLiteral("Save camera access"), group);
    saveCameraButton->setObjectName(QStringLiteral("saveCameraSettingsButton"));
    auto *cameraLayout = new QHBoxLayout;
    cameraLayout->addWidget(m_cameraIpInput, 1);
    auto *passwordLayout = new QHBoxLayout;
    passwordLayout->addWidget(m_cameraPasswordInput, 1);
    auto *showPassword = new QCheckBox(QStringLiteral("Show"), group);
    showPassword->setObjectName(QStringLiteral("showCameraPasswordCheck"));
    showPassword->setToolTip(QStringLiteral("Show the camera password on this screen"));
    passwordLayout->addWidget(showPassword);
    auto *cameraActions = new QHBoxLayout;
    cameraActions->addStretch();
    cameraActions->addWidget(saveCameraButton);

    m_serverSchemeInput = new QComboBox(group);
    m_serverSchemeInput->setObjectName(QStringLiteral("serverApiSchemeInput"));
    m_serverSchemeInput->addItems(
        {QStringLiteral("http"), QStringLiteral("https")});
    m_serverHostInput = new QLineEdit(group);
    m_serverHostInput->setObjectName(QStringLiteral("serverApiHostInput"));
    m_serverHostInput->setPlaceholderText(QStringLiteral("e.g. raspberry-pi.local"));
    m_serverHostInput->setClearButtonEnabled(true);
    m_serverPortInput = new QSpinBox(group);
    m_serverPortInput->setObjectName(QStringLiteral("serverApiPortInput"));
    m_serverPortInput->setRange(1, 65535);
    m_serverPortInput->setValue(8080);
    auto *saveServerButton = new QPushButton(QStringLiteral("Apply & reconnect"), group);
    saveServerButton->setObjectName(QStringLiteral("saveServerSettingsButton"));
    auto *reconnectButton = new QPushButton(QStringLiteral("Retry connection"), group);
    reconnectButton->setObjectName(QStringLiteral("reconnectServerButton"));
    auto *serverLayout = new QHBoxLayout;
    serverLayout->addWidget(m_serverSchemeInput);
    serverLayout->addWidget(m_serverHostInput, 1);
    serverLayout->addWidget(m_serverPortInput);
    auto *serverActions = new QHBoxLayout;
    serverActions->addStretch();
    serverActions->addWidget(reconnectButton);
    serverActions->addWidget(saveServerButton);
    m_serverConnectionLabel = new QLabel(QStringLiteral("Not connected"), group);

    auto makeSectionLabel = [group](const QString &text) {
        auto *label = new QLabel(text, group);
        label->setStyleSheet(QStringLiteral(
            "color:#34515e;font-size:12px;font-weight:900;"
            "border-bottom:1px solid #d7e0e5;padding:4px 0;"));
        return label;
    };
    grid->setHorizontalSpacing(14);
    grid->setVerticalSpacing(9);
    grid->setColumnMinimumWidth(0, 190);
    grid->setColumnStretch(1, 1);
    grid->addWidget(makeSectionLabel(QStringLiteral("CAMERA ACCESS")), 0, 0, 1, 2);
    grid->addWidget(new QLabel(QStringLiteral("Configuration file"), group), 1, 0);
    grid->addWidget(new QLabel(configPath, group), 1, 1);
    grid->addWidget(new QLabel(QStringLiteral("Active IP"), group), 2, 0);
    grid->addWidget(m_cameraIpLabel, 2, 1);
    grid->addWidget(new QLabel(QStringLiteral("Camera IP address"), group), 3, 0);
    grid->addLayout(cameraLayout, 3, 1);
    grid->addWidget(new QLabel(QStringLiteral("Username"), group), 4, 0);
    grid->addWidget(m_cameraUsernameInput, 4, 1);
    grid->addWidget(new QLabel(QStringLiteral("Password"), group), 5, 0);
    grid->addLayout(passwordLayout, 5, 1);
    grid->addWidget(new QLabel(QStringLiteral("Credential storage"), group), 6, 0);
    grid->addWidget(new QLabel(QStringLiteral("Local camera config only · Never commit real credentials."), group), 6, 1);
    grid->addLayout(cameraActions, 7, 1);
    grid->addWidget(makeSectionLabel(QStringLiteral("PI SERVER")), 8, 0, 1, 2);
    grid->addWidget(new QLabel(QStringLiteral("API endpoint"), group), 9, 0);
    grid->addLayout(serverLayout, 9, 1);
    grid->addWidget(new QLabel(QStringLiteral("Endpoint format"), group), 10, 0);
    grid->addWidget(new QLabel(QStringLiteral("Protocol  ·  Host  ·  Port"), group), 10, 1);
    grid->addWidget(new QLabel(QStringLiteral("Connection status"), group), 11, 0);
    grid->addWidget(m_serverConnectionLabel, 11, 1);
    grid->addWidget(new QLabel(QStringLiteral("MQTT routing"), group), 12, 0);
    grid->addWidget(new QLabel(QStringLiteral("Automatically follows the Pi API host."), group), 12, 1);
    grid->addWidget(new QLabel(QStringLiteral("Local override file"), group), 13, 0);
    grid->addWidget(new QLabel(QStringLiteral("client_config.local.ini"), group), 13, 1);
    grid->addWidget(new QLabel(QStringLiteral("RTSP URL pattern"), group), 14, 0);
    grid->addWidget(new QLabel(QStringLiteral("/{channel}/{profile}/media.smp"), group), 14, 1);
    grid->addLayout(serverActions, 15, 1);
    configurationLayout->addWidget(group);
    configurationLayout->addStretch();
    m_systemTabs->addTab(configurationScroll, QStringLiteral("Connections"));

    auto *policyScroll = new QScrollArea(m_systemTabs);
    policyScroll->setObjectName(QStringLiteral("systemPolicyScrollArea"));
    policyScroll->setWidgetResizable(true);
    policyScroll->setFrameShape(QFrame::NoFrame);
    auto *policy = new QWidget(policyScroll);
    auto *policyLayout = new QVBoxLayout(policy);
    policyLayout->setContentsMargins(12, 12, 12, 12);
    policyLayout->setSpacing(12);
    policyScroll->setWidget(policy);
    auto *policyNote = new QLabel(
        QStringLiteral("SERVER POLICY · Reload reads the current value from the Pi server. Apply policy updates and verifies the server value."),
        policy);
    policyNote->setWordWrap(true);
    policyNote->setStyleSheet(QStringLiteral(
        "background:#eef6ee;color:#285b2b;border:1px solid #a5d6a7;"
        "border-radius:6px;padding:8px;font-weight:700;"));
    policyLayout->addWidget(policyNote);

    auto *overstayGroup = new QGroupBox(QStringLiteral("Parking Policy"), policy);
    auto *overstayGrid = new QGridLayout(overstayGroup);
    m_currentOverstayLabel = new QLabel(QStringLiteral("Not loaded"), overstayGroup);
    m_currentOverstayLabel->setObjectName(QStringLiteral("currentOverstayThresholdLabel"));
    m_currentOverstayLabel->setStyleSheet(QStringLiteral(
        "background:#f4f7f8;color:#173b4d;border:1px solid #c7d0d8;"
        "border-radius:5px;padding:7px 9px;font-weight:800;"));
    m_applyPolicyLabel = new QLabel(QStringLiteral("Not loaded"), overstayGroup);
    m_applyPolicyLabel->setObjectName(QStringLiteral("overstayApplyPolicyLabel"));
    m_applyPolicyLabel->setWordWrap(true);
    m_applyPolicyLabel->setStyleSheet(QStringLiteral(
        "background:#f4f7f8;color:#455a64;border:1px solid #c7d0d8;"
        "border-radius:5px;padding:7px 9px;font-weight:700;"));

    m_overstayHoursInput = new QSpinBox(overstayGroup);
    m_overstayHoursInput->setObjectName(QStringLiteral("overstayHoursInput"));
    m_overstayHoursInput->setRange(0, 24);
    m_overstayHoursInput->setSuffix(QStringLiteral(" h"));
    m_overstayMinutesInput = new QSpinBox(overstayGroup);
    m_overstayMinutesInput->setObjectName(QStringLiteral("overstayMinutesInput"));
    m_overstayMinutesInput->setRange(0, 59);
    m_overstayMinutesInput->setSuffix(QStringLiteral(" min"));
    m_overstaySecondsInput = new QSpinBox(overstayGroup);
    m_overstaySecondsInput->setObjectName(QStringLiteral("overstaySecondsInput"));
    m_overstaySecondsInput->setRange(0, 59);
    m_overstaySecondsInput->setSuffix(QStringLiteral(" sec"));
    for (QSpinBox *input : {m_overstayHoursInput, m_overstayMinutesInput,
                            m_overstaySecondsInput}) {
        input->setMinimumSize(112, 34);
        input->setAlignment(Qt::AlignRight);
    }
    auto *thresholdLayout = new QHBoxLayout;
    thresholdLayout->addWidget(m_overstayHoursInput);
    thresholdLayout->addWidget(m_overstayMinutesInput);
    thresholdLayout->addWidget(m_overstaySecondsInput);
    thresholdLayout->addStretch();

    m_overstayStatusLabel = new QLabel(
        QStringLiteral("Open System to load the current policy."), overstayGroup);
    m_overstayStatusLabel->setObjectName(QStringLiteral("overstayStatusLabel"));
    m_overstayStatusLabel->setWordWrap(true);
    m_overstayStatusLabel->setMinimumHeight(38);
    m_refreshOverstayButton = new QPushButton(QStringLiteral("Reload"), overstayGroup);
    m_refreshOverstayButton->setObjectName(QStringLiteral("refreshOverstayThresholdButton"));
    m_applyOverstayButton = new QPushButton(QStringLiteral("Apply policy"), overstayGroup);
    m_applyOverstayButton->setObjectName(QStringLiteral("applyOverstayThresholdButton"));
    auto *overstayButtonLayout = new QHBoxLayout;
    overstayButtonLayout->addStretch();
    overstayButtonLayout->addWidget(m_refreshOverstayButton);
    overstayButtonLayout->addWidget(m_applyOverstayButton);

    overstayGrid->addWidget(new QLabel(QStringLiteral("Current threshold"), overstayGroup), 0, 0);
    overstayGrid->addWidget(m_currentOverstayLabel, 0, 1);
    overstayGrid->addWidget(new QLabel(QStringLiteral("Applies to"), overstayGroup), 1, 0);
    overstayGrid->addWidget(m_applyPolicyLabel, 1, 1);
    overstayGrid->addWidget(new QLabel(QStringLiteral("New threshold"), overstayGroup), 2, 0);
    overstayGrid->addLayout(thresholdLayout, 2, 1);
    overstayGrid->addWidget(new QLabel(QStringLiteral("Policy update"), overstayGroup), 3, 0);
    overstayGrid->addWidget(m_overstayStatusLabel, 3, 1);
    overstayGrid->addLayout(overstayButtonLayout, 4, 1);
    policyLayout->addWidget(overstayGroup);
    policyLayout->addStretch();
    m_systemTabs->addTab(policyScroll, QStringLiteral("Parking Policy"));
    layout->addWidget(m_systemTabs, 1);

    const QString primaryButtonStyle = QStringLiteral(
        "QPushButton { background:#ef7d00;color:white;border:1px solid #d86f00;"
        "border-radius:5px;padding:7px 12px;font-weight:800; }"
        "QPushButton:hover { background:#ff8f1f; }"
        "QPushButton:disabled { background:#d7dce0;color:#8b9499;border-color:#c7cdd1; }");
    saveCameraButton->setStyleSheet(primaryButtonStyle);
    saveServerButton->setStyleSheet(primaryButtonStyle);
    m_applyOverstayButton->setStyleSheet(primaryButtonStyle);
    reconnectButton->setStyleSheet(QStringLiteral(
        "QPushButton { background:#ffffff;color:#294b5a;border:1px solid #8ca7b3;"
        "border-radius:5px;padding:7px 12px;font-weight:700; }"
        "QPushButton:hover { background:#edf4f7;border-color:#5f8292; }"));
    m_refreshOverstayButton->setStyleSheet(reconnectButton->styleSheet());
    for (QWidget *field : {static_cast<QWidget *>(m_cameraIpInput),
                           static_cast<QWidget *>(m_cameraUsernameInput),
                           static_cast<QWidget *>(m_cameraPasswordInput),
                           static_cast<QWidget *>(m_serverSchemeInput),
                           static_cast<QWidget *>(m_serverHostInput),
                           static_cast<QWidget *>(m_serverPortInput)}) {
        field->setMinimumHeight(32);
    }
    m_serverSchemeInput->setFixedWidth(92);
    m_serverPortInput->setFixedWidth(110);
    setCameraIp(cameraIp);
    setCameraCredentials(cameraUsername, cameraPassword);

    auto requestCameraSave = [this]() {
        emit saveCameraCredentialsRequested(
            m_cameraIpInput->text().trimmed(),
            m_cameraUsernameInput->text().trimmed(),
            m_cameraPasswordInput->text());
    };
    auto requestServerSave = [this]() {
        QUrl url;
        url.setScheme(m_serverSchemeInput->currentText());
        url.setHost(m_serverHostInput->text().trimmed());
        url.setPort(m_serverPortInput->value());
        emit saveServerBaseUrlRequested(url.toString());
    };
    connect(saveCameraButton, &QPushButton::clicked, this, requestCameraSave);
    connect(m_cameraIpInput, &QLineEdit::returnPressed, this, requestCameraSave);
    connect(m_cameraUsernameInput, &QLineEdit::returnPressed, this, requestCameraSave);
    connect(m_cameraPasswordInput, &QLineEdit::returnPressed, this, requestCameraSave);
    connect(showPassword, &QCheckBox::toggled, this, [this](bool checked) {
        m_cameraPasswordInput->setEchoMode(
            checked ? QLineEdit::Normal : QLineEdit::Password);
    });
    connect(saveServerButton, &QPushButton::clicked, this, requestServerSave);
    connect(m_serverHostInput, &QLineEdit::returnPressed, this, requestServerSave);
    connect(reconnectButton, &QPushButton::clicked, this, &SettingsPage::reconnectServerRequested);
    connect(m_refreshOverstayButton, &QPushButton::clicked, this, [this]() {
        if (m_overstayRequestInFlight) return;
        setOverstayThresholdRequestStarted(QStringLiteral("Loading the current parking policy..."));
        emit overstayThresholdRefreshRequested();
    });
    connect(m_applyOverstayButton, &QPushButton::clicked, this, [this]() {
        if (m_overstayRequestInFlight || !m_serverConnected) return;
        const int seconds = overstaySeconds(
            m_overstayHoursInput->value(), m_overstayMinutesInput->value(),
            m_overstaySecondsInput->value());
        if (seconds < 60 || seconds > 86400) {
            m_overstayStatusLabel->setText(QStringLiteral(
                "Enter a threshold between 00h 01m 00s and 24h 00m 00s."));
            m_overstayStatusLabel->setStyleSheet(
                QStringLiteral("color:#b71c1c;font-weight:700;"));
            return;
        }
        setOverstayThresholdRequestStarted(
            QStringLiteral("Applying the parking policy..."));
        emit overstayThresholdUpdateRequested(seconds);
    });
    connect(m_overstayHoursInput, &QSpinBox::valueChanged, this,
            [this](int hours) {
        if (hours != 24) return;
        m_overstayMinutesInput->setValue(0);
        m_overstaySecondsInput->setValue(0);
    });
    updateOverstayButtons();
}

void SettingsPage::setCameraIp(const QString &cameraIp)
{
    m_cameraIpLabel->setText(cameraIp.isEmpty() ? QStringLiteral("Not configured") : cameraIp);
    m_cameraIpInput->setText(cameraIp);
}

void SettingsPage::setCameraCredentials(const QString &username,
                                         const QString &password)
{
    m_cameraUsernameInput->setText(username);
    m_cameraPasswordInput->setText(password);
}

void SettingsPage::setServerBaseUrl(const QString &baseUrl)
{
    const QUrl url(baseUrl.trimmed());
    if (!url.isValid() || url.host().isEmpty()) {
        m_serverHostInput->clear();
        return;
    }

    const QString scheme = url.scheme().toLower();
    const int schemeIndex = m_serverSchemeInput->findText(scheme);
    if (schemeIndex >= 0) m_serverSchemeInput->setCurrentIndex(schemeIndex);
    m_serverHostInput->setText(url.host());
    const int defaultPort = scheme == QStringLiteral("https") ? 443 : 80;
    m_serverPortInput->setValue(url.port(defaultPort));
}

void SettingsPage::setServerConnectionStatus(const QString &status, bool connected)
{
    m_serverConnected = connected;
    m_serverConnectionLabel->setText(status);
    m_serverConnectionLabel->setStyleSheet(
        connected
            ? QStringLiteral("color:#1b5e20;background:#e8f5e9;border:1px solid #a5d6a7;"
                             "border-radius:5px;padding:5px 8px;font-weight:800;")
            : QStringLiteral("color:#b71c1c;background:#ffebee;border:1px solid #ef9a9a;"
                             "border-radius:5px;padding:5px 8px;font-weight:800;"));
    updateOverstayButtons();
}

void SettingsPage::setOverstayThresholdRequestStarted(const QString &status)
{
    m_overstayRequestInFlight = true;
    m_overstayStatusLabel->setText(status);
    m_overstayStatusLabel->setStyleSheet(QStringLiteral("color:#455a64;"));
    updateOverstayButtons();
}

void SettingsPage::setOverstayThreshold(int seconds,
                                        const QString &applyPolicy,
                                        bool afterUpdate)
{
    if (seconds < 60 || seconds > 86400) {
        setOverstayThresholdError(
            QStringLiteral("The server returned an out-of-range threshold."),
            afterUpdate);
        return;
    }

    m_serverOverstaySeconds = seconds;
    m_overstayHoursInput->setValue(seconds / 3600);
    m_overstayMinutesInput->setValue((seconds % 3600) / 60);
    m_overstaySecondsInput->setValue(seconds % 60);
    m_currentOverstayLabel->setText(formatOverstayDuration(seconds));
    m_applyPolicyLabel->setText(
        applyPolicy == QStringLiteral("ACTIVE_AND_NEW_SESSIONS")
            ? QStringLiteral("Active and new sessions (ACTIVE_AND_NEW_SESSIONS)")
            : (applyPolicy.isEmpty() ? QStringLiteral("Not provided") : applyPolicy));
    m_overstayRequestInFlight = false;
    m_overstayStatusLabel->setText(
        afterUpdate
            ? QStringLiteral("Parking policy updated to %1.")
                  .arg(formatOverstayDuration(seconds))
            : QStringLiteral("Current parking policy loaded."));
    m_overstayStatusLabel->setStyleSheet(
        QStringLiteral("color:#1b5e20;font-weight:700;"));
    updateOverstayButtons();
}

void SettingsPage::setOverstayThresholdError(const QString &message,
                                              bool updateRequest)
{
    m_overstayRequestInFlight = false;
    const QString prefix = updateRequest
        ? QStringLiteral("Failed to update the parking policy.")
        : QStringLiteral("Failed to load the parking policy.");
    m_overstayStatusLabel->setText(
        message.trimmed().isEmpty()
            ? prefix
            : prefix + QStringLiteral("\nServer response: ") + message.trimmed());
    m_overstayStatusLabel->setStyleSheet(
        QStringLiteral("color:#b71c1c;font-weight:700;"));
    updateOverstayButtons();
}

int SettingsPage::overstaySeconds(int hours, int minutes, int seconds)
{
    return hours * 3600 + minutes * 60 + seconds;
}

QTabWidget *SettingsPage::systemTabs() const
{
    return m_systemTabs;
}

void SettingsPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (m_overstayRequestInFlight) return;
    setOverstayThresholdRequestStarted(QStringLiteral("Loading the current parking policy..."));
    emit overstayThresholdRefreshRequested();
}

void SettingsPage::updateOverstayButtons()
{
    m_applyOverstayButton->setEnabled(
        m_serverConnected && !m_overstayRequestInFlight);
    m_refreshOverstayButton->setEnabled(!m_overstayRequestInFlight);
    m_overstayHoursInput->setEnabled(!m_overstayRequestInFlight);
    m_overstayMinutesInput->setEnabled(!m_overstayRequestInFlight);
    m_overstaySecondsInput->setEnabled(!m_overstayRequestInFlight);
}

QString SettingsPage::formatOverstayDuration(int seconds)
{
    const int bounded = qMax(0, seconds);
    return QStringLiteral("%1h %2m %3s")
        .arg(bounded / 3600, 2, 10, QLatin1Char('0'))
        .arg((bounded % 3600) / 60, 2, 10, QLatin1Char('0'))
        .arg(bounded % 60, 2, 10, QLatin1Char('0'));
}
