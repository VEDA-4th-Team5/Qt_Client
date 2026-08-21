#include "parkingroisettingspage.h"

#include "iva/ivavideocanvas.h"
#include "widgets/pagehelp.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QMap>
#include <QPushButton>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

#include <utility>

namespace {

// Only channels with a known Parking Area naming convention are selectable
// here. Channels without an entry are not exposed as a channel button.
const QMap<int, QStringList> &parkingRoiChannelSlotLabels()
{
    static const QMap<int, QStringList> labels{
        {0, {QStringLiteral("EV-01"), QStringLiteral("EV-02"),
             QStringLiteral("EV-03"), QStringLiteral("EV-04")}},
        {2, {QStringLiteral("P-01"), QStringLiteral("P-02"),
             QStringLiteral("P-03"), QStringLiteral("P-04")}},
    };
    return labels;
}

} // namespace

ParkingRoiSettingsPage::ParkingRoiSettingsPage(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    layout->addLayout(createPageHeader(
        this, QStringLiteral("Parking ROI"),
        QStringLiteral("Configure parking-slot crop regions")));
    createPageHelpButton(
        this, this,
        {QStringLiteral("parkingRoi"), QStringLiteral("Parking ROI"),
         QStringLiteral("Parking ROI 사용 안내"),
         QStringLiteral("선택한 채널 영상에서 Parking Area의 Pi 서버 crop 영역을 설정합니다."),
         QStringLiteral(
             "<b>1. 채널·슬롯 선택</b><br>CH1(EV-01~04) 또는 CH3(P-01~04) 채널과 슬롯을 선택합니다. 페이지를 열면 서버의 전체 ROI를 불러오며 <i>Reload from Server</i>로 선택 슬롯만 다시 읽을 수 있습니다.<br><br>"
             "<b>2. 프레임 고정</b><br><i>Freeze Current Frame</i>을 누른 뒤 영상 위에서 사각형을 드래그합니다. 새 화면이 필요하면 <i>Refresh Frame</i>을 사용합니다.<br><br>"
             "<b>3. 좌표 검토</b><br>선택 영역의 pixel 좌표와 0~1 normalized 좌표를 확인합니다. 최소 크기는 원본 프레임 기준 8×8픽셀입니다.<br><br>"
             "<b>4. 저장·검증</b><br><i>Save and Apply</i>는 normalized ROI를 Pi에 PUT하고 GET 결과가 요청값과 같은지 검증합니다. <i>Reset Selection</i>과 <i>Cancel</i>은 저장하지 않은 선택을 버립니다."),
         QStringLiteral(
             "※ 이 화면은 Pi crop ROI만 변경하며 카메라 WiseAI 규칙은 변경하지 않습니다. 이미지 자체도 Pi로 업로드하지 않습니다.\n"
             "   프레임 해상도가 없거나 서버의 검증 결과가 다르면 저장 성공으로 처리하지 않습니다.")});
    auto *description = new QLabel(
        QStringLiteral("Edit parking regions on the selected channel's shared RTSP "
                       "frame. Only normalized coordinates are sent to the Pi server."),
        this);
    description->setWordWrap(true);
    layout->addWidget(description);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    auto *videoPanel = new QWidget(splitter);
    auto *videoLayout = new QVBoxLayout(videoPanel);
    videoLayout->setContentsMargins(0, 0, 0, 0);
    auto *frameButtons = new QHBoxLayout;
    m_channelLabel = new QLabel(QStringLiteral("Camera Channel: CH1"), videoPanel);
    frameButtons->addWidget(m_channelLabel);
    auto *channelGroup = new QButtonGroup(this);
    channelGroup->setExclusive(true);
    const QList<int> availableChannels = parkingRoiChannelSlotLabels().keys();
    for (int channel : availableChannels) {
        auto *button = new QPushButton(
            QStringLiteral("CH%1").arg(channel + 1), videoPanel);
        button->setObjectName(
            QStringLiteral("parkingRoiChannel%1Button").arg(channel + 1));
        button->setCheckable(true);
        button->setChecked(channel == m_selectedChannel);
        channelGroup->addButton(button, channel);
        frameButtons->addWidget(button);
        m_channelButtons.append(button);
        connect(button, &QPushButton::clicked, this,
                [this, channel]() { selectChannel(channel); });
    }
    frameButtons->addStretch(1);
    m_freezeButton = new QPushButton(QStringLiteral("Freeze Current Frame"), videoPanel);
    m_freezeButton->setObjectName(QStringLiteral("freezeParkingRoiFrameButton"));
    m_refreshFrameButton = new QPushButton(QStringLiteral("Refresh Frame"), videoPanel);
    m_refreshFrameButton->setObjectName(QStringLiteral("refreshParkingRoiFrameButton"));
    frameButtons->addWidget(m_freezeButton);
    frameButtons->addWidget(m_refreshFrameButton);
    videoLayout->addLayout(frameButtons);

    m_videoCanvas = new IvaVideoCanvas(videoPanel);
    m_videoCanvas->setObjectName(QStringLiteral("parkingRoiVideoCanvas"));
    m_videoCanvas->setChannel(m_selectedChannel, QSize());
    videoLayout->addWidget(m_videoCanvas, 1);
    m_frameStatusLabel = new QLabel(
        QStringLiteral("Waiting for the shared CH1 RTSP frame."), videoPanel);
    m_frameStatusLabel->setObjectName(QStringLiteral("parkingRoiFrameStatusLabel"));
    m_frameStatusLabel->setWordWrap(true);
    videoLayout->addWidget(m_frameStatusLabel);
    splitter->addWidget(videoPanel);

    auto *controlGroup = new QGroupBox(QStringLiteral("ROI Editor"), splitter);
    auto *controlLayout = new QVBoxLayout(controlGroup);
    auto *form = new QFormLayout;
    m_slotCombo = new QComboBox(controlGroup);
    m_slotCombo->setObjectName(QStringLiteral("parkingRoiSlotCombo"));
    m_slotCombo->addItems(parkingRoiChannelSlotLabels().value(m_selectedChannel));
    form->addRow(QStringLiteral("Parking Slot"), m_slotCombo);
    m_currentRoiLabel = new QLabel(QStringLiteral("Not loaded"), controlGroup);
    m_currentRoiLabel->setObjectName(QStringLiteral("currentParkingRoiLabel"));
    m_currentRoiLabel->setWordWrap(true);
    form->addRow(QStringLiteral("Current ROI"), m_currentRoiLabel);
    m_selectedRoiLabel = new QLabel(QStringLiteral("No selection"), controlGroup);
    m_selectedRoiLabel->setObjectName(QStringLiteral("selectedParkingRoiLabel"));
    m_selectedRoiLabel->setWordWrap(true);
    form->addRow(QStringLiteral("Selected ROI"), m_selectedRoiLabel);
    m_pixelCoordinatesLabel = new QLabel(QStringLiteral("Not available"), controlGroup);
    m_pixelCoordinatesLabel->setObjectName(
        QStringLiteral("parkingRoiPixelCoordinatesLabel"));
    m_pixelCoordinatesLabel->setWordWrap(true);
    form->addRow(QStringLiteral("Pixel Coordinates"), m_pixelCoordinatesLabel);
    m_normalizedCoordinatesLabel = new QLabel(
        QStringLiteral("Not available"), controlGroup);
    m_normalizedCoordinatesLabel->setObjectName(
        QStringLiteral("parkingRoiNormalizedCoordinatesLabel"));
    m_normalizedCoordinatesLabel->setWordWrap(true);
    form->addRow(QStringLiteral("Normalized Coordinates"),
                 m_normalizedCoordinatesLabel);
    m_serverStatusLabel = new QLabel(
        QStringLiteral("Loading ROI settings..."), controlGroup);
    m_serverStatusLabel->setObjectName(QStringLiteral("parkingRoiServerStatusLabel"));
    m_serverStatusLabel->setWordWrap(true);
    form->addRow(QStringLiteral("Server Status"), m_serverStatusLabel);
    controlLayout->addLayout(form);
    controlLayout->addStretch(1);

    auto *secondaryButtons = new QGridLayout;
    m_reloadButton = new QPushButton(QStringLiteral("Reload from Server"), controlGroup);
    m_reloadButton->setObjectName(QStringLiteral("reloadParkingRoiButton"));
    m_resetButton = new QPushButton(QStringLiteral("Reset Selection"), controlGroup);
    m_resetButton->setObjectName(QStringLiteral("resetParkingRoiSelectionButton"));
    m_saveButton = new QPushButton(QStringLiteral("Save and Apply"), controlGroup);
    m_saveButton->setObjectName(QStringLiteral("saveParkingRoiButton"));
    m_cancelButton = new QPushButton(QStringLiteral("Cancel"), controlGroup);
    m_cancelButton->setObjectName(QStringLiteral("cancelParkingRoiButton"));
    secondaryButtons->addWidget(m_reloadButton, 0, 0);
    secondaryButtons->addWidget(m_resetButton, 0, 1);
    secondaryButtons->addWidget(m_cancelButton, 1, 0);
    secondaryButtons->addWidget(m_saveButton, 1, 1);
    controlLayout->addLayout(secondaryButtons);
    splitter->addWidget(controlGroup);
    splitter->setStretchFactor(0, 4);
    splitter->setStretchFactor(1, 2);
    layout->addWidget(splitter, 1);

    connect(m_slotCombo, &QComboBox::currentIndexChanged,
            this, [this]() { handleSlotChanged(); });
    connect(m_freezeButton, &QPushButton::clicked,
            this, &ParkingRoiSettingsPage::freezeCurrentFrame);
    connect(m_refreshFrameButton, &QPushButton::clicked,
            this, &ParkingRoiSettingsPage::refreshFrame);
    connect(m_reloadButton, &QPushButton::clicked,
            this, &ParkingRoiSettingsPage::requestSelectedRoi);
    connect(m_resetButton, &QPushButton::clicked, this, [this]() {
        resetSelection(QStringLiteral("ROI selection was reset."));
    });
    connect(m_cancelButton, &QPushButton::clicked, this, [this]() {
        resetSelection(QStringLiteral("Unsaved ROI selection was discarded."));
        m_frozen = false;
        m_videoCanvas->setDrawMode(false);
        m_frameStatusLabel->setText(
            QStringLiteral("Live shared CH%1 frame preview.")
                .arg(m_selectedChannel + 1));
        updateButtons();
    });
    connect(m_saveButton, &QPushButton::clicked, this, [this]() {
        if (m_saveInFlight || !m_hasSelection) return;
        QString error;
        if (!m_selectedRoi.isValid(&error)) {
            setStatus(error, true);
            return;
        }
        if (!m_currentFrame.size().isValid()) {
            setStatus(QStringLiteral(
                "Unable to validate the ROI because the source frame size is unavailable."),
                true);
            return;
        }
        if (!m_selectedRoi.isLargeEnough(m_currentFrame.size())) {
            setStatus(QStringLiteral(
                "ROI is too small. Please select the area again.\n"
                "The selected area is too small. Please select an area of at least 8 × 8 pixels."),
                true);
            return;
        }
        m_saveInFlight = true;
        const quint64 generation = nextGeneration();
        setStatus(QStringLiteral("Saving ROI settings..."));
        updateButtons();
        emit roiSaveRequested(selectedSlotId(), m_selectedRoi, generation);
    });
    connect(m_videoCanvas, &IvaVideoCanvas::rectangleDrafted,
            this, [this](const QRectF &sourceRectangle) {
        const QRectF normalized = IvaVideoCanvas::normalizedFromSource(
            sourceRectangle, m_videoCanvas->sourceResolution());
        const ParkingRoi roi = ParkingRoi::fromRectangle(normalized);
        QString error;
        if (!roi.isValid(&error)) {
            setStatus(error, true);
            return;
        }
        m_selectedRoi = roi;
        m_hasSelection = true;
        m_videoCanvas->setDrawMode(false);
        updateCanvasOverlays();
        updateCoordinateLabels();
        setStatus(QStringLiteral("Drag complete. Review the selected ROI, then save."));
        updateButtons();
    });
    connect(m_videoCanvas, &IvaVideoCanvas::rectangleRejected,
            this, [this](const QString &message) {
        setStatus(message == QStringLiteral("The selected area is too small.")
                      ? QStringLiteral("ROI is too small. Please select the area again.\n"
                                       "The selected area is too small. Please select an area of at least 8 × 8 pixels.")
                      : message,
                  true);
    });
    connect(m_videoCanvas, &IvaVideoCanvas::frameCompatibilityChanged,
            this, [this](bool compatible, const QString &message) {
        m_frameStatusLabel->setText(message);
        m_frameStatusLabel->setStyleSheet(
            compatible ? QStringLiteral("color:#1b5e20;")
                       : QStringLiteral("color:#b71c1c;"));
        updateButtons();
    });

    m_previewTimer = new QTimer(this);
    m_previewTimer->setInterval(100);
    connect(m_previewTimer, &QTimer::timeout, this, [this]() {
        if (isVisible() && !m_frozen) emit previewFrameRequested(m_selectedChannel);
    });
    updateButtons();
}

