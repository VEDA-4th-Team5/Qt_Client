#include "ivasettingspage.h"

#include "iva/ivavideocanvas.h"

#include <QAbstractItemView>
#include <QButtonGroup>
#include <QCheckBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHideEvent>
#include <QImage>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QShowEvent>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>

IvaSettingsPage::IvaSettingsPage(const QString &cameraIp, QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    auto *title = new QLabel(QStringLiteral("Hanwha WiseAI IVA Configuration"), this);
    title->setStyleSheet(QStringLiteral("font-size:20px;font-weight:800;color:#202124;"));
    layout->addWidget(title);

    auto *description = new QLabel(
        QStringLiteral("Rules are read and written directly over HTTPS Digest. "
                       "Apply performs a fresh conflict check, PUT, camera re-read, "
                       "and last-good rollback when verification differs."),
        this);
    description->setWordWrap(true);
    layout->addWidget(description);

    m_cameraLabel = new QLabel(this);
    m_cameraLabel->setObjectName(QStringLiteral("ivaCameraAddressLabel"));
    m_channelSummaryLabel = new QLabel(QStringLiteral("Not loaded"), this);
    m_channelSummaryLabel->setObjectName(QStringLiteral("ivaChannelSummaryLabel"));
    layout->addWidget(m_cameraLabel);
    layout->addWidget(m_channelSummaryLabel);

    auto *workspaceSplitter = new QSplitter(Qt::Horizontal, this);
    auto *videoPanel = new QWidget(workspaceSplitter);
    auto *videoLayout = new QVBoxLayout(videoPanel);
    videoLayout->setContentsMargins(0, 0, 0, 0);
    auto *channelRow = new QHBoxLayout;
    auto *channelGroup = new QButtonGroup(this);
    channelGroup->setExclusive(true);
    for (int channel = 0; channel < 4; ++channel) {
        auto *button = new QPushButton(QStringLiteral("CH%1").arg(channel + 1),
                                       videoPanel);
        button->setObjectName(QStringLiteral("ivaPreviewChannel%1Button").arg(channel + 1));
        button->setCheckable(true);
        button->setProperty("channel", channel);
        channelGroup->addButton(button, channel);
        channelRow->addWidget(button);
        m_channelButtons.append(button);
        connect(button, &QPushButton::clicked, this,
                [this, channel]() { selectChannel(channel); });
    }
    m_channelButtons.constFirst()->setChecked(true);
    channelRow->addStretch(1);
    m_drawRectangleButton = new QPushButton(QStringLiteral("Draw rectangle"), videoPanel);
    m_drawRectangleButton->setObjectName(QStringLiteral("ivaDrawRectangleButton"));
    m_drawRectangleButton->setCheckable(true);
    m_discardDraftButton = new QPushButton(QStringLiteral("Discard draft"), videoPanel);
    m_discardDraftButton->setObjectName(QStringLiteral("ivaDiscardDraftButton"));
    channelRow->addWidget(m_drawRectangleButton);
    channelRow->addWidget(m_discardDraftButton);
    videoLayout->addLayout(channelRow);
    m_videoCanvas = new IvaVideoCanvas(videoPanel);
    videoLayout->addWidget(m_videoCanvas, 1);
    m_frameStatusLabel = new QLabel(
        QStringLiteral("Refresh the camera, then select a channel."), videoPanel);
    m_frameStatusLabel->setObjectName(QStringLiteral("ivaFrameStatusLabel"));
    m_frameStatusLabel->setWordWrap(true);
    videoLayout->addWidget(m_frameStatusLabel);
    workspaceSplitter->addWidget(videoPanel);

    auto *splitter = new QSplitter(Qt::Vertical, workspaceSplitter);
    m_areaTable = new QTableWidget(0, 8, splitter);
    m_areaTable->setObjectName(QStringLiteral("ivaAreaTable"));
    m_areaTable->setHorizontalHeaderLabels(
        {QStringLiteral("Channel"), QStringLiteral("Enabled"),
         QStringLiteral("Index"), QStringLiteral("Rule name"),
         QStringLiteral("Detection modes"), QStringLiteral("Object filters"),
         QStringLiteral("Points"), QStringLiteral("Durations (s)")});
    m_areaTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_areaTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_areaTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_areaTable->setAlternatingRowColors(true);
    m_areaTable->verticalHeader()->setVisible(false);
    m_areaTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_areaTable->horizontalHeader()->setStretchLastSection(true);

    auto *editorGroup = new QGroupBox(QStringLiteral("Selected rule editor"), splitter);
    editorGroup->setObjectName(QStringLiteral("ivaRuleEditor"));
    auto *editorLayout = new QGridLayout(editorGroup);

    m_channelEnabledCheck = new QCheckBox(QStringLiteral("Channel enabled"), editorGroup);
    m_channelEnabledCheck->setObjectName(QStringLiteral("ivaChannelEnabledCheck"));
    m_indexSpin = new QSpinBox(editorGroup);
    m_indexSpin->setObjectName(QStringLiteral("ivaRuleIndexSpin"));
    m_indexSpin->setRange(0, 999);
    m_nameEdit = new QLineEdit(editorGroup);
    m_nameEdit->setObjectName(QStringLiteral("ivaRuleNameEdit"));
    editorLayout->addWidget(m_channelEnabledCheck, 0, 0);
    editorLayout->addWidget(new QLabel(QStringLiteral("Rule index"), editorGroup), 0, 1);
    editorLayout->addWidget(m_indexSpin, 0, 2);
    editorLayout->addWidget(new QLabel(QStringLiteral("Rule name"), editorGroup), 0, 3);
    editorLayout->addWidget(m_nameEdit, 0, 4);

    m_detectionModesList = new QListWidget(editorGroup);
    m_detectionModesList->setObjectName(QStringLiteral("ivaDetectionModesList"));
    m_objectFiltersList = new QListWidget(editorGroup);
    m_objectFiltersList->setObjectName(QStringLiteral("ivaObjectFiltersList"));
    editorLayout->addWidget(new QLabel(QStringLiteral("Detection modes"), editorGroup),
                            1, 0, 1, 2);
    editorLayout->addWidget(new QLabel(QStringLiteral("Object filters"), editorGroup),
                            1, 2, 1, 2);
    editorLayout->addWidget(m_detectionModesList, 2, 0, 1, 2);
    editorLayout->addWidget(m_objectFiltersList, 2, 2, 1, 2);

    auto *durationWidget = new QWidget(editorGroup);
    auto *durationLayout = new QFormLayout(durationWidget);
    durationLayout->setContentsMargins(0, 0, 0, 0);
    m_appearanceDurationSpin = new QSpinBox(durationWidget);
    m_appearanceDurationSpin->setObjectName(QStringLiteral("ivaAppearanceDurationSpin"));
    m_intrusionDurationSpin = new QSpinBox(durationWidget);
    m_intrusionDurationSpin->setObjectName(QStringLiteral("ivaIntrusionDurationSpin"));
    m_loiteringDurationSpin = new QSpinBox(durationWidget);
    m_loiteringDurationSpin->setObjectName(QStringLiteral("ivaLoiteringDurationSpin"));
    for (QSpinBox *spin : {m_appearanceDurationSpin, m_intrusionDurationSpin,
                           m_loiteringDurationSpin}) {
        spin->setRange(0, 3600);
        spin->setSuffix(QStringLiteral(" s"));
    }
    durationLayout->addRow(QStringLiteral("Appearance"), m_appearanceDurationSpin);
    durationLayout->addRow(QStringLiteral("Intrusion"), m_intrusionDurationSpin);
    durationLayout->addRow(QStringLiteral("Loitering"), m_loiteringDurationSpin);
    editorLayout->addWidget(durationWidget, 2, 4);

    m_coordinateTable = new QTableWidget(0, 2, editorGroup);
    m_coordinateTable->setObjectName(QStringLiteral("ivaCoordinateTable"));
    m_coordinateTable->setHorizontalHeaderLabels(
        {QStringLiteral("X"), QStringLiteral("Y")});
    m_coordinateTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_coordinateTable->verticalHeader()->setDefaultSectionSize(24);
    auto *coordinateButtons = new QHBoxLayout;
    m_addPointButton = new QPushButton(QStringLiteral("Add point"), editorGroup);
    m_addPointButton->setObjectName(QStringLiteral("ivaAddPointButton"));
    m_removePointButton = new QPushButton(QStringLiteral("Remove point"), editorGroup);
    m_removePointButton->setObjectName(QStringLiteral("ivaRemovePointButton"));
    coordinateButtons->addWidget(m_addPointButton);
    coordinateButtons->addWidget(m_removePointButton);
    coordinateButtons->addStretch(1);
    auto *coordinateWidget = new QWidget(editorGroup);
    auto *coordinateLayout = new QVBoxLayout(coordinateWidget);
    coordinateLayout->setContentsMargins(0, 0, 0, 0);
    coordinateLayout->addWidget(new QLabel(
        QStringLiteral("Polygon points (camera pixel coordinates)"), coordinateWidget));
    coordinateLayout->addWidget(m_coordinateTable);
    coordinateLayout->addLayout(coordinateButtons);
    editorLayout->addWidget(coordinateWidget, 3, 0, 1, 5);

    m_applyButton = new QPushButton(QStringLiteral("Apply selected rule to camera"),
                                    editorGroup);
    m_applyButton->setObjectName(QStringLiteral("applyIvaConfigurationButton"));
    editorLayout->addWidget(m_applyButton, 4, 0, 1, 5, Qt::AlignRight);
    splitter->addWidget(m_areaTable);
    splitter->addWidget(editorGroup);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    workspaceSplitter->addWidget(splitter);
    workspaceSplitter->setStretchFactor(0, 3);
    workspaceSplitter->setStretchFactor(1, 2);
    layout->addWidget(workspaceSplitter, 1);

    m_statusLabel = new QLabel(
        QStringLiteral("Open this page or press Refresh to read the camera."), this);
    m_statusLabel->setObjectName(QStringLiteral("ivaStatusLabel"));
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    m_refreshButton = new QPushButton(QStringLiteral("Refresh from camera"), this);
    m_refreshButton->setObjectName(QStringLiteral("refreshIvaConfigurationButton"));
    layout->addWidget(m_refreshButton, 0, Qt::AlignRight);

    connect(m_refreshButton, &QPushButton::clicked, this, [this]() {
        if (m_requestInFlight) return;
        setRequestStarted();
        emit refreshRequested();
    });
    connect(m_areaTable, &QTableWidget::currentCellChanged,
            this, [this](int currentRow) { populateEditor(currentRow); });
    connect(m_addPointButton, &QPushButton::clicked, this, [this]() {
        double x = 0.0;
        double y = 0.0;
        const int lastRow = m_coordinateTable->rowCount() - 1;
        if (lastRow >= 0) {
            x = m_coordinateTable->item(lastRow, 0)->text().toDouble();
            y = m_coordinateTable->item(lastRow, 1)->text().toDouble();
        }
        addCoordinateRow(x, y);
        updateButtons();
    });
    connect(m_removePointButton, &QPushButton::clicked, this, [this]() {
        const int row = m_coordinateTable->currentRow();
        if (row >= 0) {
            m_coordinateTable->removeRow(row);
        }
        updateButtons();
    });
    connect(m_coordinateTable, &QTableWidget::currentCellChanged,
            this, [this]() { updateButtons(); });
    connect(m_drawRectangleButton, &QPushButton::toggled,
            m_videoCanvas, &IvaVideoCanvas::setDrawMode);
    connect(m_discardDraftButton, &QPushButton::clicked,
            this, &IvaSettingsPage::discardRectangleDraft);
    connect(m_videoCanvas, &IvaVideoCanvas::rectangleDrafted,
            this, [this](const QRectF &rectangle) {
        m_drawRectangleButton->setChecked(false);
        createRectangleDraft(rectangle);
    });
    connect(m_videoCanvas, &IvaVideoCanvas::areaSelected,
            this, [this](int areaIndex) {
        for (int row = 0; row < m_configuration.areas.size(); ++row) {
            const IvaAreaDefinition &area = m_configuration.areas.at(row);
            if (area.channel == m_selectedChannel && area.areaIndex == areaIndex) {
                m_areaTable->selectRow(row);
                return;
            }
        }
    });
    connect(m_videoCanvas, &IvaVideoCanvas::frameCompatibilityChanged,
            this, [this](bool compatible, const QString &message) {
        m_frameStatusLabel->setText(message);
        m_frameStatusLabel->setStyleSheet(
            compatible ? QStringLiteral("color:#1b5e20;")
                       : QStringLiteral("color:#b71c1c;"));
        updateButtons();
    });
    connect(m_applyButton, &QPushButton::clicked, this, [this]() {
        IvaAreaDefinition edited;
        QString errorMessage;
        if (!collectEditedArea(edited, errorMessage)) {
            QMessageBox::warning(this, QStringLiteral("IVA validation"), errorMessage);
            return;
        }
        const QMessageBox::StandardButton result = QMessageBox::question(
            this, QStringLiteral("Apply IVA configuration"),
            QStringLiteral("Write CH%1 rule %2 (%3) to camera %4?\n\n"
                           "The client will re-read the camera and roll back if verification differs.")
                .arg(edited.channel + 1)
                .arg(edited.areaIndex)
                .arg(edited.name)
                .arg(m_cameraIp),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (result != QMessageBox::Yes) {
            return;
        }
        QList<IvaAreaDefinition> channelAreas;
        for (int index = 0; index < m_configuration.areas.size(); ++index) {
            const IvaAreaDefinition &area = m_configuration.areas.at(index);
            if (area.channel != edited.channel) {
                continue;
            }
            channelAreas.append(index == m_selectedArea ? edited : area);
        }
        emit applyRequested(edited.channel, m_channelEnabledCheck->isChecked(),
                            channelAreas);
    });

    setCameraIp(cameraIp);
    m_previewTimer = new QTimer(this);
    m_previewTimer->setInterval(100);
    connect(m_previewTimer, &QTimer::timeout, this, [this]() {
        if (isVisible()) emit previewFrameRequested(m_selectedChannel);
    });
    selectChannel(0);
    clearEditor();
    updateButtons();
}

void IvaSettingsPage::setCameraIp(const QString &cameraIp)
{
    const QString normalized = cameraIp.trimmed();
    if (!m_cameraIp.isEmpty() && normalized != m_cameraIp) {
        m_loadedOnce = false;
        m_hasOptions = false;
        m_hasCapabilities = false;
        m_configuration = {};
        m_options = {};
        m_capabilities = {};
        m_draftChannel = -1;
        m_draftAreaIndex = -1;
        m_areaTable->setRowCount(0);
        m_channelSummaryLabel->setText(QStringLiteral("Not loaded"));
        clearEditor();
    }
    m_cameraIp = normalized;
    m_cameraLabel->setText(
        normalized.isEmpty()
            ? QStringLiteral("Camera: not configured")
            : QStringLiteral("Camera: https://%1").arg(normalized));
}

void IvaSettingsPage::setRequestStarted()
{
    m_requestInFlight = true;
    m_statusLabel->setText(QStringLiteral(
        "Loading WiseAI IVA options, capabilities, and configuration..."));
    m_statusLabel->setStyleSheet(QStringLiteral("color:#455a64;"));
    updateButtons();
}

void IvaSettingsPage::setOptions(const IvaAreaOptions &options)
{
    m_options = options;
    m_hasOptions = true;
    if (m_selectedArea >= 0) {
        populateEditor(m_areaTable->currentRow());
    }
    updateButtons();
}

void IvaSettingsPage::setCapabilities(
    const WiseAiCapabilities &capabilities)
{
    m_capabilities = capabilities;
    m_hasCapabilities = true;
    selectChannel(m_selectedChannel);
    updateButtons();
}

void IvaSettingsPage::setConfiguration(
    const IvaAreaConfiguration &configuration)
{
    int selectedChannel = -1;
    int selectedIndex = -1;
    if (m_selectedArea >= 0 && m_selectedArea < m_configuration.areas.size()) {
        selectedChannel = m_configuration.areas.at(m_selectedArea).channel;
        selectedIndex = m_configuration.areas.at(m_selectedArea).areaIndex;
    }
    m_configuration = configuration;
    m_draftChannel = -1;
    m_draftAreaIndex = -1;
    populateAreaTable();

    int selectedRow = -1;
    for (int row = 0; row < m_configuration.areas.size(); ++row) {
        const IvaAreaDefinition &area = m_configuration.areas.at(row);
        if (area.channel == selectedChannel && area.areaIndex == selectedIndex) {
            selectedRow = row;
            break;
        }
    }
    if (selectedRow < 0 && !m_configuration.areas.isEmpty()) {
        selectedRow = 0;
    }
    if (selectedRow >= 0) {
        m_areaTable->selectRow(selectedRow);
        populateEditor(selectedRow);
    } else {
        clearEditor();
    }
    updateVideoOverlays();

    QStringList channels;
    for (const IvaChannelDefinition &channel : configuration.channels) {
        channels.append(QStringLiteral("CH%1 %2 (%3 areas)")
                            .arg(channel.channel + 1)
                            .arg(channel.enabled ? QStringLiteral("ON")
                                                 : QStringLiteral("OFF"))
                            .arg(channel.rawContainer
                                     .value(QStringLiteral("definedArea"))
                                     .toArray().size()));
    }
    m_channelSummaryLabel->setText(channels.join(QStringLiteral("  |  ")));
    m_requestInFlight = false;
    m_loadedOnce = true;
    m_statusLabel->setText(
        configuration.warnings.isEmpty()
            ? QStringLiteral("Camera configuration loaded and validated.")
            : QStringLiteral("Loaded with warnings: %1")
                  .arg(configuration.warnings.join(QStringLiteral(" | "))));
    m_statusLabel->setStyleSheet(
        configuration.warnings.isEmpty()
            ? QStringLiteral("color:#1b5e20;font-weight:700;")
            : QStringLiteral("color:#e65100;font-weight:700;"));
    updateButtons();
}

void IvaSettingsPage::setPreviewFrame(int channel, const QImage &frame)
{
    if (channel != m_selectedChannel || !m_videoCanvas) return;
    m_videoCanvas->setFrame(frame);
}

void IvaSettingsPage::setRequestError(const QString &message)
{
    m_requestInFlight = false;
    m_statusLabel->setText(
        QStringLiteral("Failed to load camera configuration. Last-good view retained.\n%1")
            .arg(message));
    m_statusLabel->setStyleSheet(QStringLiteral("color:#b71c1c;font-weight:700;"));
    updateButtons();
}

void IvaSettingsPage::setApplyStarted(int channel)
{
    m_requestInFlight = true;
    m_statusLabel->setText(
        QStringLiteral("Applying CH%1: conflict check -> PUT -> camera verification...")
            .arg(channel + 1));
    m_statusLabel->setStyleSheet(QStringLiteral("color:#0d47a1;font-weight:700;"));
    updateButtons();
}

void IvaSettingsPage::setApplySuccess(
    int channel,
    const IvaAreaConfiguration &verifiedConfiguration)
{
    Q_UNUSED(verifiedConfiguration)
    m_requestInFlight = false;
    m_statusLabel->setText(
        QStringLiteral("CH%1 IVA settings applied and verified from the camera. "
                       "Raspberry Pi configuration refresh is still pending.")
            .arg(channel + 1));
    m_statusLabel->setStyleSheet(QStringLiteral("color:#1b5e20;font-weight:700;"));
    updateButtons();
}

void IvaSettingsPage::setApplyError(int channel,
                                    const QString &message,
                                    bool rollbackSucceeded)
{
    m_requestInFlight = false;
    const QString prefix = channel < 0
        ? QStringLiteral("IVA apply failed")
        : QStringLiteral("CH%1 IVA apply failed").arg(channel + 1);
    m_statusLabel->setText(
        rollbackSucceeded
            ? QStringLiteral("%1; last-good camera state was restored.\n%2")
                  .arg(prefix, message)
            : QStringLiteral("%1. Refresh before another edit.\n%2")
                  .arg(prefix, message));
    m_statusLabel->setStyleSheet(QStringLiteral("color:#b71c1c;font-weight:700;"));
    updateButtons();
}

void IvaSettingsPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (m_previewTimer) {
        m_previewTimer->start();
        emit previewFrameRequested(m_selectedChannel);
    }
    if (m_loadedOnce || m_requestInFlight) return;
    setRequestStarted();
    emit refreshRequested();
}

