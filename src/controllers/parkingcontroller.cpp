#include "parkingcontroller.h"

#include "adapters/incomingmessageadapter.h"
#include "api/apiclient.h"
#include "api/imageloader.h"
#include "api/parkingresponseparser.h"
#include "services/mqttserviceclient.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSettings>
#include <QTimer>

namespace {
QString numberedSlotId(const QString &prefix, int number)
{
    return QStringLiteral("%1-%2").arg(prefix).arg(number, 2, 10, QLatin1Char('0'));
}

QString evSlotId(int number)
{
    return numberedSlotId(QStringLiteral("EV"), number);
}

QString parkingSlotId(int number)
{
    return numberedSlotId(QStringLiteral("P"), number);
}

}

ParkingController::ParkingController(const QString &sharedConfigPath,
                                     const QString &localConfigPath,
                                     QObject *parent)
    : QObject(parent)
    , m_sharedConfigPath(sharedConfigPath)
    , m_localConfigPath(localConfigPath)
    , m_reconnectTimer(new QTimer(this))
{
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout,
            this, &ParkingController::reconnectNow);
}

void ParkingController::start()
{
    initializeApiClient();
    initializeMqttClient();
}

void ParkingController::initializeMqttClient()
{
    QSettings sharedSettings(m_sharedConfigPath, QSettings::IniFormat);
    QSettings localSettings(m_localConfigPath, QSettings::IniFormat);
    auto setting = [&](const QString &key, const QVariant &defaultValue) {
        return localSettings.contains(key) ? localSettings.value(key, defaultValue)
                                           : sharedSettings.value(key, defaultValue);
    };

    // Settings can be reloaded while the application is running. Tear down the
    // client first so a disabled or changed broker cannot keep retrying with a
    // stale host captured by the previous MqttSettings instance.
    if (m_mqttClient) {
        m_mqttClient->stop();
        disconnect(m_mqttClient, nullptr, this, nullptr);
        m_mqttClient->deleteLater();
        m_mqttClient = nullptr;
    }

    MqttSettings mqttSettings;
    mqttSettings.enabled = setting(QStringLiteral("mqtt/enabled"), false).toBool();
    if (!mqttSettings.enabled) {
        emit statusMessageChanged(QStringLiteral("MQTT disabled"));
        return;
    }

    mqttSettings.host = setting(QStringLiteral("mqtt/host"), QString()).toString().trimmed();
    const bool followApiHost =
        setting(QStringLiteral("mqtt/follow_api_host"), true).toBool();
    if (followApiHost) {
        const QUrl apiUrl(
            setting(QStringLiteral("api/base_url"), QString()).toString().trimmed());
        if (apiUrl.isValid() && !apiUrl.host().isEmpty()) {
            mqttSettings.host = apiUrl.host();
        }
    }
    mqttSettings.port =
        static_cast<quint16>(setting(QStringLiteral("mqtt/port"), 1883).toInt());
    mqttSettings.clientId =
        setting(QStringLiteral("mqtt/client_id"), QStringLiteral("qt-client")).toString();
    mqttSettings.reconnectIntervalMs =
        setting(QStringLiteral("mqtt/reconnect_interval_ms"), 5000).toInt();
    mqttSettings.topics = setting(QStringLiteral("mqtt/topics"),
                                  QStringLiteral("parking/fire/#"))
                              .toString()
                              .split(QLatin1Char(','), Qt::SkipEmptyParts);

    m_mqttClient = new MqttServiceClient(mqttSettings, this);
    connect(m_mqttClient, &MqttServiceClient::messageReceived,
            this, &ParkingController::handleMqttMessage);
    connect(m_mqttClient, &MqttServiceClient::connectionChanged, this,
            [this](const QString &status, bool connected) {
                emit statusMessageChanged(status);
                recordEvent(QStringLiteral("SYSTEM"), QStringLiteral("MQTT"),
                            status,
                            connected ? QStringLiteral("DONE")
                                      : QStringLiteral("FAILED"));
            });
    m_mqttClient->start();
}

void ParkingController::handleMqttMessage(const QString &topic,
                                          const QByteArray &payload)
{
    emit statusMessageChanged(QStringLiteral("MQTT RX: ") + topic);

    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        recordEvent(QStringLiteral("SYSTEM"), QStringLiteral("MQTT_PARSE_ERROR"),
                    topic + QStringLiteral(": ") + parseError.errorString(),
                    QStringLiteral("REJECTED"));
        return;
    }

    const QJsonObject event = document.object();
    const QString eventType = event.value(QStringLiteral("event_type")).toString();

    if (eventType == QStringLiteral("sensor_fire_suspected")
        || eventType == QStringLiteral("sensor_fire_cleared")) {
        applyFireEvent(event);
        return;
    }

    recordEvent(QStringLiteral("SYSTEM"), QStringLiteral("MQTT_UNSUPPORTED"),
                topic + QStringLiteral(": ") + eventType,
                QStringLiteral("REJECTED"));
}