void ParkingRoiSettingsPage::setRoiList(const ParkingRoiMap &rois,
                                        quint64 generation)
{
    if (generation != m_generation) return;
    m_loadInFlight = false;
    m_rois = rois;
    m_hasSelection = false;
    updateCanvasOverlays();
    updateCoordinateLabels();
    setStatus(QStringLiteral("ROI settings loaded."), false, true);
    updateButtons();
}

void ParkingRoiSettingsPage::setRoi(const QString &slotId,
                                    const ParkingRoi &roi,
                                    quint64 generation,
                                    bool afterSave,
                                    bool appliedImmediately)
{
    if (generation != m_generation || slotId != selectedSlotId()) return;
    m_loadInFlight = false;
    m_saveInFlight = false;
    m_rois.insert(slotId, roi);
    m_hasSelection = false;
    updateCanvasOverlays();
    updateCoordinateLabels();
    if (afterSave) {
        setStatus(appliedImmediately
                      ? QStringLiteral("%1 ROI was applied immediately.").arg(slotId)
                      : QStringLiteral("%1 ROI was saved.").arg(slotId),
                  false, true);
    } else {
        setStatus(QStringLiteral("Server settings were reloaded."), false, true);
    }
    updateButtons();
}

void ParkingRoiSettingsPage::setRequestError(const QString &slotId,
                                             const QString &message,
                                             quint64 generation,
                                             bool saveRequest)
{
    if (generation != m_generation
        || (!slotId.isEmpty() && slotId != selectedSlotId())) {
        return;
    }
    m_loadInFlight = false;
    m_saveInFlight = false;
    if (saveRequest) {
        m_hasSelection = false;
        updateCanvasOverlays();
        updateCoordinateLabels();
    }
    setStatus(friendlyError(message), true);
    updateButtons();
}

