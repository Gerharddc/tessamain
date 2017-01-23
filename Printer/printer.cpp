#include "printer.h"
#include <thread>
#include <iostream>
#include <fstream>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QDebug>

#include "Rendering/comborendering.h"
#include "Rendering/toolpathrendering.h"
#include "ChopperEngine/linewriter.h"

// Global singleton printer
Printer GlobalPrinter;
static QSerialPort *serial = nullptr;
static std::thread PrintThread;
static std::thread TempThread;
static volatile bool StopPrintThread = false;
static volatile bool CheckTemp = true;
static std::ofstream *fanGPIO = nullptr;

static const uint8_t bufSize = 12;
static GCode gcodeBuf[bufSize];
static std::atomic_int curLine { 1 };
static std::atomic_int frontLine { 1 };
static std::atomic_int bufIdx { 0 };
static const int maxLineNum = 65000;
static std::atomic_bool resending { false };

static std::mutex mtxSpace, bufMtx;
static std::condition_variable cvSpace;

#ifndef REAL_PRINTER
#define SIMULATE_PRINT
#endif

//#define PRINT_COMMUNICATION

static void CheckTempLoop()
{
    while (CheckTemp)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        // If the print thread is waiting for a respone then it means that
        // that the buffer is full. We need to ask it to send the temp request for us then
        // when it gets a chance.
        //if (WaitingForOk)
          //  CheckTemp = true;
        //else
        //{
            GCode code;
            code.setM(105);
            GlobalPrinter.sendGcode(code);
        //}

        //std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

Printer::Printer(QObject *parent) : QObject(parent)
{
#ifndef SIMULATE_PRINT
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
#endif
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
#ifndef SIMULATE_PRINT
    serial = new QSerialPort("ttyAMA0");
    serial->setBaudRate(57600);

    if (serial->open(QSerialPort::ReadWrite))
    {
        QObject::connect(serial, SIGNAL(readyRead()), this, SLOT(readPrinterOutput()));
        //QObject::connect(serial, SIGNAL(error(QSerialPort::SerialPortError)), this, SLOT(readPrinterOutput()));
        serial->waitForReadyRead(1000);
        qDebug() << serial->readAll();
        qDebug() << "Opened";

        SendLineReset();

        TempThread = std::thread(CheckTempLoop);
        TempThread.detach();
    }
    else
        qDebug() << "Couldn't open serial";
#endif
}

void Printer::readPrinterOutput()
{
    while (serial->canReadLine())
    {
        QString line(serial->readLine());

        if (line.contains("ok"))
        {
            QString rest = line.mid(line.indexOf(" ") + 1);

            if (rest.contains(" "))
                rest = line.mid(0, line.indexOf(" "));

            bool hasInt = false;
            int res = rest.toInt(&hasInt);

            if (hasInt)
                ProcessOk(res);
        }
        else if (line.contains("Resend"))
        {
            QString num = line.mid(line.indexOf(":") + 1);
            ProcessResend(num.toInt());
        }

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

#ifdef PRINT_COMMUNICATION
        qDebug() << "Read: " << line;
#endif
    }
}

void Printer::printerFinished()
{
    m_printing = false;
    emit printingChanged();
}

void Printer::ProcessOk(int lineNum)
{
    if (bufIdx == 0)
        return;

    int removeCount = 1;

    if (lineNum != frontLine)
    {
        std::cout << "ERROR: out of order lineNum: " << lineNum
                  << " but frontLine: " << frontLine << std::endl;

        // This usually means we missed an ok and should be able to
        // remove more than 1 line
        if (lineNum > frontLine && lineNum <= curLine)
        {
            removeCount = lineNum - frontLine + 1;
            std::cout << "Removing " << removeCount << " lines" << std::endl;
        }
        else
        {
            std::cout << "Invalid ok line" << std::endl;
            return;
        }
    }

    bufMtx.lock();

    for (int i = 0; i < bufIdx-removeCount; i++)
        gcodeBuf[i] = gcodeBuf[i + removeCount];

    bufIdx -= removeCount;
    frontLine += removeCount;

    if (lineNum == maxLineNum)
    {
        // Send twice just to make sure
        // TODO: actually make sure instead
        SendLineReset();
        SendLineReset();

        curLine = 1;
        frontLine = 1;

        std::cout << "Wrap ok" << std::endl;
    }

    bufMtx.unlock();

    // Alert the sending threads that space has opened
    cvSpace.notify_all();
}

void Printer::ProcessResend(int lineNum)
{
    if (bufIdx == 0)
        return;

    // Check if the command to reset the line numbers was missed
    // and resend if needed
    if (lineNum > maxLineNum)
    {
        SendLineReset();
        return;
    }

    // If the resend is smaller than the front line then something went worng
    // TODO: fix this properly
    if (lineNum < frontLine)
    {
        SendLineReset(frontLine);
        return;
    }

    // TODO: handle resend of resend properly
    if (resending)
        return;

    resending = true;

    int idx;
    // Unwrap the line number
    if (lineNum < frontLine)
        idx = (maxLineNum - frontLine) + lineNum + 1;
    else
        idx = lineNum - frontLine;

    if (idx >= bufIdx)
    {
        std::cout << "ERROR resend idx: " << idx << " > bufIdx:" << bufIdx << std::endl;
    }
    else
    {
        // We need to resend all the lines after this one as well but as they will shift places in
        // the buffer once ok's start coming in so we need to cache them
        GCode cloneBuf[bufSize];
        int cloneIdx = 0;
        for (int i = idx; i < bufSize; i++)
        {
            cloneBuf[cloneIdx] = gcodeBuf[i];
            cloneIdx++;
        }

        if (serial != nullptr && serial->isOpen())
        {
            //std::vector<uint8_t> bytes = code.getBinary(1);
            //serial->write((char*)bytes.data(), bytes.size());

            for (int i = 0; i < cloneIdx; i++)
            {
                GCode code = cloneBuf[i];

                serial->write(QString::fromStdString(code.getAscii() + '\n').toUtf8());
                serial->flush();

#ifdef PRINT_COMMUNICATION
                std::cout << "Resent: " << code.getAscii() << std::endl;
#endif
            }
        }
        else
            std::cout << "Serial port not open for sending" << std::endl;
    }

    resending = false;
    cvSpace.notify_all();
}

void Printer::SendLineReset(int n)
{
    GCode code;
    code.setM(110);
    code.setN(n);

    if (serial != nullptr && serial->isOpen())
    {
        serial->write(QString::fromStdString(code.getAscii() + '\n').toUtf8());
        serial->flush();
    }
    else
        std::cout << "Serial port not open for sending" << std::endl;
}

static std::mutex printMutex;

void Printer::sendGcode(GCode code)
{
    printMutex.lock();

    // Wait for space to open
    if (bufIdx >= bufSize || resending || curLine > maxLineNum)
    {
        std::unique_lock<std::mutex> lck(mtxSpace);
        while (bufIdx >= bufSize || resending || curLine > maxLineNum)
            cvSpace.wait(lck);
    }

    bufMtx.lock();

    code.setN(curLine);
    gcodeBuf[bufIdx] = code;

    // Store the linenumber as that for the front of the buffer
    if (bufIdx == 0)
        frontLine.store(curLine);

    bufIdx++;
    curLine++;

    bufMtx.unlock();

#ifndef SIMULATE_PRINT
    if (serial != nullptr && serial->isOpen())
    {
        //std::vector<uint8_t> bytes = code.getBinary(1);
        //serial->write((char*)bytes.data(), bytes.size());

        serial->write(QString::fromStdString(code.getAscii() + '\n').toUtf8());
        serial->flush();

#ifdef PRINT_COMMUNICATION
        std::cout << "Wrote: " << code.getAscii() << std::endl;
#endif
    }
    else
        std::cout << "Serial port not open for sending" << std::endl;
#else
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
#endif

    printMutex.unlock();
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

static void PrintFile(const ChopperEngine::MeshInfoPtr _mip)
{
    ChopperEngine::LineWriter lw(_mip);

    m_totalTime = _mip->totalMillis;
    m_timeLeft = m_totalTime;
    if (m_totalTime == -1)
        std::cout << "ERROR: mesh totalMillis not set" << std::endl;

    while (lw.HasLineToRead() && !StopPrintThread)
    {
#ifndef SIMULATE_PRINT
        if (serial != nullptr )//&& serial->isOpen())
#endif
        {
            GCode line = lw.ReadNextLine();
            // TODO: check if only comment
            while (line.hasText())
                line = lw.ReadNextLine();

            // Check for any target temperature commands
            if (line.hasM() && (line.getM() == 104 || line.getM() == 109))
            {
                if (line.hasS())
                    GlobalPrinter.SignalTargetTemp(line.getS());
            }

            // Check for fan commands if autofanning
            if (GlobalPrinter.autoFan() && line.hasM())
            {
                if (line.getM() == 106)
                    GlobalPrinter.setFanning(true);
                else if (line.getM() == 107)
                    GlobalPrinter.setFanning(false);
            }
#ifndef SIMULATE_PRINT
            if (serial->isOpen())
            {
                GlobalPrinter.sendGcode(line);
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                std::cout << "Serial not open for printing." << std::endl;
            }
#endif

            // Update the progress indicators
            if (line.hasRenderInfo())
            {
                RenderInfo *info = line.renderInfo;

                ToolpathRendering::ShowPrintedToInfo(info);
                m_timeLeft -= info->milliSecs;
                QMetaObject::invokeMethod(&GlobalPrinter, "updateEtaAndProgress");

#ifdef SIMULATE_PRINT
                //std::this_thread::sleep_for(std::chrono::milliseconds(info->milliSecs));
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
#endif
            }
        }
#ifndef SIMULATE_PRINT
        else
        {
            std::cout << "Serial port not open, print stopped" << std::endl;
            goto close;
        }
#endif

        // Stall the thread while pausing but check for a complete stop
        while (GlobalPrinter.paused() && !StopPrintThread)
            std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    close:
    GlobalPrinter.SignalPrintStop();

    // Stop the heater
    GlobalPrinter.setHeating(false);
}

void Printer::updateEtaAndProgress()
{
    emit etaChanged();
    emit percentDoneChanged();
    UpdateProgressStatus();
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
        ToolpathRendering::ShowPrintedFull();

        // Retract, home x & y, then z
        extrude(-15.0f);
        homeX();
        homeY();
        homeZ();
        extrude(-14.0f);
    }
}

void Printer::startPrint(ChopperEngine::MeshInfoPtr _mip)
{
    if (m_printing)
        return;

    StopPrintThread = false;
    PrintThread = std::thread(PrintFile, _mip);
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

    GCode code;
    code.setM(112);
    sendGcode(code);
}

void Printer::homeAll()
{
    GCode code;
    code.setG(28);
    sendGcode(code);
}

void Printer::homeX()
{
    GCode code;
    code.setG(28);
    code.setX(0.0f);
    sendGcode(code);
}

void Printer::homeY()
{
    GCode code;
    code.setG(28);
    code.setY(0.0f);
    sendGcode(code);
}

void Printer::homeZ()
{
    GCode code;
    code.setG(28);
    code.setZ(0.0f);
    sendGcode(code);
}

void Printer::move(float x, float y, float z)
{
    moveX(x);
    moveY(y);
    moveZ(z);
}

void Printer::setRelative()
{
    GCode code;
    code.setG(91);
    sendGcode(code);
}

void Printer::moveX(float distance)
{
    setRelative();

    GCode code;
    code.setG(1);
    code.setX(distance);
    sendGcode(code);
}

void Printer::moveY(float distance)
{
    setRelative();

    GCode code;
    code.setG(1);
    code.setX(distance);
    sendGcode(code);
}

void Printer::extrude(float e)
{
    setRelative();

    GCode code;
    code.setG(1);
    code.setE(e);
    sendGcode(code);
}

void Printer::moveZ(float distance)
{
    setRelative();

    GCode code;
    code.setG(1);
    code.setX(distance);
    sendGcode(code);
}

void Printer::setTargetTemp(float target)
{
    // Check for change and validity
    if (target != m_targetTemp && target >= 0.0f)
    {
        if (m_heating)
        {
            GCode code;
            code.setM(104);
            code.setS(target);
            sendGcode(code);
        }

        m_targetTemp = target;
        emit targetTempChanged();
    }
}

void Printer::setHeating(bool val)
{
    if (val != m_heating)
    {
        GCode code;
        code.setM(104);

        if (val)
            code.setS(m_targetTemp);
        else
            code.setS(0.0f);

        sendGcode(code);

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
