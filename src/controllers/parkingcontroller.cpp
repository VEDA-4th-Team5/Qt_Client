#include "parkingcontroller.h"

#include "api/apiclient.h"
#include "api/imageloader.h"
#include "api/parkingresponseparser.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QRandomGenerator>
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

SlotState mockParkingStateFor(int number)
{
    if (number == 1 || number == 3 || number == 5 || number == 8
        || number == 11 || number == 14) {
        return SlotState::Occupied;
    }
    return SlotState::Vacant;
}

SlotVisualState makeSlotVisualState(SlotState state, bool vehicleTypeKnown,
                                    bool isEv, const QString &alarmText = QString())
{
    SlotVisualState visual;
    visual.alarm = slotAlarmKindFromText(alarmText, state);
    visual.alarmAcknowledged = state == SlotState::Acked;

    if (state == SlotState::Vacant) {
        visual.occupancy = SlotOccupancy::Vacant;
        return visual;
    }
    if (state == SlotState::SensorError) {
        visual.occupancy = SlotOccupancy::Unknown;
        return visual;
    }

    visual.occupancy = SlotOccupancy::Occupied;
    if (vehicleTypeKnown) {
        visual.vehicleClass = isEv ? VehicleClass::Electric : VehicleClass::General;
    }
    return visual;
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
    initializeMockData();
    initializeApiClient();
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
    if (!m_state.apiEnabled) {
        if (m_reconnectTimer) m_reconnectTimer->stop();
        emit serverConnectionChanged(QStringLiteral("API disabled"), false);
        notifyStateChanged();
        return;
    }

    m_apiBaseUrl = QUrl(setting(QStringLiteral("api/base_url"), QString()).toString().trimmed());
    m_slotsPath = setting(QStringLiteral("api/slots_path"),
                          QStringLiteral("/api/v1/parking-slots")).toString();
    m_slotDetailPath = setting(QStringLiteral("api/slot_detail_path"),
                               QStringLiteral("/api/v1/parking-slots/{slot_id}")).toString();
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

    m_apiClient = new ApiClient(
        m_apiBaseUrl, m_apiTimeoutMs, m_allowInsecureHttp, this);
    m_imageLoader = new ImageLoader(
        m_apiTimeoutMs, m_allowInsecureHttp, this);

    connect(m_apiClient, &ApiClient::jsonReceived, this,
            [this](const QString &path, const QJsonDocument &document) {
                if (path == m_slotsPath) {
                    m_snapshotRequestInFlight = false;
                    applyParkingSnapshot(document);
                } else {
                    applyParkingSlotDetail(document);
                }
            });
    connect(m_apiClient, &ApiClient::requestFailed, this,
            [this](const QString &path, const QString &message) {
                recordEvent(QStringLiteral("SYSTEM"), QStringLiteral("API_ERROR"),
                            path + QStringLiteral(": ") + message,
                            QStringLiteral("FAILED"));
                if (path == m_slotsPath) {
                    m_snapshotRequestInFlight = false;
                    scheduleReconnect(message);
                } else {
                    emit detailError(message);
                }
            });

    emit serverBaseUrlChanged(m_apiBaseUrl.toString());
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
        QStringLiteral("Connecting to %1...").arg(m_apiBaseUrl.toString());
    emit bannerChanged(message, false);
    emit serverConnectionChanged(QStringLiteral("Connecting..."), false);
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
    localSettings.sync();
    if (localSettings.status() != QSettings::NoError) {
        emit serverConfigurationError(
            QStringLiteral("Failed to save client_config.local.ini."));
        return;
    }

    recordEvent(QStringLiteral("SYSTEM"), QStringLiteral("API_ADDRESS_UPDATED"),
                QStringLiteral("Server API changed to ") + normalized,
                QStringLiteral("DONE"));
    initializeApiClient();
}
void ParkingController::initializeMockData()
{
    m_state.evSlots.clear();
    m_state.parkingSlots.clear();
    m_state.slotImages.clear();
    m_state.slotPlateNumbers.clear();

    for (int number = 1; number <= 16; ++number) {
        const QString slotId = evSlotId(number);
        EvSlotInfo slot{slotId, QStringLiteral("-"), true, QStringLiteral("00:00"),
                        SlotState::Vacant, QStringLiteral("NORMAL")};
        slot.visual = makeSlotVisualState(slot.state, false, slot.isEv, slot.alarmText);
        m_state.evSlots.insert(slotId, slot);
    }
    EvSlotInfo nonEvAlert{QStringLiteral("EV-01"), QStringLiteral("12A3456"), false,
                          QStringLiteral("00:18"), SlotState::NonEvAlert,
                          QStringLiteral("NON_EV_ALERT")};
    nonEvAlert.visual = makeSlotVisualState(nonEvAlert.state, true, nonEvAlert.isEv,
                                             nonEvAlert.alarmText);
    m_state.evSlots[nonEvAlert.slotId] = nonEvAlert;
    EvSlotInfo overtimeAlert{QStringLiteral("EV-02"), QStringLiteral("34B7788"), true,
                             QStringLiteral("03:42"), SlotState::OvertimeAlert,
                             QStringLiteral("OVERTIME_ALERT")};
    overtimeAlert.visual = makeSlotVisualState(overtimeAlert.state, true, overtimeAlert.isEv,
                                                overtimeAlert.alarmText);
    m_state.evSlots[overtimeAlert.slotId] = overtimeAlert;
    for (int number = 1; number <= 16; ++number) {
        const QString slotId = parkingSlotId(number);
        ParkingSlotInfo slot{slotId, mockParkingStateFor(number)};
        slot.visual = makeSlotVisualState(slot.state, slot.state == SlotState::Occupied, false);
        m_state.parkingSlots.insert(slotId, slot);
    }
    notifyStateChanged();
    recordEvent(QStringLiteral("EV-01"), QStringLiteral("NON_EV_ALERT"), QStringLiteral("Non-EV vehicle detected in EV charging slot"), QStringLiteral("OPEN"));
    recordEvent(QStringLiteral("EV-02"), QStringLiteral("OVERTIME_ALERT"), QStringLiteral("EV charging dwell time exceeded"), QStringLiteral("OPEN"));
    recordEvent(QStringLiteral("P-01"), QStringLiteral("OCCUPIED"), QStringLiteral("General parking slot occupied"), QStringLiteral("RECORDED"));
    recordEvent(QStringLiteral("P-02"), QStringLiteral("VACANT"), QStringLiteral("General parking slot changed to vacant"), QStringLiteral("RECORDED"));
    recordEvent(QStringLiteral("P-05"), QStringLiteral("OCCUPIED"), QStringLiteral("General parking slot occupied"), QStringLiteral("RECORDED"));
}

