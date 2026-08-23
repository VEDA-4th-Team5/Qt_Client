#include "evidencepage.h"

#include "api/imageloader.h"
#include "widgets/pagehelp.h"

#include <QAbstractItemView>
#include <QColor>
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
#include <QListWidget>
#include <QListWidgetItem>
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

    auto *slotPanel = new QFrame(this);
    slotPanel->setObjectName(QStringLiteral("evidenceSlotPanel"));
    slotPanel->setFixedWidth(245);
    slotPanel->setStyleSheet(QStringLiteral(
        "QFrame#evidenceSlotPanel { background:#f8fafb; border:1px solid #c7cdd4; "
        "border-radius:6px; }"));
    auto *slotLayout = new QVBoxLayout(slotPanel);
    slotLayout->setContentsMargins(12, 12, 12, 12);
    slotLayout->setSpacing(8);
    auto *slotTitle = new QLabel(QStringLiteral("Parking slots"), slotPanel);
    slotTitle->setStyleSheet(QStringLiteral("font-size:16px;font-weight:800;color:#263238;"));
    slotLayout->addWidget(slotTitle);
    m_slotSearch = new QLineEdit(slotPanel);
    m_slotSearch->setPlaceholderText(QStringLiteral("Search slot"));
    m_slotSearch->setFixedHeight(32);
    m_slotSearch->setStyleSheet(QStringLiteral(
        "QLineEdit { background:white; border:1px solid #b0bec5; border-radius:5px; "
        "padding:5px 8px; color:#263238; }"
        "QLineEdit:focus { border-color:#1976d2; }"));
    slotLayout->addWidget(m_slotSearch);
    m_slotList = new QListWidget(slotPanel);
    m_slotList->setObjectName(QStringLiteral("evidenceSlotList"));
    m_slotList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_slotList->setSpacing(2);
    m_slotList->setStyleSheet(QStringLiteral(
        "QListWidget { background:transparent; border:none; outline:0; }"
        "QListWidget::item { background:white; border:1px solid #d6dde3; "
        "border-radius:5px; margin:2px 0; padding:6px; color:#263238; }"
        "QListWidget::item:selected { background:#e3f2fd; border-color:#1976d2; }"
        "QListWidget::item:hover { border-color:#90a4ae; }"));
    slotLayout->addWidget(m_slotList, 1);
    auto *refreshButton = new QPushButton(QStringLiteral("Refresh evidence"), slotPanel);
    refreshButton->setObjectName(QStringLiteral("evidenceRefreshButton"));
    refreshButton->setCursor(Qt::PointingHandCursor);
    refreshButton->setFixedHeight(34);
    refreshButton->setStyleSheet(QStringLiteral(
        "QPushButton { background:#263238; color:white; border:1px solid #455a64; "
        "border-radius:6px; padding:6px 10px; font-weight:800; }"
        "QPushButton:hover { background:#37474f; border-color:#fb8c00; }"
        "QPushButton:pressed { background:#1c252a; }"));
    slotLayout->addWidget(refreshButton);
    bodyLayout->addWidget(slotPanel);

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
    m_summaryLabel = new QLabel(QStringLiteral("Evidence gallery"), summaryFrame);
    m_summaryLabel->setObjectName(QStringLiteral("evidenceSummaryLabel"));
    m_summaryLabel->setStyleSheet(QStringLiteral(
        "border:none;font-size:17px;font-weight:800;color:#263238;"));
    m_statusLabel = new QLabel(
        QStringLiteral("Select a slot to compare its first and latest captures."), summaryFrame);
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
             "<b>1. 슬롯 선택</b><br>왼쪽 목록에서 확인할 주차 슬롯을 선택합니다.<br><br>"
             "<b>2. 캡처 비교</b><br><i>First capture</i>와 <i>Latest capture</i>를 비교합니다.<br><br>"
             "<b>3. 타임라인 확인</b><br>촬영 시간, 사유, OCR 결과와 이미지 종류를 확인합니다.<br><br>"
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
    m_captureTable = new QTableWidget(0, 5, timelineGroup);
    m_captureTable->setObjectName(QStringLiteral("evidenceCaptureTable"));
    m_captureTable->setHorizontalHeaderLabels({
        QStringLiteral("Capture"), QStringLiteral("Time"), QStringLiteral("Reason"),
        QStringLiteral("OCR"), QStringLiteral("Available")});
    m_captureTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_captureTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_captureTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_captureTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_captureTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
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

    connect(m_slotList, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem *current, QListWidgetItem *) {
                handleSlotChanged(current);
            });
    connect(m_slotSearch, &QLineEdit::textChanged,
            this, &EvidencePage::filterSlots);
    connect(refreshButton, &QPushButton::clicked,
            this, &EvidencePage::requestCurrentEvidence);
    connect(m_captureTable, &QTableWidget::currentCellChanged, this,
            [this](int currentRow, int, int, int) {
                renderSelectedCapture(currentRow);
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
    struct SlotRow {
        QString id;
        SlotState state = SlotState::Vacant;
        int imageCount = -1;
    };
    const QString activeSlotId = normalizeParkingSlotId(m_currentSlotId);
    SlotState activeSlotState = SlotState::Vacant;
    QString activePlateNumber = state.slotPlateNumbers.value(activeSlotId);
    bool activeSlotKnown = false;
    QVector<SlotRow> rows;
    rows.reserve(state.evSlots.size() + state.parkingSlots.size());
    for (const EvSlotInfo &slot : state.evSlots) {
        const int stateCount = state.slotImages.contains(slot.slotId)
            ? static_cast<int>(state.slotImages.value(slot.slotId).size())
            : -1;
        rows.append({slot.slotId, slot.state,
                     m_slotCaptureCounts.value(slot.slotId, stateCount)});
        if (normalizeParkingSlotId(slot.slotId) == activeSlotId) {
            activeSlotState = slot.state;
            if (!slot.plateNumber.trimmed().isEmpty()) {
                activePlateNumber = slot.plateNumber;
            }
            activeSlotKnown = true;
        }
    }
    for (const ParkingSlotInfo &slot : state.parkingSlots) {
        const int stateCount = state.slotImages.contains(slot.slotId)
            ? static_cast<int>(state.slotImages.value(slot.slotId).size())
            : -1;
        rows.append({slot.slotId, slot.state,
                     m_slotCaptureCounts.value(slot.slotId, stateCount)});
        if (normalizeParkingSlotId(slot.slotId) == activeSlotId) {
            activeSlotState = slot.state;
            activeSlotKnown = true;
        }
    }
    std::sort(rows.begin(), rows.end(), [](const SlotRow &left, const SlotRow &right) {
        const bool leftEv = left.id.startsWith(QStringLiteral("EV-"));
        const bool rightEv = right.id.startsWith(QStringLiteral("EV-"));
        if (leftEv != rightEv) {
            return leftEv;
        }
        return slotNumber(left.id) < slotNumber(right.id);
    });

    const QString previousSlotId = m_currentSlotId;
    const bool eventNavigationActive = !m_currentEventId.isEmpty();
    QSignalBlocker blocker(m_slotList);
    m_slotList->clear();
    int preferredRow = -1;
    int fallbackRow = -1;
    for (int row = 0; row < rows.size(); ++row) {
        const SlotRow &slot = rows.at(row);
        auto *item = new QListWidgetItem(
            QStringLiteral("%1\n%2  ·  %3")
                .arg(slot.id, slotStateText(slot.state))
                .arg(captureCountText(slot.imageCount)),
            m_slotList);
        item->setData(Qt::UserRole, slot.id);
        item->setSizeHint(QSize(0, 52));
        item->setBackground(QColor(slotStateSurface(slot.state)));
        item->setData(Qt::UserRole + 1, slotStateText(slot.state));
        item->setData(Qt::UserRole + 2, slot.imageCount);
        if (slot.state == SlotState::OvertimeAlert
            || slot.state == SlotState::NonEvAlert) {
            item->setForeground(QColor(QStringLiteral("#b71c1c")));
            QFont font = item->font();
            font.setBold(true);
            item->setFont(font);
        }
        if (slot.id == previousSlotId) {
            preferredRow = row;
        }
        if (fallbackRow < 0 && (slot.imageCount > 0
                                || slot.state != SlotState::Vacant)) {
            fallbackRow = row;
        }
    }
    if (!eventNavigationActive && preferredRow < 0) {
        preferredRow = fallbackRow >= 0 ? fallbackRow : (rows.isEmpty() ? -1 : 0);
    }
    if (preferredRow >= 0) {
        m_slotList->setCurrentRow(preferredRow);
        if (!eventNavigationActive) {
            m_currentSlotId = m_slotList->item(preferredRow)
                                  ->data(Qt::UserRole).toString();
        }
    } else if (!eventNavigationActive) {
        m_currentSlotId.clear();
    } else {
        m_slotList->setCurrentItem(nullptr);
    }
    filterSlots(m_slotSearch->text());

    const bool canUpdateActiveEvidence =
        !eventNavigationActive && activeSlotKnown
        && !activeSlotId.isEmpty()
        && state.slotImages.contains(activeSlotId);
    if (canUpdateActiveEvidence) {
        const QList<ParkingImageResource> activeImages =
            state.slotImages.value(activeSlotId);
        const int activeImageCount = activeImages.size();
        const int renderedImageCount =
            m_slotCaptureCounts.value(activeSlotId, m_captures.size());
        if (activeImageCount != renderedImageCount
            || activeImageCount != m_captures.size()) {
            showEvidence(activeSlotId, activeSlotState, activePlateNumber,
                         activeImages);
        }
    }
}

void EvidencePage::showEvidence(
    const QString &slotId,
    SlotState state,
    const QString &plateNumber,
    const QList<ParkingImageResource> &images)
{
    if (!m_currentSlotId.isEmpty() && slotId != m_currentSlotId) {
        return;
    }
    m_currentSlotId = slotId;
    m_plateNumber = plateNumber.isEmpty() ? QStringLiteral("-") : plateNumber;
    ++m_requestGeneration;
    m_requestTargets.clear();
    m_captures = buildParkingCaptureGroups(images);
    updateSlotCaptureCount(slotId, m_captures.size());
    m_summaryLabel->setText(QStringLiteral("Reviewing active parking evidence"));
    m_summaryLabel->setStyleSheet(statePillStyle(state));
    m_statusLabel->setText(
        m_captures.isEmpty()
            ? QStringLiteral("No evidence images are available for the active parking session.")
            : QStringLiteral("%1 capture%2 loaded. Select a timeline row to compare it with the first capture.")
                  .arg(m_captures.size())
                  .arg(m_captures.size() == 1 ? QString() : QStringLiteral("s")));
    updateSummaryMetrics(slotId, state, m_plateNumber, -1, m_captures.size(), QString());
    renderCaptureTable();
    renderFirstCapture();
    renderSelectedCapture(m_captures.size() > 1 ? m_captures.size() - 1 : -1);
}

void EvidencePage::showLoading(const QString &slotId)
{
    if (!slotId.isEmpty() && slotId != m_currentSlotId) {
        return;
    }
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
    m_currentEventId = eventId.trimmed();
    m_currentSlotId = slotId;
    m_currentSessionId = -1;

    QSignalBlocker blocker(m_slotList);
    m_slotSearch->clear();
    QListWidgetItem *matchingItem = nullptr;
    for (int row = 0; row < m_slotList->count(); ++row) {
        QListWidgetItem *item = m_slotList->item(row);
        if (item->data(Qt::UserRole).toString() == slotId) {
            matchingItem = item;
            break;
        }
    }
    m_slotList->setCurrentItem(matchingItem);
    if (matchingItem) {
        m_slotList->scrollToItem(
            matchingItem, QAbstractItemView::PositionAtCenter);
    }

    ++m_requestGeneration;
    m_requestTargets.clear();
    m_captures.clear();
    m_captureTable->setRowCount(0);
    m_summaryLabel->setText(
        QStringLiteral("Resolving event evidence"));
    m_summaryLabel->setStyleSheet(statePillStyle(SlotState::Vacant));
    m_statusLabel->setText(
        QStringLiteral("Resolving the parking session recorded by this event..."));
    updateSummaryMetrics(slotId, SlotState::Vacant, QString(), -1, -1,
                         m_currentEventId);
    clearCaptureCard(m_firstImageLabel, m_firstTitleLabel,
                     m_firstMetadataLabel, m_firstOpenButton,
                     QStringLiteral("First capture"), QStringLiteral("Loading..."));
    clearCaptureCard(m_selectedImageLabel, m_selectedTitleLabel,
                     m_selectedMetadataLabel, m_selectedOpenButton,
                     QStringLiteral("Latest capture"), QStringLiteral("Loading..."));
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
    m_summaryLabel->setText(
        QStringLiteral("Reviewing event-linked evidence"));
    m_summaryLabel->setStyleSheet(statePillStyle(state));
    m_statusLabel->setText(
        m_captures.isEmpty()
            ? QStringLiteral("No evidence images are stored for this event session.")
            : QStringLiteral(
                  "%1 capture%2 loaded from the event session. Select a timeline row to compare it with the first capture.")
                  .arg(m_captures.size())
                  .arg(m_captures.size() == 1 ? QString()
                                              : QStringLiteral("s")));
    updateSummaryMetrics(slotId, state, plateNumber, sessionId,
                         m_captures.size(), eventId);
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
    if (slotId.isEmpty()) {
        return false;
    }

    m_slotSearch->clear();
    for (int row = 0; row < m_slotList->count(); ++row) {
        QListWidgetItem *item = m_slotList->item(row);
        if (item->data(Qt::UserRole).toString() != slotId) {
            continue;
        }

        const bool selectionChanged = m_slotList->currentItem() != item;
        m_currentEventId.clear();
        m_currentSessionId = -1;
        m_slotList->setCurrentItem(item);
        m_slotList->scrollToItem(item, QAbstractItemView::PositionAtCenter);
        if (!selectionChanged) {
            m_currentSlotId = slotId;
            requestCurrentEvidence();
        }
        return true;
    }
    return false;
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
    if (m_currentSlotId.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("No parking slot is available."));
        return;
    }
    showLoading(m_currentSlotId);
    emit slotEvidenceRequested(m_currentSlotId);
}

