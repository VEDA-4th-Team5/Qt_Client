#ifndef MQTTSERVICECLIENT_H
#define MQTTSERVICECLIENT_H

#include <QObject>
#include <QString>
#include <QStringList>

class QTimer;

#ifdef SMART_PARKING_HAS_QT_MQTT
class QMqttClient;
#endif

struct MqttSettings {
    bool enabled = false;
    QString host;
    quint16 port = 1883;
    QStringList topics;
    QString clientId;
    int reconnectIntervalMs = 5000;
};

// Service 계층의 MQTT 수신 담당이다. Pi 가 발행하는 실시간 알림을 구독만 하고,
// 페이로드 해석과 화면 상태 반영은 상위(Controller) 책임이다.
//
// Qt Mqtt 모듈이 설치되지 않은 환경에서도 빌드가 깨지지 않도록
// SMART_PARKING_HAS_QT_MQTT 정의 여부로 갈라진다. 모듈이 없으면 이 클래스는
// 아무것도 하지 않고 isAvailable() 이 false 를 돌려준다.
class MqttServiceClient : public QObject
{
    Q_OBJECT

public:
    explicit MqttServiceClient(const MqttSettings &settings,
                               QObject *parent = nullptr);

    // Qt Mqtt 모듈과 함께 빌드되었는지. 설정의 enabled 와는 별개다.
    static bool isAvailable();

    void start();
    void stop();

signals:
    void messageReceived(const QString &topic, const QByteArray &payload);
    void connectionChanged(const QString &status, bool connected);

private:
    void scheduleReconnect();

    MqttSettings m_settings;
    QTimer *m_reconnectTimer = nullptr;

#ifdef SMART_PARKING_HAS_QT_MQTT
    void subscribeAll();
    QMqttClient *m_client = nullptr;
#endif
};

#endif
