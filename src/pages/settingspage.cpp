#include "settingspage.h"

#include "widgets/pagehelp.h"

#include <QFormLayout>
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
#include <QUrl>
#include <QVBoxLayout>

namespace {
QLabel *detailLabel(const QString &text, QWidget *parent,
                    const QString &objectName = QString())
{
    auto *label = new QLabel(text, parent);
    if (!objectName.isEmpty()) label->setObjectName(objectName);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}

void configureForm(QFormLayout *form)
{
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(10);
}

void configureActionButton(QPushButton *button)
{
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}
}

SettingsPage::SettingsPage(const QString &configPath, const QString &cameraIp,
                           QWidget *parent,
                           const QString &cameraUsername,
                           bool cameraPasswordConfigured)
    : QWidget(parent)
{
    auto *pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->addLayout(createPageHeader(
        this, QStringLiteral("Settings"),
        QStringLiteral("Connection, local storage, and server policy")));
    createPageHelpButton(
        this, this,
        {QStringLiteral("settings"), QStringLiteral("Settings"),
         QStringLiteral("Settings 사용 안내"),
         QStringLiteral(
             "카메라·Pi 연결 상태와 로컬 저장 경계, 서버의 초과주차 정책을 관리합니다."),
         QStringLiteral(
             "<b>1. Camera Connection</b><br>카메라 IPv4와 계정을 "
             "camera_config.ini에 저장하고 현재 IVA/RTSP 연결에 적용합니다. "
             "저장된 비밀번호는 화면에 다시 표시하지 않으며, 빈 칸으로 저장하면 "
             "기존 비밀번호를 유지합니다.<br><br>"
             "<b>2. Pi Server</b><br>로그인된 서버 origin은 현재 세션 동안 "
             "고정됩니다. <i>Test current API</i>는 주소나 MQTT 설정을 변경하지 "
             "않고 현재 API의 주차 슬롯 데이터를 다시 요청합니다. 다른 서버는 "
             "다시 로그인한 뒤 선택합니다.<br><br>"
             "<b>3. Local Client</b><br>client_config.ini는 공용 기본값이고 "
             "client_config.local.ini는 이 PC의 로컬 override입니다. 서버 "
             "origin은 로그인에 성공한 경우에만 로컬 override에 저장됩니다."
             "<br><br><b>4. Overstay Policy</b><br>페이지를 열거나 "
             "<i>Reload policy</i>를 누르면 서버 설정을 읽습니다. 1분~24시간 "
             "범위로 입력하고 <i>Apply policy</i>하면 서버가 저장한 값을 다시 "
             "검증합니다."),
         QStringLiteral(
             "※ 카메라 설정, 로그인 서버, 로컬 클라이언트 설정은 서로 다른 "
             "저장 경계를 사용합니다. TLS 실패 시 평문으로 자동 전환하지 않으며 "
             "비밀번호와 Bearer token을 로그나 Git에 저장하면 안 됩니다.")});

    auto *scrollArea = new QScrollArea(this);
    scrollArea->setObjectName(QStringLiteral("settingsScrollArea"));
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *content = new QWidget(scrollArea);
    content->setObjectName(QStringLiteral("settingsContentWidget"));
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 8, 0);
    contentLayout->setSpacing(12);

    auto *cameraGroup = new QGroupBox(QStringLiteral("Camera Connection"), content);
    cameraGroup->setObjectName(QStringLiteral("cameraSettingsGroup"));
    auto *cameraForm = new QFormLayout(cameraGroup);
    configureForm(cameraForm);

    auto *cameraStorage = detailLabel(
        configPath.isEmpty()
            ? QStringLiteral("camera_config.ini (local camera settings)")
            : configPath,
        cameraGroup, QStringLiteral("cameraStorageTargetLabel"));
    m_cameraIpLabel = detailLabel(QString(), cameraGroup,
                                  QStringLiteral("currentCameraIpLabel"));
    m_cameraIpInput = new QLineEdit(cameraGroup);
    m_cameraIpInput->setObjectName(QStringLiteral("cameraIpInput"));
    m_cameraIpInput->setPlaceholderText(QStringLiteral("e.g. 192.168.10.20"));
    m_cameraIpInput->setMinimumWidth(0);
    m_cameraUsernameInput = new QLineEdit(cameraGroup);
    m_cameraUsernameInput->setObjectName(QStringLiteral("cameraUsernameInput"));
    m_cameraUsernameInput->setPlaceholderText(QStringLiteral("e.g. admin"));
    m_cameraUsernameInput->setMinimumWidth(0);
    m_cameraPasswordInput = new QLineEdit(cameraGroup);
    m_cameraPasswordInput->setObjectName(QStringLiteral("cameraPasswordInput"));
    m_cameraPasswordInput->setEchoMode(QLineEdit::Password);
    m_cameraPasswordInput->setMinimumWidth(0);
    m_cameraPasswordStateLabel = detailLabel(
        QString(), cameraGroup, QStringLiteral("cameraPasswordStateLabel"));
    m_cameraStatusLabel = detailLabel(
        QString(), cameraGroup, QStringLiteral("cameraSettingsStatusLabel"));
    auto *cameraResultScope = detailLabel(
        QStringLiteral(
            "A successful save confirms local persistence and client "
            "reconfiguration, not network connectivity. Check Dashboard RTSP "
            "and IVA Setup for live camera health."),
        cameraGroup, QStringLiteral("cameraOperationScopeLabel"));

    m_reloadCameraButton = new QPushButton(
        QStringLiteral("Reload applied values"), cameraGroup);
    m_reloadCameraButton->setObjectName(
        QStringLiteral("reloadCameraSettingsButton"));
    m_saveCameraButton = new QPushButton(
        QStringLiteral("Save && apply locally"), cameraGroup);
    m_saveCameraButton->setObjectName(
        QStringLiteral("saveCameraSettingsButton"));
    configureActionButton(m_reloadCameraButton);
    configureActionButton(m_saveCameraButton);
    auto *cameraActions = new QGridLayout;
    cameraActions->setContentsMargins(0, 0, 0, 0);
    cameraActions->addWidget(m_reloadCameraButton, 0, 0);
    cameraActions->addWidget(m_saveCameraButton, 0, 1);
    cameraActions->setColumnStretch(0, 1);
    cameraActions->setColumnStretch(1, 1);

    cameraForm->addRow(QStringLiteral("Storage target"), cameraStorage);
    cameraForm->addRow(QStringLiteral("Current camera IP"), m_cameraIpLabel);
    cameraForm->addRow(QStringLiteral("Camera IPv4 address"), m_cameraIpInput);
    cameraForm->addRow(QStringLiteral("Camera username"),
                       m_cameraUsernameInput);
    cameraForm->addRow(QStringLiteral("Replacement password"),
                       m_cameraPasswordInput);
    cameraForm->addRow(QStringLiteral("Password state"),
                       m_cameraPasswordStateLabel);
    cameraForm->addRow(QStringLiteral("Operation status"), m_cameraStatusLabel);
    cameraForm->addRow(QStringLiteral("Result scope"), cameraResultScope);
    cameraForm->addRow(cameraActions);
    contentLayout->addWidget(cameraGroup);

    auto *serverGroup = new QGroupBox(QStringLiteral("Pi Server"), content);
    serverGroup->setObjectName(QStringLiteral("serverSettingsGroup"));
    auto *serverForm = new QFormLayout(serverGroup);
    configureForm(serverForm);

    m_authenticationStatusLabel = detailLabel(
        QString(), serverGroup,
        QStringLiteral("settingsAuthenticationStatusLabel"));
    m_authenticatedUserLabel = detailLabel(
        QStringLiteral("Not available"), serverGroup,
        QStringLiteral("settingsAuthenticatedUserLabel"));
    m_authenticatedServerOrigin = new QLineEdit(serverGroup);
    m_authenticatedServerOrigin->setObjectName(
        QStringLiteral("authenticatedServerOriginValue"));
    m_authenticatedServerOrigin->setReadOnly(true);
    m_authenticatedServerOrigin->setMinimumWidth(0);
    m_authenticatedServerOrigin->setPlaceholderText(
        QStringLiteral("Not configured"));
    m_serverConnectionLabel = detailLabel(
        QString(), serverGroup,
        QStringLiteral("serverConnectionStatusLabel"));
    auto *serverScope = detailLabel(
        QStringLiteral(
            "The signed-in origin is immutable for this session. API testing "
            "requests the current parking-slot snapshot; it does not save an "
            "address or restart MQTT."),
        serverGroup, QStringLiteral("serverOperationScopeLabel"));
    auto *serverStorage = detailLabel(
        QStringLiteral(
            "client_config.local.ini is updated only after a successful sign-in."),
        serverGroup, QStringLiteral("serverStorageTargetLabel"));

    m_testServerButton = new QPushButton(
        QStringLiteral("Test current API"), serverGroup);
    m_testServerButton->setObjectName(
        QStringLiteral("testServerConnectionButton"));
    auto *reauthenticateButton = new QPushButton(
        QStringLiteral("Sign in to another server"), serverGroup);
    reauthenticateButton->setObjectName(
        QStringLiteral("reauthenticateServerButton"));
    configureActionButton(m_testServerButton);
    configureActionButton(reauthenticateButton);
    auto *serverActions = new QGridLayout;
    serverActions->setContentsMargins(0, 0, 0, 0);
    serverActions->addWidget(m_testServerButton, 0, 0);
    serverActions->addWidget(reauthenticateButton, 0, 1);
    serverActions->setColumnStretch(0, 1);
    serverActions->setColumnStretch(1, 1);

    serverForm->addRow(QStringLiteral("Authentication"),
                       m_authenticationStatusLabel);
    serverForm->addRow(QStringLiteral("Signed-in account"),
                       m_authenticatedUserLabel);
    serverForm->addRow(QStringLiteral("Signed-in server origin"),
                       m_authenticatedServerOrigin);
    serverForm->addRow(QStringLiteral("API status"), m_serverConnectionLabel);
    serverForm->addRow(QStringLiteral("Test scope"), serverScope);
    serverForm->addRow(QStringLiteral("Local save boundary"), serverStorage);
    serverForm->addRow(serverActions);
    contentLayout->addWidget(serverGroup);

    auto *localGroup = new QGroupBox(QStringLiteral("Local Client"), content);
    localGroup->setObjectName(QStringLiteral("localClientSettingsGroup"));
    auto *localForm = new QFormLayout(localGroup);
    configureForm(localForm);
    auto *sharedConfig = detailLabel(
        QStringLiteral("client_config.ini — shared defaults; read-only here"),
        localGroup, QStringLiteral("sharedClientConfigLabel"));
    auto *localOverride = detailLabel(
        QStringLiteral(
            "client_config.local.ini — this PC override; takes precedence over "
            "shared defaults"),
        localGroup, QStringLiteral("localOverrideScopeLabel"));
    m_runtimeDataSourceLabel = detailLabel(
        QString(), localGroup, QStringLiteral("runtimeDataSourceLabel"));
    auto *sourceExplanation = detailLabel(
        QStringLiteral(
            "SERVER means normalized Pi data, MOCK means local simulation, and "
            "MIXED means simulation was applied after server data."),
        localGroup, QStringLiteral("runtimeDataSourceExplanationLabel"));
    localForm->addRow(QStringLiteral("Shared configuration"), sharedConfig);
    localForm->addRow(QStringLiteral("Local override"), localOverride);
    localForm->addRow(QStringLiteral("Parking data source"),
                      m_runtimeDataSourceLabel);
    localForm->addRow(QStringLiteral("Source meaning"), sourceExplanation);
    contentLayout->addWidget(localGroup);

    auto *overstayGroup = new QGroupBox(QStringLiteral("Overstay Policy"), content);
    overstayGroup->setObjectName(QStringLiteral("overstaySettingsGroup"));
    auto *overstayForm = new QFormLayout(overstayGroup);
    configureForm(overstayForm);
    m_currentOverstayLabel = detailLabel(
        QStringLiteral("Not loaded"), overstayGroup,
        QStringLiteral("currentOverstayThresholdLabel"));
    m_applyPolicyLabel = detailLabel(
        QStringLiteral("Not loaded"), overstayGroup,
        QStringLiteral("overstayApplyPolicyLabel"));

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
    auto *thresholdLayout = new QHBoxLayout;
    thresholdLayout->setContentsMargins(0, 0, 0, 0);
    thresholdLayout->addWidget(m_overstayHoursInput);
    thresholdLayout->addWidget(m_overstayMinutesInput);
    thresholdLayout->addWidget(m_overstaySecondsInput);
    thresholdLayout->addStretch();

    m_overstayStatusLabel = detailLabel(
        QString(), overstayGroup, QStringLiteral("overstayStatusLabel"));
    m_refreshOverstayButton = new QPushButton(
        QStringLiteral("Reload policy"), overstayGroup);
    m_refreshOverstayButton->setObjectName(
        QStringLiteral("refreshOverstayThresholdButton"));
    m_applyOverstayButton = new QPushButton(
        QStringLiteral("Apply policy"), overstayGroup);
    m_applyOverstayButton->setObjectName(
        QStringLiteral("applyOverstayThresholdButton"));
    configureActionButton(m_refreshOverstayButton);
    configureActionButton(m_applyOverstayButton);
    auto *overstayActions = new QGridLayout;
    overstayActions->setContentsMargins(0, 0, 0, 0);
    overstayActions->addWidget(m_refreshOverstayButton, 0, 0);
    overstayActions->addWidget(m_applyOverstayButton, 0, 1);
    overstayActions->setColumnStretch(0, 1);
    overstayActions->setColumnStretch(1, 1);

    overstayForm->addRow(QStringLiteral("Current server setting"),
                         m_currentOverstayLabel);
    overstayForm->addRow(QStringLiteral("Apply policy"), m_applyPolicyLabel);
    overstayForm->addRow(QStringLiteral("Overstay threshold"), thresholdLayout);
    overstayForm->addRow(QStringLiteral("Operation status"),
                         m_overstayStatusLabel);
    overstayForm->addRow(overstayActions);
    contentLayout->addWidget(overstayGroup);
    contentLayout->addStretch();

    scrollArea->setWidget(content);
    pageLayout->addWidget(scrollArea, 1);

    setCameraConfiguration(cameraIp, cameraUsername,
                           cameraPasswordConfigured);
    setStatus(m_cameraStatusLabel,
              QStringLiteral("Ready. No camera values have been changed."),
              StatusKind::Idle);
    setStatus(m_authenticationStatusLabel,
              QStringLiteral("Authentication state not provided."),
              StatusKind::Idle);
    setStatus(m_serverConnectionLabel,
              QStringLiteral("Waiting for the current API state."),
              StatusKind::Idle);
    setRuntimeDataSource(QStringLiteral("UNKNOWN"));
    setStatus(m_overstayStatusLabel,
              QStringLiteral("Open this page to load the server setting."),
              StatusKind::Idle);

    connect(m_saveCameraButton, &QPushButton::clicked, this, [this]() {
        if (m_cameraOperationInFlight) return;
        m_cameraOperationInFlight = true;
        setStatus(m_cameraStatusLabel,
                  QStringLiteral("Saving local camera settings and applying "
                                 "them to the current camera clients..."),
                  StatusKind::Pending);
        updateCameraButtons();
        emit saveCameraCredentialsRequested(
            m_cameraIpInput->text().trimmed(),
            m_cameraUsernameInput->text().trimmed(),
            m_cameraPasswordInput->text());
    });
    connect(m_reloadCameraButton, &QPushButton::clicked, this, [this]() {
        if (m_cameraOperationInFlight) return;
        m_cameraOperationInFlight = true;
        setStatus(m_cameraStatusLabel,
                  QStringLiteral("Restoring the camera values currently "
                                 "applied to this client..."),
                  StatusKind::Pending);
        updateCameraButtons();
        emit reloadCameraSettingsRequested();
    });
    connect(m_cameraIpInput, &QLineEdit::returnPressed,
            m_saveCameraButton, &QPushButton::click);
    connect(m_cameraUsernameInput, &QLineEdit::returnPressed,
            m_saveCameraButton, &QPushButton::click);
    connect(m_cameraPasswordInput, &QLineEdit::returnPressed,
            m_saveCameraButton, &QPushButton::click);
    connect(m_testServerButton, &QPushButton::clicked, this, [this]() {
        m_testServerButton->setEnabled(false);
        setStatus(m_serverConnectionLabel,
                  QStringLiteral("Testing the current authenticated API..."),
                  StatusKind::Pending);
        emit testCurrentApiRequested();
    });
    connect(reauthenticateButton, &QPushButton::clicked,
            this, &SettingsPage::reauthenticationRequested);
    connect(m_refreshOverstayButton, &QPushButton::clicked, this, [this]() {
        if (m_overstayRequestInFlight) return;
        setOverstayThresholdRequestStarted(
            QStringLiteral("Loading the server setting..."));
        emit overstayThresholdRefreshRequested();
    });
    connect(m_applyOverstayButton, &QPushButton::clicked, this, [this]() {
        if (m_overstayRequestInFlight || !m_serverConnected) return;
        const int seconds = overstaySeconds(
            m_overstayHoursInput->value(), m_overstayMinutesInput->value(),
            m_overstaySecondsInput->value());
        if (seconds < 60 || seconds > 86400) {
            setStatus(
                m_overstayStatusLabel,
                QStringLiteral(
                    "The threshold must be between 00h 01m 00s and "
                    "24h 00m 00s."),
                StatusKind::Error);
            return;
        }
        setOverstayThresholdRequestStarted(
            QStringLiteral("Applying the new threshold..."));
        emit overstayThresholdUpdateRequested(seconds);
    });
    connect(m_overstayHoursInput, &QSpinBox::valueChanged, this,
            [this](int hours) {
        if (hours != 24) return;
        m_overstayMinutesInput->setValue(0);
        m_overstaySecondsInput->setValue(0);
    });
    updateCameraButtons();
    updateOverstayButtons();
}