void EvidencePage::handleSlotChanged(QListWidgetItem *current)
{
    if (!current) {
        return;
    }
    m_currentEventId.clear();
    m_currentSessionId = -1;
    m_currentSlotId = current->data(Qt::UserRole).toString();
    requestCurrentEvidence();
}

void EvidencePage::filterSlots(const QString &text)
{
    const QString needle = text.trimmed();
    for (int row = 0; row < m_slotList->count(); ++row) {
        QListWidgetItem *item = m_slotList->item(row);
        item->setHidden(!needle.isEmpty()
                        && !item->text().contains(needle, Qt::CaseInsensitive));
    }
}

void EvidencePage::renderCaptureTable()
{
    QSignalBlocker blocker(m_captureTable);
    m_captureTable->setRowCount(m_captures.size());
    for (int row = 0; row < m_captures.size(); ++row) {
        const ParkingCaptureGroup &capture = m_captures.at(row);
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
            : QStringLiteral("#%1").arg(row + 1);
        const QStringList values = {
            captureName,
            captureTimeText(capture.timestamp),
            captureReasonText(capture.reason),
            capture.ocrResult.isEmpty() ? QStringLiteral("-") : capture.ocrResult,
            variants.join(QStringLiteral(" / "))};
        for (int column = 0; column < values.size(); ++column) {
            m_captureTable->setItem(row, column,
                                    new QTableWidgetItem(values.at(column)));
        }
    }
    if (!m_captures.isEmpty()) {
        m_captureTable->setCurrentCell(m_captures.size() - 1, 0);
    }
}

