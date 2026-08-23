#include "pages/settingspage.h"

#include <QApplication>
#include <QDialog>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>

#include <iostream>

namespace {
int fail(int code, const char *message)
{
    std::cerr << "FAIL " << code << ": " << message << '\n';
    return code;
}

bool exposesText(QWidget *root, const QString &secret)
{
    QList<QWidget *> widgets = root->findChildren<QWidget *>();
    widgets.prepend(root);
    for (QWidget *widget : widgets) {
        QStringList values = {
            widget->toolTip(), widget->statusTip(), widget->whatsThis(),
            widget->accessibleName(), widget->accessibleDescription()};
        if (auto *label = qobject_cast<QLabel *>(widget))
            values.append(label->text());
        if (auto *button = qobject_cast<QPushButton *>(widget))
            values.append(button->text());
        if (auto *lineEdit = qobject_cast<QLineEdit *>(widget))
            values.append(lineEdit->text());
        if (auto *group = qobject_cast<QGroupBox *>(widget))
            values.append(group->title());
        for (const QString &value : values) {
            if (value.contains(secret)) return true;
        }
    }
    return false;
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    SettingsPage page(QStringLiteral(
                          "C:/Users/example/AppData/Local/SmartParking/"
                          "configuration/camera_config.ini"),
                      QStringLiteral("172.20.35.216"), nullptr,
                      QStringLiteral("camera-admin"), true);

    int cameraSaveRequests = 0;
    int cameraReloadRequests = 0;
    int serverTestRequests = 0;
    int reauthenticationRequests = 0;
    int refreshRequests = 0;
    int updateRequests = 0;
    int requestedSeconds = -1;
    QString requestedCameraIp;
    QString requestedCameraUsername;
    QString requestedCameraPassword;
    QObject::connect(
        &page, &SettingsPage::saveCameraCredentialsRequested, &app,
        [&](const QString &ip, const QString &username,
            const QString &replacementPassword) {
            ++cameraSaveRequests;
            requestedCameraIp = ip;
            requestedCameraUsername = username;
            requestedCameraPassword = replacementPassword;
        });
    QObject::connect(&page, &SettingsPage::reloadCameraSettingsRequested,
                     &app, [&]() { ++cameraReloadRequests; });
    QObject::connect(&page, &SettingsPage::testCurrentApiRequested,
                     &app, [&]() {
        ++serverTestRequests;
        page.setServerConnectionStatus(QStringLiteral("Connecting..."), false);
    });
    QObject::connect(&page, &SettingsPage::reauthenticationRequested,
                     &app, [&]() { ++reauthenticationRequests; });
    QObject::connect(&page, &SettingsPage::overstayThresholdRefreshRequested,
                     &app, [&]() { ++refreshRequests; });
    QObject::connect(&page, &SettingsPage::overstayThresholdUpdateRequested,
                     &app, [&](int seconds) {
                         ++updateRequests;
                         requestedSeconds = seconds;
                     });

    if (SettingsPage::overstaySeconds(1, 0, 0) != 3600)
        return fail(1, "one hour conversion");
    if (SettingsPage::overstaySeconds(0, 30, 0) != 1800)
        return fail(2, "thirty minute conversion");
    if (SettingsPage::overstaySeconds(2, 0, 0) != 7200)
        return fail(3, "two hour conversion");

    auto *scrollArea = page.findChild<QScrollArea *>(
        QStringLiteral("settingsScrollArea"));
    auto *cameraGroup = page.findChild<QGroupBox *>(
        QStringLiteral("cameraSettingsGroup"));
    auto *serverGroup = page.findChild<QGroupBox *>(
        QStringLiteral("serverSettingsGroup"));
    auto *localGroup = page.findChild<QGroupBox *>(
        QStringLiteral("localClientSettingsGroup"));
    auto *overstayGroup = page.findChild<QGroupBox *>(
        QStringLiteral("overstaySettingsGroup"));
    if (!scrollArea || !cameraGroup || !serverGroup || !localGroup
        || !overstayGroup) {
        return fail(100, "four settings sections and scroll area must exist");
    }
    if (!cameraGroup->title().contains(QStringLiteral("Camera"))
        || !serverGroup->title().contains(QStringLiteral("Server"))
        || !localGroup->title().contains(QStringLiteral("Local"))
        || !overstayGroup->title().contains(QStringLiteral("Overstay"))) {
        return fail(101, "section titles must identify their responsibilities");
    }
    auto *cameraStorage = page.findChild<QLabel *>(
        QStringLiteral("cameraStorageTargetLabel"));
    auto *serverStorage = page.findChild<QLabel *>(
        QStringLiteral("serverStorageTargetLabel"));
    auto *localBoundary = page.findChild<QLabel *>(
        QStringLiteral("localOverrideScopeLabel"));
    if (!cameraStorage || !serverStorage || !localBoundary
        || !cameraStorage->text().contains(QStringLiteral("camera_config.ini"))
        || !serverStorage->text().contains(
            QStringLiteral("client_config.local.ini"))
        || !localBoundary->text().contains(
            QStringLiteral("client_config.local.ini"))
        || !localBoundary->text().contains(QStringLiteral("precedence"))) {
        return fail(102, "local persistence boundaries must be explicit");
    }

    auto *cameraIp = page.findChild<QLineEdit *>(
        QStringLiteral("cameraIpInput"));
    auto *cameraUsername = page.findChild<QLineEdit *>(
        QStringLiteral("cameraUsernameInput"));
    auto *cameraPassword = page.findChild<QLineEdit *>(
        QStringLiteral("cameraPasswordInput"));
    auto *cameraPasswordState = page.findChild<QLabel *>(
        QStringLiteral("cameraPasswordStateLabel"));
    auto *cameraStatus = page.findChild<QLabel *>(
        QStringLiteral("cameraSettingsStatusLabel"));
    auto *cameraScope = page.findChild<QLabel *>(
        QStringLiteral("cameraOperationScopeLabel"));
    auto *saveCamera = page.findChild<QPushButton *>(
        QStringLiteral("saveCameraSettingsButton"));
    auto *reloadCamera = page.findChild<QPushButton *>(
        QStringLiteral("reloadCameraSettingsButton"));
    if (!cameraIp || !cameraUsername || !cameraPassword
        || !cameraPasswordState || !cameraStatus || !saveCamera
        || !reloadCamera || !cameraScope
        || cameraPassword->echoMode() != QLineEdit::Password) {
        return fail(110, "camera section controls must exist");
    }
    if (!cameraPassword->text().isEmpty()
        || !cameraPassword->placeholderText().contains(
            QStringLiteral("keep"), Qt::CaseInsensitive)
        || !cameraPasswordState->text().contains(
            QStringLiteral("not displayed"), Qt::CaseInsensitive)) {
        return fail(111, "stored camera password must never be rendered");
    }
    if (!cameraScope->text().contains(
            QStringLiteral("not network connectivity"),
            Qt::CaseInsensitive)) {
        return fail(118, "camera save result scope must be explicit");
    }
    if (!cameraGroup->isAncestorOf(cameraPassword)
        || !cameraGroup->isAncestorOf(saveCamera)
        || serverGroup->isAncestorOf(cameraPassword)) {
        return fail(112, "camera controls must stay inside camera section");
    }

    cameraIp->setText(QStringLiteral("172.20.35.220"));
    cameraUsername->setText(QStringLiteral("replacement-user"));
    cameraPassword->setText(QStringLiteral("replacement-secret"));
    saveCamera->click();
    if (cameraSaveRequests != 1
        || requestedCameraIp != QStringLiteral("172.20.35.220")
        || requestedCameraUsername != QStringLiteral("replacement-user")
        || requestedCameraPassword != QStringLiteral("replacement-secret")) {
        return fail(113, "camera save must emit only user-entered replacement");
    }
    if (cameraStatus->property("statusKind").toString()
            != QStringLiteral("pending")
        || saveCamera->isEnabled() || reloadCamera->isEnabled()) {
        return fail(114, "camera operation must expose isolated pending state");
    }
    page.setCameraOperationResult(
        false, QStringLiteral("Camera settings were not saved."));
    if (cameraStatus->property("statusKind").toString()
            != QStringLiteral("error")
        || !cameraPassword->text().isEmpty()
        || !saveCamera->isEnabled() || !reloadCamera->isEnabled()
        || cameraStatus->text().contains(
            QStringLiteral("replacement-secret"))) {
        return fail(115, "camera failure must be inline and clear the secret");
    }
    reloadCamera->click();
    if (cameraReloadRequests != 1
        || cameraStatus->property("statusKind").toString()
            != QStringLiteral("pending")) {
        return fail(116, "camera reload must have its own request and state");
    }
    page.setCameraConfiguration(QStringLiteral("172.20.35.216"),
                                QStringLiteral("camera-admin"), true);
    page.setCameraOperationResult(
        true, QStringLiteral("Reloaded local camera settings."));
    if (!cameraPassword->text().isEmpty()
        || cameraStatus->property("statusKind").toString()
            != QStringLiteral("success")) {
        return fail(117, "camera reload result must preserve password secrecy");
    }
    saveCamera->click();
    if (cameraSaveRequests != 2 || !requestedCameraPassword.isEmpty()) {
        return fail(119, "blank password input must request preservation");
    }
    page.setCameraOperationResult(
        true, QStringLiteral("Camera values preserved."));

    page.setAuthenticationState(
        QStringLiteral("Taejun Min"), QStringLiteral("taejun"),
        QStringLiteral(
            "https://embedded-user:embedded-pass@pi.example.test:8443/"
            "private/path?token=secret#fragment"),
        true);
    auto *authStatus = page.findChild<QLabel *>(
        QStringLiteral("settingsAuthenticationStatusLabel"));
    auto *serverOrigin = page.findChild<QLineEdit *>(
        QStringLiteral("authenticatedServerOriginValue"));
    auto *serverStatus = page.findChild<QLabel *>(
        QStringLiteral("serverConnectionStatusLabel"));
    auto *testServer = page.findChild<QPushButton *>(
        QStringLiteral("testServerConnectionButton"));
    auto *reauthenticate = page.findChild<QPushButton *>(
        QStringLiteral("reauthenticateServerButton"));
    if (!authStatus || !serverOrigin || !serverStatus || !testServer
        || !reauthenticate || !serverOrigin->isReadOnly()) {
        return fail(120, "server session controls must exist and be read-only");
    }
    if (serverOrigin->text()
            != QStringLiteral("https://pi.example.test:8443")
        || serverOrigin->text().contains(QStringLiteral("embedded"))
        || serverOrigin->text().contains(QStringLiteral("private"))
        || serverOrigin->text().contains(QStringLiteral("secret"))
        || authStatus->property("statusKind").toString()
            != QStringLiteral("success")) {
        return fail(121, "only the sanitized authenticated origin may be shown");
    }
    if (exposesText(&page, QStringLiteral("replacement-secret"))
        || exposesText(&page, QStringLiteral("embedded-pass"))
        || exposesText(&page, QStringLiteral("token=secret"))) {
        return fail(125, "credentials must not escape into rendered UI metadata");
    }
    testServer->click();
    testServer->click();
    if (serverTestRequests != 1 || testServer->isEnabled()
        || serverStatus->property("statusKind").toString()
            != QStringLiteral("pending")) {
        return fail(122, "API test must not mutate settings and must show pending");
    }
    page.setServerConnectionStatus(QStringLiteral("Connected"), true);
    if (!testServer->isEnabled()
        || serverStatus->property("statusKind").toString()
            != QStringLiteral("success")) {
        return fail(123, "API test completion must restore its action");
    }
    reauthenticate->click();
    if (reauthenticationRequests != 1)
        return fail(124, "server changes must route through reauthentication");

    auto *dataSource = page.findChild<QLabel *>(
        QStringLiteral("runtimeDataSourceLabel"));
    page.setRuntimeDataSource(QStringLiteral("MOCK"));
    if (!dataSource
        || dataSource->property("sourceKind").toString()
            != QStringLiteral("mock")
        || !dataSource->text().contains(QStringLiteral("MOCK"))) {
        return fail(130, "mock data source must be distinct from API status");
    }
    page.setRuntimeDataSource(QStringLiteral("SERVER"));
    if (dataSource->property("sourceKind").toString()
            != QStringLiteral("server")) {
        return fail(131, "server data source must be represented semantically");
    }

    page.resize(420, 420);
    page.show();
    app.processEvents();
    if (refreshRequests != 1)
        return fail(10, "showing settings must load overstay policy once");
    if (scrollArea->horizontalScrollBarPolicy()
            != Qt::ScrollBarAlwaysOff
        || scrollArea->horizontalScrollBar()->maximum() != 0) {
        return fail(140, "narrow settings page must not require horizontal scroll");
    }
    if (!scrollArea->widget()
        || scrollArea->widget()->width() > scrollArea->viewport()->width() + 1) {
        return fail(141, "scroll content must fit the narrow viewport");
    }
    const QList<QGroupBox *> groups = {
        cameraGroup, serverGroup, localGroup, overstayGroup};
    for (QGroupBox *group : groups) {
        if (group->width() > scrollArea->viewport()->width() + 1)
            return fail(142, "settings section exceeds viewport width");
    }
    auto *refreshPolicyAction = page.findChild<QPushButton *>(
        QStringLiteral("refreshOverstayThresholdButton"));
    auto *applyPolicyAction = page.findChild<QPushButton *>(
        QStringLiteral("applyOverstayThresholdButton"));
    if (!refreshPolicyAction || !applyPolicyAction)
        return fail(145, "overstay actions must exist in compact layout");
    const QList<QPushButton *> actions = {
        saveCamera, reloadCamera, testServer, reauthenticate,
        refreshPolicyAction, applyPolicyAction};
    for (QPushButton *button : actions) {
        if (button->width() < button->minimumSizeHint().width())
            return fail(143, "narrow layout clips an action button");
    }
    if (!cameraStatus->wordWrap() || !serverStatus->wordWrap()
        || scrollArea->verticalScrollBar()->maximum() <= 0) {
        return fail(144, "long settings content must remain vertically reachable");
    }
    scrollArea->verticalScrollBar()->setValue(
        scrollArea->verticalScrollBar()->maximum());
    app.processEvents();
    const QRect bottomActionRect(
        applyPolicyAction->mapTo(scrollArea->viewport(), QPoint(0, 0)),
        applyPolicyAction->size());
    if (!scrollArea->viewport()->rect().intersects(bottomActionRect))
        return fail(147, "vertical scrolling must reach the last settings action");

    auto *hours = page.findChild<QSpinBox *>(
        QStringLiteral("overstayHoursInput"));
    auto *minutes = page.findChild<QSpinBox *>(
        QStringLiteral("overstayMinutesInput"));
    auto *seconds = page.findChild<QSpinBox *>(
        QStringLiteral("overstaySecondsInput"));
    auto *current = page.findChild<QLabel *>(
        QStringLiteral("currentOverstayThresholdLabel"));
    auto *policy = page.findChild<QLabel *>(
        QStringLiteral("overstayApplyPolicyLabel"));
    auto *status = page.findChild<QLabel *>(
        QStringLiteral("overstayStatusLabel"));
    auto *apply = applyPolicyAction;
    if (!hours || !minutes || !seconds || !current || !policy || !status
        || !apply) {
        return fail(20, "overstay controls must exist");
    }
    const QList<QSpinBox *> thresholdInputs = {hours, minutes, seconds};
    for (QSpinBox *input : thresholdInputs) {
        if (input->width() < input->minimumSizeHint().width())
            return fail(146, "compact layout clips an overstay input");
    }

    page.setOverstayThreshold(
        3600, QStringLiteral("ACTIVE_AND_NEW_SESSIONS"), false);
    if (hours->value() != 1 || minutes->value() != 0
        || seconds->value() != 0)
        return fail(21, "server threshold must populate inputs");
    if (current->text() != QStringLiteral("01h 00m 00s"))
        return fail(22, "current threshold formatting");
    if (!policy->text().contains(
            QStringLiteral("ACTIVE_AND_NEW_SESSIONS")))
        return fail(23, "apply policy must be visible");

    hours->setValue(0);
    minutes->setValue(30);
    seconds->setValue(0);
    apply->click();
    apply->click();
    if (updateRequests != 1 || requestedSeconds != 1800)
        return fail(24, "duplicate overstay updates must be blocked");
    if (apply->isEnabled())
        return fail(25, "apply must be disabled during update");

    page.setOverstayThresholdError(QStringLiteral("HTTP 500"), true);
    if (current->text() != QStringLiteral("01h 00m 00s"))
        return fail(26, "failed update must retain server value");
    if (hours->value() != 0 || minutes->value() != 30
        || seconds->value() != 0)
        return fail(27, "failed update must retain edited value");
    if (!status->text().contains(QStringLiteral("HTTP 500"))
        || status->property("statusKind").toString()
            != QStringLiteral("error"))
        return fail(28, "overstay server error must be inline");

    apply->click();
    if (updateRequests != 2 || requestedSeconds != 1800)
        return fail(33, "overstay retry must emit a new update request");
    page.setOverstayThreshold(
        1800, QStringLiteral("ACTIVE_AND_NEW_SESSIONS"), true);
    if (current->text() != QStringLiteral("00h 30m 00s")
        || !status->text().contains(QStringLiteral("changed")))
        return fail(29, "successful update must show verified server value");

    hours->setValue(0);
    minutes->setValue(0);
    seconds->setValue(59);
    const int requestsBeforeInvalid = updateRequests;
    apply->click();
    if (updateRequests != requestsBeforeInvalid
        || !status->text().contains(QStringLiteral("between")))
        return fail(30, "invalid overstay range must stay local");

    page.setServerConnectionStatus(QStringLiteral("Disconnected"), false);
    if (apply->isEnabled())
        return fail(31, "overstay apply requires server connection");
    hours->setValue(24);
    minutes->setValue(10);
    hours->setValue(23);
    hours->setValue(24);
    if (minutes->value() != 0 || seconds->value() != 0)
        return fail(32, "24 hour input must clamp minutes and seconds");

    auto *helpButton = page.findChild<QPushButton *>(
        QStringLiteral("settingsHelpButton"));
    if (!helpButton || helpButton->icon().isNull())
        return fail(150, "settings help button must remain available");
    helpButton->click();
    QApplication::processEvents();
    auto *helpDialog = page.findChild<QDialog *>(
        QStringLiteral("settingsHelpDialog"));
    auto *helpSteps = helpDialog
        ? helpDialog->findChild<QLabel *>(
              QStringLiteral("settingsHelpSteps"))
        : nullptr;
    if (!helpDialog || !helpSteps
        || !helpSteps->text().contains(QStringLiteral("Test current API"))
        || helpSteps->text().contains(QStringLiteral("Save and reconnect"))) {
        return fail(151, "help must describe the safe server workflow");
    }
    helpDialog->close();

    std::cout << "PASS: modular Settings UI preserves auth, storage, and "
                 "responsive boundaries\n";
    return 0;
}