void ParkingController::applyFireEvent(const QJsonObject &event)
{
    // The event type already distinguishes activation from clearing. Do not
    // depend on an optional compatibility field such as "active".
    const bool active = event.value(QStringLiteral("event_type")).toString()
        == QStringLiteral("sensor_fire_suspected");
    const QString sensorId = event.value(QStringLiteral("source_id")).toString();
    const QString rawSlotId = event.value(QStringLiteral("slot_id")).toString();
    const QString rawPayload = event.value(QStringLiteral("raw_payload")).toString();

    // Pi 가 주차면 매핑에 실패해도 알림 자체는 올라온다. 화면에서 지우지 않는다.
    if (rawSlotId.isEmpty()) {
        recordEvent(QStringLiteral("UNMAPPED"),
                    active ? QStringLiteral("FIRE_SUSPECTED")
                           : QStringLiteral("FIRE_CLEARED"),
                    QStringLiteral("Fire candidate from unmapped sensor %1 (%2)")
                        .arg(sensorId, rawPayload),
                    active ? QStringLiteral("OPEN") : QStringLiteral("CLOSED"));
        if (active) {
            emit bannerChanged(
                QStringLiteral("FIRE SUSPECTED | unmapped sensor %1 | operator must verify")
                    .arg(sensorId),
                true);
        } else {
            refreshAlert();
        }
        return;
    }

    const QString slotId = normalizeParkingSlotId(rawSlotId);
    if (!m_state.evSlots.contains(slotId) && !m_state.parkingSlots.contains(slotId)) {
        recordEvent(slotId, QStringLiteral("FIRE_SUSPECTED"),
                    QStringLiteral("Fire candidate for unknown slot (sensor %1)")
                        .arg(sensorId),
                    QStringLiteral("SKIPPED"));
        return;
    }

    if (active) {
        if (m_state.evSlots.contains(slotId)) {
            EvSlotInfo &slot = m_state.evSlots[slotId];
            if (!m_visualBeforeFire.contains(slotId)) {
                m_visualBeforeFire.insert(slotId, slot.visual);
                m_evAlarmTextBeforeFire.insert(slotId, slot.alarmText);
            }
            slot.visual.alarm = SlotAlarmKind::FireSuspected;
            slot.visual.alarmAcknowledged = false;
            slot.alarmText = QStringLiteral("FIRE_SUSPECTED");
        } else {
            ParkingSlotInfo &slot = m_state.parkingSlots[slotId];
            if (!m_visualBeforeFire.contains(slotId)) {
                m_visualBeforeFire.insert(slotId, slot.visual);
            }
            slot.visual.alarm = SlotAlarmKind::FireSuspected;
            slot.visual.alarmAcknowledged = false;
        }
        notifyStateChanged();
        recordEvent(slotId, QStringLiteral("FIRE_SUSPECTED"),
                    QStringLiteral("Fire candidate from sensor %1 (%2) - "
                                   "operator confirmation required")
                        .arg(sensorId, rawPayload),
                    QStringLiteral("OPEN"));
        return;
    }

    // 해제: 점유 상태는 건드리지 않고 화재 전에 존재하던 경고 상태만 복원한다.
    if (m_state.evSlots.contains(slotId)) {
        EvSlotInfo &slot = m_state.evSlots[slotId];
        if (m_visualBeforeFire.contains(slotId)) {
            slot.visual = m_visualBeforeFire.take(slotId);
            slot.alarmText = m_evAlarmTextBeforeFire.take(slotId);
        } else {
            slot.visual.alarm = SlotAlarmKind::None;
            slot.visual.alarmAcknowledged = false;
            slot.alarmText = QStringLiteral("NORMAL");
        }
    } else {
        ParkingSlotInfo &slot = m_state.parkingSlots[slotId];
        if (m_visualBeforeFire.contains(slotId)) {
            slot.visual = m_visualBeforeFire.take(slotId);
        } else {
            slot.visual.alarm = SlotAlarmKind::None;
            slot.visual.alarmAcknowledged = false;
        }
    }
    notifyStateChanged();
    recordEvent(slotId, QStringLiteral("FIRE_CLEARED"),
                QStringLiteral("Fire candidate cleared by sensor %1 (%2)")
                    .arg(sensorId, rawPayload),
                QStringLiteral("CLOSED"));
}