void IvaSettingsPage::hideEvent(QHideEvent *event)
{
    if (m_previewTimer) m_previewTimer->stop();
    QWidget::hideEvent(event);
}

QString IvaSettingsPage::durationText(const IvaAreaDefinition &area)
{
    auto value = [](int seconds) {
        return seconds < 0 ? QStringLiteral("-") : QString::number(seconds);
    };
    return QStringLiteral("A:%1 / I:%2 / L:%3")
        .arg(value(area.appearanceDuration), value(area.intrusionDuration),
             value(area.loiteringDuration));
}

QString IvaSettingsPage::coordinateText(double value)
{
    return std::floor(value) == value
        ? QString::number(static_cast<qint64>(value))
        : QString::number(value, 'f', 3);
}

void IvaSettingsPage::populateAreaTable()
{
    m_areaTable->setRowCount(m_configuration.areas.size());
    for (int row = 0; row < m_configuration.areas.size(); ++row) {
        const IvaAreaDefinition &area = m_configuration.areas.at(row);
        const QStringList values{
            QStringLiteral("CH%1").arg(area.channel + 1),
            area.channelEnabled ? QStringLiteral("ON") : QStringLiteral("OFF"),
            area.areaIndex < 0 ? QStringLiteral("-") : QString::number(area.areaIndex),
            area.name,
            area.detectionModes.join(QStringLiteral(", ")),
            area.objectTypeFilter.join(QStringLiteral(", ")),
            QString::number(area.areaCoordinates.size()),
            durationText(area)};
        for (int column = 0; column < values.size(); ++column) {
            m_areaTable->setItem(row, column, new QTableWidgetItem(values.at(column)));
        }
    }
}

