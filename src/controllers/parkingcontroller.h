#ifndef PARKINGCONTROLLER_H
#define PARKINGCONTROLLER_H

#include "models/parkingstate.h"

#include <QObject>
#include <QUrl>

class ApiClient;
class QDateTime;
class ImageLoader;
class QJsonDocument;
class QTimer;

class ParkingController : public QObject
{
    Q_OBJECT

public:
    explicit ParkingController(const QString &sharedConfigPath,
                               const QString &localConfigPath,
                               QObject *parent = nullptr);

    void start();
    const ParkingViewState &state() const { return m_state; }
    ImageLoader *imageLoader() const { return m_imageLoader; }
    SlotState slotState(const QString &slotId) const;
    QString plateNumber(const QString &slotId) const;
    QList<ParkingImageResource> images(const QString &slotId) const;

public slots:
    void requestSlotDetail(const QString &slotId);
    void updateServerBaseUrl(const QString &baseUrl);
    void reconnectNow();
    void clearAlarms();
    void toggleMockEv();
    void triggerNonEvAlert();
    void triggerOvertimeAlert();
    void triggerSensorError();
    void randomizeParkingSlots();
    void simulateIncomingMessages();
    void processIncomingMessage(const QString &message);
    void recordEvent(const QString &zone, const QString &eventType,
                     const QString &message, const QString &status);

signals:
    void stateChanged();
    void bannerChanged(const QString &message, bool hasAlert);
    void statusMessageChanged(const QString &message);
    void eventLogged(const QString &time, const QString &zone, const QString &eventType,
                     const QString &message, const QString &status);
    void slotDetailReady(const QString &slotId);
    void detailError(const QString &message);
    void serverBaseUrlChanged(const QString &baseUrl);
    void serverConnectionChanged(const QString &status, bool connected);
    void serverConfigurationError(const QString &message);

private:
    void initializeMockData();
    void initializeApiClient();
    void rebuildApiClient();
    void scheduleReconnect(const QString &reason);
    void applyParkingSnapshot(const QJsonDocument &document);
    void applyParkingSlotDetail(const QJsonDocument &document);
    void resetSlotsForSnapshot();
    void updateEvSlotState(const QString &slotId, SlotState state,
                           const QString &plateNumber, bool isEv,
                           const QString &occupiedTime, const QString &alarmText);
    void updateParkingSlotState(const QString &slotId, SlotState state);
    void notifyStateChanged();
    void refreshAlert();
    QUrl resolveApiUrl(const QUrl &url) const;
    QString occupiedDurationText(const QDateTime &occupiedSince, int elapsedSeconds) const;

    QString m_sharedConfigPath;
    QString m_localConfigPath;
    ParkingViewState m_state;
    ApiClient *m_apiClient = nullptr;
    ImageLoader *m_imageLoader = nullptr;
    QTimer *m_reconnectTimer = nullptr;
    QUrl m_apiBaseUrl;
    QString m_slotsPath;
    QString m_slotDetailPath;
    int m_apiTimeoutMs = 5000;
    int m_reconnectIntervalMs = 5000;
    int m_maxReconnectIntervalMs = 60000;
    int m_currentReconnectDelayMs = 5000;
    bool m_allowInsecureHttp = false;
    bool m_snapshotRequestInFlight = false;
    int m_mockStep = 0;
};

#endif