void ParkingController::initializeApiClient()
{
    QSettings sharedSettings(m_sharedConfigPath, QSettings::IniFormat);
    QSettings localSettings(m_localConfigPath, QSettings::IniFormat);
    auto setting = [&](const QString &key, const QVariant &defaultValue) {
        return localSettings.contains(key) ? localSettings.value(key, defaultValue)
                                           : sharedSettings.value(key, defaultValue);
    };

    m_state.apiEnabled = setting(QStringLiteral("api/enabled"), false).toBool();
    m_apiDiagnostic = ApiDiagnosticState{};
    m_apiDiagnostic.enabled = m_state.apiEnabled;
    if (!m_state.apiEnabled) {
        if (m_reconnectTimer) m_reconnectTimer->stop();
        emit serverConnectionChanged(QStringLiteral("API disabled"), false);
        publishApiDiagnostic();
        notifyStateChanged();
        return;
    }

    m_apiBaseUrl = QUrl(setting(QStringLiteral("api/base_url"), QString()).toString().trimmed());
    m_apiDiagnostic.endpoint = m_apiBaseUrl.toString(
        QUrl::RemoveUserInfo | QUrl::RemoveQuery | QUrl::RemoveFragment);
    m_apiDiagnostic.status = QStringLiteral("DISCONNECTED");
    m_slotsPath = setting(QStringLiteral("api/slots_path"),
                          QStringLiteral("/api/v1/parking-slots")).toString();
    m_slotDetailPath = setting(QStringLiteral("api/slot_detail_path"),
                               QStringLiteral("/api/v1/parking-slots/{slot_id}")).toString();
    m_sessionImagesPath = setting(
        QStringLiteral("api/session_images_path"),
        QStringLiteral("/api/v1/parking-sessions/{session_id}/images")).toString();
    m_apiTimeoutMs = setting(QStringLiteral("api/timeout_ms"), 5000).toInt();
    m_allowInsecureHttp =
        setting(QStringLiteral("api/allow_insecure_http"), false).toBool();
    m_reconnectIntervalMs =
        qMax(1000, setting(QStringLiteral("api/reconnect_interval_ms"), 5000).toInt());
    m_maxReconnectIntervalMs =
        qMax(m_reconnectIntervalMs,
             setting(QStringLiteral("api/max_reconnect_interval_ms"), 60000).toInt());
    m_currentReconnectDelayMs = m_reconnectIntervalMs;
    rebuildApiClient();
    reconnectNow();
}

void ParkingController::rebuildApiClient()
{
    if (m_reconnectTimer) m_reconnectTimer->stop();
    m_snapshotRequestInFlight = false;
    if (m_apiClient) {
        disconnect(m_apiClient, nullptr, this, nullptr);
        m_apiClient->deleteLater();
    }
    if (m_imageLoader) m_imageLoader->deleteLater();
    m_pendingDetailRequests.clear();
    m_pendingImageRequests.clear();

    m_apiClient = new ApiClient(
        m_apiBaseUrl, m_apiTimeoutMs, m_allowInsecureHttp, this);
    m_imageLoader = new ImageLoader(
        m_apiTimeoutMs, m_allowInsecureHttp, this);

    connect(m_apiClient, &ApiClient::jsonReceived, this,
            [this](const QString &path, const QJsonDocument &document,
                   int latencyMs, int httpStatus) {
                if (path == m_slotsPath) {
                    m_snapshotRequestInFlight = false;
                    m_apiDiagnostic.lastLatencyMs = latencyMs;
                    m_apiDiagnostic.lastHttpStatus = httpStatus;
                    applyParkingSnapshot(document);
                } else if (m_pendingImageRequests.contains(path)) {
                    applyParkingSessionImages(m_pendingImageRequests.take(path), document);
                } else {
                    const QString requestedSlotId = m_pendingDetailRequests.take(path);
                    applyParkingSlotDetail(requestedSlotId, document);
                }
            });
    connect(m_apiClient, &ApiClient::requestFailed, this,
            [this](const QString &path, const QString &message,
                   int latencyMs, int httpStatus) {
                recordEvent(QStringLiteral("SYSTEM"), QStringLiteral("API_ERROR"),
                            path + QStringLiteral(": ") + message,
                            QStringLiteral("FAILED"));
                if (path == m_slotsPath) {
                    m_snapshotRequestInFlight = false;
                    m_apiDiagnostic.connected = false;
                    m_apiDiagnostic.status = QStringLiteral("ERROR");
                    m_apiDiagnostic.lastError = message;
                    m_apiDiagnostic.lastLatencyMs = latencyMs;
                    m_apiDiagnostic.lastHttpStatus = httpStatus;
                    ++m_apiDiagnostic.consecutiveFailures;
                    scheduleReconnect(message);
                } else if (m_pendingImageRequests.contains(path)) {
                    const QString slotId = m_pendingImageRequests.take(path);
                    emit slotDetailFailed(slotId, message);
                } else if (m_pendingDetailRequests.contains(path)) {
                    const QString slotId = m_pendingDetailRequests.take(path);
                    emit slotDetailFailed(slotId, message);
                } else {
                    emit detailError(message);
                }
            });

    emit serverBaseUrlChanged(m_apiBaseUrl.toString());
    publishApiDiagnostic();
    notifyStateChanged();
}
void ParkingController::reconnectNow()
{
    if (!m_state.apiEnabled || !m_apiClient) {
        emit serverConnectionChanged(QStringLiteral("API disabled"), false);
        return;
    }
    if (m_reconnectTimer) m_reconnectTimer->stop();
    if (m_snapshotRequestInFlight) {
        emit serverConnectionChanged(QStringLiteral("Connection already in progress"), false);
        return;
    }
    const QString message =
        QStringLiteral("Connecting to %1...").arg(m_apiBaseUrl.toString(
            QUrl::RemoveUserInfo | QUrl::RemoveQuery | QUrl::RemoveFragment));
    emit bannerChanged(message, false);
    emit serverConnectionChanged(QStringLiteral("Connecting..."), false);
    m_apiDiagnostic.connected = false;
    m_apiDiagnostic.status = QStringLiteral("CONNECTING");
    m_apiDiagnostic.lastAttemptAt = QDateTime::currentDateTime();
    m_apiDiagnostic.nextRetrySeconds = 0;
    publishApiDiagnostic();
    m_snapshotRequestInFlight = true;
    m_apiClient->getJson(m_slotsPath);
}