void IvaSettingsPage::populateEditor(int row)
{
    if (row < 0 || row >= m_configuration.areas.size()) {
        clearEditor();
        return;
    }
    m_updatingEditor = true;
    m_selectedArea = row;
    const IvaAreaDefinition &area = m_configuration.areas.at(row);
    if (area.channel != m_selectedChannel) {
        selectChannel(area.channel);
    }
    m_videoCanvas->setSelectedAreaIndex(area.areaIndex);
    const IvaChannelOptions *options = m_options.forChannel(area.channel);
    m_channelEnabledCheck->setChecked(area.channelEnabled);
    if (options) {
        m_indexSpin->setRange(options->areaIndex.minimum, options->areaIndex.maximum);
        m_appearanceDurationSpin->setRange(options->appearanceDuration.minimum,
                                           options->appearanceDuration.maximum);
        m_intrusionDurationSpin->setRange(options->intrusionDuration.minimum,
                                          options->intrusionDuration.maximum);
        m_loiteringDurationSpin->setRange(options->loiteringDuration.minimum,
                                          options->loiteringDuration.maximum);
        populateChecklist(m_detectionModesList, options->detectionModes,
                          area.detectionModes);
        populateChecklist(m_objectFiltersList, options->objectTypeFilters,
                          area.objectTypeFilter);
    } else {
        m_indexSpin->setRange(0, 999);
        populateChecklist(m_detectionModesList, area.detectionModes,
                          area.detectionModes);
        populateChecklist(m_objectFiltersList, area.objectTypeFilter,
                          area.objectTypeFilter);
    }
    m_indexSpin->setValue(area.areaIndex);
    m_nameEdit->setText(area.name);
    m_appearanceDurationSpin->setValue(qMax(0, area.appearanceDuration));
    m_intrusionDurationSpin->setValue(qMax(0, area.intrusionDuration));
    m_loiteringDurationSpin->setValue(qMax(0, area.loiteringDuration));
    m_coordinateTable->setRowCount(0);
    for (const QPointF &point : area.areaCoordinates) {
        addCoordinateRow(point.x(), point.y());
    }
    m_updatingEditor = false;
    updateButtons();
}

