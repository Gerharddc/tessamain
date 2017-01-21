#include "linewriter.h"

#include <sstream>
#include <iostream>
#include <iomanip>

using namespace ChopperEngine;

void LineWriter::WriteLinesFunc()
{
    float currentE = 0.0f;
    float prevX = 0.0f;
    float prevY = 0.0f;
    float prevZ = 0.0f;
    int prev0F = 0;
    int prev1F = 0;
    bool retracted = false;

    std::stringstream os;

    os << std::fixed << std::setprecision(3);

    // This macro adds a string line to the queue
#define ADDLINE(text) \
    { \
        std::lock_guard<std::mutex> lock(bufMtx); \
        stringBuf.push(text); \
        cv.notify_all(); \
    }

    // This macro pushes the current stringbuf line to the
    // queue and waits if the queue is full
#define PUSHLINE() \
    { \
        std::lock_guard<std::mutex> lock(bufMtx); \
        stringBuf.push(os.str()); \
        os.str(""); \
        cv.notify_all(); \
    } \
    \
    std::unique_lock<std::mutex> lck(mtx); \
    while (!done && (stringBuf.size() > TargetBufSize)) \
        cv.wait(lck); \
    if (done) \
        return;

    ADDLINE(";Total amount of layers: " + std::to_string(mip->layerCount));
    ADDLINE(";Estimated time: " + std::to_string(0)); // TODO
    ADDLINE(";Estimated filament: " + std::to_string(0)); // TODO
    ADDLINE("G21");
    ADDLINE("G90");
    ADDLINE("G28 X0 Y0 Z0");
    if (GlobalSettings::PrintTemperature.Get() != -1)
        ADDLINE("M109 T0 S" + std::to_string(GlobalSettings::PrintTemperature.Get()));
    ADDLINE("G92 E0");
    ADDLINE("G1 F600");

    for (std::size_t layerNum = 0; layerNum < mip->layerCount; layerNum++)
    {
        const LayerComponent &layer = mip->layerComponents[layerNum];
        ADDLINE(";Layer: " + std::to_string(layerNum));

        for (const TravelSegment &move : layer.initialLayerMoves)
        {
            os << "G0";

            float newX = (float)(move.p2.X / scaleFactor);
            float newY = (float)(move.p2.Y / scaleFactor);
            float newZ = (float)(move.p2.Z / scaleFactor);

            if (newX != prevX)
            {
                prevX = newX;
                os << " X" << prevX;
            }

            if (newY != prevY)
            {
                prevY = newY;
                os << " Y" << prevY;
            }

            if (newZ != prevZ)
            {
                prevZ = newZ;
                os << " Z" << prevZ;
            }

            if (move.speed != prev0F)
            {
                prev0F = move.speed;
                os << " F" << prev0F;
            }

            PUSHLINE();
        }

        for (const LayerIsland &isle : layer.islandList)
        {
            ADDLINE(";Island");

            for (const LayerSegment *seg : isle.segments)
            {
                // TODO: write segment type name
                ADDLINE(";Segment: " + std::to_string((int)seg->type));

                for (ToolSegment *ts : seg->toolSegments)
                {
                    if (ts->type == ToolSegType::Retraction)
                    {
                        os << "G1";
                        os << " E" << (currentE - (float)(((RetractSegment*)(ts))->distance / scaleFactor));

                        if (GlobalSettings::RetractionSpeed.Get() != prev1F)
                        {
                            prev1F = GlobalSettings::RetractionSpeed.Get();
                            os << " F" << prev1F;
                        }

                        retracted = true;
                    }
                    else if (MovingSegment* ms = dynamic_cast<MovingSegment*>(ts))
                    {
                        if (ms->p1 == ms->p2)
                            continue;

                        if (ms->type == ToolSegType::Extruded)
                        {
                            // If the printhead has retracted then we first need to get it back at the correct e before continuing
                            if (retracted)
                            {
                                os << "G1 E" << currentE;
                                retracted = false;
                            }

                            os << "G1";
                        }
                        else
                            os << "G0";

                        float newX = (float)(ms->p2.X / scaleFactor);
                        float newY = (float)(ms->p2.Y / scaleFactor);
                        float newZ = (float)(ms->p2.Z / scaleFactor);

                        if (newX != prevX)
                        {
                            prevX = newX;
                            os << " X" << prevX;
                        }

                        if (newY != prevY)
                        {
                            prevY = newY;
                            os << " Y" << prevY;
                        }

                        if (newZ != prevZ)
                        {
                            prevZ = newZ;
                            os << " Z" << prevZ;
                        }

                        if (ms->type == ToolSegType::Extruded)
                        {
                            ExtrudeSegment *es = (ExtrudeSegment*)(ms);

                            // The e position should always change so there is no need to check if it changed
                            currentE += es->ExtrusionDistance(); // *layer.InfillMulti

                            os << " E" << currentE;

                            if (ms->speed != prev1F)
                            {
                                prev1F = ms->speed;
                                os << " F" << prev1F;
                            }
                        }
                        else
                        {
                            if (ms->speed != prev0F)
                            {
                                prev0F = ms->speed;
                                os << " F" << prev0F;
                            }
                        }
                    }
                    else
                    {
                        std::cout << "Trying to write unsupported segment to gcode." << std::endl;
                        ADDLINE("; Unknown type: " + std::to_string((int)ts->type));
                    }

                    PUSHLINE();
                }
            }
        }
    }

    // Check if the thread hasn't been notified to stop
    if (done)
        return;

    ADDLINE("M104 S0");
    ADDLINE("G91");
    ADDLINE("G1 E-5 F4800");
    ADDLINE("G1 Z+0.5 X-15 Y-15 F4800");
    ADDLINE("G28 X0 Y0");

    done = true;
}

LineWriter::LineWriter(const MeshInfoPtr _mip) : mip(_mip)
{
    bufferThread = std::thread(&LineWriter::WriteLinesFunc, this);
}

LineWriter::~LineWriter()
{
    // Tell the thread to stop in case it is still running
    done = true;
    cv.notify_all();

    if (bufferThread.joinable())
        bufferThread.join();
}

std::string LineWriter::ReadNextLine()
{
    // Wait for the buffer to get a line if needed
    if (!done && stringBuf.empty())
    {
        // The buffering thread will notify when a line has been added
        std::unique_lock<std::mutex> lck(mtx);
        while (!done && stringBuf.empty())
            cv.wait(lck);
    }

    // Return the next string in the queue and tell the thread generating the
    // lines that a spot might have opened.
    std::lock_guard<std::mutex> lock(bufMtx);
    std::string ret = ";ERROR: Nothing to read";

    ret = stringBuf.front();
    stringBuf.pop();

    // Tell the buffering thread that space has opened
    cv.notify_all();

    return ret;
}

bool LineWriter::HasLineToRead() const
{
    return !(done && stringBuf.empty());
}