void ParkingController::scheduleReconnect(const QString &reason)
{
    if (!m_state.apiEnabled || !m_reconnectTimer) return;
    const int delayMs = m_currentReconnectDelayMs;
    emit bannerChanged(
        QStringLiteral("Parking API unavailable | Retrying in %1 seconds")
            .arg(delayMs / 1000),
        true);
    emit serverConnectionChanged(
        QStringLiteral("Disconnected: %1 | retry in %2 seconds")
            .arg(reason).arg(delayMs / 1000),
        false);
    m_apiDiagnostic.connected = false;
    m_apiDiagnostic.status = QStringLiteral("RETRYING");
    m_apiDiagnostic.lastError = reason;
    m_apiDiagnostic.nextRetrySeconds = delayMs / 1000;
    publishApiDiagnostic();
    if (!m_reconnectTimer->isActive()) m_reconnectTimer->start(delayMs);
    m_currentReconnectDelayMs =
        qMin(m_currentReconnectDelayMs * 2, m_maxReconnectIntervalMs);
}

void ParkingController::updateServerBaseUrl(const QString &baseUrl)
{
    QString normalized = baseUrl.trimmed();
    while (normalized.endsWith(QLatin1Char('/'))) normalized.chop(1);
    const QUrl url(normalized);
    const bool allowedScheme = url.scheme() == QStringLiteral("https")
        || (m_allowInsecureHttp && url.scheme() == QStringLiteral("http"));
    if (!url.isValid() || url.host().isEmpty() || !allowedScheme) {
        emit serverConfigurationError(
            QStringLiteral("Enter a valid %1 server URL including port.")
                .arg(m_allowInsecureHttp ? QStringLiteral("HTTP or HTTPS")
                                        : QStringLiteral("HTTPS")));
        return;
    }

    QSettings localSettings(m_localConfigPath, QSettings::IniFormat);
    localSettings.setValue(QStringLiteral("api/enabled"), true);
    localSettings.setValue(QStringLiteral("api/base_url"), normalized);
    // The deployed Mosquitto broker runs on the same Pi as the HTTP API. Keep
    // the effective broker address in sync with the full URL entered by the
    // user instead of preserving a host from a previous subnet.
    localSettings.setValue(QStringLiteral("mqtt/follow_api_host"), true);
    localSettings.setValue(QStringLiteral("mqtt/host"), url.host());
    localSettings.sync();
    if (localSettings.status() != QSettings::NoError) {
        emit serverConfigurationError(
            QStringLiteral("Failed to save client_config.local.ini."));
        return;
    }

    recordEvent(QStringLiteral("SYSTEM"), QStringLiteral("API_ADDRESS_UPDATED"),
                QStringLiteral("Server API changed to ")
                    + url.toString(QUrl::RemoveUserInfo | QUrl::RemoveQuery
                                   | QUrl::RemoveFragment),
                QStringLiteral("DONE"));
    initializeApiClient();
    initializeMqttClient();
}
void ParkingController::applyParkingSnapshot(const QJsonDocument &document)
{
    ParkingSnapshot snapshot;
    QString error;
    if (!ParkingResponseParser::parseSnapshot(document, snapshot, error)) {
        emit bannerChanged(QStringLiteral("Invalid parking API response | Previous state retained"), true);
        recordEvent(QStringLiteral("SYSTEM"), QStringLiteral("API_PARSE_ERROR"), error, QStringLiteral("FAILED"));
        m_apiDiagnostic.connected = false;
        m_apiDiagnostic.status = QStringLiteral("INVALID_RESPONSE");
        m_apiDiagnostic.lastError = error;
        ++m_apiDiagnostic.consecutiveFailures;
        publishApiDiagnostic();
        return;
    }
    if (m_reconnectTimer) m_reconnectTimer->stop();
    m_snapshotRequestInFlight = false;
    m_currentReconnectDelayMs = m_reconnectIntervalMs;
    emit serverConnectionChanged(QStringLiteral("Connected"), true);
    resetSlotsForSnapshot();
    int appliedCount = 0;
    for (const ParkingSlotSnapshot &slot : snapshot.parkingSlots) {
        const QString slotId = normalizeParkingSlotId(slot.slotId);
        if (!m_state.evSlots.contains(slotId) && !m_state.parkingSlots.contains(slotId)) {
            recordEvent(slotId, QStringLiteral("API_SLOT_SKIPPED"), QStringLiteral("Unknown parking slot in response"), QStringLiteral("SKIPPED"));
            continue;
        }
        SlotState state = slotStateFromText(slot.state);
        const SlotAlarmKind alarmKind = slotAlarmKindFromText(slot.alarm, state);
        if (alarmKind == SlotAlarmKind::NonEvViolation) state = SlotState::NonEvAlert;
        else if (alarmKind == SlotAlarmKind::Overstay) state = SlotState::OvertimeAlert;
        else if (alarmKind == SlotAlarmKind::SensorError) state = SlotState::SensorError;
        SlotVisualState visual = deriveSlotVisualState(
            state, slot.vehicleTypeKnown, slot.isEv, slot.alarm);
        if (slotStateFromText(slot.state) == SlotState::Acked) {
            visual.alarmAcknowledged = true;
        }
        if (slotId.startsWith(QStringLiteral("EV-"))) {
            EvSlotInfo info{slotId, slot.plateNumber.isEmpty() ? QStringLiteral("-") : slot.plateNumber,
                            slot.isEv, occupiedDurationText(slot.occupiedSince, slot.elapsedSeconds), state,
                            slot.alarm.isEmpty() ? slotStateText(state) : slot.alarm};
            info.visual = visual;
            m_state.evSlots[slotId] = info;
        } else {
            ParkingSlotInfo info{slotId, state};
            info.visual = visual;
            m_state.parkingSlots[slotId] = info;
        }
        m_state.slotPlateNumbers.insert(slotId, slot.plateNumber);
        QList<ParkingImageResource> images;
        for (ParkingImageResource image : slot.images) { image.url = resolveApiUrl(image.url); images.append(image); }
        if (!images.isEmpty()) m_state.slotImages.insert(slotId, images);
        ++appliedCount;
    }
    notifyStateChanged();
    const QString generatedAt = snapshot.generatedAt.isValid()
        ? snapshot.generatedAt.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
        : QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    emit statusMessageChanged(QStringLiteral("API synchronized: %1 slots at %2").arg(appliedCount).arg(generatedAt));
    m_apiDiagnostic.connected = true;
    m_apiDiagnostic.status = QStringLiteral("CONNECTED");
    m_apiDiagnostic.lastError.clear();
    m_apiDiagnostic.lastSuccessAt = QDateTime::currentDateTime();
    m_apiDiagnostic.consecutiveFailures = 0;
    m_apiDiagnostic.nextRetrySeconds = 0;
    m_apiDiagnostic.appliedSlotCount = appliedCount;
    publishApiDiagnostic();
    recordEvent(QStringLiteral("SYSTEM"), QStringLiteral("API_SYNC"), QStringLiteral("Applied %1 parking slots").arg(appliedCount), QStringLiteral("DONE"));
}