void IvaSettingsPage::clearEditor()
{
    m_selectedArea = -1;
    m_channelEnabledCheck->setChecked(false);
    m_indexSpin->setValue(0);
    m_nameEdit->clear();
    m_detectionModesList->clear();
    m_objectFiltersList->clear();
    m_coordinateTable->setRowCount(0);
    updateButtons();
}

void IvaSettingsPage::populateChecklist(QListWidget *list,
                                        const QStringList &available,
                                        const QStringList &selected)
{
    list->clear();
    for (const QString &value : available) {
        auto *item = new QListWidgetItem(value, list);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(selected.contains(value) ? Qt::Checked : Qt::Unchecked);
    }
}

QStringList IvaSettingsPage::checkedValues(const QListWidget *list) const
{
    QStringList values;
    for (int row = 0; row < list->count(); ++row) {
        const QListWidgetItem *item = list->item(row);
        if (item->checkState() == Qt::Checked) {
            values.append(item->text());
        }
    }
    return values;
}

bool IvaSettingsPage::collectEditedArea(IvaAreaDefinition &editedArea,
                                        QString &errorMessage) const
{
    if (m_selectedArea < 0 || m_selectedArea >= m_configuration.areas.size()) {
        errorMessage = QStringLiteral("Select an IVA rule first");
        return false;
    }
    editedArea = m_configuration.areas.at(m_selectedArea);
    editedArea.areaIndex = m_indexSpin->value();
    editedArea.name = m_nameEdit->text().trimmed();
    editedArea.detectionModes = checkedValues(m_detectionModesList);
    editedArea.objectTypeFilter = checkedValues(m_objectFiltersList);
    editedArea.appearanceDuration = m_appearanceDurationSpin->value();
    editedArea.intrusionDuration = m_intrusionDurationSpin->value();
    editedArea.loiteringDuration = m_loiteringDurationSpin->value();
    editedArea.areaCoordinates.clear();
    for (int row = 0; row < m_coordinateTable->rowCount(); ++row) {
        bool xOk = false;
        bool yOk = false;
        const double x = m_coordinateTable->item(row, 0)->text().toDouble(&xOk);
        const double y = m_coordinateTable->item(row, 1)->text().toDouble(&yOk);
        if (!xOk || !yOk || !std::isfinite(x) || !std::isfinite(y)
            || x < 0.0 || y < 0.0 || std::round(x) != x
            || std::round(y) != y) {
            errorMessage = QStringLiteral(
                "Polygon point %1 requires finite non-negative integer-pixel X and Y")
                               .arg(row + 1);
            return false;
        }
        editedArea.areaCoordinates.append(QPointF(x, y));
    }
    if (editedArea.name.isEmpty()) {
        errorMessage = QStringLiteral("Rule name cannot be empty");
        return false;
    }
    errorMessage.clear();
    return true;
}