void ParkingRoiSettingsPage::setPreviewFrame(int channel, const QImage &frame)
{
    if (channel != m_selectedChannel || frame.isNull()
        || (m_frozen && !m_refreshPending)) {
        return;
    }
    const bool resolutionChanged = frame.size() != m_videoCanvas->sourceResolution();
    m_currentFrame = frame;
    if (resolutionChanged) {
        m_videoCanvas->setChannel(channel, frame.size());
    }
    m_videoCanvas->setFrame(frame);
    updateCanvasOverlays();
    updateCoordinateLabels();
    if (m_refreshPending) {
        m_refreshPending = false;
        m_frozen = true;
        m_videoCanvas->setDrawMode(true);
        m_frameStatusLabel->setText(QStringLiteral("Frame was refreshed."));
    }
    updateButtons();
}

void ParkingRoiSettingsPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    m_previewTimer->start();
    emit previewFrameRequested(m_selectedChannel);
    if (!m_loadInFlight && !m_saveInFlight) requestAllRois();
}

void ParkingRoiSettingsPage::hideEvent(QHideEvent *event)
{
    m_previewTimer->stop();
    QWidget::hideEvent(event);
}

quint64 ParkingRoiSettingsPage::nextGeneration()
{
    return ++m_generation;
}

QString ParkingRoiSettingsPage::selectedSlotId() const
{
    return m_slotCombo->currentText();
}