void ParkingController::publishApiDiagnostic()
{
    emit apiDiagnosticChanged(m_apiDiagnostic);
}

void ParkingController::applyParkingSlotDetail(
    const QString &requestedSlotId,
    const QJsonDocument &document)
{
    ParkingSlotSnapshot slot;
    QString error;
    if (!ParkingResponseParser::parseSlotDetail(document, slot, error)) {
        emit slotDetailFailed(requestedSlotId, error);
        refreshAlert();
        return;
    }
    const QString slotId = normalizeParkingSlotId(slot.slotId);
    m_state.slotPlateNumbers.insert(slotId, slot.plateNumber);
    QList<ParkingImageResource> images;
    for (ParkingImageResource image : slot.images) { image.url = resolveApiUrl(image.url); images.append(image); }
    if (images.isEmpty()) m_state.slotImages.remove(slotId); else m_state.slotImages.insert(slotId, images);
    notifyStateChanged();

    if (slot.sessionId <= 0 || !m_apiClient) {
        if (images.isEmpty()) {
            m_state.slotImages.remove(slotId);
        }
        refreshAlert();
        emit slotDetailReady(slotId);
        return;
    }

    QString path = m_sessionImagesPath;
    path.replace(QStringLiteral("{session_id}"), QString::number(slot.sessionId));
    if (m_pendingImageRequests.contains(path)) {
        return;
    }
    m_pendingImageRequests.insert(path, slotId);
    emit bannerChanged(QStringLiteral("Loading %1 evidence images...").arg(slotId), false);
    m_apiClient->getJson(path);
}

