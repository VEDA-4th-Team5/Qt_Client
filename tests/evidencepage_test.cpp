#include "pages/evidencepage.h"
#include "api/imageloader.h"
#include "pages/eventspage.h"

#include <QApplication>
#include <QBuffer>
#include <QDialog>
#include <QEventLoop>
#include <QImage>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    EvidencePage page;

    ParkingViewState state;
    EvSlotInfo evSlot;
    evSlot.slotId = QStringLiteral("EV-02");
    evSlot.state = SlotState::OvertimeAlert;
    evSlot.plateNumber = QStringLiteral("34B7788");
    state.evSlots.insert(evSlot.slotId, evSlot);
    ParkingSlotInfo generalSlot;
    generalSlot.slotId = QStringLiteral("P-01");
    generalSlot.state = SlotState::Vacant;
    state.parkingSlots.insert(generalSlot.slotId, generalSlot);
    page.render(state);

    QComboBox *slotFilter = page.findChild<QComboBox *>(
        QStringLiteral("evidenceSlotFilter"));
    if (!slotFilter || slotFilter->count() != 3) return 1;
    if (!page.currentSlotId().isEmpty()) return 2;

    int requestCount = 0;
    QString requestedSlot;
    QObject::connect(&page, &EvidencePage::slotEvidenceRequested,
                     [&](const QString &slotId) {
        ++requestCount;
        requestedSlot = slotId;
    });
    page.requestCurrentEvidence();
    if (requestCount != 0 || !requestedSlot.isEmpty()) return 3;
    if (!page.selectSlot(QStringLiteral("P01"))
        || page.currentSlotId() != QStringLiteral("P-01")
        || requestCount != 0) return 13;
    if (!page.selectSlot(QStringLiteral("EV02"))
        || page.currentSlotId() != QStringLiteral("EV-02")
        || requestCount != 0) return 14;
    if (!page.selectSlot(QStringLiteral("EV-02"))
        || requestCount != 0) return 15;

    QImage sampleImage(8, 8, QImage::Format_RGB32);
    sampleImage.fill(QColor(QStringLiteral("#1976d2")));
    QByteArray pngBytes;
    QBuffer imageBuffer(&pngBytes);
    if (!imageBuffer.open(QIODevice::WriteOnly)
        || !sampleImage.save(&imageBuffer, "PNG")) return 9;

    QTcpServer imageServer;
    if (!imageServer.listen(QHostAddress::LocalHost, 0)) return 10;
    QObject::connect(&imageServer, &QTcpServer::newConnection, [&]() {
        while (QTcpSocket *socket = imageServer.nextPendingConnection()) {
            QObject::connect(socket, &QTcpSocket::readyRead, socket,
                             [socket, pngBytes]() {
                socket->readAll();
                const QByteArray header =
                    "HTTP/1.1 200 OK\r\nContent-Type: image/png\r\nContent-Length: "
                    + QByteArray::number(pngBytes.size())
                    + "\r\nConnection: close\r\n\r\n";
                socket->write(header);
                socket->write(pngBytes);
                socket->disconnectFromHost();
            });
        }
    });
    ImageLoader imageLoader(1500, true);
    page.setImageLoader(&imageLoader);
    const QString imageBaseUrl = QStringLiteral("http://127.0.0.1:%1/images/")
        .arg(imageServer.serverPort());

    ParkingImageResource firstOriginal;
    firstOriginal.imageId = 10;
    firstOriginal.processing = QStringLiteral("ORIGINAL");
    firstOriginal.url = QUrl(imageBaseUrl + QStringLiteral("10/original"));
    firstOriginal.timestamp = QDateTime::fromString(
        QStringLiteral("2026-07-27T09:00:00+09:00"), Qt::ISODate);
    ParkingImageResource latestOriginal;
    latestOriginal.imageId = 20;
    latestOriginal.processing = QStringLiteral("ORIGINAL");
    latestOriginal.url = QUrl(imageBaseUrl + QStringLiteral("20/original"));
    latestOriginal.timestamp = QDateTime::fromString(
        QStringLiteral("2026-07-27T10:00:01+09:00"), Qt::ISODate);
    latestOriginal.evidenceReason = QStringLiteral("OVERSTAY_EVIDENCE");
    ParkingImageResource latestEnhanced = latestOriginal;
    latestEnhanced.processing = QStringLiteral("ENHANCED");
    latestEnhanced.url = QUrl(imageBaseUrl + QStringLiteral("20/enhanced"));

    int loadedImageCount = 0;
    QEventLoop imageLoadLoop;
    QObject::connect(&imageLoader, &ImageLoader::imageLoaded,
                     [&](const QString &, const QPixmap &) {
        if (++loadedImageCount >= 2) imageLoadLoop.quit();
    });

    page.showEvidence(QStringLiteral("EV-02"), SlotState::OvertimeAlert,
                      QStringLiteral("34B7788"),
                      {firstOriginal, latestOriginal, latestEnhanced});
    if (page.captureCount() != 2) return 4;
    QTableWidget *table = page.findChild<QTableWidget *>(
        QStringLiteral("evidenceCaptureTable"));
    if (!table || table->rowCount() != 2 || table->currentRow() != 0) return 5;
    QLabel *summary = page.findChild<QLabel *>(QStringLiteral("evidenceSummaryLabel"));
    if (!summary || !summary->text().contains(QStringLiteral("local evidence timeline"))) return 6;
    QLabel *plateMetric = page.findChild<QLabel *>(QStringLiteral("evidencePlateMetric"));
    QLabel *captureMetric = page.findChild<QLabel *>(QStringLiteral("evidenceCaptureMetric"));
    if (!plateMetric || !plateMetric->text().contains(QStringLiteral("34B7788"))) return 27;
    if (!captureMetric || !captureMetric->text().contains(QStringLiteral("2 image groups"))) return 28;
    QLabel *firstTitle = page.findChild<QLabel *>(QStringLiteral("evidenceFirstTitle"));
    QLabel *selectedTitle = page.findChild<QLabel *>(QStringLiteral("evidenceSelectedTitle"));
    if (!firstTitle || !firstTitle->text().startsWith(
            QStringLiteral("Earliest loaded capture"))) return 7;
    if (!selectedTitle || selectedTitle->text() != QStringLiteral("Selected loaded capture")) return 8;
    QTimer::singleShot(3000, &imageLoadLoop, &QEventLoop::quit);
    imageLoadLoop.exec();
    if (loadedImageCount != 2) return 11;
    QPushButton *firstOpen = page.findChild<QPushButton *>(
        QStringLiteral("evidenceFirstOpenButton"));
    QPushButton *selectedOpen = page.findChild<QPushButton *>(
        QStringLiteral("evidenceSelectedOpenButton"));
    if (!firstOpen || !firstOpen->isEnabled()
        || !selectedOpen || !selectedOpen->isEnabled()) return 12;

    ParkingImageResource newestOriginal = latestOriginal;
    newestOriginal.imageId = 30;
    newestOriginal.timestamp = QDateTime::fromString(
        QStringLiteral("2026-07-27T10:05:01+09:00"), Qt::ISODate);
    newestOriginal.url = QUrl(imageBaseUrl + QStringLiteral("30/original"));
    state.slotImages.insert(QStringLiteral("EV-02"),
                            {firstOriginal, latestOriginal, newestOriginal});
    page.render(state);
    if (page.captureCount() != 3) return 26;

    ParkingImageResource generalCapture = firstOriginal;
    generalCapture.imageId = 40;
    generalCapture.timestamp = QDateTime::fromString(
        QStringLiteral("2026-07-27T10:07:01+09:00"), Qt::ISODate);
    state.slotImages.insert(QStringLiteral("P-01"), {generalCapture});
    slotFilter->setCurrentIndex(0);
    page.showLocalEvidenceSnapshot(state);
    if (page.captureCount() != 4) return 29;
    if (!table || table->columnCount() != 6
        || table->rowCount() != 4
        || !table->item(0, 1)
        || table->item(0, 1)->text() != QStringLiteral("P-01")) return 30;
    QLineEdit *plateFilter = page.findChild<QLineEdit *>(
        QStringLiteral("evidencePlateFilter"));
    QLineEdit *reasonFilter = page.findChild<QLineEdit *>(
        QStringLiteral("evidenceReasonFilter"));
    if (!plateFilter || !reasonFilter) return 31;
    plateFilter->setText(QStringLiteral("34B7788"));
    if (page.captureCount() != 3) return 32;
    reasonFilter->setText(QStringLiteral("overstay"));
    if (page.captureCount() != 1) return 33;
    plateFilter->clear();
    reasonFilter->clear();
    if (page.captureCount() != 4) return 34;

    int eventRequestCount = 0;
    QString requestedEventId;
    QObject::connect(&page, &EvidencePage::eventEvidenceRequested,
                     [&](const QString &eventId) {
        ++eventRequestCount;
        requestedEventId = eventId;
    });
    page.openEvent(QStringLiteral("session-8-overstay"),
                   QStringLiteral("EV-02"));
    if (page.currentEventId() != QStringLiteral("session-8-overstay")
        || page.currentSlotId() != QStringLiteral("EV-02")) return 23;
    page.requestCurrentEvidence();
    if (eventRequestCount != 1
        || requestedEventId != QStringLiteral("session-8-overstay")) return 24;
    page.showEventEvidence(
        QStringLiteral("session-8-overstay"), QStringLiteral("EV-02"), 8,
        SlotState::OvertimeAlert, QStringLiteral("34B7788"),
        {firstOriginal, latestOriginal, latestEnhanced});
    if (page.captureCount() != 2
        || !summary->text().contains(QStringLiteral("local evidence timeline"))
        || !captureMetric->text().contains(QStringLiteral("2 image groups"))) return 25;

    EventsPage eventsPage;
    MonitoringEvent event;
    event.id = QStringLiteral("session-8-overstay");
    event.sourceId = QStringLiteral("EV-02");
    event.evidenceSlotId = QStringLiteral("EV-02");
    event.parkingSessionId = 8;
    event.eventType = QStringLiteral("OVERTIME_ALERT");
    event.message = QStringLiteral("Parking time exceeded");
    event.status = QStringLiteral("OPEN");
    eventsPage.appendEvent(event);
    QTableWidget *eventTable = eventsPage.findChild<QTableWidget *>(
        QStringLiteral("eventLogTable"));
    if (!eventTable || eventTable->rowCount() != 1) return 16;
    int eventNavigationCount = 0;
    QString navigationEventId;
    QObject::connect(&eventsPage, &EventsPage::eventEvidenceRequested,
                     [&](const QString &eventId) {
        ++eventNavigationCount;
        navigationEventId = eventId;
    });
    if (!QMetaObject::invokeMethod(
            eventTable, "cellDoubleClicked", Qt::DirectConnection,
            Q_ARG(int, 0), Q_ARG(int, 2))) return 17;
    if (eventNavigationCount != 1
        || navigationEventId != QStringLiteral("session-8-overstay")) return 18;

    QPushButton *helpButton = page.findChild<QPushButton *>(
        QStringLiteral("evidenceHelpButton"));
    if (!helpButton || helpButton->text() != QStringLiteral("도움말")
        || helpButton->icon().isNull()) return 19;
    helpButton->click();
    QApplication::processEvents();
    QDialog *helpDialog = page.findChild<QDialog *>(
        QStringLiteral("evidenceHelpDialog"));
    if (!helpDialog || !helpDialog->isModal()) return 20;
    QLabel *helpSteps = helpDialog->findChild<QLabel *>(
        QStringLiteral("evidenceHelpSteps"));
    QLabel *helpNote = helpDialog->findChild<QLabel *>(
        QStringLiteral("evidenceHelpNote"));
    if (!helpSteps || !helpSteps->text().contains(QStringLiteral("시간순 확인"))) return 21;
    if (!helpNote || !helpNote->text().contains(QStringLiteral("서버"))) return 22;
    helpDialog->close();
    QApplication::processEvents();

    return 0;
}