void ParkingRoiSettingsPage::requestAllRois()
{
    m_loadInFlight = true;
    const quint64 generation = nextGeneration();
    setStatus(QStringLiteral("Loading ROI settings..."));
    updateButtons();
    emit roiListRequested(generation);
}

void ParkingRoiSettingsPage::requestSelectedRoi()
{
    if (m_saveInFlight) return;
    m_loadInFlight = true;
    m_hasSelection = false;
    const quint64 generation = nextGeneration();
    updateCanvasOverlays();
    updateCoordinateLabels();
    setStatus(QStringLiteral("Loading ROI settings..."));
    updateButtons();
    emit roiRequested(selectedSlotId(), generation);
}

void ParkingRoiSettingsPage::handleSlotChanged()
{
    if (m_saveInFlight) return;
    m_hasSelection = false;
    updateCanvasOverlays();
    updateCoordinateLabels();
    requestSelectedRoi();
}

void ParkingRoiSettingsPage::freezeCurrentFrame()
{
    if (m_currentFrame.isNull()) {
        setStatus(QStringLiteral("No CH%1 frame is available.")
                      .arg(m_selectedChannel + 1), true);
        return;
    }
    m_frozen = true;
    m_videoCanvas->setDrawMode(true);
    m_frameStatusLabel->setText(QStringLiteral("Current frame was frozen."));
    setStatus(QStringLiteral("Drag on the image to select a parking area."));
    updateButtons();
}