void IvaSettingsPage::addCoordinateRow(double x, double y)
{
    const int row = m_coordinateTable->rowCount();
    m_coordinateTable->insertRow(row);
    m_coordinateTable->setItem(row, 0, new QTableWidgetItem(coordinateText(x)));
    m_coordinateTable->setItem(row, 1, new QTableWidgetItem(coordinateText(y)));
}

void IvaSettingsPage::selectChannel(int channel)
{
    if (channel < 0) return;
    m_selectedChannel = channel;
    for (int index = 0; index < m_channelButtons.size(); ++index) {
        m_channelButtons.at(index)->setChecked(index == channel);
    }
    const IvaChannelCapability *capability = m_capabilities.forChannel(channel);
    const QSize coordinateResolution = capability && capability->ivaAreaSupported
        ? capability->maxResolution : QSize();
    m_videoCanvas->setChannel(channel, coordinateResolution);
    updateVideoOverlays();
    if (m_previewTimer && m_previewTimer->isActive()) {
        emit previewFrameRequested(channel);
    }
    updateButtons();
}

void IvaSettingsPage::updateVideoOverlays()
{
    if (!m_videoCanvas) return;
    m_videoCanvas->setAreas(m_configuration.areas);
    if (m_selectedArea >= 0 && m_selectedArea < m_configuration.areas.size()) {
        const IvaAreaDefinition &area = m_configuration.areas.at(m_selectedArea);
        if (area.channel == m_selectedChannel) {
            m_videoCanvas->setSelectedAreaIndex(area.areaIndex);
        }
    }
}

