#include "settingspage.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStringList>
#include <QVBoxLayout>

SettingsPage::SettingsPage(const QString &configPath, const QString &cameraIp,
                           QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *group = new QGroupBox(QStringLiteral("Runtime Configuration"), this);
    auto *grid = new QGridLayout(group);
    m_cameraIpLabel = new QLabel(group);
    m_lastOctetInput = new QLineEdit(group);
    m_lastOctetInput->setMaximumWidth(80);
    m_lastOctetInput->setPlaceholderText(QStringLiteral("0-255"));
    auto *saveCameraButton = new QPushButton(QStringLiteral("Save camera IP"), group);
    auto *cameraLayout = new QHBoxLayout;
    cameraLayout->addWidget(new QLabel(QStringLiteral("172.20.35."), group));
    cameraLayout->addWidget(m_lastOctetInput);
    cameraLayout->addWidget(saveCameraButton);
    cameraLayout->addStretch();

    m_serverBaseUrlInput = new QLineEdit(group);
    m_serverBaseUrlInput->setPlaceholderText(QStringLiteral("http://server-ip:8080"));
    auto *saveServerButton = new QPushButton(QStringLiteral("Save and reconnect"), group);
    auto *reconnectButton = new QPushButton(QStringLiteral("Reconnect now"), group);
    auto *serverLayout = new QHBoxLayout;
    serverLayout->addWidget(m_serverBaseUrlInput, 1);
    serverLayout->addWidget(saveServerButton);
    serverLayout->addWidget(reconnectButton);
    m_serverConnectionLabel = new QLabel(QStringLiteral("Not connected"), group);

    grid->addWidget(new QLabel(QStringLiteral("Camera config"), group), 0, 0);
    grid->addWidget(new QLabel(configPath, group), 0, 1);
    grid->addWidget(new QLabel(QStringLiteral("Current camera IP"), group), 1, 0);
    grid->addWidget(m_cameraIpLabel, 1, 1);
    grid->addWidget(new QLabel(QStringLiteral("Edit last octet"), group), 2, 0);
    grid->addLayout(cameraLayout, 2, 1);
    grid->addWidget(new QLabel(QStringLiteral("Server API URL"), group), 3, 0);
    grid->addLayout(serverLayout, 3, 1);
    grid->addWidget(new QLabel(QStringLiteral("Server connection"), group), 4, 0);
    grid->addWidget(m_serverConnectionLabel, 4, 1);
    grid->addWidget(new QLabel(QStringLiteral("Address save target"), group), 5, 0);
    grid->addWidget(new QLabel(QStringLiteral("client_config.local.ini (this PC override)"), group), 5, 1);
    grid->addWidget(new QLabel(QStringLiteral("Channel source"), group), 6, 0);
    grid->addWidget(new QLabel(QStringLiteral("Final URL pattern: /{channel}/{profile}/media.smp"), group), 6, 1);
    grid->addWidget(new QLabel(QStringLiteral("Credentials"), group), 7, 0);
    grid->addWidget(new QLabel(QStringLiteral("Stored outside the UI. Do not commit real passwords."), group), 7, 1);
    layout->addWidget(group);
    layout->addStretch();
    setCameraIp(cameraIp);

    auto requestCameraSave = [this]() {
        emit saveCameraIpRequested(m_lastOctetInput->text().trimmed());
    };
    auto requestServerSave = [this]() {
        emit saveServerBaseUrlRequested(m_serverBaseUrlInput->text().trimmed());
    };
    connect(saveCameraButton, &QPushButton::clicked, this, requestCameraSave);
    connect(m_lastOctetInput, &QLineEdit::returnPressed, this, requestCameraSave);
    connect(saveServerButton, &QPushButton::clicked, this, requestServerSave);
    connect(m_serverBaseUrlInput, &QLineEdit::returnPressed, this, requestServerSave);
    connect(reconnectButton, &QPushButton::clicked, this, &SettingsPage::reconnectServerRequested);
}

void SettingsPage::setCameraIp(const QString &cameraIp)
{
    m_cameraIpLabel->setText(cameraIp.isEmpty() ? QStringLiteral("Not configured") : cameraIp);
    const QStringList parts = cameraIp.split(QLatin1Char('.'));
    m_lastOctetInput->setText(parts.size() == 4 ? parts.last() : QString());
}

void SettingsPage::setServerBaseUrl(const QString &baseUrl)
{
    m_serverBaseUrlInput->setText(baseUrl);
}

void SettingsPage::setServerConnectionStatus(const QString &status, bool connected)
{
    m_serverConnectionLabel->setText(status);
    m_serverConnectionLabel->setStyleSheet(
        connected ? QStringLiteral("color:#1b5e20;font-weight:700;")
                  : QStringLiteral("color:#b71c1c;font-weight:700;"));
}