void ParkingController::applyParkingSnapshot(const QJsonDocument &document)
{
    ParkingSnapshot snapshot;
    QString error;
    if (!ParkingResponseParser::parseSnapshot(document, snapshot, error)) {
        emit bannerChanged(QStringLiteral("Invalid parking API response | Mock data displayed"), true);
        recordEvent(QStringLiteral("SYSTEM"), QStringLiteral("API_PARSE_ERROR"), error, QStringLiteral("FAILED"));
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
        SlotVisualState visual = makeSlotVisualState(
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
    recordEvent(QStringLiteral("SYSTEM"), QStringLiteral("API_SYNC"), QStringLiteral("Applied %1 parking slots").arg(appliedCount), QStringLiteral("DONE"));
}

void ParkingController::applyParkingSlotDetail(const QJsonDocument &document)
{
    ParkingSlotSnapshot slot;
    QString error;
    if (!ParkingResponseParser::parseSlotDetail(document, slot, error)) {
        emit detailError(error);
        refreshAlert();
        return;
    }
    const QString slotId = normalizeParkingSlotId(slot.slotId);
    m_state.slotPlateNumbers.insert(slotId, slot.plateNumber);
    QList<ParkingImageResource> images;
    for (ParkingImageResource image : slot.images) { image.url = resolveApiUrl(image.url); images.append(image); }
    if (images.isEmpty()) m_state.slotImages.remove(slotId); else m_state.slotImages.insert(slotId, images);
    notifyStateChanged();
    emit slotDetailReady(slotId);
}

void ParkingController::resetSlotsForSnapshot()
{
    m_state.evSlots.clear(); m_state.parkingSlots.clear(); m_state.slotImages.clear(); m_state.slotPlateNumbers.clear();
    for (int number = 1; number <= 16; ++number) {
        const QString slotId = evSlotId(number);
        EvSlotInfo slot{slotId, QStringLiteral("-"), false, QStringLiteral("-"),
                        SlotState::Vacant, QStringLiteral("NORMAL")};
        slot.visual = makeSlotVisualState(slot.state, false, false, slot.alarmText);
        m_state.evSlots.insert(slotId, slot);
    }
    for (int number = 1; number <= 16; ++number) {
        const QString slotId = parkingSlotId(number);
        ParkingSlotInfo slot{slotId, SlotState::Vacant};
        slot.visual = makeSlotVisualState(slot.state, false, false);
        m_state.parkingSlots.insert(slotId, slot);
    }
}

void ParkingController::requestSlotDetail(const QString &rawSlotId)
{
    const QString slotId = normalizeParkingSlotId(rawSlotId);
    if (!m_apiClient) { emit slotDetailReady(slotId); return; }
    QString compact = slotId; compact.remove(QLatin1Char('-'));
    QString path = m_slotDetailPath; path.replace(QStringLiteral("{slot_id}"), compact);
    emit bannerChanged(QStringLiteral("Loading %1 details...").arg(slotId), false);
    m_apiClient->getJson(path);
}

void ParkingController::updateEvSlotState(const QString &slotId, SlotState state,
                                          const QString &plateNumber, bool isEv,
                                          const QString &occupiedTime, const QString &alarmText)
{
    EvSlotInfo updated{slotId, plateNumber, isEv, occupiedTime, state, alarmText};
    if (state == SlotState::Acked && m_state.evSlots.contains(slotId)) {
        updated.visual = m_state.evSlots.value(slotId).visual;
        updated.visual.alarmAcknowledged = true;
    } else {
        updated.visual = makeSlotVisualState(state, state != SlotState::Vacant,
                                             isEv, alarmText);
    }
    m_state.evSlots[slotId] = updated;
    notifyStateChanged();
}

void ParkingController::updateParkingSlotState(const QString &slotId, SlotState state)
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
        updated.visual = makeSlotVisualState(state, state == SlotState::Occupied, false);
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
    for (const EvSlotInfo &slot : m_state.evSlots) {
        if (slot.visual.alarm != SlotAlarmKind::None && !slot.visual.alarmAcknowledged) {
            alerts << slot.slotId + QLatin1Char(':') + slotAlarmText(slot.visual.alarm);
        }
    }
    for (const ParkingSlotInfo &slot : m_state.parkingSlots) {
        if (slot.visual.alarm != SlotAlarmKind::None && !slot.visual.alarmAcknowledged) {
            alerts << slot.slotId + QLatin1Char(':') + slotAlarmText(slot.visual.alarm);
        }
    }
    emit bannerChanged(alerts.isEmpty() ? QStringLiteral("Monitoring normal | No active alerts")
                                        : QStringLiteral("Active alerts: %1 | %2").arg(alerts.size()).arg(alerts.join(QStringLiteral(" / "))),
                       !alerts.isEmpty());
}

void ParkingController::recordEvent(const QString &zone, const QString &eventType,
                                    const QString &message, const QString &status)
{
    emit eventLogged(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")), zone, eventType, message, status);
}

void ParkingController::clearAlarms()
{
    bool cleared = false;
    for (auto it = m_state.evSlots.begin(); it != m_state.evSlots.end(); ++it) {
        if (it->visual.alarm != SlotAlarmKind::None && !it->visual.alarmAcknowledged) {
            it->state = SlotState::Acked;
            it->visual.alarmAcknowledged = true;
            it->alarmText = QStringLiteral("ACKED");
            cleared = true;
        }
    }
    for (auto it = m_state.parkingSlots.begin(); it != m_state.parkingSlots.end(); ++it) {
        if (it->visual.alarm != SlotAlarmKind::None && !it->visual.alarmAcknowledged) {
            it->state = SlotState::Acked;
            it->visual.alarmAcknowledged = true;
            cleared = true;
        }
    }
    notifyStateChanged();
    recordEvent(QStringLiteral("ALL"), QStringLiteral("ALARM_ACK"), cleared ? QStringLiteral("Active alarms acknowledged") : QStringLiteral("No alarms to clear"), QStringLiteral("ACKED"));
}

void ParkingController::toggleMockEv()
{
    ++m_mockStep;
    const bool occupied = (m_mockStep % 2) == 0;
    updateEvSlotState(QStringLiteral("EV-03"), occupied ? SlotState::Occupied : SlotState::Vacant,
                      occupied ? QStringLiteral("56C9012") : QStringLiteral("-"), true,
                      occupied ? QStringLiteral("00:07") : QStringLiteral("00:00"), QStringLiteral("NORMAL"));
    recordEvent(QStringLiteral("EV-03"), occupied ? QStringLiteral("OCCUPIED") : QStringLiteral("VACANT"), QStringLiteral("Mock EV-03 state changed"), QStringLiteral("RECORDED"));
}

void ParkingController::triggerNonEvAlert() { updateEvSlotState(QStringLiteral("EV-01"), SlotState::NonEvAlert, QStringLiteral("12A3456"), false, QStringLiteral("00:19"), QStringLiteral("NON_EV_ALERT")); recordEvent(QStringLiteral("EV-01"), QStringLiteral("NON_EV_ALERT"), QStringLiteral("Non-EV alert test executed"), QStringLiteral("OPEN")); }
void ParkingController::triggerOvertimeAlert() { updateEvSlotState(QStringLiteral("EV-02"), SlotState::OvertimeAlert, QStringLiteral("34B7788"), true, QStringLiteral("03:43"), QStringLiteral("OVERTIME_ALERT")); recordEvent(QStringLiteral("EV-02"), QStringLiteral("OVERTIME_ALERT"), QStringLiteral("Overtime alert test executed"), QStringLiteral("OPEN")); }
void ParkingController::triggerSensorError() { updateParkingSlotState(QStringLiteral("P-03"), SlotState::SensorError); recordEvent(QStringLiteral("P-03"), QStringLiteral("HALL_SENSOR_ERROR"), QStringLiteral("Hall sensor error test executed"), QStringLiteral("OPEN")); }

void ParkingController::randomizeParkingSlots()
{
    for (int number = 1; number <= 16; ++number) {
        const QString slotId = parkingSlotId(number);
        const bool occupied = QRandomGenerator::global()->bounded(2) == 1;
        updateParkingSlotState(slotId, occupied ? SlotState::Occupied : SlotState::Vacant);
        recordEvent(slotId, occupied ? QStringLiteral("OCCUPIED") : QStringLiteral("VACANT"), QStringLiteral("Parking slot randomized"), QStringLiteral("RECORDED"));
    }
}

void ParkingController::simulateIncomingMessages()
{
    for (const QString &message : {
             QStringLiteral("PARKING_SLOT,P01,OCCUPIED"),
             QStringLiteral("PARKING_SLOT,P02,VACANT"),
             QStringLiteral("EV_ALERT,EV01,NON_EV"),
             QStringLiteral("EV_ALERT,EV02,OVERTIME"),
             QStringLiteral("FIRE_ALARM,CH2,DETECTED"),
             QStringLiteral("HALL_SENSOR,P03,ERROR"),
             QStringLiteral("EVENT,CH1,CAMERA_DISCONNECTED,FAILED,RTSP stream disconnected")
         }) processIncomingMessage(message);
}

void ParkingController::processIncomingMessage(const QString &message)
{
    const QString trimmed = message.trimmed();
    emit statusMessageChanged(QStringLiteral("Last RX: ") + trimmed);
    QStringList parts = trimmed.split(QLatin1Char(','));
    for (QString &part : parts) part = part.trimmed();
    parts.removeAll(QString());
    if (parts.size() < 3) { recordEvent(QStringLiteral("SYSTEM"), QStringLiteral("RX_ERROR"), QStringLiteral("Invalid message format: ") + trimmed, QStringLiteral("REJECTED")); return; }
    const QString type = parts.at(0).toUpper();
    const QString slotId = normalizeParkingSlotId(parts.at(1));
    const QString value = parts.at(2).toUpper();
    if (type == QStringLiteral("EVENT") || type == QStringLiteral("DB_EVENT")) {
        if (parts.size() < 4) {
            recordEvent(QStringLiteral("SYSTEM"), QStringLiteral("RX_ERROR"),
                        QStringLiteral("Invalid normalized event: ") + trimmed,
                        QStringLiteral("REJECTED"));
            return;
        }
        const QString source = normalizeParkingSlotId(parts.at(1));
        const QString normalizedEventType = parts.at(2).toUpper();
        const QString normalizedStatus = parts.at(3).toUpper();
        const QString normalizedMessage = parts.size() > 4
            ? parts.mid(4).join(QStringLiteral(", "))
            : QStringLiteral("Normalized event received");
        recordEvent(source, normalizedEventType, normalizedMessage, normalizedStatus);
        return;
    }
    if (type == QStringLiteral("PARKING_SLOT")) {
        const SlotState state = slotStateFromText(value); updateParkingSlotState(slotId, state);
        recordEvent(slotId, slotStateText(state), QStringLiteral("Parking slot state updated from RX message"), QStringLiteral("RECORDED")); return;
    }
    if (type == QStringLiteral("HALL_SENSOR") || type == QStringLiteral("HALL_SENSOR_EVENT")) {
        if (value == QStringLiteral("ERROR") || value == QStringLiteral("FAILED")) {
            updateParkingSlotState(slotId, SlotState::SensorError);
            recordEvent(slotId, QStringLiteral("HALL_SENSOR_ERROR"),
                        QStringLiteral("Hall sensor error received"),
                        QStringLiteral("OPEN"));
            return;
        }
        if (value == QStringLiteral("ACKED")) {
            updateParkingSlotState(slotId, SlotState::Acked);
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
        updateParkingSlotState(slotId, state);
        recordEvent(slotId, QStringLiteral("HALL_SENSOR_CHANGED"),
                    QStringLiteral("Hall sensor state updated from RX message"),
                    QStringLiteral("RECORDED"));
        return;
    }
    if (type == QStringLiteral("EV_ALERT")) {
        if (value == QStringLiteral("NON_EV")) { updateEvSlotState(slotId, SlotState::NonEvAlert, QStringLiteral("UNKNOWN"), false, QStringLiteral("00:00"), QStringLiteral("NON_EV_ALERT")); recordEvent(slotId, QStringLiteral("NON_EV_ALERT"), QStringLiteral("Non-EV alert received"), QStringLiteral("OPEN")); return; }
        if (value == QStringLiteral("OVERTIME")) { updateEvSlotState(slotId, SlotState::OvertimeAlert, QStringLiteral("UNKNOWN"), true, QStringLiteral("02:00+"), QStringLiteral("OVERTIME_ALERT")); recordEvent(slotId, QStringLiteral("OVERTIME_ALERT"), QStringLiteral("Overtime alert received"), QStringLiteral("OPEN")); return; }
        if (value == QStringLiteral("ACKED")) { updateEvSlotState(slotId, SlotState::Acked, QStringLiteral("UNKNOWN"), true, QStringLiteral("00:00"), QStringLiteral("ACKED")); recordEvent(slotId, QStringLiteral("ALARM_ACK"), QStringLiteral("Alarm acknowledged from RX message"), QStringLiteral("ACKED")); return; }
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
    }
    if (type == QStringLiteral("FIRE_ALARM") || type == QStringLiteral("FIRE_EVENT")) {
        const QString channel = parts.at(1).toUpper();
        if (value == QStringLiteral("CLEAR") || value == QStringLiteral("ACKED")) {
            recordEvent(channel, QStringLiteral("FIRE_ALARM_ACK"),
                        QStringLiteral("Fire alarm acknowledged from RX message"),
                        QStringLiteral("ACKED"));
            return;
        }
        recordEvent(channel, QStringLiteral("FIRE_ALARM"),
                    QStringLiteral("Fire detected on ") + channel,
                    QStringLiteral("OPEN"));
        return;
    }
    recordEvent(slotId, QStringLiteral("RX_UNSUPPORTED"), QStringLiteral("Unsupported message: ") + trimmed, QStringLiteral("REJECTED"));
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
