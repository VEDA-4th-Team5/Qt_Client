#ifndef MQTTSERVICECLIENT_H
#define MQTTSERVICECLIENT_H

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>

class QTcpSocket;
class QTimer;

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
// Qt 의 Mqtt 애드온 모듈은 팀 계정에서 배포되지 않아 설치할 수 없었다.
// 그래서 QTcpSocket 위에 MQTT 3.1.1 최소 구현(CONNECT/SUBSCRIBE/PUBLISH 수신)을
// 직접 얹었다 — 발행·인증·TLS·QoS2 는 이 프로젝트에서 쓰지 않으므로 뺐다.
class MqttServiceClient : public QObject
{
    Q_OBJECT

public:
    explicit MqttServiceClient(const MqttSettings &settings,
                               QObject *parent = nullptr);

    void start();
    void stop();

signals:
    void messageReceived(const QString &topic, const QByteArray &payload);
    void connectionChanged(const QString &status, bool connected);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError();
    void onReadyRead();

private:
    void scheduleReconnect();
    void subscribeAll();
    void startKeepAlive();
    void handlePacket(quint8 fixedHeaderByte, const QByteArray &body);
    void handleConnAck(const QByteArray &body);
    void handlePublish(quint8 fixedHeaderByte, const QByteArray &body);

    MqttSettings m_settings;
    QTimer *m_reconnectTimer = nullptr;
    QTimer *m_keepAliveTimer = nullptr;
    QTcpSocket *m_socket = nullptr;
    QByteArray m_readBuffer;
    quint16 m_nextPacketId = 1;
};

#endif