void ParkingRoiSettingsPage::refreshFrame()
{
    m_refreshPending = true;
    m_frozen = false;
    m_videoCanvas->setDrawMode(false);
    m_frameStatusLabel->setText(
        QStringLiteral("Refreshing the shared CH%1 frame...")
            .arg(m_selectedChannel + 1));
    emit previewFrameRequested(m_selectedChannel);
    updateButtons();
}

void ParkingRoiSettingsPage::selectChannel(int channel)
{
    if (channel < 0 || channel == m_selectedChannel
        || !parkingRoiChannelSlotLabels().contains(channel)) {
        return;
    }
    m_selectedChannel = channel;
    for (QPushButton *button : std::as_const(m_channelButtons)) {
        button->setChecked(button->objectName()
                            == QStringLiteral("parkingRoiChannel%1Button")
                                   .arg(channel + 1));
    }
    m_channelLabel->setText(
        QStringLiteral("Camera Channel: CH%1").arg(channel + 1));
    if (m_slotCombo) {
        const QSignalBlocker blocker(m_slotCombo);
        m_slotCombo->clear();
        m_slotCombo->addItems(parkingRoiChannelSlotLabels().value(channel));
    }
    m_currentFrame = QImage();
    m_frozen = false;
    m_refreshPending = false;
    m_videoCanvas->setDrawMode(false);
    m_videoCanvas->setChannel(channel, QSize());
    m_frameStatusLabel->setText(
        QStringLiteral("Waiting for the shared CH%1 RTSP frame.")
            .arg(channel + 1));
    resetSelection(QStringLiteral("Switched to CH%1.").arg(channel + 1));
    if (isVisible()) emit previewFrameRequested(m_selectedChannel);
    requestSelectedRoi();
}

void ParkingRoiSettingsPage::resetSelection(const QString &statusMessage)
{
    m_hasSelection = false;
    m_videoCanvas->setDrawMode(m_frozen && !m_currentFrame.isNull());
    updateCanvasOverlays();
    updateCoordinateLabels();
    setStatus(statusMessage);
    updateButtons();
}

void ParkingRoiSettingsPage::updateCanvasOverlays()
{
    const QString slotId = selectedSlotId();
    const QRectF saved = m_rois.contains(slotId)
        ? m_rois.value(slotId).rectangle() : QRectF();
    const QRectF selected = m_hasSelection
        ? m_selectedRoi.rectangle() : QRectF();
    m_videoCanvas->setParkingRoiOverlays(saved, selected);
}