void IvaSettingsPage::createRectangleDraft(const QRectF &sourceRectangle)
{
    if (m_draftChannel >= 0) {
        m_statusLabel->setText(QStringLiteral(
            "Discard or apply the existing rectangle draft before creating another."));
        m_statusLabel->setStyleSheet(QStringLiteral("color:#e65100;font-weight:700;"));
        return;
    }
    const IvaChannelOptions *options = m_options.forChannel(m_selectedChannel);
    const IvaChannelCapability *capability = m_capabilities.forChannel(
        m_selectedChannel);
    if (!options || !capability || !capability->ivaAreaSupported
        || !capability->maxResolution.isValid()) {
        m_statusLabel->setText(QStringLiteral(
            "The selected channel does not provide usable IVA options/capability."));
        m_statusLabel->setStyleSheet(QStringLiteral("color:#b71c1c;font-weight:700;"));
        return;
    }
    if (!options->areaCoordinateCount.contains(4)) {
        m_statusLabel->setText(QStringLiteral(
            "The selected camera channel does not accept four-point rectangles."));
        m_statusLabel->setStyleSheet(QStringLiteral("color:#b71c1c;font-weight:700;"));
        return;
    }

    QSet<int> usedIndexes;
    for (const IvaAreaDefinition &area : m_configuration.areas) {
        if (area.channel == m_selectedChannel) usedIndexes.insert(area.areaIndex);
    }
    int freeIndex = -1;
    for (int index = options->areaIndex.minimum;
         index <= options->areaIndex.maximum; ++index) {
        if (!usedIndexes.contains(index)) {
            freeIndex = index;
            break;
        }
    }
    if (freeIndex < 0) {
        m_statusLabel->setText(QStringLiteral(
            "CH%1 already uses every camera-supported IVA rule index (%2-%3).")
                                   .arg(m_selectedChannel + 1)
                                   .arg(options->areaIndex.minimum)
                                   .arg(options->areaIndex.maximum));
        m_statusLabel->setStyleSheet(QStringLiteral("color:#b71c1c;font-weight:700;"));
        return;
    }

    bool channelEnabled = false;
    for (const IvaChannelDefinition &channel : m_configuration.channels) {
        if (channel.channel == m_selectedChannel) {
            channelEnabled = channel.enabled;
            break;
        }
    }
    IvaAreaDefinition draft;
    draft.channel = m_selectedChannel;
    draft.channelEnabled = channelEnabled;
    draft.areaIndex = freeIndex;
    draft.name = QStringLiteral("IVA-CH%1-%2")
                     .arg(m_selectedChannel + 1)
                     .arg(freeIndex);
    draft.areaCoordinates = IvaVideoCanvas::rectangleCoordinates(sourceRectangle);
    draft.appearanceDuration = options->appearanceDuration.minimum;
    draft.intrusionDuration = options->intrusionDuration.minimum;
    draft.loiteringDuration = options->loiteringDuration.minimum;
    m_configuration.areas.append(draft);
    m_draftChannel = m_selectedChannel;
    m_draftAreaIndex = freeIndex;
    populateAreaTable();
    const int row = m_configuration.areas.size() - 1;
    m_areaTable->selectRow(row);
    populateEditor(row);
    updateVideoOverlays();
    m_statusLabel->setText(QStringLiteral(
        "Rectangle draft created for CH%1/index %2. Choose detection modes and object filters, then Apply.")
                               .arg(m_selectedChannel + 1)
                               .arg(freeIndex));
    m_statusLabel->setStyleSheet(QStringLiteral("color:#e65100;font-weight:700;"));
    updateButtons();
}

