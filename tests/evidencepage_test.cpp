#include "pages/evidencepage.h"
#include "api/imageloader.h"
#include "pages/eventspage.h"

#include <QApplication>
#include <QBuffer>
#include <QDialog>
#include <QEventLoop>
#include <QFrame>
#include <QGroupBox>
#include <QImage>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QStringList>
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
    QFrame *filterPanel = page.findChild<QFrame *>(
        QStringLiteral("evidenceFilterPanel"));
    QFrame *summaryFrame = page.findChild<QFrame *>(
        QStringLiteral("evidenceSummaryFrame"));
    QWidget *leftSidebar = page.findChild<QWidget *>(
        QStringLiteral("evidenceLeftSidebar"));
    QWidget *content = page.findChild<QWidget *>(
        QStringLiteral("evidenceContent"));
    if (!filterPanel || filterPanel->width() != 245
        || !summaryFrame || !leftSidebar || leftSidebar->width() != 245
        || !content || summaryFrame->parentWidget() != leftSidebar
        || filterPanel->parentWidget() != leftSidebar
        || content->parentWidget() == leftSidebar) return 41;
    QTimer *autoRefreshTimer = page.findChild<QTimer *>(
        QStringLiteral("evidenceAutoRefreshTimer"));
    QLabel *statusLabel = page.findChild<QLabel *>(
        QStringLiteral("evidenceStatusLabel"));
    if (!autoRefreshTimer || autoRefreshTimer->interval() != 5000
        || autoRefreshTimer->isSingleShot() || !statusLabel) return 39;

    QStringList requestedSlots;
    QObject::connect(&page, &EvidencePage::slotEvidenceRequested,
                     [&](const QString &slotId) {
        requestedSlots.append(slotId);
    });
    page.requestCurrentEvidence();
    if (requestedSlots.size() != 2
        || !requestedSlots.contains(QStringLiteral("EV-02"))
        || !requestedSlots.contains(QStringLiteral("P-01"))) return 3;
    if (!page.selectSlot(QStringLiteral("P01"))
        || page.currentSlotId() != QStringLiteral("P-01")
        || requestedSlots.size() != 2) return 13;
    if (!page.selectSlot(QStringLiteral("EV02"))
        || page.currentSlotId() != QStringLiteral("EV-02")
        || requestedSlots.size() != 2) return 14;
    page.requestCurrentEvidence();
    if (requestedSlots.size() != 3
        || requestedSlots.last() != QStringLiteral("EV-02")) return 15;
    requestedSlots.clear();
    if (!QMetaObject::invokeMethod(autoRefreshTimer, "timeout",
                                   Qt::DirectConnection)
        || requestedSlots.size() != 1
        || requestedSlots.constFirst() != QStringLiteral("EV-02")) return 40;

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
    firstOriginal.sessionId = 7;
    firstOriginal.processing = QStringLiteral("ORIGINAL");
    firstOriginal.url = QUrl(imageBaseUrl + QStringLiteral("10/original"));
    firstOriginal.timestamp = QDateTime::fromString(
        QStringLiteral("2026-07-27T09:00:00+09:00"), Qt::ISODate);
    ParkingImageResource latestOriginal;
    latestOriginal.imageId = 20;
    latestOriginal.sessionId = 7;
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
    QGroupBox *timelineGroup = page.findChild<QGroupBox *>(
        QStringLiteral("evidenceTimelineGroup"));
    if (!timelineGroup || !timelineGroup->isCheckable()
        || !timelineGroup->isChecked() || table->isHidden()) return 45;
    timelineGroup->setChecked(false);
    QApplication::processEvents();
    if (!table->isHidden() || timelineGroup->maximumHeight() != 38
        || !timelineGroup->title().contains(QStringLiteral("▶"))) return 46;
    timelineGroup->setChecked(true);
    QApplication::processEvents();
    if (table->isHidden() || timelineGroup->maximumHeight() != QWIDGETSIZE_MAX
        || !timelineGroup->title().contains(QStringLiteral("▼"))) return 47;
    QLabel *summary = page.findChild<QLabel *>(QStringLiteral("evidenceSummaryLabel"));
    if (!summary || !summary->text().contains(QStringLiteral("local evidence timeline"))) return 6;
    QLabel *plateMetric = page.findChild<QLabel *>(QStringLiteral("evidencePlateMetric"));
    QLabel *sessionMetric = page.findChild<QLabel *>(QStringLiteral("evidenceSessionMetric"));
    QLabel *captureMetric = page.findChild<QLabel *>(QStringLiteral("evidenceCaptureMetric"));
    if (!plateMetric || !plateMetric->text().contains(QStringLiteral("34B7788"))) return 27;
    if (!sessionMetric || sessionMetric->text() != QStringLiteral("Session 7")) return 36;
    if (!captureMetric || !captureMetric->text().contains(QStringLiteral("2 image groups"))) return 28;
    QLabel *firstTitle = page.findChild<QLabel *>(QStringLiteral("evidenceFirstTitle"));
    QLabel *selectedTitle = page.findChild<QLabel *>(QStringLiteral("evidenceSelectedTitle"));
    QLabel *firstImage = page.findChild<QLabel *>(QStringLiteral("evidenceFirstImage"));
    QLabel *selectedImage = page.findChild<QLabel *>(QStringLiteral("evidenceSelectedImage"));
    QLabel *firstMetadata = page.findChild<QLabel *>(
        QStringLiteral("evidenceFirstMetadata"));
    QLabel *selectedMetadata = page.findChild<QLabel *>(
        QStringLiteral("evidenceSelectedMetadata"));
    if (!firstTitle || firstTitle->text()
            != QStringLiteral("First capture · EV-02 · Session 7")) return 7;
    if (!selectedTitle || selectedTitle->text()
            != QStringLiteral("Latest capture · EV-02 · Session 7")) return 8;
    if (!firstMetadata || !firstMetadata->text().contains(
            QStringLiteral("2026-07-27 09:00:00"))
        || !selectedMetadata || !selectedMetadata->text().contains(
            QStringLiteral("2026-07-27 10:00:01"))) return 42;
    if (!firstImage || !selectedImage || firstImage->minimumHeight() != 320
        || selectedImage->minimumHeight() != 320
        || firstImage->maximumHeight() != 320
        || selectedImage->maximumHeight() != 320) return 35;
    QTimer::singleShot(3000, &imageLoadLoop, &QEventLoop::quit);
    imageLoadLoop.exec();
    if (loadedImageCount != 2) return 11;
    QPushButton *firstOpen = page.findChild<QPushButton *>(
        QStringLiteral("evidenceFirstOpenButton"));
    QPushButton *selectedOpen = page.findChild<QPushButton *>(
        QStringLiteral("evidenceSelectedOpenButton"));
    if (!firstOpen || !firstOpen->isEnabled()
        || !selectedOpen || !selectedOpen->isEnabled()) return 12;

    requestedSlots.clear();
    const int imageLoadCountBeforeAutoRefresh = loadedImageCount;
    const QString statusBeforeAutoRefresh = statusLabel->text();
    const qint64 firstPixmapBeforeAutoRefresh = firstImage->pixmap().cacheKey();
    const qint64 latestPixmapBeforeAutoRefresh = selectedImage->pixmap().cacheKey();
    if (!QMetaObject::invokeMethod(autoRefreshTimer, "timeout",
                                   Qt::DirectConnection)
        || requestedSlots.size() != 1
        || requestedSlots.constFirst() != QStringLiteral("EV-02")
        || statusLabel->text() != statusBeforeAutoRefresh
        || loadedImageCount != imageLoadCountBeforeAutoRefresh
        || firstImage->pixmap().cacheKey() != firstPixmapBeforeAutoRefresh
        || selectedImage->pixmap().cacheKey() != latestPixmapBeforeAutoRefresh) return 43;

    page.showEvidence(QStringLiteral("EV-02"), SlotState::OvertimeAlert,
                      QStringLiteral("34B7788"),
                      {firstOriginal, latestOriginal, latestEnhanced});
    QApplication::processEvents();
    if (loadedImageCount != imageLoadCountBeforeAutoRefresh
        || firstImage->pixmap().cacheKey() != firstPixmapBeforeAutoRefresh
        || selectedImage->pixmap().cacheKey() != latestPixmapBeforeAutoRefresh) return 44;

    const int imageLoadCountBeforeSelection = loadedImageCount;
    table->setCurrentCell(1, 0);
    QApplication::processEvents();
    if (loadedImageCount != imageLoadCountBeforeSelection
        || firstImage->pixmap().isNull()
        || selectedImage->pixmap().isNull()) return 38;

    ParkingImageResource previousSessionOriginal = firstOriginal;
    previousSessionOriginal.imageId = 5;
    previousSessionOriginal.sessionId = 6;
    previousSessionOriginal.timestamp = QDateTime::fromString(
        QStringLiteral("2026-07-27T08:00:00+09:00"), Qt::ISODate);
    previousSessionOriginal.url = QUrl(
        imageBaseUrl + QStringLiteral("5/original"));
    ParkingImageResource newestOriginal = latestOriginal;
    newestOriginal.imageId = 30;
    newestOriginal.timestamp = QDateTime::fromString(
        QStringLiteral("2026-07-27T10:05:01+09:00"), Qt::ISODate);
    newestOriginal.url = QUrl(imageBaseUrl + QStringLiteral("30/original"));
    state.slotImages.insert(QStringLiteral("EV-02"),
                            {previousSessionOriginal, firstOriginal,
                             latestOriginal, newestOriginal});
    page.render(state);
    if (page.captureCount() != 4
        || sessionMetric->text() != QStringLiteral("Session 7")
        || !firstMetadata->text().contains(QStringLiteral("2026-07-27 09:00:00"))
        || !selectedMetadata->text().contains(QStringLiteral("2026-07-27 10:05:01"))) return 26;

    ParkingViewState imageOmittingSnapshot = state;
    imageOmittingSnapshot.slotImages.clear();
    page.render(imageOmittingSnapshot);
    if (page.captureCount() != 4) return 37;

    ParkingImageResource generalCapture = firstOriginal;
    generalCapture.imageId = 40;
    generalCapture.sessionId = 8;
    generalCapture.timestamp = QDateTime::fromString(
        QStringLiteral("2026-07-27T10:07:01+09:00"), Qt::ISODate);
    state.slotImages.insert(QStringLiteral("P-01"), {generalCapture});
    slotFilter->setCurrentIndex(0);
    page.showLocalEvidenceSnapshot(state);
    if (page.captureCount() != 5
        || sessionMetric->text() != QStringLiteral("Session 8")
        || !firstMetadata->text().contains(QStringLiteral("2026-07-27 10:07:01"))
        || !selectedMetadata->text().contains(QStringLiteral("2026-07-27 10:07:01"))) return 29;
    if (!table || table->columnCount() != 6
        || table->rowCount() != 5
        || !table->item(0, 1)
        || table->item(0, 1)->text() != QStringLiteral("P-01")) return 30;
    QLineEdit *plateFilter = page.findChild<QLineEdit *>(
        QStringLiteral("evidencePlateFilter"));
    QLineEdit *reasonFilter = page.findChild<QLineEdit *>(
        QStringLiteral("evidenceReasonFilter"));
    if (!plateFilter || !reasonFilter) return 31;
    plateFilter->setText(QStringLiteral("34B7788"));
    if (page.captureCount() != 4) return 32;
    reasonFilter->setText(QStringLiteral("overstay"));
    if (page.captureCount() != 2
        || !firstMetadata->text().contains(QStringLiteral("2026-07-27 09:00:00"))
        || !selectedMetadata->text().contains(QStringLiteral("2026-07-27 10:05:01"))) return 33;
    plateFilter->clear();
    reasonFilter->clear();
    if (page.captureCount() != 5) return 34;

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
    ParkingImageResource eventFirstOriginal = firstOriginal;
    eventFirstOriginal.sessionId = -1;
    ParkingImageResource eventLatestOriginal = latestOriginal;
    eventLatestOriginal.sessionId = -1;
    ParkingImageResource eventLatestEnhanced = latestEnhanced;
    eventLatestEnhanced.sessionId = -1;
    page.showEventEvidence(
        QStringLiteral("session-8-overstay"), QStringLiteral("EV-02"), 8,
        SlotState::OvertimeAlert, QStringLiteral("34B7788"),
        {eventFirstOriginal, eventLatestOriginal, eventLatestEnhanced});
    if (page.captureCount() != 2
        || !summary->text().contains(QStringLiteral("local evidence timeline"))
        || sessionMetric->text() != QStringLiteral("Session 8")
        || firstTitle->text()
            != QStringLiteral("First capture · EV-02 · Session 8")
        || selectedTitle->text()
            != QStringLiteral("Latest capture · EV-02 · Session 8")
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
