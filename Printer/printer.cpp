#include "printer.h"
#include <thread>
#include <iostream>
#include <fstream>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QDebug>
#include "Rendering/comborendering.h"
#include "Rendering/toolpathrendering.h"

// Global singleton printer
Printer GlobalPrinter;
static QSerialPort *serial = nullptr;
static std::thread PrintThread;
static std::thread TempThread;
static volatile bool WaitingForOk = false;
static volatile bool StopPrintThread = false;
static volatile bool NeedTemp = false;
static volatile bool CheckTemp = true;
static std::ofstream *fanGPIO = nullptr;

static void CheckTempLoop()
{
    while (CheckTemp)
    {
        // If the print thread is waiting for a respone then it means that
        // that the buffer is full. We need to ask it to send the temp request for us then
        // when it gets a chance.
        if (WaitingForOk)
            CheckTemp = true;
        else
            GlobalPrinter.sendCommand("M105");

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

Printer::Printer(QObject *parent) : QObject(parent)
{
    TempThread = std::thread(CheckTempLoop);
    TempThread.detach();

    // Set the fan gpio to output
    std::ofstream dir("/sys/class/gpio/gpio146/direction");
    if (!dir) {
        std::cout << "Couldn't open gpio direction" << std::endl;
    }
    else {
        dir << "out";
        dir.flush();
        dir.close();

        // Setup the value writer
        fanGPIO = new std::ofstream("/sys/class/gpio/gpio146/value");
        if (!*fanGPIO) {
            std::cout << "Couldn't open gpio direction" << std::endl;
            fanGPIO = nullptr;
        }
        else
            *fanGPIO << "0";
    }
}

Printer::~Printer()
{
    if (serial != nullptr)
    {
        serial->close();
        serial->disconnect();
        delete serial;
    }

    if (m_printing)
        stopPrint();

    CheckTemp = false;

    if (fanGPIO != nullptr)
    {
        fanGPIO->close();
        delete fanGPIO;
    }
}

// This has to be called from the main thread
void Printer::Connect()
{
    //serial = new QSerialPort("rfcomm0");
    serial = new QSerialPort("ttyHS2");
    //serial->setBaudRate(9600);
    serial->setBaudRate(57600);

    if (serial->open(QSerialPort::ReadWrite))
    {
        QObject::connect(serial, SIGNAL(readyRead()), this, SLOT(readPrinterOutput()));
        //QObject::connect(serial, SIGNAL(error(QSerialPort::SerialPortError)), this, SLOT(readPrinterOutput()));
        serial->waitForReadyRead(1000);
        qDebug() << serial->readAll();
        qDebug() << "Opened";
    }
    else
        qDebug() << "Couldn't open serial";
}

void Printer::readPrinterOutput()
{
    while (serial->canReadLine())
    {
        QString line(serial->readLine());

        if (line.contains("ok"))
            WaitingForOk = false;

        if (line.contains("T:"))
        {
            QString num = line.mid(line.indexOf("T:") + 2, line.indexOf(' ') - line.indexOf("T:") - 2);
            float newTemp = num.toFloat();
            if (newTemp != m_curTemp)
            {
                m_curTemp = newTemp;
                emit curTempChanged();
            }
        }

        //qDebug() << line;
    }
}

void Printer::printerFinished()
{
    m_printing = false;
    emit printingChanged();
}

void Printer::sendCommand(QString cmd)
{
    if (serial != nullptr && serial->isOpen())
    {
        serial->write((cmd + '\n').toUtf8());
        serial->flush();
        //qDebug() << "Wrote: " << cmd;
    }
    else
        std::cout << "Serial port not open for sending" << std::endl;
}

static inline void WaitForOk()
{
    WaitingForOk = true;
    while (WaitingForOk && !StopPrintThread)
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
}

// Millis
static double m_totalTime = 0;
static double m_timeLeft = 0;

float Printer::eta()
{
    return (float)(m_timeLeft / 60000.0);
}

float Printer::percentDone()
{
    return (float)((m_totalTime - m_timeLeft) / m_totalTime * 100.0);
}

static void PrintFile(std::string path)
{
    std::ifstream is(path);

    if (is)
    {
        m_totalTime = ComboRendering::getToolpath()->totalMillis;
        m_timeLeft = m_totalTime;
        emit GlobalPrinter.etaChanged();
        emit GlobalPrinter.percentDoneChanged();

        std::string line;
        int64_t lineNum = 0;
        while (std::getline(is, line) && !StopPrintThread)
        {
            lineNum++;

            if (serial != nullptr )//&& serial->isOpen())
            {
                // TODO: check for legal numbers two
                if (line[0] != 'G' && line[0] != 'M')
                    continue;

                // Check for any target temperature commands
                if (line.find("M104 S") != std::string::npos || line.find("M109 S") != std::string::npos)
                    GlobalPrinter.SignalTargetTemp(std::stof(line.substr(line.find('S') + 1)));

                // Check for fan commands if autofanning
                if (GlobalPrinter.autoFan())
                {
                    if (line.find("M106") != std::string::npos)
                        GlobalPrinter.setFanning(true);
                    else if (line.find("M107") != std::string::npos)
                        GlobalPrinter.setFanning(false);
                }

                if (serial->isOpen())
                {
                    serial->write((QString::fromStdString(line) + '\n').toUtf8());
                    serial->flush();

                    // wait for an ok before sending the next line
                    WaitForOk();

                    // Check if there is a temp request waiting
                    if (NeedTemp)
                    {
                        GlobalPrinter.sendCommand("M105");
                        // wait for an ok before sending the next line
                        WaitForOk();
                    }
                }
                else
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    //std::cout << "Serial not open for printing." << std::endl;
                }
                //std::cout << "Printed: " << line << std::endl;

                // Update the progress indicators
                ToolpathRendering::ShowPrintedToLine(lineNum);
                m_timeLeft -= ComboRendering::getToolpath()->lineInfos[lineNum - 1].milliSecs;
                emit GlobalPrinter.etaChanged();
                emit GlobalPrinter.percentDoneChanged();
                GlobalPrinter.UpdateProgressStatus();
            }
            else
            {
                std::cout << "Serial port not open, print stopped" << std::endl;
                goto close;
            }

            // Stall the thread while pausing but check for a complete stop
            while (GlobalPrinter.paused() && !StopPrintThread)
                std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }
    else
    {
        // TODO: report error
        std::cout << "Could not open file for print";
    }

    close:
    is.close();
    GlobalPrinter.SignalPrintStop();

    // Stop the heater
    GlobalPrinter.setHeating(false);
}

void Printer::UpdateProgressStatus()
{
    float mins;
    float secs = std::modf(eta(), &mins) * 60.0f;
    m_status = "ETA: "
            + QString::number((int)mins) + ":" + QString::number((int)secs)
            + " " + QString::number(percentDone(), 'f', 2) + "%";
    emit statusChanged();
}

void Printer::SignalPrintStop()
{
    if (m_printing)
    {
        m_printing = false;
        emit printingChanged();

        m_status = "Not printing";
        emit statusChanged();

        // Reset the view
        ToolpathRendering::ShowPrintedToLine(-1);

        // Retract, home x & y, then z
        extrude(-15.0f);
        homeX();
        homeY();
        homeZ();
        extrude(-14.0f);
    }
}

void Printer::startPrint(QString path)
{
    if (m_printing)
        return;

    StopPrintThread = false;
    PrintThread = std::thread(PrintFile, path.toStdString());
    PrintThread.detach();
    m_printing = true;
    emit printingChanged();

    m_status = "Printing";
    emit statusChanged();
}

void Printer::pauseResume()
{
    m_paused = !m_paused;

    if (m_paused) {
        m_status = "Paused";
        emit statusChanged();
    }

    emit pausedChanged();
}

void Printer::stopPrint()
{
    std::cout << "Stop" << std::endl;

    if (!m_printing)
        return;

    StopPrintThread = true;
}

void Printer::emergencyStop()
{
    stopPrint();
    sendCommand("M112");
}

void Printer::homeAll()
{
    sendCommand("G28");
}

void Printer::homeX()
{
    sendCommand("G28 X0");
}

void Printer::homeY()
{
    sendCommand("G28 Y0");
}

void Printer::homeZ()
{
    sendCommand("G28 Z0");
}

void Printer::move(float x, float y, float z)
{
    moveX(x);
    moveY(y);
    moveZ(z);
}

void Printer::moveX(float distance)
{
    sendCommand("G91"); // relative
    sendCommand("G1 X" + QString::number(distance));
}

void Printer::moveY(float distance)
{
    sendCommand("G91"); // relative
    sendCommand("G1 Y" + QString::number(distance));
}

void Printer::extrude(float e)
{
    sendCommand("G91"); // relative
    sendCommand("G1 E" + QString::number(e) + "F900");
}

void Printer::moveZ(float distance)
{
    sendCommand("G91"); // relative
    sendCommand("G1 Z" + QString::number(distance));
}

void Printer::setTargetTemp(float target)
{
    // Check for change and validity
    if (target != m_targetTemp && target >= 0.0f)
    {
        if (m_heating)
            sendCommand("M104 S" + QString::number(target));

        m_targetTemp = target;
        emit targetTempChanged();
    }
}

void Printer::setHeating(bool val)
{
    if (val != m_heating)
    {
        if (val)
            sendCommand("M104 S" + QString::number(m_targetTemp));
        else
            sendCommand("M104 S0");

        m_heating = val;
        emit heatingChanged();
    }
}

void Printer::SignalTargetTemp(float temp)
{
    if (temp == 0 && m_heating)
    {
        m_heating = false;
        emit heatingChanged();
    }
    else
    {
        if (!m_heating)
        {
            m_heating = true;
            emit heatingChanged();
        }

        if (temp != m_targetTemp)
        {
            m_targetTemp = temp;
            emit targetTempChanged();
        }
    }
}

void Printer::setFanning(bool val)
{
    if (val != m_fanning)
    {
        if (fanGPIO != nullptr)
        {
            if (val)
                *fanGPIO << "1";
            else
                *fanGPIO << "0";

            fanGPIO->flush();
        }
        else
            std::cout << "Fan gpio not open" << std::endl;

        m_fanning = val;

        emit fanningChanged();
    }
}