void IvaSettingsPage::discardRectangleDraft()
{
    if (m_draftChannel < 0) return;
    const int channel = m_draftChannel;
    for (int index = m_configuration.areas.size() - 1; index >= 0; --index) {
        const IvaAreaDefinition &area = m_configuration.areas.at(index);
        if (area.channel == m_draftChannel && area.areaIndex == m_draftAreaIndex) {
            m_configuration.areas.removeAt(index);
            break;
        }
    }
    m_draftChannel = -1;
    m_draftAreaIndex = -1;
    populateAreaTable();
    clearEditor();
    selectChannel(channel);

    int fallbackRow = -1;
    for (int row = 0; row < m_configuration.areas.size(); ++row) {
        if (m_configuration.areas.at(row).channel == channel) {
            fallbackRow = row;
        }
    }
    if (fallbackRow >= 0) {
        m_areaTable->selectRow(fallbackRow);
        populateEditor(fallbackRow);
    } else {
        m_areaTable->clearSelection();
        m_areaTable->setCurrentCell(-1, -1);
    }
    m_statusLabel->setText(QStringLiteral("Rectangle draft discarded."));
    m_statusLabel->setStyleSheet(QStringLiteral("color:#455a64;"));
    updateButtons();
}

void IvaSettingsPage::updateButtons()
{
    if (!m_refreshButton || !m_applyButton) {
        return;
    }
    const bool hasSelection = m_selectedArea >= 0
        && m_selectedArea < m_configuration.areas.size();
    m_refreshButton->setEnabled(!m_requestInFlight);
    m_applyButton->setEnabled(!m_requestInFlight && m_hasOptions && hasSelection);
    m_addPointButton->setEnabled(!m_requestInFlight && hasSelection);
    m_removePointButton->setEnabled(!m_requestInFlight && hasSelection
                                    && m_coordinateTable->currentRow() >= 0);
    const IvaChannelCapability *capability = m_capabilities.forChannel(
        m_selectedChannel);
    const bool canDraw = !m_requestInFlight && m_hasOptions && m_hasCapabilities
        && capability && capability->ivaAreaSupported
        && capability->maxResolution.isValid()
        && m_videoCanvas->frameCompatible() && m_draftChannel < 0;
    m_drawRectangleButton->setEnabled(canDraw);
    m_discardDraftButton->setEnabled(!m_requestInFlight && m_draftChannel >= 0);
}
