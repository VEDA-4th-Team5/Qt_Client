#ifndef SETTINGSPAGE_H
#define SETTINGSPAGE_H

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QShowEvent;
class QSpinBox;

class SettingsPage : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPage(const QString &configPath, const QString &cameraIp,
                          QWidget *parent = nullptr,
                          const QString &cameraUsername = QString(),
                          bool cameraPasswordConfigured = false);

    void setCameraConfiguration(const QString &cameraIp,
                                const QString &username,
                                bool passwordConfigured);
    void setCameraOperationResult(bool success, const QString &message);
    void setAuthenticationState(const QString &displayName,
                                const QString &accountId,
                                const QString &serverOrigin,
                                bool authenticated);
    void setServerBaseUrl(const QString &baseUrl);
    void setServerConnectionStatus(const QString &status, bool connected);
    void setRuntimeDataSource(const QString &dataSource);
    void setOverstayThresholdRequestStarted(const QString &status);
    void setOverstayThreshold(int seconds, const QString &applyPolicy,
                              bool afterUpdate);
    void setOverstayThresholdError(const QString &message, bool updateRequest);
    static int overstaySeconds(int hours, int minutes, int seconds);

signals:
    void saveCameraCredentialsRequested(const QString &cameraIp,
                                        const QString &username,
                                        const QString &replacementPassword);
    void reloadCameraSettingsRequested();
    void testCurrentApiRequested();
    void reauthenticationRequested();
    void overstayThresholdRefreshRequested();
    void overstayThresholdUpdateRequested(int seconds);

protected:
    void showEvent(QShowEvent *event) override;

private:
    enum class StatusKind {
        Idle,
        Pending,
        Success,
        Error
    };

    static QString sanitizedOrigin(const QString &baseUrl);
    static void setStatus(QLabel *label, const QString &text, StatusKind kind);
    void updateCameraButtons();
    void updateOverstayButtons();
    static QString formatOverstayDuration(int seconds);

    QLabel *m_cameraIpLabel = nullptr;
    QLineEdit *m_cameraIpInput = nullptr;
    QLineEdit *m_cameraUsernameInput = nullptr;
    QLineEdit *m_cameraPasswordInput = nullptr;
    QLabel *m_cameraPasswordStateLabel = nullptr;
    QLabel *m_cameraStatusLabel = nullptr;
    QPushButton *m_saveCameraButton = nullptr;
    QPushButton *m_reloadCameraButton = nullptr;
    QLabel *m_authenticationStatusLabel = nullptr;
    QLabel *m_authenticatedUserLabel = nullptr;
    QLineEdit *m_authenticatedServerOrigin = nullptr;
    QLabel *m_serverConnectionLabel = nullptr;
    QPushButton *m_testServerButton = nullptr;
    QLabel *m_runtimeDataSourceLabel = nullptr;
    QSpinBox *m_overstayHoursInput = nullptr;
    QSpinBox *m_overstayMinutesInput = nullptr;
    QSpinBox *m_overstaySecondsInput = nullptr;
    QLabel *m_currentOverstayLabel = nullptr;
    QLabel *m_applyPolicyLabel = nullptr;
    QLabel *m_overstayStatusLabel = nullptr;
    QPushButton *m_applyOverstayButton = nullptr;
    QPushButton *m_refreshOverstayButton = nullptr;
    int m_serverOverstaySeconds = -1;
    bool m_cameraOperationInFlight = false;
    bool m_serverConnected = false;
    bool m_overstayRequestInFlight = false;
};

#endif
