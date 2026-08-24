#ifndef EVIDENCEPAGE_H
#define EVIDENCEPAGE_H

#include "api/parkingmodels.h"
#include "models/parkingcapturegroup.h"
#include "models/parkingstate.h"

#include <QList>
#include <QHash>
#include <QPixmap>
#include <QPointer>
#include <QVector>
#include <QWidget>

class EvidenceImageLabel;
class ImageLoader;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QTableWidget;

class EvidencePage : public QWidget
{
    Q_OBJECT

public:
    explicit EvidencePage(QWidget *parent = nullptr);

    void setImageLoader(ImageLoader *imageLoader);
    void render(const ParkingViewState &state);
    // v0.1 client-side fallback: combine only evidence already delivered by
    // the existing slot/session APIs; no new history endpoint is assumed.
    void showLocalEvidenceSnapshot(const ParkingViewState &state);
    void showEvidence(const QString &slotId, SlotState state,
                      const QString &plateNumber,
                      const QList<ParkingImageResource> &images);
    void showLoading(const QString &slotId);
    void showError(const QString &slotId, const QString &message);
    void openEvent(const QString &eventId, const QString &slotId);
    void showEventEvidence(const QString &eventId, const QString &slotId,
                           qint64 sessionId, SlotState state,
                           const QString &plateNumber,
                           const QList<ParkingImageResource> &images);
    void showEventError(const QString &eventId, const QString &slotId,
                        const QString &message);
    bool selectSlot(const QString &slotId);
    QString currentSlotId() const;
    QString currentEventId() const;
    int captureCount() const;

public slots:
    void requestCurrentEvidence();

signals:
    void slotEvidenceRequested(const QString &slotId);
    void eventEvidenceRequested(const QString &eventId);

private:
    void handleSlotChanged(QListWidgetItem *current);
    void filterSlots(const QString &text);
    void renderCaptureTable();
    void renderFirstCapture();
    void renderSelectedCapture(int row);
    void updateSlotCaptureCount(const QString &slotId, int captureCount);
    void updateSummaryMetrics(const QString &slotId,
                              SlotState state,
                              const QString &plateNumber,
                              qint64 sessionId,
                              int captureCount,
                              const QString &eventId);
    void renderCaptureCard(const ParkingCaptureGroup *capture,
                           const QString &heading,
                           EvidenceImageLabel *imageLabel,
                           QLabel *titleLabel,
                           QLabel *metadataLabel,
                           QPushButton *openButton,
                           const QString &requestRole);
    void clearCaptureCard(EvidenceImageLabel *imageLabel,
                          QLabel *titleLabel,
                          QLabel *metadataLabel,
                          QPushButton *openButton,
                          const QString &title,
                          const QString &message);
    void showFullImage(EvidenceImageLabel *source, const QString &title);

    struct LocalTimelineEntry {
        QString slotId;
        QString plateNumber;
        SlotState state = SlotState::Vacant;
        ParkingCaptureGroup capture;
    };

    SlotState stateForSlot(const ParkingViewState &state,
                           const QString &slotId) const;
    QString plateForSlot(const ParkingViewState &state,
                         const QString &slotId) const;
    void resetLocalTimeline();

    QPointer<ImageLoader> m_imageLoader;
    QListWidget *m_slotList = nullptr;
    QLineEdit *m_slotSearch = nullptr;
    QLabel *m_summaryLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_slotMetricLabel = nullptr;
    QLabel *m_plateMetricLabel = nullptr;
    QLabel *m_sessionMetricLabel = nullptr;
    QLabel *m_captureMetricLabel = nullptr;
    EvidenceImageLabel *m_firstImageLabel = nullptr;
    EvidenceImageLabel *m_selectedImageLabel = nullptr;
    QLabel *m_firstTitleLabel = nullptr;
    QLabel *m_selectedTitleLabel = nullptr;
    QLabel *m_firstMetadataLabel = nullptr;
    QLabel *m_selectedMetadataLabel = nullptr;
    QPushButton *m_firstOpenButton = nullptr;
    QPushButton *m_selectedOpenButton = nullptr;
    QTableWidget *m_captureTable = nullptr;
    QString m_currentSlotId;
    QString m_currentEventId;
    qint64 m_currentSessionId = -1;
    QString m_plateNumber;
    QVector<ParkingCaptureGroup> m_captures;
    QVector<LocalTimelineEntry> m_localTimelineEntries;
    ParkingViewState m_latestState;
    bool m_localTimelineMode = false;
    QHash<QString, int> m_slotCaptureCounts;
    QHash<QString, EvidenceImageLabel *> m_requestTargets;
    quint64 m_requestGeneration = 0;
    quint64 m_requestSequence = 0;
};

#endif