void ParkingController::applyParkingSessionImages(
    const QString &slotId,
    const QJsonDocument &document)
{
    QList<ParkingImageResource> images;
    QString error;
    if (!ParkingResponseParser::parseSessionImages(document, images, error)) {
        emit slotDetailFailed(slotId, error);
        refreshAlert();
        return;
    }

    for (ParkingImageResource &image : images) {
        image.url = resolveApiUrl(image.url);
    }
    if (images.isEmpty()) {
        m_state.slotImages.remove(slotId);
    } else {
        m_state.slotImages.insert(slotId, images);
    }
    notifyStateChanged();
    refreshAlert();
    emit slotDetailReady(slotId);
}

void ParkingController::resetSlotsForSnapshot()
{
    m_visualBeforeFire.clear();
    m_evAlarmTextBeforeFire.clear();
    m_state.evSlots.clear(); m_state.parkingSlots.clear(); m_state.slotImages.clear(); m_state.slotPlateNumbers.clear();
    for (int number = 1; number <= 16; ++number) {
        const QString slotId = evSlotId(number);
        EvSlotInfo slot{slotId, QStringLiteral("-"), false, QStringLiteral("-"),
                        SlotState::Vacant, QStringLiteral("NORMAL")};
        slot.visual = deriveSlotVisualState(slot.state, false, false, slot.alarmText);
        m_state.evSlots.insert(slotId, slot);
    }
    for (int number = 1; number <= 16; ++number) {
        const QString slotId = parkingSlotId(number);
        ParkingSlotInfo slot{slotId, SlotState::Vacant};
        slot.visual = deriveSlotVisualState(slot.state, false, false);
        m_state.parkingSlots.insert(slotId, slot);
    }
}

void ParkingController::requestSlotDetail(const QString &rawSlotId)
{
    const QString slotId = normalizeParkingSlotId(rawSlotId);
    if (!m_apiClient) { emit slotDetailReady(slotId); return; }
    QString compact = slotId; compact.remove(QLatin1Char('-'));
    QString path = m_slotDetailPath; path.replace(QStringLiteral("{slot_id}"), compact);
    if (m_pendingDetailRequests.contains(path)) {
        emit statusMessageChanged(QStringLiteral("%1 evidence request is already in progress").arg(slotId));
        return;
    }
    m_pendingDetailRequests.insert(path, slotId);
    emit bannerChanged(QStringLiteral("Loading %1 details...").arg(slotId), false);
    m_apiClient->getJson(path);
}

void ParkingController::replaceViewState(const ParkingViewState &state)
{
    m_visualBeforeFire.clear();
    m_evAlarmTextBeforeFire.clear();
    m_state = state;
    notifyStateChanged();
}

void ParkingController::applyEvSlotUpdate(const QString &slotId, SlotState state,
                                          const QString &plateNumber, bool isEv,
                                          const QString &occupiedTime, const QString &alarmText)
{
    EvSlotInfo updated{slotId, plateNumber, isEv, occupiedTime, state, alarmText};
    if (state == SlotState::Acked && m_state.evSlots.contains(slotId)) {
        updated.visual = m_state.evSlots.value(slotId).visual;
        updated.visual.alarmAcknowledged = true;
    } else {
        updated.visual = deriveSlotVisualState(state, state != SlotState::Vacant,
                                               isEv, alarmText);
    }
    m_state.evSlots[slotId] = updated;
    notifyStateChanged();
}

void ParkingController::applyParkingSlotUpdate(const QString &slotId, SlotState state)
{
    ParkingSlotInfo updated{slotId, state};
    const ParkingSlotInfo previous = m_state.parkingSlots.value(slotId);
    if (state == SlotState::SensorError) {
        updated.visual = previous.visual;
        updated.visual.alarm = SlotAlarmKind::SensorError;
        updated.visual.alarmAcknowledged = false;
    } else if (state == SlotState::Acked) {
        updated.visual = previous.visual;
        updated.visual.alarmAcknowledged = true;
    } else {
        updated.visual = deriveSlotVisualState(state, state == SlotState::Occupied, false);
    }
    m_state.parkingSlots[slotId] = updated;
    notifyStateChanged();
}

void ParkingController::notifyStateChanged()
{
    emit stateChanged();
    refreshAlert();
}

void ParkingController::refreshAlert()
{
    QStringList alerts;
    QStringList fires;
    for (const EvSlotInfo &slot : m_state.evSlots) {
        if (slot.visual.alarm == SlotAlarmKind::FireSuspected) {
            fires << slot.slotId;
        } else if (slot.visual.alarm != SlotAlarmKind::None
                   && !slot.visual.alarmAcknowledged) {
            alerts << slot.slotId + QLatin1Char(':') + slotAlarmText(slot.visual.alarm);
        }
    }
    for (const ParkingSlotInfo &slot : m_state.parkingSlots) {
        if (slot.visual.alarm == SlotAlarmKind::FireSuspected) {
            fires << slot.slotId;
        } else if (slot.visual.alarm != SlotAlarmKind::None
                   && !slot.visual.alarmAcknowledged) {
            alerts << slot.slotId + QLatin1Char(':') + slotAlarmText(slot.visual.alarm);
        }
    }

    // 화재 후보는 다른 알람보다 앞에 세운다. 확정 표현은 쓰지 않는다.
    if (!fires.isEmpty()) {
        emit bannerChanged(QStringLiteral("FIRE SUSPECTED: %1 | operator must verify")
                               .arg(fires.join(QStringLiteral(" / "))),
                           true);
        return;
    }

    emit bannerChanged(alerts.isEmpty() ? QStringLiteral("Monitoring normal | No active alerts")
                                        : QStringLiteral("Active alerts: %1 | %2").arg(alerts.size()).arg(alerts.join(QStringLiteral(" / "))),
                       !alerts.isEmpty());
}

