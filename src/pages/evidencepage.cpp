#include "evidencepage.h"

#include "api/imageloader.h"
#include "widgets/pagehelp.h"

#include <QAbstractItemView>
#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFrame>
#include <QFont>
#include <QGridLayout>
#include <QGroupBox>
#include <QHash>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSignalBlocker>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <algorithm>

class EvidenceImageLabel : public QLabel
{
public:
    explicit EvidenceImageLabel(QWidget *parent = nullptr)
        : QLabel(parent)
    {
        setAlignment(Qt::AlignCenter);
        setMinimumSize(320, 180);
        setFixedHeight(190);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        setText(QStringLiteral("Select a parking slot to load evidence"));
        setStyleSheet(QStringLiteral(
            "QLabel { background:#111820; color:#b0bec5; border:1px solid #455a64; "
            "border-radius:6px; padding:8px; font-weight:700; }"));
    }

    void setSourcePixmap(const QPixmap &pixmap)
    {
        m_sourcePixmap = pixmap;
        updateScaledPixmap();
    }

    const QPixmap &sourcePixmap() const
    {
        return m_sourcePixmap;
    }

    QSize sizeHint() const override
    {
        return QSize(360, 190);
    }

    QSize minimumSizeHint() const override
    {
        return QSize(320, 180);
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QLabel::resizeEvent(event);
        updateScaledPixmap();
    }

private:
    void updateScaledPixmap()
    {
        if (m_sourcePixmap.isNull()) {
            setPixmap(QPixmap());
            return;
        }
        setPixmap(m_sourcePixmap.scaled(
            size() - QSize(16, 16), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    QPixmap m_sourcePixmap;
};

namespace {
QString captureTimeText(const QDateTime &timestamp)
{
    return timestamp.isValid()
        ? timestamp.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
        : QStringLiteral("Time unavailable");
}

QString captureReasonText(const QString &reason)
{
    if (reason.isEmpty() || reason == QStringLiteral("EVIDENCE")) {
        return QStringLiteral("Evidence capture");
    }
    QString text = reason;
    text.replace(QLatin1Char('_'), QLatin1Char(' '));
    return text;
}

int slotNumber(const QString &slotId)
{
    bool ok = false;
    const int number = slotId.section(QLatin1Char('-'), -1).toInt(&ok);
    return ok ? number : 0;
}

QString captureCountText(int imageCount)
{
    if (imageCount < 0) {
        return QStringLiteral("Not loaded");
    }
    return QStringLiteral("%1 image group%2")
        .arg(imageCount)
        .arg(imageCount == 1 ? QString() : QStringLiteral("s"));
}

QString slotStateAccent(SlotState state)
{
    switch (state) {
    case SlotState::Vacant:
        return QStringLiteral("#2e7d32");
    case SlotState::Occupied:
        return QStringLiteral("#ef6c00");
    case SlotState::OvertimeAlert:
    case SlotState::NonEvAlert:
        return QStringLiteral("#c62828");
    case SlotState::SensorError:
        return QStringLiteral("#6a1b9a");
    case SlotState::Acked:
        return QStringLiteral("#1565c0");
    default:
        return QStringLiteral("#546e7a");
    }
}

QString slotStateSurface(SlotState state)
{
    switch (state) {
    case SlotState::Vacant:
        return QStringLiteral("#e8f5e9");
    case SlotState::Occupied:
        return QStringLiteral("#fff3e0");
    case SlotState::OvertimeAlert:
    case SlotState::NonEvAlert:
        return QStringLiteral("#ffebee");
    case SlotState::SensorError:
        return QStringLiteral("#f3e5f5");
    case SlotState::Acked:
        return QStringLiteral("#e3f2fd");
    default:
        return QStringLiteral("#f5f7f9");
    }
}

QString statePillStyle(SlotState state)
{
    return QStringLiteral(
               "QLabel { background:%1; color:%2; border:1px solid %2; "
               "border-radius:6px; padding:5px 8px; font-weight:900; }")
        .arg(slotStateSurface(state), slotStateAccent(state));
}

QFrame *createMetricCard(QWidget *parent,
                         const QString &title,
                         const QString &objectName,
                         QLabel **valueLabel)
{
    auto *card = new QFrame(parent);
    card->setObjectName(objectName + QStringLiteral("Card"));
    card->setStyleSheet(QStringLiteral(
        "QFrame { background:#f8fafb; border:1px solid #cfd8dc; border-radius:6px; }"));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(2);

    auto *titleLabel = new QLabel(title, card);
    titleLabel->setStyleSheet(QStringLiteral(
        "border:none; color:#607d8b; font-size:11px; font-weight:800;"));
    *valueLabel = new QLabel(QStringLiteral("-"), card);
    (*valueLabel)->setObjectName(objectName);
    (*valueLabel)->setMinimumWidth(112);
    (*valueLabel)->setStyleSheet(QStringLiteral(
        "border:none; color:#17212b; font-size:16px; font-weight:900;"));
    (*valueLabel)->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(titleLabel);
    layout->addWidget(*valueLabel);
    return card;
}

} // namespace

EvidencePage::EvidencePage(QWidget *parent)
    : QWidget(parent)
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(12);
    rootLayout->addLayout(createPageHeader(
        this, QStringLiteral("Evidence"),
        QStringLiteral("Operator review for session captures and event evidence")));

    auto *bodyLayout = new QHBoxLayout;
    bodyLayout->setSpacing(12);

    auto *filterPanel = new QFrame(this);
    filterPanel->setObjectName(QStringLiteral("evidenceFilterPanel"));
    filterPanel->setFixedWidth(245);
    filterPanel->setStyleSheet(QStringLiteral(
        "QFrame#evidenceFilterPanel { background:#f8fafb; border:1px solid #c7cdd4; "
        "border-radius:6px; }"));
    auto *filterLayout = new QVBoxLayout(filterPanel);
    filterLayout->setContentsMargins(12, 12, 12, 12);
    filterLayout->setSpacing(8);
    auto *filterTitle = new QLabel(QStringLiteral("Evidence filters"), filterPanel);
    filterTitle->setStyleSheet(QStringLiteral("font-size:16px;font-weight:800;color:#263238;"));
    filterLayout->addWidget(filterTitle);
    auto *filterHint = new QLabel(
        QStringLiteral("Timeline is ordered by capture time. Slots narrow the result; they are not the navigation."),
        filterPanel);
    filterHint->setWordWrap(true);
    filterHint->setStyleSheet(QStringLiteral("color:#546e7a;font-size:12px;"));
    filterLayout->addWidget(filterHint);
    auto *slotLabel = new QLabel(QStringLiteral("Slot"), filterPanel);
    slotLabel->setStyleSheet(QStringLiteral("font-weight:800;color:#455a64;"));
    filterLayout->addWidget(slotLabel);
    m_slotFilter = new QComboBox(filterPanel);
    m_slotFilter->setObjectName(QStringLiteral("evidenceSlotFilter"));
    m_slotFilter->setFixedHeight(32);
    auto *plateLabel = new QLabel(QStringLiteral("Plate"), filterPanel);
    plateLabel->setStyleSheet(QStringLiteral("font-weight:800;color:#455a64;"));
    filterLayout->addWidget(m_slotFilter);
    filterLayout->addWidget(plateLabel);
    m_plateFilter = new QLineEdit(filterPanel);
    m_plateFilter->setObjectName(QStringLiteral("evidencePlateFilter"));
    m_plateFilter->setPlaceholderText(QStringLiteral("Any plate"));
    m_plateFilter->setFixedHeight(32);
    filterLayout->addWidget(m_plateFilter);
    auto *reasonLabel = new QLabel(QStringLiteral("Reason"), filterPanel);
    reasonLabel->setStyleSheet(QStringLiteral("font-weight:800;color:#455a64;"));
    filterLayout->addWidget(reasonLabel);
    m_reasonFilter = new QLineEdit(filterPanel);
    m_reasonFilter->setObjectName(QStringLiteral("evidenceReasonFilter"));
    m_reasonFilter->setPlaceholderText(QStringLiteral("Any reason"));
    m_reasonFilter->setFixedHeight(32);
    filterLayout->addWidget(m_reasonFilter);
    filterLayout->addStretch(1);
    auto *refreshButton = new QPushButton(QStringLiteral("Refresh timeline"), filterPanel);
    refreshButton->setObjectName(QStringLiteral("evidenceRefreshButton"));
    refreshButton->setCursor(Qt::PointingHandCursor);
    refreshButton->setFixedHeight(34);
    refreshButton->setStyleSheet(QStringLiteral(
        "QPushButton { background:#263238; color:white; border:1px solid #455a64; "
        "border-radius:6px; padding:6px 10px; font-weight:800; }"
        "QPushButton:hover { background:#37474f; border-color:#fb8c00; }"
        "QPushButton:pressed { background:#1c252a; }"));
    filterLayout->addWidget(refreshButton);
    bodyLayout->addWidget(filterPanel);

    auto *content = new QWidget(this);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(10);

    auto *summaryFrame = new QFrame(content);
    summaryFrame->setStyleSheet(QStringLiteral(
        "QFrame { background:white; border:1px solid #c7cdd4; border-radius:6px; }"));
    auto *summaryLayout = new QHBoxLayout(summaryFrame);
    summaryLayout->setContentsMargins(14, 10, 14, 10);
    summaryLayout->setSpacing(12);
    auto *summaryTextLayout = new QVBoxLayout;
    m_summaryLabel = new QLabel(QStringLiteral("Evidence timeline"), summaryFrame);
    m_summaryLabel->setObjectName(QStringLiteral("evidenceSummaryLabel"));
    m_summaryLabel->setStyleSheet(QStringLiteral(
        "border:none;font-size:17px;font-weight:800;color:#263238;"));
    m_statusLabel = new QLabel(
        QStringLiteral("Showing loaded evidence in capture-time order."), summaryFrame);
    m_statusLabel->setObjectName(QStringLiteral("evidenceStatusLabel"));
    m_statusLabel->setStyleSheet(QStringLiteral("border:none;color:#546e7a;"));
    summaryTextLayout->addWidget(m_summaryLabel);
    summaryTextLayout->addWidget(m_statusLabel);
    summaryLayout->addLayout(summaryTextLayout, 1);

    auto *metricGrid = new QGridLayout;
    metricGrid->setSpacing(8);
    metricGrid->addWidget(createMetricCard(summaryFrame, QStringLiteral("Slot"),
                                           QStringLiteral("evidenceSlotMetric"),
                                           &m_slotMetricLabel), 0, 0);
    metricGrid->addWidget(createMetricCard(summaryFrame, QStringLiteral("Plate"),
                                           QStringLiteral("evidencePlateMetric"),
                                           &m_plateMetricLabel), 0, 1);
    metricGrid->addWidget(createMetricCard(summaryFrame, QStringLiteral("Session"),
                                           QStringLiteral("evidenceSessionMetric"),
                                           &m_sessionMetricLabel), 1, 0);
    metricGrid->addWidget(createMetricCard(summaryFrame, QStringLiteral("Captures"),
                                           QStringLiteral("evidenceCaptureMetric"),
                                           &m_captureMetricLabel), 1, 1);
    summaryLayout->addLayout(metricGrid);
    auto *helpButton = createPageHelpButton(
        this, summaryFrame,
        {QStringLiteral("evidence"), QStringLiteral("Evidence"),
         QStringLiteral("Evidence 사용 안내"),
         QStringLiteral("주정차 증거를 확인하는 기본 흐름입니다."),
         QStringLiteral(
             "<b>1. 시간순 확인</b><br>가장 최근 캡처가 타임라인 위에 표시됩니다.<br><br>"
             "<b>2. 필터 적용</b><br>슬롯, 차량번호, 사유로 시간순 결과를 좁힙니다.<br><br>"
             "<b>3. 캡처 선택</b><br>촬영 시간, 사유, OCR 결과와 이미지 종류를 확인합니다.<br><br>"
             "<b>4. 원본 이미지 열기</b><br><i>Open full image</i>로 원본 크기 사진을 확인합니다."),
         QStringLiteral(
             "※ 사진이 표시되지 않으면 서버에 저장된 증거가 없거나 아직 이미지가 전달되지 않은 상태입니다.\n"
             "   촬영 사유는 서버 metadata가 제공될 때 표시됩니다.")});
    Q_UNUSED(helpButton);
    contentLayout->addWidget(summaryFrame);

    auto *comparisonLayout = new QHBoxLayout;
    comparisonLayout->setSpacing(10);
    auto createCaptureCard = [this](const QString &title,
                                    EvidenceImageLabel *&imageLabel,
                                    QLabel *&titleLabel,
                                    QLabel *&metadataLabel,
                                    QPushButton *&openButton) {
        auto *group = new QGroupBox(title, this);
        group->setMinimumWidth(0);
        group->setMaximumWidth(560);
        group->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        group->setStyleSheet(QStringLiteral(
            "QGroupBox { background:white; border:1px solid #c7cdd4; border-radius:6px; "
            "margin-top:12px; font-weight:800; color:#263238; }"
            "QGroupBox::title { subcontrol-origin:margin; left:10px; padding:0 4px; }"));
        auto *layout = new QVBoxLayout(group);
        layout->setContentsMargins(10, 12, 10, 10);
        layout->setSpacing(7);
        titleLabel = new QLabel(title, group);
        titleLabel->setVisible(false);
        imageLabel = new EvidenceImageLabel(group);
        metadataLabel = new QLabel(QStringLiteral("No capture loaded"), group);
        metadataLabel->setWordWrap(true);
        metadataLabel->setFixedHeight(42);
        metadataLabel->setStyleSheet(QStringLiteral(
            "color:#455a64; background:#f8fafb; border:1px solid #d6dde3; "
            "border-radius:5px; padding:6px;"));
        openButton = new QPushButton(QStringLiteral("Open full frame"), group);
        openButton->setEnabled(false);
        openButton->setCursor(Qt::PointingHandCursor);
        openButton->setStyleSheet(QStringLiteral(
            "QPushButton { background:#f5f7f9; color:#263238; border:1px solid #b0bec5; "
            "border-radius:5px; padding:5px 10px; font-weight:800; }"
            "QPushButton:hover:enabled { background:#e3f2fd; border-color:#1976d2; }"
            "QPushButton:disabled { color:#90a4ae; background:#eceff1; }"));
        openButton->setFixedHeight(34);
        layout->addWidget(imageLabel);
        layout->addWidget(metadataLabel);
        auto *buttonLayout = new QHBoxLayout;
        buttonLayout->addStretch();
        buttonLayout->addWidget(openButton);
        layout->addLayout(buttonLayout);
        return group;
    };

    QGroupBox *firstGroup = createCaptureCard(
        QStringLiteral("First capture"), m_firstImageLabel, m_firstTitleLabel,
        m_firstMetadataLabel, m_firstOpenButton);
    QGroupBox *selectedGroup = createCaptureCard(
        QStringLiteral("Latest capture"), m_selectedImageLabel, m_selectedTitleLabel,
        m_selectedMetadataLabel, m_selectedOpenButton);
    m_firstTitleLabel->setObjectName(QStringLiteral("evidenceFirstTitle"));
    m_selectedTitleLabel->setObjectName(QStringLiteral("evidenceSelectedTitle"));
    m_firstOpenButton->setObjectName(QStringLiteral("evidenceFirstOpenButton"));
    m_selectedOpenButton->setObjectName(QStringLiteral("evidenceSelectedOpenButton"));
    comparisonLayout->addStretch(1);
    comparisonLayout->addWidget(firstGroup, 1);
    comparisonLayout->addWidget(selectedGroup, 1);
    comparisonLayout->addStretch(1);
    contentLayout->addLayout(comparisonLayout);

    auto *timelineGroup = new QGroupBox(QStringLiteral("Capture timeline"), content);
    timelineGroup->setStyleSheet(QStringLiteral(
        "QGroupBox { background:white; border:1px solid #c7cdd4; border-radius:6px; "
        "margin-top:12px; font-weight:800; color:#263238; }"
        "QGroupBox::title { subcontrol-origin:margin; left:10px; padding:0 4px; }"));
    auto *timelineLayout = new QVBoxLayout(timelineGroup);
    timelineLayout->setContentsMargins(10, 12, 10, 10);
    m_captureTable = new QTableWidget(0, 6, timelineGroup);
    m_captureTable->setObjectName(QStringLiteral("evidenceCaptureTable"));
    m_captureTable->setHorizontalHeaderLabels({
        QStringLiteral("Capture"), QStringLiteral("Slot"), QStringLiteral("Time"), QStringLiteral("Reason"),
        QStringLiteral("OCR"), QStringLiteral("Available")});
    m_captureTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_captureTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_captureTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_captureTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_captureTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_captureTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_captureTable->verticalHeader()->setVisible(false);
    m_captureTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_captureTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_captureTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_captureTable->setMinimumHeight(240);
    m_captureTable->setMaximumHeight(340);
    m_captureTable->setAlternatingRowColors(true);
    m_captureTable->setStyleSheet(QStringLiteral(
        "QTableWidget { background:white; alternate-background-color:#f8fafb; "
        "border:1px solid #d6dde3; gridline-color:#e5eaee; selection-background-color:#e3f2fd; "
        "selection-color:#17212b; }"
        "QHeaderView::section { background:#eceff1; color:#263238; border:none; "
        "border-right:1px solid #cfd8dc; padding:6px; font-weight:800; }"));
    timelineLayout->addWidget(m_captureTable);
    contentLayout->addWidget(timelineGroup);
    bodyLayout->addWidget(content, 1);
    rootLayout->addLayout(bodyLayout, 1);

    updateSummaryMetrics(QString(), SlotState::Vacant, QString(), -1, -1, QString());

    connect(refreshButton, &QPushButton::clicked,
            this, &EvidencePage::requestCurrentEvidence);
    connect(m_slotFilter, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int) { showLocalEvidenceSnapshot(m_latestState); });
    connect(m_plateFilter, &QLineEdit::textChanged, this,
            [this](const QString &) { showLocalEvidenceSnapshot(m_latestState); });
    connect(m_reasonFilter, &QLineEdit::textChanged, this,
            [this](const QString &) { showLocalEvidenceSnapshot(m_latestState); });
    connect(m_captureTable, &QTableWidget::currentCellChanged, this,
            [this](int currentRow, int, int, int) {
                const QTableWidgetItem *captureItem =
                    m_captureTable->item(currentRow, 0);
                renderSelectedCapture(captureItem
                    ? captureItem->data(Qt::UserRole).toInt() : -1);
            });
    connect(m_firstOpenButton, &QPushButton::clicked, this,
            [this]() { showFullImage(m_firstImageLabel, m_firstTitleLabel->text()); });
    connect(m_selectedOpenButton, &QPushButton::clicked, this,
            [this]() { showFullImage(m_selectedImageLabel, m_selectedTitleLabel->text()); });
}

void EvidencePage::setImageLoader(ImageLoader *imageLoader)
{
    if (m_imageLoader == imageLoader) {
        return;
    }
    if (m_imageLoader) {
        disconnect(m_imageLoader, nullptr, this, nullptr);
    }
    m_imageLoader = imageLoader;
    if (!m_imageLoader) {
        return;
    }

    connect(m_imageLoader, &ImageLoader::imageLoaded, this,
            [this](const QString &requestId, const QPixmap &pixmap) {
                EvidenceImageLabel *target = m_requestTargets.take(requestId);
                if (!target || target->property("evidenceRequestId").toString() != requestId) {
                    return;
                }
                target->setText(QString());
                target->setSourcePixmap(pixmap);
                if (target == m_firstImageLabel) {
                    m_firstOpenButton->setEnabled(true);
                } else if (target == m_selectedImageLabel) {
                    m_selectedOpenButton->setEnabled(true);
                }
            });
    connect(m_imageLoader, &ImageLoader::imageFailed, this,
            [this](const QString &requestId, const QString &message) {
                EvidenceImageLabel *target = m_requestTargets.take(requestId);
                if (!target || target->property("evidenceRequestId").toString() != requestId) {
                    return;
                }
                target->setSourcePixmap(QPixmap());
                target->setText(QStringLiteral("Image load failed\n%1").arg(message));
            });
}

void EvidencePage::render(const ParkingViewState &state)
{
    m_latestState = state;
    rebuildTimelineFilters(state);
    showLocalEvidenceSnapshot(state);
}

void EvidencePage::rebuildTimelineFilters(const ParkingViewState &state)
{
    if (!m_slotFilter) {
        return;
    }

    QStringList slotIds;
    for (auto it = state.evSlots.cbegin(); it != state.evSlots.cend(); ++it) {
        slotIds.append(normalizeParkingSlotId(it.key()));
    }
    for (auto it = state.parkingSlots.cbegin(); it != state.parkingSlots.cend(); ++it) {
        slotIds.append(normalizeParkingSlotId(it.key()));
    }
    for (auto it = state.slotImages.cbegin(); it != state.slotImages.cend(); ++it) {
        slotIds.append(normalizeParkingSlotId(it.key()));
    }
    slotIds.removeAll(QString());
    std::sort(slotIds.begin(), slotIds.end(), [](const QString &left,
                                                  const QString &right) {
        const bool leftEv = left.startsWith(QStringLiteral("EV-"));
        const bool rightEv = right.startsWith(QStringLiteral("EV-"));
        if (leftEv != rightEv) {
            return leftEv;
        }
        return slotNumber(left) < slotNumber(right);
    });
    slotIds.removeDuplicates();

    const QString selectedSlotId = m_slotFilter->currentData().toString();
    QSignalBlocker blocker(m_slotFilter);
    m_slotFilter->clear();
    m_slotFilter->addItem(QStringLiteral("All slots"), QString());
    for (const QString &slotId : slotIds) {
        m_slotFilter->addItem(slotId, slotId);
    }
    const int selectedIndex = m_slotFilter->findData(selectedSlotId);
    m_slotFilter->setCurrentIndex(selectedIndex >= 0 ? selectedIndex : 0);
}

void EvidencePage::showLocalEvidenceSnapshot(const ParkingViewState &state)
{
    m_latestState = state;
    m_localTimelineMode = true;
    m_currentEventId.clear();
    m_currentSessionId = -1;
    const QString selectedSlotId = m_slotFilter
        ? m_slotFilter->currentData().toString() : QString();
    const QString plateNeedle = m_plateFilter
        ? m_plateFilter->text().trimmed() : QString();
    const QString reasonNeedle = m_reasonFilter
        ? m_reasonFilter->text().trimmed() : QString();
    m_currentSlotId = selectedSlotId;
    m_plateNumber.clear();
    ++m_requestGeneration;
    m_requestTargets.clear();
    m_captures.clear();
    m_localTimelineEntries.clear();

    for (auto it = state.slotImages.cbegin(); it != state.slotImages.cend(); ++it) {
        const QString slotId = normalizeParkingSlotId(it.key());
        const QVector<ParkingCaptureGroup> groups = buildParkingCaptureGroups(it.value());
        m_slotCaptureCounts.insert(slotId, groups.size());
        for (const ParkingCaptureGroup &capture : groups) {
            LocalTimelineEntry entry;
            entry.slotId = slotId;
            entry.plateNumber = plateForSlot(state, slotId);
            entry.state = stateForSlot(state, slotId);
            entry.capture = capture;
            if (!selectedSlotId.isEmpty() && entry.slotId != selectedSlotId) {
                continue;
            }
            if (!plateNeedle.isEmpty()
                && !entry.plateNumber.contains(plateNeedle, Qt::CaseInsensitive)
                && !entry.capture.ocrResult.contains(plateNeedle,
                                                     Qt::CaseInsensitive)) {
                continue;
            }
            if (!reasonNeedle.isEmpty()
                && !captureReasonText(entry.capture.reason).contains(
                    reasonNeedle, Qt::CaseInsensitive)) {
                continue;
            }
            m_localTimelineEntries.append(entry);
        }
    }

    std::stable_sort(m_localTimelineEntries.begin(), m_localTimelineEntries.end(),
                     [](const LocalTimelineEntry &left,
                        const LocalTimelineEntry &right) {
        if (left.capture.timestamp.isValid() != right.capture.timestamp.isValid()) {
            // The table is rendered in reverse order (newest first), so keep
            // timestamp-less legacy records at the beginning of this backing
            // list and therefore at the bottom of the visible timeline.
            return !left.capture.timestamp.isValid();
        }
        if (left.capture.timestamp.isValid()
            && left.capture.timestamp != right.capture.timestamp) {
            return left.capture.timestamp < right.capture.timestamp;
        }
        if (left.slotId != right.slotId) {
            return left.slotId < right.slotId;
        }
        return left.capture.imageId < right.capture.imageId;
    });
    m_captures.reserve(m_localTimelineEntries.size());
    for (const LocalTimelineEntry &entry : m_localTimelineEntries) {
        m_captures.append(entry.capture);
    }

    m_summaryLabel->setText(QStringLiteral("Qt local evidence timeline"));
    m_summaryLabel->setStyleSheet(statePillStyle(SlotState::Acked));
    m_statusLabel->setText(m_captures.isEmpty()
        ? QStringLiteral("No loaded evidence matches the current filters.")
        : QStringLiteral("%1 capture groups ordered by captured time (newest first). Historical sessions are not fetched in v0.1.")
              .arg(m_captures.size()));
    updateSummaryMetrics(selectedSlotId.isEmpty()
                             ? QStringLiteral("All slots") : selectedSlotId,
                         selectedSlotId.isEmpty()
                             ? SlotState::Acked
                             : stateForSlot(state, selectedSlotId),
                         selectedSlotId.isEmpty()
                             ? QString() : plateForSlot(state, selectedSlotId),
                         -1, m_captures.size(),
                         QStringLiteral("Qt local snapshot"));
    renderCaptureTable();
    renderFirstCapture();
    renderSelectedCapture(m_captures.isEmpty() ? -1 : m_captures.size() - 1);
}

void EvidencePage::showEvidence(
    const QString &slotId,
    SlotState state,
    const QString &plateNumber,
    const QList<ParkingImageResource> &images)
{
    const QString normalizedSlotId = normalizeParkingSlotId(slotId);
    if (normalizedSlotId.isEmpty()) {
        return;
    }
    m_latestState.slotImages.insert(normalizedSlotId, images);
    m_latestState.slotPlateNumbers.insert(normalizedSlotId, plateNumber);
    if (normalizedSlotId.startsWith(QStringLiteral("EV-"))) {
        EvSlotInfo slot = m_latestState.evSlots.value(normalizedSlotId);
        slot.slotId = normalizedSlotId;
        slot.state = state;
        slot.plateNumber = plateNumber;
        m_latestState.evSlots.insert(normalizedSlotId, slot);
    } else {
        ParkingSlotInfo slot = m_latestState.parkingSlots.value(normalizedSlotId);
        slot.slotId = normalizedSlotId;
        slot.state = state;
        m_latestState.parkingSlots.insert(normalizedSlotId, slot);
    }
    showLocalEvidenceSnapshot(m_latestState);
}

void EvidencePage::showLoading(const QString &slotId)
{
    if (!slotId.isEmpty() && slotId != m_currentSlotId) {
        return;
    }
    resetLocalTimeline();
    m_currentEventId.clear();
    m_currentSessionId = -1;
    ++m_requestGeneration;
    m_requestTargets.clear();
    m_captures.clear();
    m_captureTable->setRowCount(0);
    m_summaryLabel->setText(QStringLiteral("Loading parking evidence"));
    m_summaryLabel->setStyleSheet(statePillStyle(SlotState::Vacant));
    m_statusLabel->setText(QStringLiteral("Loading active session and evidence images..."));
    updateSummaryMetrics(slotId, SlotState::Vacant, QString(), -1, -1, QString());
    clearCaptureCard(m_firstImageLabel, m_firstTitleLabel, m_firstMetadataLabel,
                     m_firstOpenButton, QStringLiteral("First capture"),
                     QStringLiteral("Loading..."));
    clearCaptureCard(m_selectedImageLabel, m_selectedTitleLabel,
                     m_selectedMetadataLabel, m_selectedOpenButton,
                     QStringLiteral("Latest capture"), QStringLiteral("Loading..."));
}

void EvidencePage::showError(const QString &slotId, const QString &message)
{
    if (!slotId.isEmpty() && slotId != m_currentSlotId) {
        return;
    }
    m_statusLabel->setText(QStringLiteral("Could not load evidence: %1").arg(message));
    updateSummaryMetrics(slotId, SlotState::SensorError, m_plateNumber, -1,
                         m_captures.size(), QString());
    clearCaptureCard(m_firstImageLabel, m_firstTitleLabel, m_firstMetadataLabel,
                     m_firstOpenButton, QStringLiteral("First capture"),
                     QStringLiteral("Evidence request failed"));
    clearCaptureCard(m_selectedImageLabel, m_selectedTitleLabel,
                     m_selectedMetadataLabel, m_selectedOpenButton,
                     QStringLiteral("Latest capture"),
                     QStringLiteral("Evidence request failed"));
}

void EvidencePage::openEvent(const QString &eventId, const QString &rawSlotId)
{
    const QString slotId = normalizeParkingSlotId(rawSlotId);
    if (m_slotFilter) {
        const int index = m_slotFilter->findData(slotId);
        if (index >= 0) {
            m_slotFilter->setCurrentIndex(index);
        }
    }
    m_currentEventId = eventId.trimmed();
    m_currentSlotId = slotId;
    m_currentSessionId = -1;
    m_statusLabel->setText(
        QStringLiteral("Resolving event evidence into the time-ordered timeline..."));
}

void EvidencePage::showEventEvidence(
    const QString &eventId,
    const QString &slotId,
    qint64 sessionId,
    SlotState state,
    const QString &plateNumber,
    const QList<ParkingImageResource> &images)
{
    if (eventId != m_currentEventId) {
        return;
    }
    m_currentSessionId = sessionId;
    showEvidence(slotId, state, plateNumber, images);
    m_currentEventId = eventId;
}

void EvidencePage::showEventError(const QString &eventId,
                                  const QString &slotId,
                                  const QString &message)
{
    if (eventId != m_currentEventId) {
        return;
    }
    m_summaryLabel->setText(
        QStringLiteral("Event evidence unavailable"));
    m_summaryLabel->setStyleSheet(statePillStyle(SlotState::SensorError));
    m_statusLabel->setText(
        QStringLiteral("Could not load event evidence: %1").arg(message));
    updateSummaryMetrics(slotId, SlotState::SensorError, m_plateNumber,
                         m_currentSessionId, m_captures.size(), eventId);
    clearCaptureCard(m_firstImageLabel, m_firstTitleLabel,
                     m_firstMetadataLabel, m_firstOpenButton,
                     QStringLiteral("First capture"),
                     QStringLiteral("Event evidence request failed"));
    clearCaptureCard(m_selectedImageLabel, m_selectedTitleLabel,
                     m_selectedMetadataLabel, m_selectedOpenButton,
                     QStringLiteral("Latest capture"),
                     QStringLiteral("Event evidence request failed"));
}

bool EvidencePage::selectSlot(const QString &rawSlotId)
{
    const QString slotId = normalizeParkingSlotId(rawSlotId);
    if (slotId.isEmpty() || !m_slotFilter) {
        return false;
    }
    const int index = m_slotFilter->findData(slotId);
    if (index < 0) {
        return false;
    }
    m_currentEventId.clear();
    m_currentSessionId = -1;
    m_slotFilter->setCurrentIndex(index);
    showLocalEvidenceSnapshot(m_latestState);
    return true;
}

QString EvidencePage::currentSlotId() const
{
    return m_currentSlotId;
}

QString EvidencePage::currentEventId() const
{
    return m_currentEventId;
}

int EvidencePage::captureCount() const
{
    return m_captures.size();
}

void EvidencePage::requestCurrentEvidence()
{
    if (!m_currentEventId.isEmpty()) {
        openEvent(m_currentEventId, m_currentSlotId);
        emit eventEvidenceRequested(m_currentEventId);
        return;
    }
    showLocalEvidenceSnapshot(m_latestState);
}

void EvidencePage::renderCaptureTable()
{
    QSignalBlocker blocker(m_captureTable);
    m_captureTable->setRowCount(m_captures.size());
    for (int row = 0; row < m_captures.size(); ++row) {
        const int captureIndex = m_localTimelineMode
            ? m_captures.size() - 1 - row : row;
        const ParkingCaptureGroup &capture = m_captures.at(captureIndex);
        QStringList variants;
        for (const ParkingImageResource &variant : capture.variants) {
            const QString name = variant.processing.isEmpty()
                ? QStringLiteral("Image") : variant.processing;
            if (!variants.contains(name, Qt::CaseInsensitive)) {
                variants.append(name);
            }
        }
        const QString captureName = capture.imageId >= 0
            ? QStringLiteral("#%1").arg(capture.imageId)
            : QStringLiteral("#%1").arg(captureIndex + 1);
        const QString slotId = m_localTimelineMode
            ? m_localTimelineEntries.at(captureIndex).slotId
            : m_currentSlotId;
        const QStringList values = {
            captureName,
            slotId.isEmpty() ? QStringLiteral("-") : slotId,
            captureTimeText(capture.timestamp),
            captureReasonText(capture.reason),
            capture.ocrResult.isEmpty() ? QStringLiteral("-") : capture.ocrResult,
            variants.join(QStringLiteral(" / "))};
        for (int column = 0; column < values.size(); ++column) {
            auto *item = new QTableWidgetItem(values.at(column));
            if (column == 0) {
                item->setData(Qt::UserRole, captureIndex);
            }
            m_captureTable->setItem(row, column, item);
        }
    }
    if (!m_captures.isEmpty()) {
        m_captureTable->setCurrentCell(m_localTimelineMode ? 0 : m_captures.size() - 1, 0);
    }
}

void EvidencePage::renderFirstCapture()
{
    const ParkingCaptureGroup *capture = m_captures.isEmpty()
        ? nullptr : &m_captures.first();
    const QString originalPlate = m_plateNumber;
    if (m_localTimelineMode && !m_localTimelineEntries.isEmpty()) {
        m_plateNumber = m_localTimelineEntries.first().plateNumber;
    }
    renderCaptureCard(capture,
                      m_localTimelineMode ? QStringLiteral("Earliest loaded capture")
                                          : QStringLiteral("First capture"),
                      m_firstImageLabel,
                      m_firstTitleLabel, m_firstMetadataLabel,
                      m_firstOpenButton, QStringLiteral("first"));
    m_plateNumber = originalPlate;
}

void EvidencePage::renderSelectedCapture(int row)
{
    if (row < 0 || row >= m_captures.size()) {
        clearCaptureCard(m_selectedImageLabel, m_selectedTitleLabel,
                         m_selectedMetadataLabel, m_selectedOpenButton,
                         QStringLiteral("Latest capture"),
                         m_captures.isEmpty()
                             ? QStringLiteral("No later capture available")
                             : QStringLiteral("Select a capture from the timeline"));
        return;
    }
    const QString heading = m_localTimelineMode
        ? QStringLiteral("Selected loaded capture")
        : (row == m_captures.size() - 1
           ? QStringLiteral("Latest capture") : QStringLiteral("Selected capture"));
    const QString originalPlate = m_plateNumber;
    if (m_localTimelineMode && row < m_localTimelineEntries.size()) {
        m_plateNumber = m_localTimelineEntries.at(row).plateNumber;
    }
    renderCaptureCard(&m_captures.at(row), heading, m_selectedImageLabel,
                      m_selectedTitleLabel, m_selectedMetadataLabel,
                      m_selectedOpenButton, QStringLiteral("selected"));
    m_plateNumber = originalPlate;
}

SlotState EvidencePage::stateForSlot(const ParkingViewState &state,
                                     const QString &slotId) const
{
    if (state.evSlots.contains(slotId)) {
        return state.evSlots.value(slotId).state;
    }
    if (state.parkingSlots.contains(slotId)) {
        return state.parkingSlots.value(slotId).state;
    }
    return SlotState::Vacant;
}

QString EvidencePage::plateForSlot(const ParkingViewState &state,
                                   const QString &slotId) const
{
    if (state.evSlots.contains(slotId)) {
        const QString plate = state.evSlots.value(slotId).plateNumber.trimmed();
        if (!plate.isEmpty()) {
            return plate;
        }
    }
    return state.slotPlateNumbers.value(slotId);
}

void EvidencePage::resetLocalTimeline()
{
    m_localTimelineMode = true;
    m_localTimelineEntries.clear();
}

void EvidencePage::updateSlotCaptureCount(const QString &slotId, int captureCount)
{
    const QString normalizedSlotId = normalizeParkingSlotId(slotId);
    if (normalizedSlotId.isEmpty()) {
        return;
    }

    m_slotCaptureCounts.insert(normalizedSlotId, captureCount);
}

void EvidencePage::updateSummaryMetrics(const QString &slotId,
                                        SlotState state,
                                        const QString &plateNumber,
                                        qint64 sessionId,
                                        int captureCount,
                                        const QString &eventId)
{
    if (m_slotMetricLabel) {
        m_slotMetricLabel->setText(
            slotId.isEmpty()
                ? QStringLiteral("-")
                : QStringLiteral("%1 · %2").arg(slotId, slotStateText(state)));
        m_slotMetricLabel->setStyleSheet(QStringLiteral(
            "border:none; color:%1; font-size:16px; font-weight:900;")
                                             .arg(slotStateAccent(state)));
    }
    if (m_plateMetricLabel) {
        const QString cleanPlate = plateNumber.trimmed();
        m_plateMetricLabel->setText(
            cleanPlate.isEmpty() || cleanPlate == QStringLiteral("-")
                ? QStringLiteral("Unconfirmed")
                : cleanPlate);
    }
    if (m_sessionMetricLabel) {
        QString sessionText;
        if (sessionId > 0) {
            sessionText = QStringLiteral("Session %1").arg(sessionId);
        } else if (!eventId.trimmed().isEmpty()) {
            sessionText = QStringLiteral("Event linked");
        } else {
            sessionText = QStringLiteral("Active slot");
        }
        m_sessionMetricLabel->setText(sessionText);
    }
    if (m_captureMetricLabel) {
        m_captureMetricLabel->setText(
            captureCount < 0 ? QStringLiteral("Loading") : captureCountText(captureCount));
    }
}

void EvidencePage::renderCaptureCard(
    const ParkingCaptureGroup *capture,
    const QString &heading,
    EvidenceImageLabel *imageLabel,
    QLabel *titleLabel,
    QLabel *metadataLabel,
    QPushButton *openButton,
    const QString &requestRole)
{
    if (!capture) {
        clearCaptureCard(imageLabel, titleLabel, metadataLabel, openButton,
                         heading, QStringLiteral("No capture available"));
        return;
    }
    const ParkingImageResource *variant =
        preferredParkingCaptureVariant(*capture);
    titleLabel->setText(heading);
    // OCR 은 HALL_30S 캡처 한 장에만 돌아서 다른 증거 이미지 행에는
    // ocr_result 가 없다. 그 경우 세션에서 확정된 번호판으로 폴백하되,
    // 이 이미지에서 직접 읽은 값이 아님을 (session) 으로 구분한다.
    QString ocrText = capture->ocrResult;
    if (ocrText.isEmpty()) {
        ocrText = (m_plateNumber.isEmpty() || m_plateNumber == QStringLiteral("-"))
            ? QStringLiteral("-")
            : QStringLiteral("%1 (session)").arg(m_plateNumber);
    }
    QString metadata = QStringLiteral("%1")
        .arg(captureTimeText(capture->timestamp));
    if (variant && !variant->processing.isEmpty()) {
        metadata += QStringLiteral("  |  %1").arg(variant->processing);
    }
    if (ocrText != QStringLiteral("-")) {
        metadata += QStringLiteral("  |  OCR available");
    }
    metadataLabel->setText(metadata);
    imageLabel->setSourcePixmap(QPixmap());
    imageLabel->setProperty("evidenceRequestId", QString());
    openButton->setEnabled(false);

    if (!variant || variant->url.isEmpty()) {
        imageLabel->setText(QStringLiteral("Image URL is not available"));
        return;
    }
    if (!m_imageLoader) {
        imageLabel->setText(QStringLiteral("Image loader is not available"));
        return;
    }

    imageLabel->setText(QStringLiteral("Loading image..."));
    const QString requestId = QStringLiteral("evidence:%1:%2:%3")
        .arg(reinterpret_cast<quintptr>(this))
        .arg(m_requestGeneration)
        .arg(requestRole + QLatin1Char(':') + QString::number(++m_requestSequence));
    imageLabel->setProperty("evidenceRequestId", requestId);
    m_requestTargets.insert(requestId, imageLabel);
    m_imageLoader->load(requestId, variant->url);
}

void EvidencePage::clearCaptureCard(
    EvidenceImageLabel *imageLabel,
    QLabel *titleLabel,
    QLabel *metadataLabel,
    QPushButton *openButton,
    const QString &title,
    const QString &message)
{
    imageLabel->setSourcePixmap(QPixmap());
    imageLabel->setProperty("evidenceRequestId", QString());
    imageLabel->setText(message);
    titleLabel->setText(title);
    metadataLabel->setText(QStringLiteral("No capture metadata"));
    openButton->setEnabled(false);
}

void EvidencePage::showFullImage(EvidenceImageLabel *source, const QString &title)
{
    if (!source || source->sourcePixmap().isNull()) {
        return;
    }
    auto *dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(title);
    dialog->resize(1100, 760);
    auto *layout = new QVBoxLayout(dialog);
    auto *scrollArea = new QScrollArea(dialog);
    auto *imageLabel = new QLabel(scrollArea);
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setPixmap(source->sourcePixmap());
    imageLabel->resize(source->sourcePixmap().size());
    scrollArea->setWidget(imageLabel);
    scrollArea->setAlignment(Qt::AlignCenter);
    layout->addWidget(scrollArea, 1);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);
    layout->addWidget(buttons);
    dialog->show();
}
