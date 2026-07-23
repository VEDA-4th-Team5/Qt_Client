#include "diagnostics/diagnosticsservice.h"

#include <QCoreApplication>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    DiagnosticsService diagnostics;

    if (DiagnosticsService::sanitizedUrl(
            QStringLiteral("https://user:secret@example.com:8443/api?token=abc"))
        != QStringLiteral("https://example.com:8443/api")) return 1;

    diagnostics.markMockStateLoaded(32);
    if (diagnostics.parkingState().dataSource != QStringLiteral("MOCK")) return 2;
    if (diagnostics.parkingState().slotCount != 32) return 3;

    ApiDiagnosticState api;
    api.enabled = true;
    api.connected = true;
    api.status = QStringLiteral("CONNECTED");
    api.endpoint = QStringLiteral("http://user:password@192.0.2.5:8080");
    api.lastSuccessAt = QDateTime::currentDateTime().addSecs(1);
    api.lastLatencyMs = 42;
    api.lastHttpStatus = 200;
    api.appliedSlotCount = 4;
    diagnostics.setApiState(api);
    if (diagnostics.parkingState().dataSource != QStringLiteral("SERVER")) return 4;
    if (diagnostics.apiState().endpoint.contains(QStringLiteral("password"))) return 5;
    if (diagnostics.parkingState().lastServerSlotCount != 4) return 6;

    diagnostics.markSimulationApplied(QStringLiteral("Non-EV violation"));
    if (diagnostics.parkingState().dataSource != QStringLiteral("MIXED")) return 7;

    QList<RtspChannelDiagnostic> channels;
    channels.append({QStringLiteral("CH1"), true, QStringLiteral("Playing"),
                     QString(), QSize(2592, 1520), 320, QDateTime::currentMSecsSinceEpoch()});
    diagnostics.setRtspChannels(channels);
    if (diagnostics.rtspChannels().size() != 1) return 8;
    if (diagnostics.logs().isEmpty()) return 9;

    for (int i = 0; i < 1100; ++i) {
        diagnostics.ingestDomainEvent(
            QString(), QStringLiteral("SYSTEM"), QStringLiteral("TEST_EVENT"),
            QString::number(i), QStringLiteral("DONE"));
    }
    if (diagnostics.logs().size() != 1000) return 10;
    return 0;
}