void ParkingController::recordEvent(const QString &zone, const QString &eventType,
                                    const QString &message, const QString &status)
{
    MonitoringEvent event;
    event.occurredAt = QDateTime::currentDateTime();
    event.id = QStringLiteral("client-%1-%2")
                   .arg(event.occurredAt.toMSecsSinceEpoch())
                   .arg(m_nextEventSequence++);
    event.sourceId = zone;
    event.eventType = eventType;
    event.ackState = eventAckStateFromStatus(status);
    event.message = message;
    event.status = status;
    emit eventLogged(event);
}

void ParkingController::clearAlarms()
{
    bool cleared = false;
    for (auto it = m_state.evSlots.begin(); it != m_state.evSlots.end(); ++it) {
        if (it->visual.alarm != SlotAlarmKind::None
            && it->visual.alarm != SlotAlarmKind::FireSuspected
            && !it->visual.alarmAcknowledged) {
            it->state = SlotState::Acked;
            it->visual.alarmAcknowledged = true;
            it->alarmText = QStringLiteral("ACKED");
            cleared = true;
        }
    }
    for (auto it = m_state.parkingSlots.begin(); it != m_state.parkingSlots.end(); ++it) {
        if (it->visual.alarm != SlotAlarmKind::None
            && it->visual.alarm != SlotAlarmKind::FireSuspected
            && !it->visual.alarmAcknowledged) {
            it->state = SlotState::Acked;
            it->visual.alarmAcknowledged = true;
            cleared = true;
        }
    }
    notifyStateChanged();
    recordEvent(QStringLiteral("ALL"), QStringLiteral("ALARM_ACK"), cleared ? QStringLiteral("Active alarms acknowledged") : QStringLiteral("No alarms to clear"), QStringLiteral("ACKED"));
}

