#ifndef PRINTER_H
#define PRINTER_H

#include <QObject>
#include <QString>

class Printer : public QObject
{
    Q_OBJECT
private:
    float m_targetTemp = 0.0f;
    float m_curTemp = 25.0f;
    bool m_printing = false;
    bool m_heating = false;
    bool m_paused = false;
    bool m_fanning = false;
    bool m_autoFan = true;
    QString m_status = "Not printing";

public:
    explicit Printer(QObject *parent = 0);
    ~Printer();
    void Connect();
    void SignalPrintStop();
    void SignalTargetTemp(float temp);
    void sendCommand(QString cmd);
    void UpdateProgressStatus();

    Q_INVOKABLE void startPrint(QString path);
    Q_INVOKABLE void stopPrint();
    Q_INVOKABLE void pauseResume();
    Q_INVOKABLE void emergencyStop();
    Q_INVOKABLE void homeAll();
    Q_INVOKABLE void homeX();
    Q_INVOKABLE void homeY();
    Q_INVOKABLE void homeZ();
    Q_INVOKABLE void moveX(float distance);
    Q_INVOKABLE void moveY(float distance);
    Q_INVOKABLE void moveZ(float distance);
    Q_INVOKABLE void move(float x, float y, float z);
    Q_INVOKABLE void extrude(float e);

    Q_PROPERTY(float targetTemp READ targetTemp WRITE setTargetTemp NOTIFY targetTempChanged)
    void setTargetTemp(float target);
    float targetTemp() { return m_targetTemp; }

    Q_PROPERTY(bool heating READ heating WRITE setHeating NOTIFY heatingChanged)
    void setHeating(bool val);
    bool heating() { return m_heating; }

    Q_PROPERTY(bool printing READ printing NOTIFY printingChanged)
    bool printing() { return m_printing; }

    Q_PROPERTY(bool paused READ paused NOTIFY pausedChanged)
    bool paused() { return m_paused; }

    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    QString status() { return m_status; }

    Q_PROPERTY(float curTemp READ curTemp NOTIFY curTempChanged)
    float curTemp() { return m_curTemp; }

    Q_PROPERTY(bool fanning READ fanning WRITE setFanning NOTIFY fanningChanged)
    bool fanning() { return m_fanning; }
    void setFanning(bool val);

    Q_PROPERTY(bool autoFan READ autoFan WRITE setAutoFan NOTIFY autoFanChanged)
    bool autoFan() { return m_autoFan; }
    void setAutoFan(bool val) {
        if (val != m_autoFan)
        {
            m_autoFan = val;
            emit autoFanChanged();
        }
    }

    // Minutes
    Q_PROPERTY(float eta READ eta NOTIFY etaChanged)
    float eta();

    Q_PROPERTY(float percentDone READ percentDone NOTIFY percentDoneChanged)
    float percentDone();

signals:
    void targetTempChanged();
    void heatingChanged();
    void printingChanged();
    void statusChanged();
    void pausedChanged();
    void curTempChanged();
    void fanningChanged();
    void autoFanChanged();
    void etaChanged();
    void percentDoneChanged();

public slots:
    void readPrinterOutput();
    void printerFinished();
};

extern Printer GlobalPrinter;

#endif // PRINTER_H
