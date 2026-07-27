#include "evidencepage.h"

#include "api/imageloader.h"

#include <QAbstractItemView>
#include <QColor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFrame>
#include <QFont>
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
        setMinimumSize(320, 230);
        setText(QStringLiteral("Select a parking slot to load evidence"));
        setStyleSheet(QStringLiteral(
            "QLabel { background:#111820; color:#b0bec5; border:1px solid #455a64; "
            "border-radius:5px; padding:8px; }"));
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

const ParkingImageResource *preferredVariant(
    const QList<ParkingImageResource> &variants)
{
    for (const ParkingImageResource &variant : variants) {
        if (variant.processing.compare(QStringLiteral("ORIGINAL"), Qt::CaseInsensitive) == 0) {
            return &variant;
        }
    }
    return variants.isEmpty() ? nullptr : &variants.first();
}

int slotNumber(const QString &slotId)
{
    bool ok = false;
    const int number = slotId.section(QLatin1Char('-'), -1).toInt(&ok);
    return ok ? number : 0;
}
} // namespace

EvidencePage::EvidencePage(QWidget *parent)
    : QWidget(parent)
{
    auto *rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(12);

    auto *slotPanel = new QFrame(this);
    slotPanel->setObjectName(QStringLiteral("evidenceSlotPanel"));
    slotPanel->setFixedWidth(245);
    slotPanel->setStyleSheet(QStringLiteral(
        "QFrame#evidenceSlotPanel { background:white; border:1px solid #c7cdd4; "
        "border-radius:6px; }"));
    auto *slotLayout = new QVBoxLayout(slotPanel);
    slotLayout->setContentsMargins(12, 12, 12, 12);
    slotLayout->setSpacing(8);
    auto *slotTitle = new QLabel(QStringLiteral("Parking slots"), slotPanel);
    slotTitle->setStyleSheet(QStringLiteral("font-size:16px;font-weight:800;color:#263238;"));
    slotLayout->addWidget(slotTitle);
    m_slotSearch = new QLineEdit(slotPanel);
    m_slotSearch->setPlaceholderText(QStringLiteral("Search slot"));
    slotLayout->addWidget(m_slotSearch);
    m_slotList = new QListWidget(slotPanel);
    m_slotList->setObjectName(QStringLiteral("evidenceSlotList"));
    m_slotList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_slotList->setSpacing(2);
    slotLayout->addWidget(m_slotList, 1);
    auto *refreshButton = new QPushButton(QStringLiteral("Refresh evidence"), slotPanel);
    refreshButton->setObjectName(QStringLiteral("evidenceRefreshButton"));
    slotLayout->addWidget(refreshButton);
    rootLayout->addWidget(slotPanel);

    auto *content = new QWidget(this);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(10);

    auto *summaryFrame = new QFrame(content);
    summaryFrame->setStyleSheet(QStringLiteral(
        "QFrame { background:white; border:1px solid #c7cdd4; border-radius:6px; }"));
    auto *summaryLayout = new QHBoxLayout(summaryFrame);
    summaryLayout->setContentsMargins(14, 10, 14, 10);
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
    contentLayout->addWidget(summaryFrame);

    auto *comparisonLayout = new QHBoxLayout;
    comparisonLayout->setSpacing(10);
    auto createCaptureCard = [this](const QString &title,
                                    EvidenceImageLabel *&imageLabel,
                                    QLabel *&titleLabel,
                                    QLabel *&metadataLabel,
                                    QPushButton *&openButton) {
        auto *group = new QGroupBox(title, this);
        auto *layout = new QVBoxLayout(group);
        layout->setSpacing(7);
        titleLabel = new QLabel(title, group);
        titleLabel->setStyleSheet(QStringLiteral("font-size:15px;font-weight:800;color:#263238;"));
        imageLabel = new EvidenceImageLabel(group);
        metadataLabel = new QLabel(QStringLiteral("No capture loaded"), group);
        metadataLabel->setWordWrap(true);
        metadataLabel->setMinimumHeight(38);
        metadataLabel->setStyleSheet(QStringLiteral("color:#455a64;"));
        openButton = new QPushButton(QStringLiteral("Open full image"), group);
        openButton->setEnabled(false);
        layout->addWidget(titleLabel);
        layout->addWidget(imageLabel, 1);
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
    comparisonLayout->addWidget(firstGroup, 1);
    comparisonLayout->addWidget(selectedGroup, 1);
    contentLayout->addLayout(comparisonLayout, 1);

    auto *timelineGroup = new QGroupBox(QStringLiteral("Capture timeline"), content);
    auto *timelineLayout = new QVBoxLayout(timelineGroup);
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
    m_captureTable->setMaximumHeight(190);
    timelineLayout->addWidget(m_captureTable);
    contentLayout->addWidget(timelineGroup);
    rootLayout->addWidget(content, 1);

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
        int imageCount = 0;
    };
    QVector<SlotRow> rows;
    rows.reserve(state.evSlots.size() + state.parkingSlots.size());
    for (const EvSlotInfo &slot : state.evSlots) {
        rows.append({slot.slotId, slot.state,
                     static_cast<int>(state.slotImages.value(slot.slotId).size())});
    }
    for (const ParkingSlotInfo &slot : state.parkingSlots) {
        rows.append({slot.slotId, slot.state,
                     static_cast<int>(state.slotImages.value(slot.slotId).size())});
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
    QSignalBlocker blocker(m_slotList);
    m_slotList->clear();
    int preferredRow = -1;
    int fallbackRow = -1;
    for (int row = 0; row < rows.size(); ++row) {
        const SlotRow &slot = rows.at(row);
        auto *item = new QListWidgetItem(
            QStringLiteral("%1\n%2  ·  %3 image%4")
                .arg(slot.id, slotStateText(slot.state))
                .arg(slot.imageCount)
                .arg(slot.imageCount == 1 ? QString() : QStringLiteral("s")),
            m_slotList);
        item->setData(Qt::UserRole, slot.id);
        item->setSizeHint(QSize(0, 52));
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
    if (preferredRow < 0) {
        preferredRow = fallbackRow >= 0 ? fallbackRow : (rows.isEmpty() ? -1 : 0);
    }
    if (preferredRow >= 0) {
        m_slotList->setCurrentRow(preferredRow);
        m_currentSlotId = m_slotList->item(preferredRow)->data(Qt::UserRole).toString();
    } else {
        m_currentSlotId.clear();
    }
    filterSlots(m_slotSearch->text());
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
    m_captures = buildCaptureGroups(images);
    m_summaryLabel->setText(QStringLiteral("%1  |  %2  |  Plate: %3")
                                .arg(slotId, slotStateText(state), m_plateNumber));
    m_statusLabel->setText(
        m_captures.isEmpty()
            ? QStringLiteral("No evidence images are available for the active parking session.")
            : QStringLiteral("%1 capture%2 loaded. Select a timeline row to compare it with the first capture.")
                  .arg(m_captures.size())
                  .arg(m_captures.size() == 1 ? QString() : QStringLiteral("s")));
    renderCaptureTable();
    renderFirstCapture();
    renderSelectedCapture(m_captures.size() > 1 ? m_captures.size() - 1 : -1);
}

void EvidencePage::showLoading(const QString &slotId)
{
    if (!slotId.isEmpty() && slotId != m_currentSlotId) {
        return;
    }
    ++m_requestGeneration;
    m_requestTargets.clear();
    m_captures.clear();
    m_captureTable->setRowCount(0);
    m_summaryLabel->setText(QStringLiteral("%1 evidence").arg(slotId));
    m_statusLabel->setText(QStringLiteral("Loading active session and evidence images..."));
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
    clearCaptureCard(m_firstImageLabel, m_firstTitleLabel, m_firstMetadataLabel,
                     m_firstOpenButton, QStringLiteral("First capture"),
                     QStringLiteral("Evidence request failed"));
    clearCaptureCard(m_selectedImageLabel, m_selectedTitleLabel,
                     m_selectedMetadataLabel, m_selectedOpenButton,
                     QStringLiteral("Latest capture"),
                     QStringLiteral("Evidence request failed"));
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

int EvidencePage::captureCount() const
{
    return m_captures.size();
}

void EvidencePage::requestCurrentEvidence()
{
    if (m_currentSlotId.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("No parking slot is available."));
        return;
    }
    showLoading(m_currentSlotId);
    emit evidenceRequested(m_currentSlotId);
}

QVector<EvidencePage::CaptureGroup> EvidencePage::buildCaptureGroups(
    const QList<ParkingImageResource> &images)
{
    QVector<CaptureGroup> groups;
    QHash<QString, int> groupIndexes;
    for (int index = 0; index < images.size(); ++index) {
        const ParkingImageResource &image = images.at(index);
        QString key;
        if (image.imageId >= 0) {
            key = QStringLiteral("id:%1").arg(image.imageId);
        } else if (!image.role.isEmpty()) {
            key = QStringLiteral("role:%1").arg(image.role.toUpper());
        } else if (image.timestamp.isValid()) {
            key = QStringLiteral("time:%1").arg(image.timestamp.toMSecsSinceEpoch());
        } else {
            key = QStringLiteral("item:%1").arg(index);
        }

        int groupIndex = groupIndexes.value(key, -1);
        if (groupIndex < 0) {
            CaptureGroup capture;
            capture.imageId = image.imageId;
            capture.timestamp = image.timestamp;
            capture.reason = image.evidenceReason.isEmpty() ? image.role.toUpper()
                                                             : image.evidenceReason;
            capture.ocrResult = image.ocrResult;
            groupIndex = groups.size();
            groups.append(capture);
            groupIndexes.insert(key, groupIndex);
        }
        CaptureGroup &capture = groups[groupIndex];
        capture.variants.append(image);
        if (!capture.timestamp.isValid() && image.timestamp.isValid()) {
            capture.timestamp = image.timestamp;
        }
        if (capture.ocrResult.isEmpty()) {
            capture.ocrResult = image.ocrResult;
        }
        if ((capture.reason.isEmpty() || capture.reason == QStringLiteral("EVIDENCE"))
            && !image.evidenceReason.isEmpty()) {
            capture.reason = image.evidenceReason;
        }
    }

    std::stable_sort(groups.begin(), groups.end(),
                     [](const CaptureGroup &left, const CaptureGroup &right) {
        if (left.timestamp.isValid() != right.timestamp.isValid()) {
            return left.timestamp.isValid();
        }
        if (left.timestamp.isValid() && left.timestamp != right.timestamp) {
            return left.timestamp < right.timestamp;
        }
        if (left.imageId >= 0 && right.imageId >= 0) {
            return left.imageId < right.imageId;
        }
        return false;
    });
    return groups;
}

void EvidencePage::handleSlotChanged(QListWidgetItem *current)
{
    if (!current) {
        return;
    }
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
        const CaptureGroup &capture = m_captures.at(row);
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
    const CaptureGroup *capture = m_captures.isEmpty() ? nullptr : &m_captures.first();
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

void EvidencePage::renderCaptureCard(
    const CaptureGroup *capture,
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
    const ParkingImageResource *variant = preferredVariant(capture->variants);
    const QString reason = captureReasonText(capture->reason);
    titleLabel->setText(QStringLiteral("%1 · %2").arg(heading, reason));
    QString metadata = QStringLiteral("%1  |  OCR: %2")
        .arg(captureTimeText(capture->timestamp),
             capture->ocrResult.isEmpty() ? QStringLiteral("-") : capture->ocrResult);
    if (variant && !variant->processing.isEmpty()) {
        metadata += QStringLiteral("  |  %1").arg(variant->processing);
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