void ParkingController::processIncomingMessage(const QString &message)
{
    const ParsedIncomingMessage parsed = IncomingMessageAdapter::parse(message);
    emit statusMessageChanged(QStringLiteral("Last RX: ") + parsed.raw);

    if (!parsed.isValid()) {
        recordEvent(QStringLiteral("SYSTEM"), QStringLiteral("RX_ERROR"),
                    parsed.errorMessage, QStringLiteral("REJECTED"));
        return;
    }

    const QString &slotId = parsed.sourceId;
    const QString &value = parsed.value;
    switch (parsed.kind) {
    case IncomingMessageKind::NormalizedEvent:
        recordEvent(slotId, parsed.eventType, parsed.message, parsed.status);
        return;

    case IncomingMessageKind::ParkingSlot: {
        const SlotState state = slotStateFromText(value);
        applyParkingSlotUpdate(slotId, state);
        recordEvent(slotId, slotStateText(state),
                    QStringLiteral("Parking slot state updated from RX message"),
                    QStringLiteral("RECORDED"));
        return;
    }

    case IncomingMessageKind::HallSensor: {
        if (value == QStringLiteral("ERROR") || value == QStringLiteral("FAILED")) {
            applyParkingSlotUpdate(slotId, SlotState::SensorError);
            recordEvent(slotId, QStringLiteral("HALL_SENSOR_ERROR"),
                        QStringLiteral("Hall sensor error received"),
                        QStringLiteral("OPEN"));
            return;
        }
        if (value == QStringLiteral("ACKED")) {
            applyParkingSlotUpdate(slotId, SlotState::Acked);
            recordEvent(slotId, QStringLiteral("HALL_SENSOR_ACK"),
                        QStringLiteral("Hall sensor alarm acknowledged from RX message"),
                        QStringLiteral("ACKED"));
            return;
        }
        if (value == QStringLiteral("CLEAR")) {
            ParkingSlotInfo &slot = m_state.parkingSlots[slotId];
            slot.visual.alarm = SlotAlarmKind::None;
            slot.visual.alarmAcknowledged = false;
            slot.state = slot.visual.occupancy == SlotOccupancy::Occupied
                ? SlotState::Occupied : SlotState::Vacant;
            notifyStateChanged();
            recordEvent(slotId, QStringLiteral("HALL_SENSOR_CLEAR"),
                        QStringLiteral("Hall sensor alarm cleared from RX message"),
                        QStringLiteral("CLEARED"));
            return;
        }
        const SlotState state = slotStateFromText(value);
        applyParkingSlotUpdate(slotId, state);
        recordEvent(slotId, QStringLiteral("HALL_SENSOR_CHANGED"),
                    QStringLiteral("Hall sensor state updated from RX message"),
                    QStringLiteral("RECORDED"));
        return;
    }

    case IncomingMessageKind::EvAlert:
        if (value == QStringLiteral("NON_EV")) {
            applyEvSlotUpdate(slotId, SlotState::NonEvAlert,
                              QStringLiteral("UNKNOWN"), false,
                              QStringLiteral("00:00"), QStringLiteral("NON_EV_ALERT"));
            recordEvent(slotId, QStringLiteral("NON_EV_ALERT"),
                        QStringLiteral("Non-EV alert received"), QStringLiteral("OPEN"));
            return;
        }
        if (value == QStringLiteral("OVERTIME")) {
            applyEvSlotUpdate(slotId, SlotState::OvertimeAlert,
                              QStringLiteral("UNKNOWN"), true,
                              QStringLiteral("02:00+"), QStringLiteral("OVERTIME_ALERT"));
            recordEvent(slotId, QStringLiteral("OVERTIME_ALERT"),
                        QStringLiteral("Overtime alert received"), QStringLiteral("OPEN"));
            return;
        }
        if (value == QStringLiteral("ACKED")) {
            applyEvSlotUpdate(slotId, SlotState::Acked,
                              QStringLiteral("UNKNOWN"), true,
                              QStringLiteral("00:00"), QStringLiteral("ACKED"));
            recordEvent(slotId, QStringLiteral("ALARM_ACK"),
                        QStringLiteral("Alarm acknowledged from RX message"),
                        QStringLiteral("ACKED"));
            return;
        }
        if (value == QStringLiteral("CLEAR")) {
            EvSlotInfo &slot = m_state.evSlots[slotId];
            slot.visual.alarm = SlotAlarmKind::None;
            slot.visual.alarmAcknowledged = false;
            slot.state = slot.visual.occupancy == SlotOccupancy::Occupied
                ? SlotState::Occupied : SlotState::Vacant;
            slot.alarmText = QStringLiteral("NORMAL");
            notifyStateChanged();
            recordEvent(slotId, QStringLiteral("ALARM_CLEAR"), QStringLiteral("Alarm cleared from RX message"), QStringLiteral("CLEARED"));
            return;
        }
        break;

    case IncomingMessageKind::FireAlarm:
        if (value == QStringLiteral("CLEAR") || value == QStringLiteral("ACKED")) {
            recordEvent(slotId, QStringLiteral("FIRE_ALARM_ACK"),
                        QStringLiteral("Fire alarm acknowledged from RX message"),
                        QStringLiteral("ACKED"));
            return;
        }
        recordEvent(slotId, QStringLiteral("FIRE_ALARM"),
                    QStringLiteral("Fire detected on ") + slotId,
                    QStringLiteral("OPEN"));
        return;

    case IncomingMessageKind::Unsupported:
        break;

    case IncomingMessageKind::Invalid:
        return;
    }

    recordEvent(slotId, QStringLiteral("RX_UNSUPPORTED"),
                QStringLiteral("Unsupported message: ") + parsed.raw,
                QStringLiteral("REJECTED"));
}

SlotState ParkingController::slotState(const QString &slotId) const
{
    if (m_state.evSlots.contains(slotId)) return m_state.evSlots.value(slotId).state;
    return m_state.parkingSlots.value(slotId).state;
}

QString ParkingController::plateNumber(const QString &slotId) const
{
    if (m_state.evSlots.contains(slotId)) return m_state.evSlots.value(slotId).plateNumber;
    return m_state.slotPlateNumbers.value(slotId, QStringLiteral("-"));
}

QList<ParkingImageResource> ParkingController::images(const QString &slotId) const
{
    return m_state.slotImages.value(slotId);
}

QUrl ParkingController::resolveApiUrl(const QUrl &url) const
{
    if (url.isEmpty() || !url.isRelative()) return url;
    QString base = m_apiBaseUrl.toString(); while (base.endsWith(QLatin1Char('/'))) base.chop(1);
    QString path = url.toString(); if (!path.startsWith(QLatin1Char('/'))) path.prepend(QLatin1Char('/'));
    return QUrl(base + path);
}

QString ParkingController::occupiedDurationText(const QDateTime &occupiedSince, int elapsedSeconds) const
{
    qint64 seconds = elapsedSeconds;
    if (seconds <= 0 && occupiedSince.isValid()) seconds = qMax<qint64>(0, occupiedSince.secsTo(QDateTime::currentDateTime()));
    if (seconds <= 0) return QStringLiteral("-");
    return QStringLiteral("%1:%2:%3").arg(seconds / 3600, 2, 10, QLatin1Char('0')).arg((seconds % 3600) / 60, 2, 10, QLatin1Char('0')).arg(seconds % 60, 2, 10, QLatin1Char('0'));
}
