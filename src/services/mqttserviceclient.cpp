#include "mqttserviceclient.h"

#include <QTimer>

#ifdef SMART_PARKING_HAS_QT_MQTT
#include <QMqttClient>
#include <QMqttSubscription>
#endif

MqttServiceClient::MqttServiceClient(const MqttSettings &settings,
                                     QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_reconnectTimer(new QTimer(this))
{
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &MqttServiceClient::start);
}

bool MqttServiceClient::isAvailable()
{
#ifdef SMART_PARKING_HAS_QT_MQTT
    return true;
#else
    return false;
#endif
}

#ifdef SMART_PARKING_HAS_QT_MQTT

void MqttServiceClient::start()
{
    if (!m_settings.enabled) {
        emit connectionChanged(QStringLiteral("MQTT disabled"), false);
        return;
    }
    if (m_settings.host.isEmpty()) {
        emit connectionChanged(QStringLiteral("MQTT host is not configured"), false);
        return;
    }

    if (!m_client) {
        m_client = new QMqttClient(this);

        connect(m_client, &QMqttClient::connected, this, [this]() {
            emit connectionChanged(
                QStringLiteral("MQTT connected: %1:%2")
                    .arg(m_settings.host)
                    .arg(m_settings.port),
                true);
            subscribeAll();
        });

        connect(m_client, &QMqttClient::disconnected, this, [this]() {
            emit connectionChanged(QStringLiteral("MQTT disconnected"), false);
            scheduleReconnect();
        });

        connect(m_client, &QMqttClient::errorChanged, this,
                [this](QMqttClient::ClientError error) {
                    if (error == QMqttClient::NoError) return;
                    emit connectionChanged(
                        QStringLiteral("MQTT error: %1").arg(int(error)), false);
                    scheduleReconnect();
                });

        // topic 도 함께 넘긴다. 어느 주차면의 알림인지가 토픽에 들어있다.
        connect(m_client, &QMqttClient::messageReceived, this,
                [this](const QByteArray &payload, const QMqttTopicName &topic) {
                    emit messageReceived(topic.name(), payload);
                });
    }

    if (m_client->state() != QMqttClient::Disconnected) {
        return;
    }

    m_client->setHostname(m_settings.host);
    m_client->setPort(m_settings.port);
    if (!m_settings.clientId.isEmpty()) {
        m_client->setClientId(m_settings.clientId);
    }

    emit connectionChanged(
        QStringLiteral("MQTT connecting: %1:%2")
            .arg(m_settings.host)
            .arg(m_settings.port),
        false);
    m_client->connectToHost();
}

void MqttServiceClient::subscribeAll()
{
    if (!m_client) return;

    for (const QString &topic : m_settings.topics) {
        const QString trimmed = topic.trimmed();
        if (trimmed.isEmpty()) continue;

        // QoS 1: 화재 알림을 브로커 재연결 구간에서 조용히 잃지 않게 한다.
        if (!m_client->subscribe(QMqttTopicFilter(trimmed), 1)) {
            emit connectionChanged(
                QStringLiteral("MQTT subscribe failed: %1").arg(trimmed), true);
            continue;
        }
        emit connectionChanged(
            QStringLiteral("MQTT subscribed: %1").arg(trimmed), true);
    }
}

void MqttServiceClient::stop()
{
    if (m_reconnectTimer) m_reconnectTimer->stop();
    if (m_client && m_client->state() != QMqttClient::Disconnected) {
        m_client->disconnectFromHost();
    }
}

#else  // Qt Mqtt 모듈 없이 빌드된 경우

void MqttServiceClient::start()
{
    if (!m_settings.enabled) return;
    emit connectionChanged(
        QStringLiteral("MQTT unavailable: build without the Qt Mqtt module"),
        false);
}

void MqttServiceClient::stop()
{
    if (m_reconnectTimer) m_reconnectTimer->stop();
}

#endif

void MqttServiceClient::scheduleReconnect()
{
    if (!m_settings.enabled || !m_reconnectTimer) return;
    if (m_reconnectTimer->isActive()) return;
    m_reconnectTimer->start(qMax(1000, m_settings.reconnectIntervalMs));
}
