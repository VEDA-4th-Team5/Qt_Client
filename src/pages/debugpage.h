#ifndef DEBUGPAGE_H
#define DEBUGPAGE_H

#include <QWidget>

class QLabel;
class QLineEdit;

class DebugPage : public QWidget
{
    Q_OBJECT

public:
    explicit DebugPage(QWidget *parent = nullptr);
    void setLastMessage(const QString &message);

signals:
    void clearAlarmsRequested();
    void toggleMockEvRequested();
    void nonEvAlertRequested();
    void overtimeAlertRequested();
    void sensorErrorRequested();
    void randomizeParkingRequested();
    void sampleMessagesRequested();
    void manualMessageRequested(const QString &message);

private:
    QLineEdit *m_messageInput = nullptr;
    QLabel *m_lastMessageLabel = nullptr;
};

#endif