void EvidencePage::renderFirstCapture()
{
    const ParkingCaptureGroup *capture = m_captures.isEmpty()
        ? nullptr : &m_captures.first();
    renderCaptureCard(capture, QStringLiteral("First capture"), m_firstImageLabel,
                      m_firstTitleLabel, m_firstMetadataLabel,
                      m_firstOpenButton, QStringLiteral("first"));
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
    const QString heading = row == m_captures.size() - 1
        ? QStringLiteral("Latest capture") : QStringLiteral("Selected capture");
    renderCaptureCard(&m_captures.at(row), heading, m_selectedImageLabel,
                      m_selectedTitleLabel, m_selectedMetadataLabel,
                      m_selectedOpenButton, QStringLiteral("selected"));
}

void EvidencePage::updateSlotCaptureCount(const QString &slotId, int captureCount)
{
    const QString normalizedSlotId = normalizeParkingSlotId(slotId);
    if (normalizedSlotId.isEmpty()) {
        return;
    }

    m_slotCaptureCounts.insert(normalizedSlotId, captureCount);
    for (int row = 0; row < m_slotList->count(); ++row) {
        QListWidgetItem *item = m_slotList->item(row);
        if (!item || item->data(Qt::UserRole).toString() != normalizedSlotId) {
            continue;
        }

        const QString stateText =
            item->data(Qt::UserRole + 1).toString().trimmed();
        item->setData(Qt::UserRole + 2, captureCount);
        item->setText(QStringLiteral("%1\n%2  ·  %3")
                          .arg(normalizedSlotId,
                               stateText.isEmpty()
                                   ? QStringLiteral("Unknown")
                                   : stateText,
                               captureCountText(captureCount)));
        break;
    }
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