void SettingsPage::setCameraConfiguration(const QString &cameraIp,
                                          const QString &username,
                                          bool passwordConfigured)
{
    const QString normalizedIp = cameraIp.trimmed();
    m_cameraIpLabel->setText(
        normalizedIp.isEmpty() ? QStringLiteral("Not configured") : normalizedIp);
    m_cameraIpInput->setText(normalizedIp);
    m_cameraUsernameInput->setText(username);
    m_cameraPasswordInput->clear();
    m_cameraPasswordInput->setPlaceholderText(
        passwordConfigured
            ? QStringLiteral("Leave blank to keep the configured password")
            : QStringLiteral("Required because no password is configured"));
    m_cameraPasswordStateLabel->setText(
        passwordConfigured
            ? QStringLiteral(
                  "Configured. The stored value is not displayed; enter a new "
                  "value only to replace it.")
            : QStringLiteral(
                  "Not configured. Enter a password before saving."));
}

void SettingsPage::setCameraOperationResult(bool success,
                                            const QString &message)
{
    m_cameraOperationInFlight = false;
    m_cameraPasswordInput->clear();
    setStatus(
        m_cameraStatusLabel,
        message.trimmed().isEmpty()
            ? (success ? QStringLiteral("Camera settings updated.")
                       : QStringLiteral("Camera settings were not updated."))
            : message.trimmed(),
        success ? StatusKind::Success : StatusKind::Error);
    updateCameraButtons();
}

