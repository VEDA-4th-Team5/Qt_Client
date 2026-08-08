#ifndef PARKINGCONTROLLER_H
#define PARKINGCONTROLLER_H

#include "diagnostics/diagnostictypes.h"
#include "api/parkingroi.h"
#include "models/monitoringevent.h"
#include "models/parkingstate.h"
#include "models/slotidmapper.h"

#include <QObject>
#include <QHash>
#include <QQueue>
#include <QSet>
#include <QUrl>

class ApiClient;
class QDateTime;
class ImageLoader;
class QJsonDocument;
class QJsonObject;
class MqttServiceClient;
class QTimer;
struct ServerParkingEvent;
struct ServerFireEvent;

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
    void replaceViewState(const ParkingViewState &state);
    void applyEvSlotUpdate(const QString &slotId, SlotState state,
                           const QString &plateNumber, bool isEv,
                           const QString &occupiedTime, const QString &alarmText);
    void applyParkingSlotUpdate(const QString &slotId, SlotState state);

public slots:
    void requestSlotDetail(const QString &slotId);
    void updateServerBaseUrl(const QString &baseUrl);
    void reconnectNow();
    void requestOverstayThreshold();
    void updateOverstayThreshold(int seconds);
    void requestParkingRois(quint64 generation);
    void requestParkingRoi(const QString &slotId, quint64 generation);
    void updateParkingRoi(const QString &slotId, const ParkingRoi &roi,
                          quint64 generation);
    void clearAlarms();
    void acknowledgeFireAlarm(const QString &channelId);
    void applyManualJsonMessage(const QJsonObject &json);
    void recordEvent(const QString &zone, const QString &eventType,
                     const QString &message, const QString &status);

signals:
    void stateChanged();
    void bannerChanged(const QString &message, bool hasAlert);
    void statusMessageChanged(const QString &message);
    void eventLogged(const MonitoringEvent &event);
    void slotDetailReady(const QString &slotId);
    void slotDetailFailed(const QString &slotId, const QString &message);
    void detailError(const QString &message);
    void serverBaseUrlChanged(const QString &baseUrl);
    void serverConnectionChanged(const QString &status, bool connected);
    void apiDiagnosticChanged(const ApiDiagnosticState &state);
    void serverConfigurationError(const QString &message);
    void overstayThresholdRequestStarted(const QString &status);
    void overstayThresholdReceived(int seconds, const QString &applyPolicy,
                                   bool afterUpdate);
    void overstayThresholdRequestFailed(const QString &message,
                                        bool updateRequest);
    void parkingRoiListReceived(const ParkingRoiMap &rois,
                                quint64 generation);
    void parkingRoiReceived(const QString &slotId, const ParkingRoi &roi,
                            quint64 generation, bool afterSave,
                            bool appliedImmediately);
    void parkingRoiRequestFailed(const QString &slotId,
                                 const QString &message,
                                 quint64 generation,
                                 bool saveRequest);
    void fireAcknowledgementCommandPrepared(const QString &topic,
                                             const QByteArray &payload);
    void fireConfirmationRequested(const QString &channelId,
                                   const QString &alarmId);
    void fireConfirmationRetryRequested(const QString &channelId,
                                        const QString &alarmId);
    void fireConfirmationClosed(const QString &channelId,
                                const QString &alarmId);

private slots:
    void handleMqttMessage(const QString &topic, const QByteArray &payload);
    void handleMqttMessageWithMetadata(const QString &topic,
                                       const QByteArray &payload,
                                       bool retained);
    void applyParkingSnapshot(const QJsonDocument &document);

private:
    void initializeApiClient();
    void initializeMqttClient();
    void applyChannelFireEvent(const ServerFireEvent &event,
                               const QString &topic,
                               bool retained);
    void recordChannelFireEvent(const ServerFireEvent &event);
    bool applyServerParkingEvent(const QJsonObject &event,
                                 const QString &topic);
    void recordServerEvent(const ServerParkingEvent &event,
                           const QString &slotId);
    bool rememberServerEventId(const QString &eventId);
    void rebuildApiClient();
    void scheduleReconnect(const QString &reason);
    void applyParkingSlotDetail(const QString &requestedSlotId,
                                const QJsonDocument &document);
    void applyParkingSessionImages(const QString &slotId,
                                   const QJsonDocument &document);
    void applyOverstayThresholdResponse(const QJsonDocument &document);
    void applyParkingRoiResponse(const QString &requestTag,
                                 const QJsonDocument &document);
    void applyParkingRoiError(const QString &requestTag,
                              const QString &message);
    QString parkingRoiPath(const QString &slotId) const;
    void resetSlotsForSnapshot();
    void notifyStateChanged();
    void refreshAlert();
    void publishApiDiagnostic();
    QUrl resolveApiUrl(const QUrl &url) const;
    QString occupiedDurationText(const QDateTime &occupiedSince, int elapsedSeconds) const;

    QString m_sharedConfigPath;
    QString m_localConfigPath;
    ParkingViewState m_state;
    ApiClient *m_apiClient = nullptr;
    ImageLoader *m_imageLoader = nullptr;
    MqttServiceClient *m_mqttClient = nullptr;
    QTimer *m_reconnectTimer = nullptr;
    QSet<QString> m_seenServerEventIds;
    QQueue<QString> m_seenServerEventOrder;
    QSet<QString> m_fireAckCommandKeys;
    QUrl m_apiBaseUrl;
    QString m_slotsPath;
    QString m_slotDetailPath;
    QString m_sessionImagesPath;
    QString m_overstayThresholdPath;
    QString m_parkingRoiListPath;
    QString m_parkingRoiPathTemplate;
    QHash<QString, QString> m_pendingDetailRequests;
    QHash<QString, QString> m_pendingImageRequests;
    QSet<QString> m_pendingParkingRoiTags;
    int m_apiTimeoutMs = 5000;
    int m_reconnectIntervalMs = 5000;
    int m_maxReconnectIntervalMs = 60000;
    int m_currentReconnectDelayMs = 5000;
    bool m_allowInsecureHttp = false;
    bool m_snapshotRequestInFlight = false;
    enum class OverstayRequest { None, Fetch, Update, Verify };
    OverstayRequest m_overstayRequest = OverstayRequest::None;
    quint64 m_nextEventSequence = 1;
    ApiDiagnosticState m_apiDiagnostic;
    SlotIdMapper m_slotIdMapper;
};

#endif
