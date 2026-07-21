#include "debugpage.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

DebugPage::DebugPage(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *controls = new QGroupBox(QStringLiteral("Simulation Controls"), this);
    auto *grid = new QGridLayout(controls);
    auto *clearButton = new QPushButton(QStringLiteral("Clear alarms"), controls);
    auto *mockButton = new QPushButton(QStringLiteral("Toggle mock EV"), controls);
    auto *nonEvButton = new QPushButton(QStringLiteral("Test non-EV"), controls);
    auto *overtimeButton = new QPushButton(QStringLiteral("Test overtime"), controls);
    auto *sensorButton = new QPushButton(QStringLiteral("Test Hall sensor error"), controls);
    auto *randomButton = new QPushButton(QStringLiteral("Randomize parking"), controls);
    auto *sampleButton = new QPushButton(QStringLiteral("Run RX sample"), controls);
    grid->addWidget(clearButton, 0, 0); grid->addWidget(mockButton, 0, 1);
    grid->addWidget(nonEvButton, 1, 0); grid->addWidget(overtimeButton, 1, 1);
    grid->addWidget(sensorButton, 2, 0); grid->addWidget(randomButton, 2, 1);
    grid->addWidget(sampleButton, 3, 0, 1, 2);
    layout->addWidget(controls);

    auto *messageGroup = new QGroupBox(QStringLiteral("Manual RX Message"), this);
    auto *messageLayout = new QHBoxLayout(messageGroup);
    m_messageInput = new QLineEdit(QStringLiteral("EV_ALERT,EV01,NON_EV"), messageGroup);
    m_messageInput->setPlaceholderText(QStringLiteral("Example: PARKING_SLOT,P01,OCCUPIED"));
    auto *applyButton = new QPushButton(QStringLiteral("Apply message"), messageGroup);
    messageLayout->addWidget(new QLabel(QStringLiteral("RX message"), messageGroup));
    messageLayout->addWidget(m_messageInput, 1);
    messageLayout->addWidget(applyButton);
    layout->addWidget(messageGroup);
    m_lastMessageLabel = new QLabel(QStringLiteral("Last RX: -"), this);
    m_lastMessageLabel->setStyleSheet(QStringLiteral("color: #455a64; font-size: 12px;"));
    layout->addWidget(m_lastMessageLabel);
    layout->addStretch();

    connect(clearButton, &QPushButton::clicked, this, &DebugPage::clearAlarmsRequested);
    connect(mockButton, &QPushButton::clicked, this, &DebugPage::toggleMockEvRequested);
    connect(nonEvButton, &QPushButton::clicked, this, &DebugPage::nonEvAlertRequested);
    connect(overtimeButton, &QPushButton::clicked, this, &DebugPage::overtimeAlertRequested);
    connect(sensorButton, &QPushButton::clicked, this, &DebugPage::sensorErrorRequested);
    connect(randomButton, &QPushButton::clicked, this, &DebugPage::randomizeParkingRequested);
    connect(sampleButton, &QPushButton::clicked, this, &DebugPage::sampleMessagesRequested);
    auto sendManual = [this]() { emit manualMessageRequested(m_messageInput->text().trimmed()); };
    connect(applyButton, &QPushButton::clicked, this, sendManual);
    connect(m_messageInput, &QLineEdit::returnPressed, this, sendManual);
}

void DebugPage::setLastMessage(const QString &message)
{
    m_lastMessageLabel->setText(message);
}