void SettingsPage::setAuthenticationState(const QString &displayName,
                                          const QString &accountId,
                                          const QString &serverOrigin,
                                          bool authenticated)
{
    const QString user = displayName.trimmed();
    const QString account = accountId.trimmed();
    if (user.isEmpty() && account.isEmpty()) {
        m_authenticatedUserLabel->setText(QStringLiteral("Not available"));
    } else if (user.isEmpty() || account.isEmpty() || user == account) {
        m_authenticatedUserLabel->setText(user.isEmpty() ? account : user);
    } else {
        m_authenticatedUserLabel->setText(
            QStringLiteral("%1 (%2)").arg(user, account));
    }
    setServerBaseUrl(serverOrigin);
    setStatus(m_authenticationStatusLabel,
              authenticated ? QStringLiteral("Authenticated session")
                            : QStringLiteral("Authentication required"),
              authenticated ? StatusKind::Success : StatusKind::Error);
}

void SettingsPage::setServerBaseUrl(const QString &baseUrl)
{
    const QString origin = sanitizedOrigin(baseUrl);
    m_authenticatedServerOrigin->setText(origin);
    m_authenticatedServerOrigin->setToolTip(origin);
}

void SettingsPage::setServerConnectionStatus(const QString &status,
                                             bool connected)
{
    m_serverConnected = connected;
    const QString normalizedStatus = status.trimmed();
    const bool pending = !connected
        && (normalizedStatus.contains(QStringLiteral("connecting"),
                                      Qt::CaseInsensitive)
            || normalizedStatus.contains(QStringLiteral("progress"),
                                         Qt::CaseInsensitive));
    m_testServerButton->setEnabled(!pending);
    setStatus(
        m_serverConnectionLabel,
        normalizedStatus.isEmpty()
            ? (connected ? QStringLiteral("Connected")
                         : QStringLiteral("Disconnected"))
            : normalizedStatus,
        connected ? StatusKind::Success
                  : (pending ? StatusKind::Pending : StatusKind::Error));
    updateOverstayButtons();
}