void ParkingRoiSettingsPage::updateCoordinateLabels()
{
    const QString slotId = selectedSlotId();
    if (m_rois.contains(slotId)) {
        m_currentRoiLabel->setText(normalizedText(m_rois.value(slotId)));
    } else {
        m_currentRoiLabel->setText(QStringLiteral("Not loaded"));
    }
    if (m_hasSelection) {
        m_selectedRoiLabel->setText(normalizedText(m_selectedRoi));
        m_normalizedCoordinatesLabel->setText(normalizedText(m_selectedRoi));
        m_pixelCoordinatesLabel->setText(
            pixelText(m_selectedRoi, m_currentFrame.size()));
    } else {
        m_selectedRoiLabel->setText(QStringLiteral("No selection"));
        m_normalizedCoordinatesLabel->setText(QStringLiteral("Not available"));
        m_pixelCoordinatesLabel->setText(QStringLiteral("Not available"));
    }
}

void ParkingRoiSettingsPage::updateButtons()
{
    const bool hasFrame = !m_currentFrame.isNull();
    m_freezeButton->setEnabled(hasFrame && !m_frozen && !m_saveInFlight);
    m_refreshFrameButton->setEnabled(!m_saveInFlight && !m_refreshPending);
    m_reloadButton->setEnabled(!m_loadInFlight && !m_saveInFlight);
    m_resetButton->setEnabled(m_hasSelection && !m_saveInFlight);
    m_saveButton->setEnabled(m_hasSelection && !m_saveInFlight
                             && m_rois.contains(selectedSlotId()));
    m_cancelButton->setEnabled((m_hasSelection || m_frozen) && !m_saveInFlight);
    m_slotCombo->setEnabled(!m_saveInFlight);
    for (QPushButton *button : std::as_const(m_channelButtons)) {
        button->setEnabled(!m_saveInFlight);
    }
}

void ParkingRoiSettingsPage::setStatus(const QString &message,
                                       bool error, bool success)
{
    m_serverStatusLabel->setText(message);
    m_serverStatusLabel->setStyleSheet(
        error ? QStringLiteral("color:#b71c1c;font-weight:700;")
              : (success ? QStringLiteral("color:#1b5e20;font-weight:700;")
                         : QStringLiteral("color:#455a64;")));
}

QString ParkingRoiSettingsPage::normalizedText(const ParkingRoi &roi)
{
    return QStringLiteral("x=%1, y=%2, width=%3, height=%4")
        .arg(roi.x, 0, 'f', 6).arg(roi.y, 0, 'f', 6)
        .arg(roi.width, 0, 'f', 6).arg(roi.height, 0, 'f', 6);
}

QString ParkingRoiSettingsPage::pixelText(const ParkingRoi &roi,
                                          const QSize &frameSize)
{
    if (!frameSize.isValid()) return QStringLiteral("Not available");
    const QRectF rectangle = IvaVideoCanvas::sourceFromNormalized(
        roi.rectangle(), frameSize);
    return QStringLiteral("x=%1, y=%2, width=%3, height=%4 px")
        .arg(qRound(rectangle.x())).arg(qRound(rectangle.y()))
        .arg(qRound(rectangle.width())).arg(qRound(rectangle.height()));
}

QString ParkingRoiSettingsPage::friendlyError(const QString &message)
{
    if (message.contains(QStringLiteral("timed out"), Qt::CaseInsensitive)) {
        return QStringLiteral("The request timed out.");
    }
    if (message.contains(QStringLiteral("HTTP 404"))) {
        return QStringLiteral("Parking slot was not found.");
    }
    if (message.contains(QStringLiteral("HTTP 400"))) {
        return QStringLiteral("The server rejected the ROI settings. %1").arg(message);
    }
    if (message.contains(QStringLiteral("HTTP 500"))) {
        return QStringLiteral("Failed to save ROI settings. %1").arg(message);
    }
    if (message.contains(QStringLiteral("Invalid JSON"), Qt::CaseInsensitive)
        || (message.contains(QStringLiteral("ROI"), Qt::CaseInsensitive)
            && message.contains(QStringLiteral("response"), Qt::CaseInsensitive))) {
        return QStringLiteral("Failed to load ROI settings. %1").arg(message);
    }
    return QStringLiteral("Failed to connect to the Pi server. %1").arg(message);
}
