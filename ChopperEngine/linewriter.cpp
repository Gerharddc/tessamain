#include "linewriter.h"

#include <iostream>

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

#define ADDGCODE(gcode) \
    { \
        std::lock_guard<std::mutex> lock(bufMtx); \
        gcodeBuf.push(gcode); \
        cv.notify_all(); \
    }

    // This macro adds a string line to the queue
#define ADDLINE(text) \
    { \
        GCode gcode; \
        gcode.setText(text); \
        ADDGCODE(gcode) \
    }

    ADDLINE(";Total amount of layers: " + std::to_string(mip->layerCount));
    ADDLINE(";Estimated time: " + std::to_string(0)); // TODO
    ADDLINE(";Estimated filament: " + std::to_string(0)); // TODO

    {
        GCode gcode;

        gcode.setG(21);
        ADDGCODE(gcode);

        gcode.setG(90);
        ADDGCODE(gcode);

        gcode.setG(28);
        gcode.setX(0);
        gcode.setY(0);
        gcode.setZ(0);
        ADDGCODE(gcode);

        GCode mcode;
        mcode.setM(109);
        mcode.setT(0);
        mcode.setS(GlobalSettings::PrintTemperature.Get());
        ADDGCODE(mcode);

        gcode = GCode();
        gcode.setG(92);
        gcode.setE(0.0f);
        ADDGCODE(gcode);

        gcode = GCode();
        gcode.setG(1);
        gcode.setF(600.0f);
        ADDGCODE(gcode);
    }

    for (std::size_t layerNum = 0; layerNum < mip->layerCount; layerNum++)
    {
        const LayerComponent &layer = mip->layerComponents[layerNum];
        ADDLINE(";Layer: " + std::to_string(layerNum));

        for (const TravelSegment &move : layer.initialLayerMoves)
        {
            GCode gcode;
            gcode.setG(0);

            float newX = (float)(move.p2.X / scaleFactor);
            float newY = (float)(move.p2.Y / scaleFactor);
            float newZ = (float)(move.p2.Z / scaleFactor);

            if (newX != prevX)
            {
                prevX = newX;
                gcode.setX(prevX);
            }

            if (newY != prevY)
            {
                prevY = newY;
                gcode.setY(prevY);
            }

            if (newZ != prevZ)
            {
                prevZ = newZ;
                gcode.setZ(prevZ);
            }

            if (move.speed != prev0F)
            {
                prev0F = move.speed;
                gcode.setF(prev0F);
            }

            ADDGCODE(gcode);
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
                    GCode gcode;

                    if (ts->type == ToolSegType::Retraction)
                    {
                        gcode.setG(1);
                        gcode.setE(currentE - (float)(((RetractSegment*)(ts))->distance / scaleFactor));

                        if (GlobalSettings::RetractionSpeed.Get() != prev1F)
                        {
                            prev1F = GlobalSettings::RetractionSpeed.Get();
                            gcode.setF(prev1F);
                        }

                        retracted = true;
                    }
                    else if (MovingSegment* ms = dynamic_cast<MovingSegment*>(ts))
                    {
                        if (ms->p1 == ms->p2)
                            continue;

                        if (ms->type == ToolSegType::Extruded)
                        {
                            gcode.setG(1);

                            // If the printhead has retracted then we first need to get it back at the correct e before continuing
                            if (retracted)
                            {
                                gcode.setE(currentE);
                                retracted = false;
                            }
                        }
                        else
                            gcode.setG(0);

                        float newX = (float)(ms->p2.X / scaleFactor);
                        float newY = (float)(ms->p2.Y / scaleFactor);
                        float newZ = (float)(ms->p2.Z / scaleFactor);

                        if (newX != prevX)
                        {
                            prevX = newX;
                            gcode.setX(prevX);
                        }

                        if (newY != prevY)
                        {
                            prevY = newY;
                            gcode.setY(prevY);
                        }

                        if (newZ != prevZ)
                        {
                            prevZ = newZ;
                            gcode.setZ(prevZ);
                        }

                        if (ms->type == ToolSegType::Extruded)
                        {
                            ExtrudeSegment *es = (ExtrudeSegment*)(ms);

                            // The e position should always change so there is no need to check if it changed
                            currentE += es->ExtrusionDistance(); // *layer.InfillMulti

                            gcode.setE(currentE);

                            if (ms->speed != prev1F)
                            {
                                prev1F = ms->speed;
                                gcode.setF(prev1F);
                            }
                        }
                        else
                        {
                            if (ms->speed != prev0F)
                            {
                                prev0F = ms->speed;
                                gcode.setF(prev0F);
                            }
                        }
                    }
                    else
                    {
                        std::cout << "Trying to write unsupported segment to gcode." << std::endl;
                        ADDLINE("; Unknown type: " + std::to_string((int)ts->type));
                    }

                    ADDGCODE(gcode);
                }
            }
        }
    }

    // Check if the thread hasn't been notified to stop
    if (done)
        return;

    {
        GCode mcode;
        mcode.setM(104);
        mcode.setS(0);
        ADDGCODE(mcode);

        GCode gcode;

        gcode.setG(91);
        ADDGCODE(gcode);

        gcode.setG(1);
        gcode.setE(-5.0f);
        gcode.setF(4800.0f);
        ADDGCODE(gcode);

        gcode = GCode();
        gcode.setG(1);
        gcode.setX(-15.0f);
        gcode.setY(-15.0f);
        gcode.setZ(0.5f);
        ADDGCODE(gcode);

        gcode = GCode();
        gcode.setG(28);
        gcode.setX(0.0f);
        gcode.setY(0.0f);
        ADDGCODE(gcode);
    }

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
    if (!done && gcodeBuf.empty())
    {
        // The buffering thread will notify when a line has been added
        std::unique_lock<std::mutex> lck(mtx);
        while (!done && gcodeBuf.empty())
            cv.wait(lck);
    }

    // Return the next string in the queue and tell the thread generating the
    // lines that a spot might have opened.
    std::lock_guard<std::mutex> lock(bufMtx);

    GCode gcode = gcodeBuf.front();
    gcodeBuf.pop();

    // Tell the buffering thread that space has opened
    cv.notify_all();

    return gcode.getAscii();
}

bool LineWriter::HasLineToRead() const
{
    return !(done && gcodeBuf.empty());
}