void SettingsPage::setRuntimeDataSource(const QString &dataSource)
{
    QString source = dataSource.trimmed().toUpper();
    if (source != QStringLiteral("SERVER")
        && source != QStringLiteral("MOCK")
        && source != QStringLiteral("MIXED")) {
        source = QStringLiteral("UNKNOWN");
    }

    QString detail;
    QString color;
    if (source == QStringLiteral("SERVER")) {
        detail = QStringLiteral("SERVER — normalized state from the Pi");
        color = QStringLiteral("#1b5e20");
    } else if (source == QStringLiteral("MOCK")) {
        detail = QStringLiteral("MOCK — local simulation data");
        color = QStringLiteral("#8a4b08");
    } else if (source == QStringLiteral("MIXED")) {
        detail = QStringLiteral("MIXED — server state with later simulation");
        color = QStringLiteral("#6a1b9a");
    } else {
        detail = QStringLiteral("UNKNOWN — waiting for runtime data");
        color = QStringLiteral("#455a64");
    }
    m_runtimeDataSourceLabel->setText(detail);
    m_runtimeDataSourceLabel->setProperty(
        "sourceKind", source.toLower());
    m_runtimeDataSourceLabel->setStyleSheet(
        QStringLiteral("color:%1;font-weight:700;").arg(color));
}

void SettingsPage::setOverstayThresholdRequestStarted(const QString &status)
{
    m_overstayRequestInFlight = true;
    setStatus(m_overstayStatusLabel, status, StatusKind::Pending);
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
            ? QStringLiteral(
                  "Active and new sessions (ACTIVE_AND_NEW_SESSIONS)")
            : (applyPolicy.isEmpty() ? QStringLiteral("Not provided")
                                     : applyPolicy));
    m_overstayRequestInFlight = false;
    setStatus(
        m_overstayStatusLabel,
        afterUpdate
            ? QStringLiteral("Overstay threshold changed to %1.")
                  .arg(formatOverstayDuration(seconds))
            : QStringLiteral("Server setting loaded successfully."),
        StatusKind::Success);
    updateOverstayButtons();
}

void SettingsPage::setOverstayThresholdError(const QString &message,
                                             bool updateRequest)
{
    m_overstayRequestInFlight = false;
    const QString prefix = updateRequest
        ? QStringLiteral("Failed to update the overstay threshold.")
        : QStringLiteral("Failed to load the server setting.");
    setStatus(
        m_overstayStatusLabel,
        message.trimmed().isEmpty()
            ? prefix
            : prefix + QStringLiteral("\nServer response: ")
                  + message.trimmed(),
        StatusKind::Error);
    updateOverstayButtons();
}

int SettingsPage::overstaySeconds(int hours, int minutes, int seconds)
{
    return hours * 3600 + minutes * 60 + seconds;
}

void SettingsPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (m_overstayRequestInFlight) return;
    setOverstayThresholdRequestStarted(
        QStringLiteral("Loading the server setting..."));
    emit overstayThresholdRefreshRequested();
}

QString SettingsPage::sanitizedOrigin(const QString &baseUrl)
{
    const QUrl input(baseUrl.trimmed());
    const QString scheme = input.scheme().toLower();
    if (!input.isValid() || input.host().isEmpty()
        || (scheme != QStringLiteral("http")
            && scheme != QStringLiteral("https"))) {
        return QString();
    }

    QUrl origin;
    origin.setScheme(scheme);
    origin.setHost(input.host());
    if (input.port(-1) > 0) origin.setPort(input.port());
    return origin.toString(QUrl::RemovePath | QUrl::RemoveQuery
                           | QUrl::RemoveFragment | QUrl::RemoveUserInfo);
}

void SettingsPage::setStatus(QLabel *label, const QString &text,
                             StatusKind kind)
{
    if (!label) return;
    QString property;
    QString style;
    switch (kind) {
    case StatusKind::Pending:
        property = QStringLiteral("pending");
        style = QStringLiteral("color:#455a64;");
        break;
    case StatusKind::Success:
        property = QStringLiteral("success");
        style = QStringLiteral("color:#1b5e20;font-weight:700;");
        break;
    case StatusKind::Error:
        property = QStringLiteral("error");
        style = QStringLiteral("color:#b71c1c;font-weight:700;");
        break;
    case StatusKind::Idle:
    default:
        property = QStringLiteral("idle");
        style = QStringLiteral("color:#455a64;");
        break;
    }
    label->setText(text);
    label->setProperty("statusKind", property);
    label->setStyleSheet(style);
}

void SettingsPage::updateCameraButtons()
{
    m_saveCameraButton->setEnabled(!m_cameraOperationInFlight);
    m_reloadCameraButton->setEnabled(!m_cameraOperationInFlight);
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
