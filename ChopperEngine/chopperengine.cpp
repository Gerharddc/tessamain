#include "chopperengine.h"

#include <iostream>
#include <vector>
#include <map>
#include <algorithm>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "Misc/globalsettings.h"
#include "clipper.hpp"
#include "pmvector.h"
#include "toolpath.h"

using namespace ChopperEngine;
using namespace ClipperLib;

static LogDelegate slicerLogger = nullptr;

// Below are some experimental features disabled by default
// Uncomment to combine a few layers' worth of infill segment
//#define COMBINE_INFILL

void ChopperEngine::SlicerLog(std::string message)
{
    if (slicerLogger != nullptr)
        slicerLogger(message);

    //std::cout << message << std::endl;
}

static inline void getTrigPointFloats(Triangle &trig, double *arr, uint8_t pnt, MeshInfoPtr mip)
{
    arr[0] = (double)mip->sliceMesh->vertexFloats[trig.vertIdxs[0] * 3 + pnt];
    arr[1] = (double)mip->sliceMesh->vertexFloats[trig.vertIdxs[1] * 3 + pnt];
    arr[2] = (double)mip->sliceMesh->vertexFloats[trig.vertIdxs[2] * 3 + pnt];
}

static inline Vertex &VertAtIdx(std::size_t idx, MeshInfoPtr mip)
{
    if (idx > mip->sliceMesh->vertexCount)
    {
        std::cout << "Vertex: " << std::to_string(idx) << " not in "
                  << std::to_string(mip->sliceMesh->vertexCount) << std::endl;
        throw std::runtime_error("Vertex idx too large.");
    }

    return mip->sliceMesh->vertices[idx];
}

static inline Triangle &TrigAtIdx(std::size_t idx, MeshInfoPtr mip)
{
    if (idx > mip->sliceMesh->trigCount)
    {
        std::cout << "Trig: " << std::to_string(idx) << " not in "
                  << std::to_string(mip->sliceMesh->trigCount) << std::endl;
        throw std::runtime_error("Trig idx too large.");
    }

    return mip->sliceMesh->trigs[idx];
}

// TODO: reduce allocation of items to vectors, rather emplaceback

// This function tries to run as many threads of the specified function as
// is reasonable and only returns when all have finished
// The functions need to run on data between two indices
// and toggle a flag when done
typedef void(*MultiFunction)(std::size_t, std::size_t, bool*, MeshInfoPtr, Progressor&);
static void MultiRunFunction(MultiFunction function,
                             std::size_t startIdx, std::size_t endIdx,
                             MeshInfoPtr mip, Progressor &prog)
{
    // Notify the progressor we are starting another step
    prog.StartNextStep(endIdx - startIdx);

    cInt idsLeft = endIdx - startIdx + 1;
    std::size_t lastIdx = 0;

    unsigned int cores = std::thread::hardware_concurrency();
    if (cores == 0)
        cores = 2;

    std::size_t blockSize = std::max(idsLeft / (cores * 3), (cInt)1);

    std::thread threads[cores];
    bool doneFlags[cores];
    unsigned int threadCount = 0;

    // Init the flags
    for (unsigned int i = 0; i < cores; i++)
    {
        doneFlags[i] = false;
    }

    // Spawn the initial threads
    while (idsLeft > 0 && threadCount < cores)
    {
        threads[threadCount] = std::thread(function, lastIdx, std::min(endIdx, lastIdx + blockSize),
                                           &(doneFlags[threadCount]), mip, std::ref(prog));
        lastIdx += blockSize;
        idsLeft -= blockSize;
        threadCount++;
    }

    // TODO: Implement non wait based thread management

    /*std::condition_variable cv;
    std::mutex mtx;

    while (idsLeft > 0)
    {
        // Determine how many threads are active
        threadCount = 0;
        for (unsigned int i = 0; i < cores; i++)
        {
            if (!doneFlags[i])
                threadCount++;
        }

        // wait for a thread to finish if needed
        if (threadCount < cores)
        {
        }
        else
        {
            std::unique_lock<std::mutex> lck(mtx);
            cv.wait(lck);
        }
    }*/

    auto sleepTime = std::chrono::milliseconds(50);
    // Respawn threads with new data until we have processed everything
    while (threadCount > 0)
    {
        // Check if any threads have finished
        unsigned int prevCount = threadCount;
        threadCount = 0;
        for (unsigned int i = 0; i < cores; i++)
        {
            if (!doneFlags[i])
                threadCount++;
        }

        // Spawn new threads if there is work left and slots have become open
        if (threadCount != prevCount)
        {
            unsigned int i = 0;
            while ((idsLeft > 0) && (i < cores))
            {
                if (doneFlags[i])
                {
                    // Just wait for it to properly finish
                    threads[i].join();

                    threads[i] = std::thread(function, lastIdx, std::min(endIdx, lastIdx + blockSize),
                                             &(doneFlags[threadCount]), mip, std::ref(prog));
                    lastIdx += blockSize;
                    idsLeft -= blockSize;
                    threadCount++;
                }

                i++;
            }

            // Adjust the sleeptime based on the current completion rate
            sleepTime /= 1.5;
        }
        else
            sleepTime *= 1.5;

        // Give the threads a little time to possibly finish
        if (threadCount > 0)
        {
            if (sleepTime == std::chrono::milliseconds(0))
                sleepTime = std::chrono::milliseconds(50);

            std::this_thread::sleep_for(sleepTime);
        }
    }

    // Ensure all the threads have properly terminated
    for (unsigned int i = 0; i < cores; i++)
        threads[i].join();
}

static void SliceTrigsToLayersMF(std::size_t startIdx, std::size_t endIdx, bool* doneFlag,
                                 MeshInfoPtr mip, Progressor &prog)
{
    SlicerLog(std::string("Slicing trigs: ") + std::to_string(startIdx) + std::string(" to ") + std::to_string(endIdx));

    for (std::size_t i = startIdx; i < endIdx; i++)
    {
        double zPoint = (double)i * GlobalSettings::LayerHeight.Get();
        std::vector<TrigLineSegment> &lineList = mip->layerComponents[i].initialLineList;
        lineList.reserve(50); // Maybe a nice amount?

        for (std::size_t j = 0; j < mip->sliceMesh->trigCount; j++)
        {
            Triangle &trig = mip->sliceMesh->trigs[j];

            // Check if the triangle contains the current layer's z
            double z[3];
            getTrigPointFloats(trig, z, 2, mip);
            double minZ = std::min(z[0], std::min(z[1], z[2]));
            double maxZ = std::max(z[0], std::max(z[1], z[2]));
            if (minZ != maxZ && zPoint <= maxZ && zPoint >= minZ)
            {
                // Determine at which points the z is on the triangle

                // We need to determine which two sides of the triangle intersect with the z point
                // point A should be the point where these two sides connect
                // point B and C should therefore be the other two points
                uint8_t a, b, c;
                bool set = false;
                if (zPoint == z[0])
                {
                    if (zPoint == z[1])
                    {
                        a = 2;
                        b = 0;
                        c = 1;
                        set = true;
                    }
                    else if (zPoint == z[2])
                    {
                        a = 1;
                        b = 2;
                        c = 0;
                        set = true;
                    }
                }
                else if (zPoint == z[1] && zPoint == z[2])
                {
                    a = 0;
                    b = 1;
                    c = 2;
                    set = true;
                }

                if (!set)
                {
                    bool oneTwo = false;
                    bool oneThree = false;
                    bool twoThree = false;

                    if ((zPoint <= z[0] && zPoint >= z[1]) || (zPoint >= z[0] && zPoint <= z[1]))
                        oneTwo = true;
                    if ((zPoint <= z[0] && zPoint >= z[2]) || (zPoint >= z[0] && zPoint <= z[2]))
                        oneThree = true;
                    if ((zPoint <= z[1] && zPoint >= z[2]) || (zPoint >= z[1] && zPoint <= z[2]))
                        twoThree = true;

                    if (oneTwo && oneThree)
                    {
                        a = 0;
                        b = 1;
                        c = 2;
                    }
                    else if (oneTwo && twoThree)
                    {
                        a = 1;
                        b = 2;
                        c = 0;
                    }
                    else
                    {
                        a = 2;
                        b = 0;
                        c = 1;
                    }
                }

                double x[3], y[3];
                getTrigPointFloats(trig, x, 0, mip);
                getTrigPointFloats(trig, y, 1, mip);

                // First calculate the relationship of z to x on the one side of the triangle
                double zToX1 = (z[a] != z[b]) ? ((x[a] - x[b]) / (z[a] - z[b])) : 0;
                // Then calculate the relationship of z to y on the one side of the triangle
                double zToY1 = (z[a]  != z[b]) ? ((y[a] - y[b]) / (z[a] - z[b])) : 0;
                // Then calculate the relationship of z to x on the other side of the triangle
                double zToX2 = (z[a]  != z[c]) ? ((x[a] - x[c]) / (z[a] - z[c])) : 0;
                // Then calculate the relationship of z to y on the other side of the triangle
                double zToY2 = (z[a]  != z[c]) ? ((y[a] - y[c]) / (z[a] - z[c])) : 0;

                // Now calculate the z rise above pointB
                double zRise1 = zPoint - z[b];
                // And also the z rise above pointC
                double zRise2 = zPoint - z[c];

                // We can now calculatet the x and y points on both sides of the triangle using the z rises that were calculated above
                IntPoint p1 = IntPoint((cInt)((x[b] + zToX1 * zRise1) * scaleFactor),
                                       (cInt)((y[b] + zToY1 * zRise1) * scaleFactor));
                IntPoint p2 = IntPoint((cInt)((x[c] + zToX2 * zRise2) * scaleFactor),
                                       (cInt)((y[c] + zToY2 * zRise2) * scaleFactor));

                if (p1 != p2)
                {
                    // Add the line and keep of track of which face it relates to
                    mip->layerComponents[i].faceToLineIdxs.insert(std::make_pair(j, lineList.size()));
                    lineList.push_back(TrigLineSegment(p1, p2, j));
                }
            }
        }

        prog.CompleteStepPart();
    }

    *doneFlag = true;
}

static inline void SliceTrigsToLayers(MeshInfoPtr mip, Progressor &prog)
{
    SlicerLog("Slicing triangles into layers");

    MultiRunFunction(SliceTrigsToLayersMF, 0, mip->layerCount, mip, prog);
}

static inline long SquaredDist(const IntPoint& p1, const IntPoint& p2)
{
    long a = p2.X - p1.X;
    long b = p2.Y - p1.Y;
    return (a*a) + (b*b);
}

static void ProcessPolyNode(PolyNode *pNode, std::vector<LayerIsland> &isleList)
{
    for (PolyNode *child : pNode->Childs)
    {
        // Trying to emplace here does not work because the reference to the
        // back iterator clashes with the recursive function
        LayerIsland isle;
        isle.outlinePaths.push_back(child->Contour);

        for (PolyNode *grandChild : child->Childs)
        {
            isle.outlinePaths.push_back(grandChild->Contour);
            ProcessPolyNode(grandChild, isleList);
        }

        isleList.emplace_back(std::move(isle));
    }
}

static IntPoint operator-(const IntPoint &p1, const IntPoint &p2)
{
    return IntPoint(p1.X - p2.X, p1.Y - p2.Y);
}

static bool InALine(const IntPoint &p1, const IntPoint &p2, const IntPoint &p3)
{
    // We calculate the cosine between p2p1 and p2p3 to see if they
    // are almost in a straight line

    IntPoint A = p1 - p2;
    IntPoint B = p3 - p2;
    double dotP = A.X * A.X + B.Y * B.Y;
    double magA = std::sqrt(A.X * A.X + A.Y * A.Y);
    double magB = std::sqrt(B.X * B.X + B.Y * B.Y);

    double magAB = magA * magB;
    if (magAB != 0)
    {
        double cos = dotP / (magA * magB);
        const double thresh = std::cos(177.5 / 180.0 * PI);

        // More negative cos is closer to 180 degrees
        return (cos < thresh);
    }
    else
        return true;
}

static void OptimizePaths(Paths& paths)
{
    for (std::size_t i = 0; i < paths.size(); i++)
    {
        Path &path = paths[i];
        Path optiPath;
        optiPath.reserve(path.size());

        if (path.size() < 3)
            continue;

        // Go through each point and check if the next one is either
        // too close or part of an almost straight line
        std::size_t j = 0;
        while (true)
        {
            IntPoint p1 = path[j];

            const cInt minDiff = (cInt)(0.075 * 0.075 * scaleFactor * scaleFactor);
            if (j == path.size()-1)
            {
                // The last point should be checked differently to avoid checking the first point again
                IntPoint p2 = path[0];
                if ((SquaredDist(p1, p2) >= minDiff) && (!InALine(p1, p2, path[1])))
                    optiPath.push_back(p2);

                break;
            }
            else
            {
                std::size_t k = j + 1;
                IntPoint p2 = path[k];

                // Skip past all the very close points
                while ((k < path.size()-1) && (SquaredDist(p1, p2) < minDiff))
                {
                    k++;
                    p2 = path[k];
                }

                // Skip past points almost in line with their following
                // and previous points
                bool inLine = true;
                while ((k < path.size()-1) && inLine)
                {
                    IntPoint p3 = path[(k == path.size()) ? 0 : k + 1];

                    if (InALine(p1, p2, p3))
                    {
                        k++;
                        p2 = path[k];
                    }
                    else
                        inLine = false;
                }

                if (k >= path.size())
                    j = path.size()-1;
                else
                    j = k;

                // We can now add the valid point and search for the next one
                optiPath.push_back(p2);
            }
        }

        // Finally we can move to the optimized path
        optiPath.shrink_to_fit();
        paths[i] = optiPath;
    }
}

static void CalculateIslandsFromInitialLinesMF(std::size_t startIdx, std::size_t endIdx, bool* doneFlag,
                                               MeshInfoPtr mip, Progressor &prog)
{
    SlicerLog(std::string("Calculating islands: ") + std::to_string(startIdx)
              + std::string(" to ") + std::to_string(endIdx));

    for (std::size_t i = startIdx; i < endIdx; i++)
    {
        SlicerLog("Calculating islands for layer: " + std::to_string(i));
        LayerComponent &layerComp = mip->layerComponents[i];
        std::vector<TrigLineSegment> &lineList = layerComp.initialLineList;

        if (lineList.size() < 2)
            continue;

        // We need a list of polygons which have already been closed and those that still need closing
        Paths closedPaths, openPaths;

        for (std::size_t startIdx = 0; startIdx < lineList.size(); startIdx++)
        {
            TrigLineSegment &startLine = lineList[startIdx];
            if (startLine.usedInPolygon)
                continue;

            startLine.usedInPolygon = true;

            // Start with the assumption that we are working on a closed path and move it if needed
            closedPaths.emplace_back();
            Path &curPath = closedPaths.back();
            std::size_t lineIdxToConnectFrom = startIdx;
            bool open = true;
            curPath.push_back(startLine.p1);
            curPath.push_back(startLine.p2);
            IntPoint pointToConnectTo = startLine.p2;

            // Try to build a closed polygon until we have exhausted all available connected lines
            while (open)
            {
                bool connected = false;
                const Triangle &trigToConnectFrom = TrigAtIdx(lineList[lineIdxToConnectFrom].trigIdx, mip);

                // Go through all three vertices on the triangle
                for (uint8_t j = 0; j < 3; j++)
                {
                    const Vertex &v = VertAtIdx(trigToConnectFrom.vertIdxs[j], mip);

                    // Test against all triangles that share a vertex for a line connection
                    for (std::size_t touchIdx : v.trigIdxs)
                    {
                        // Check if this triangle has a linesegment on this layer
                        auto touchLineItr = layerComp.faceToLineIdxs.find(touchIdx);
                        if (touchLineItr == layerComp.faceToLineIdxs.end())
                            continue;
                        std::size_t touchLineIdx = touchLineItr->second;
                        TrigLineSegment &touchLine = lineList[touchLineIdx];

                        // Do not check the line against itself
                        if (touchLineIdx == lineIdxToConnectFrom)
                            continue;

                        // Prevent infinite loops by reconnecting to old lines
                        if (touchLine.usedInPolygon)
                            continue;

                        if (pointToConnectTo == touchLine.p1)
                            connected = true;
                        else if (pointToConnectTo == touchLine.p2)
                        {
                            touchLine.SwapPoints();
                            connected = true;
                        }

                        if (connected)
                        {
                            touchLine.usedInPolygon = true;

                            if (touchLine.p2 == startLine.p1)
                                open = false;
                            else
                            {
                                curPath.push_back(touchLine.p2);
                                pointToConnectTo = touchLine.p2;
                                lineIdxToConnectFrom = touchLineIdx;
                            }

                            break;
                        }
                    }

                    if (connected)
                        break;
                }

                if (!connected)
                    break;
            }

            if (open)
            {
                if (closedPaths.back().size() > 0)
                    openPaths.emplace_back(std::move(closedPaths.back()));

                closedPaths.pop_back();
            }
        }

        // The list is no longer needed and can be removed to save memory
        lineList.clear();
        lineList.shrink_to_fit();

        // TODO: closing needs to be tested

        if (openPaths.size() > 0)
            std::cout << "Open paths: " << openPaths.size()
                      << " closed: " << closedPaths.size() << std::endl;

        const cInt minDiff = (cInt)(0.05 * 0.05 * scaleFactor * scaleFactor);

        //Paths toForceClose;
        Paths toClose;

        // First try to close up little gaps or create longer chains
        for (std::size_t a = 0; a < openPaths.size(); a++)
        {
            if (openPaths[a].size() == 0)
                continue;

            closedPaths.emplace_back(openPaths[a]);
            Path &closedPath = closedPaths.back();
            openPaths[a].clear();

            while (SquaredDist(closedPath.front(), closedPath.back()) > minDiff)
            {
                cInt bestDiff = minDiff * 3;
                long bestIdx = -1;
                bool bestSwapped = false;

                for (std::size_t b = a + 1; b < openPaths.size(); b++)
                {
                    if (openPaths[b].size() == 0)
                        continue;

                    const Path &testPath = openPaths[b];
                    if (SquaredDist(closedPath.back(), testPath.front()) < bestDiff)
                    {
                        bestIdx = b;
                        bestDiff = SquaredDist(closedPath.back(), testPath.front());
                        bestSwapped = false;
                    }
                    else if (SquaredDist(closedPath.back(), testPath.back()) < bestDiff)
                    {
                        bestIdx = b;
                        bestDiff = SquaredDist(closedPath.back(), testPath.back());
                        bestSwapped = true;
                    }
                }

                if (bestIdx == -1)
                {
                    // We will try to force it closed later
                    toClose.emplace_back(std::move(closedPath));
                    closedPaths.pop_back();

                    break;
                }
                else
                {
                    if (bestSwapped)
                        std::reverse(openPaths[bestIdx].begin(), openPaths[bestIdx].end());

                    closedPath.reserve(closedPath.size() + openPaths[bestIdx].size());
                    closedPath.insert(closedPath.begin(), openPaths[bestIdx].begin(), openPaths[bestIdx].end());
                    openPaths[bestIdx].clear();
                }
            }
        }

        if (toClose.size() > 0)
            std::cout << "To force: " << toClose.size()
                      << " closed: " << closedPaths.size() << std::endl;

        // Finally pair up the chains that need to be forced close
        for (std::size_t a = 0; a < toClose.size(); a++)
        {
            if (toClose[a].size() == 0)
                continue;

            closedPaths.emplace_back(toClose[a]);
            Path &forcedPath = closedPaths.back();
            toClose[a].clear();

            while (true)
            {
                cInt bestDiff = SquaredDist(forcedPath.front(), forcedPath.back());
                long bestIdx = -1;
                bool bestSwapped = false;

                for (std::size_t b = a + 1; b < toClose.size(); b++)
                {
                    if (toClose[b].size() == 0)
                        continue;

                    const Path &testPath = toClose[b];
                    if (SquaredDist(forcedPath.back(), testPath.front()) < bestDiff)
                    {
                        bestIdx = b;
                        bestDiff = SquaredDist(forcedPath.back(), testPath.front());
                        bestSwapped = false;
                    }
                    else if (SquaredDist(forcedPath.back(), testPath.back()) < bestDiff)
                    {
                        bestIdx = b;
                        bestDiff = SquaredDist(forcedPath.back(), testPath.back());
                        bestSwapped = true;
                    }
                }

                if (bestIdx == -1)
                {
                    // Close is up
                    std::cout << "Forced close: " << a << std::endl;
                    break;
                }
                else
                {
                    if (bestSwapped)
                        std::reverse(toClose[bestIdx].begin(), toClose[bestIdx].end());

                    forcedPath.reserve(forcedPath.size() + toClose[bestIdx].size());
                    forcedPath.insert(forcedPath.begin(), toClose[bestIdx].begin(), toClose[bestIdx].end());
                    toClose[bestIdx].clear();
                }
            }
        }

//#define TEST_NO_OPTIMIZE
#ifndef TEST_NO_OPTIMIZE
        OptimizePaths(closedPaths);
#endif

        // We now need to put the newly created polygons through clipper sothat it can detect holes for us
        // and then make proper islands with the returned data

        PolyTree resultTree;
        Clipper clipper;
        clipper.AddPaths(closedPaths, PolyType::ptSubject, true);
        clipper.Execute(ClipType::ctUnion, resultTree, PolyFillType::pftNonZero, PolyFillType::pftNonZero);
        clipper.Execute(ClipType::ctUnion, resultTree);

        // We need to itterate through the tree recursively because of its child structure
        ProcessPolyNode(&resultTree, layerComp.islandList);

        // Optimize memory usage
        layerComp.islandList.shrink_to_fit();

        prog.CompleteStepPart();
    }

    *doneFlag = true;
}

static inline void CalculateIslandsFromInitialLines(MeshInfoPtr mip, Progressor &prog)
{
    SlicerLog("Calculating initial islands");

    MultiRunFunction(CalculateIslandsFromInitialLinesMF, 0, mip->layerCount, mip, prog);
}

static void GenerateOutlineSegmentsMF(std::size_t startIdx, std::size_t endIdx, bool* doneFlag,
                                      MeshInfoPtr mip, Progressor &prog)
{
    SlicerLog(std::string("Outline: ") + std::to_string(startIdx) + std::string(" to ") + std::to_string(endIdx));

    cInt halfNozzle = -(NozzleWidth * scaleFactor / 2.0);

    for (std::size_t i = startIdx; i < endIdx; i++)
    {
        LayerComponent &layerComp = mip->layerComponents[i];

        SlicerLog("Outline: " + std::to_string(i));

        for (LayerIsland &isle : layerComp.islandList)
        {
            // TODO: remove the invalid islands
            if (isle.outlinePaths.size() < 1)
                continue;

            // The first outline will be one that is half an extrusion thinner
            // than the sliced outline, ths is sothat the dimensions
            // do not change once extruded
            Paths outline;
            ClipperOffset offset;
            offset.AddPaths(isle.outlinePaths, JoinType::jtMiter, EndType::etClosedPolygon);
            offset.Execute(outline, halfNozzle);

            for (std::size_t j = 0; j < GlobalSettings::ShellThickness.Get(); j++)
            {
                // Place the newly created outline in its own segment
                LayerSegment &outlineSegment = isle.segments.emplace<LayerSegment>(SegmentType::OutlineSegment);
                outlineSegment.segmentSpeed = layerComp.layerSpeed;
                outlineSegment.outlinePaths = outline;
                cInt dist = halfNozzle - (cInt)(NozzleWidth * scaleFactor * (j + 1));

                //We now shrink the outline with one extrusion width for the next shell if any
                offset.Clear();
                offset.AddPaths(isle.outlinePaths, JoinType::jtMiter, EndType::etClosedPolygon);
                offset.Execute(outline, dist);
            }

            // We now need to store the smallest outline as the new layer outline for infill trimming purposes
            // the current outline though is just half an extrusion width to small
            offset.Clear();
            offset.AddPaths(outline, JoinType::jtMiter, EndType::etClosedPolygon);
            offset.Execute(isle.outlinePaths, NozzleWidth * scaleFactor);
        }

        prog.CompleteStepPart();
    }

    *doneFlag = true;
}

static inline void GenerateOutlineSegments(MeshInfoPtr mip, Progressor &prog)
{
    SlicerLog("Generating outline segments");

    // Check if there should be at least one shell
    if (GlobalSettings::ShellThickness.Get() < 1)
        return;

    MultiRunFunction(GenerateOutlineSegmentsMF, 0, mip->layerCount, mip, prog);
}

std::unordered_map<float, cInt> densityDividers;

static void CalculateDensityDivider(float density)
{
    // Calculate the needed spacing

    // d% = 1 / (x% + 1)
    // d% * x + d% = 1
    // a + d% = 1
    // a = 1 - d%
    // x = a / d%

    float d = density / 100.0f;
    float a = 1 - d;
    float x = a / d;
    cInt spacing = (cInt)(NozzleWidth * scaleFactor * x);
    densityDividers[density] = spacing + (NozzleWidth * scaleFactor);
}

static inline void CalculateDensityDividers()
{
    densityDividers.clear();

    CalculateDensityDivider(15.0f);
    CalculateDensityDivider(100.0f);
    CalculateDensityDivider(10.0f);
}

static inline void CalculateTopBottomSegments(MeshInfoPtr mip, Progressor &prog)
{
    SlicerLog("Calculating top and bottom segments");

    // TODO: implement seperate top and bottom thickness
    cInt partNozzle = (NozzleWidth * scaleFactor / 10.0);

    std::size_t tBCount = std::ceil(GlobalSettings::TopBottomThickness.Get() / GlobalSettings::LayerHeight.Get());

    // We can run the top and bottom segment generation in 2 threads because they are independant

    // Determine how many layers have to be done
    int workParts = (mip->layerCount - tBCount) - 1;
    workParts += (mip->layerCount - 1) - (mip->layerCount - tBCount - 1);
    workParts += (mip->layerCount - 2) - (tBCount - 1);
    workParts += tBCount;
    prog.StartNextStep(workParts);

    std::thread topThread([&]()
    {
        // To calculate the top segments we need to go from the bottom up,
        // take each island as a subject, take the outline of the above layer
        // as a clipper and perform a difference operation. The result will
        // then be the top segment(s) for each layer

        // Go from the second layer to the second highest layer as everything
        // on the top layer is a top segment in any case
        // and everything on the first layer is a bottom segment in any case

        Clipper clipper;
        ClipperOffset offset;

        if (tBCount > 0) // TODO: top
        {
            for (std::size_t i = 1; i < mip->layerCount - tBCount; i++)
            {
                SlicerLog("Top: " + std::to_string(i));

                //First we need to calculate the intersection of the top few layers above it
                Paths aboveIntersection;

                for (std::size_t j = i + 1; j < std::min(i + tBCount + 1, mip->layerCount); j++)
                {
                    Paths combinedIsles;

                    // Combine the outlines of the islands into one
                    for (LayerIsland &isle : mip->layerComponents[j].islandList)
                    {
                        clipper.Clear();
                        clipper.AddPaths(combinedIsles, PolyType::ptClip, true);
                        clipper.AddPaths(isle.outlinePaths, PolyType::ptSubject, true);
                        clipper.Execute(ClipType::ctUnion, combinedIsles);
                    }

                    // Set the intial top if still unset
                    if (aboveIntersection.size() < 1)
                    {
                        aboveIntersection = combinedIsles;
                        continue;
                    }

                    clipper.Clear();
                    clipper.AddPaths(aboveIntersection, PolyType::ptSubject, true);
                    clipper.AddPaths(combinedIsles, PolyType::ptClip, true);
                    clipper.Execute(ClipType::ctIntersection, aboveIntersection);
                }

                // Grow the intersection a bit just to get rid of noise when cutting from it
                offset.Clear();
                offset.AddPaths(aboveIntersection, JoinType::jtMiter, EndType::etClosedPolygon);
                offset.Execute(aboveIntersection, partNozzle);

                for (LayerIsland &isle : mip->layerComponents[i].islandList)
                {
                    SegmentWithInfill &topSegment = isle.segments.emplace<SegmentWithInfill>(SegmentType::TopSegment);
                    clipper.Clear();
                    clipper.AddPaths(isle.outlinePaths, PolyType::ptSubject, true);
                    clipper.AddPaths(aboveIntersection, PolyType::ptClip, true);
                    clipper.Execute(ClipType::ctDifference, topSegment.outlinePaths);

                    if (topSegment.outlinePaths.size() > 0)
                    {
                        // All top segments are probably bridges
                        // TODO: implement bridge speed
                        topSegment.segmentSpeed = GlobalSettings::TravelSpeed.Get();
                        // Extrude more for a bridge
                        topSegment.infillMultiplier = 2.0f;
                    }
                    else
                        isle.segments.pop_back();
                }

                prog.CompleteStepPart();
            }

            for (std::size_t i = mip->layerCount - 1; i > mip->layerCount - tBCount - 1; i--)
            {
                SlicerLog("Top: " + std::to_string(i));

                for (LayerIsland &isle : mip->layerComponents[i].islandList)
                {
                    SegmentWithInfill &topSegment = isle.segments.emplace<SegmentWithInfill>(SegmentType::TopSegment);
                    topSegment.outlinePaths = isle.outlinePaths;
                    // All top segments are probably bridges
                    // TODO: implement bridge speed
                    topSegment.segmentSpeed = GlobalSettings::TravelSpeed.Get();
                    // Extrude more for a bridge
                    topSegment.infillMultiplier = 2.0f;
                }

                prog.CompleteStepPart();
            }
        }
    });

    std::thread bottomThread([&](){
        // To calculate the bottom segments we need to go from the top down,
        // take each island as a subject, take the outline of the layer below
        // as a clipper and perform a difference operation. The result will
        // then be the bottom segment(s) for each layer

        // Go through every layer from the second highest layer to the second lowest layer

        Clipper clipper;
        ClipperOffset offset;

        if (tBCount > 0) // TODO: bottom
        {
            for (std::size_t i = mip->layerCount - 2; i > tBCount - 1; i--)
            {
                if (i < 1)
                    continue;

                SlicerLog("Bottom: " + std::to_string(i));

                // First we need to calculate the intersection of the bottom few layers below it
                Paths belowIntersection;

                for (std::size_t j = i - 1; j > std::max(i - 1 - tBCount, (std::size_t)0); j--)
                {
                    Paths combinedIsles;

                    // Combine the outlines of the islands into one
                    for (LayerIsland &isle : mip->layerComponents[j].islandList)
                    {
                        clipper.Clear();
                        clipper.AddPaths(combinedIsles, PolyType::ptClip, true);
                        clipper.AddPaths(isle.outlinePaths, PolyType::ptSubject, true);
                        clipper.Execute(ClipType::ctUnion, combinedIsles);
                    }

                    // Set the intial top if still unset
                    if (belowIntersection.size() < 1)
                    {
                        belowIntersection = combinedIsles;
                        continue;
                    }

                    clipper.Clear();
                    clipper.AddPaths(belowIntersection, PolyType::ptSubject, true);
                    clipper.AddPaths(combinedIsles, PolyType::ptClip, true);
                    clipper.Execute(ClipType::ctIntersection, belowIntersection);
                }

                // Grow the intersection a bit just to get rid of noise when cutting from it
                offset.Clear();
                offset.AddPaths(belowIntersection, JoinType::jtMiter, EndType::etClosedPolygon);
                offset.Execute(belowIntersection, partNozzle);

                for (LayerIsland &isle : mip->layerComponents[i].islandList)
                {
                    SegmentWithInfill &bottomSegment = isle.segments.emplace<SegmentWithInfill>(SegmentType::BottomSegment);
                    clipper.Clear();
                    clipper.AddPaths(isle.outlinePaths, PolyType::ptSubject, true);
                    clipper.AddPaths(belowIntersection, PolyType::ptClip, true);
                    clipper.Execute(ClipType::ctDifference, bottomSegment.outlinePaths);

                    if (bottomSegment.outlinePaths.size() > 0)
                    {
                        // All non initial layer bottom segments are probably bridges
                        // TODO: implement bridge speed
                        bottomSegment.segmentSpeed = GlobalSettings::TravelSpeed.Get();
                        // Extrude more for a bridge
                        bottomSegment.infillMultiplier = 2.0f;
                    }
                    else
                        isle.segments.pop_back();
                }

                prog.CompleteStepPart();
            }

            // Every island in the bottom layer is obviously a bottom segment
            for (std::size_t i = 0; i < tBCount; i++)
            {
                SlicerLog("Bottom: " + std::to_string(i));

                for (LayerIsland &isle : mip->layerComponents[i].islandList)
                {
                    SegmentWithInfill &bottomSegment = isle.segments.emplace<SegmentWithInfill>(SegmentType::BottomSegment);
                    bottomSegment.outlinePaths = isle.outlinePaths;
                    // Initial bottom segments should not be bridges
                    bottomSegment.segmentSpeed = GlobalSettings::InfillSpeed.Get();
                }

                prog.CompleteStepPart();
            }
        }
    });

    // Wait for the threads to finish
    topThread.join();
    bottomThread.join();
}

static void CalculateInfillSegmentsMF(std::size_t startIdx, std::size_t endIdx, bool* doneFlag,
                                      MeshInfoPtr mip, Progressor &prog)
{
    SlicerLog(std::string("Infill: ") + std::to_string(startIdx) + std::string(" to ") + std::to_string(endIdx));

    // To calculate the segments that need normal infill we need to go through each island in each layer, we then need to subtract the
    // top or bottom segments from the outline shape polygons of the layer and we then have the segments that need normal infill

    Clipper clipper;

    for (std::size_t i = startIdx; i < endIdx; i++)
    {
        SlicerLog("Infill: " + std::to_string(i));

        for (LayerIsland &isle : mip->layerComponents[i].islandList)
        {
            clipper.Clear();

            // Add the outline shape polygons as the subject
            clipper.AddPaths(isle.outlinePaths, PolyType::ptSubject, true);

            // Then add existing segments such as top or bottom and outline as the clip
            for (const LayerSegment *seg : isle.segments)
            {
                // We do not want to subtract outline segments because we already use the overall outline of the island
                if (seg->type == SegmentType::OutlineSegment)
                    continue;

                clipper.AddPaths(seg->outlinePaths, PolyType::ptClip, true);
            }

            //SegmentWithInfill infillSeg(SegmentType::InfillSegment);
            SegmentWithInfill &infillSeg = isle.segments.emplace<SegmentWithInfill>(SegmentType::InfillSegment);
            infillSeg.segmentSpeed = GlobalSettings::InfillSpeed.Get();

            // We then need to perform a difference operation to determine the infill segments
            clipper.Execute(ClipType::ctDifference, infillSeg.outlinePaths);
        }

        prog.CompleteStepPart();
    }

    *doneFlag = true;
}

static inline void CalculateInfillSegments(MeshInfoPtr mip, Progressor &prog)
{
    SlicerLog("Calculating infill segments");

    MultiRunFunction(CalculateInfillSegmentsMF, 0, mip->layerCount, mip, prog);
}

static inline void CalculateSupportSegments(MeshInfoPtr mip, Progressor &prog)
{
    // TODO: implement this
}

#ifdef COMBINE_INFILL
static inline void CombineInfillSegments(MeshInfoPtr mip, Progressor &prog)
{
    SlicerLog("Combining infill segments");

    // To combine the infill segments we have to go through each layer, for each infill segment int that layer we need to perform
    // an intersection test with all the above layers infill segments. The combined segments should then be separated from its original
    // segments in both the first and the second layer, the same operation should then be performed with the result and all the infill
    // segments above it untill it has been done for the amount of layers specified int global values. The bottom one of the infill segment
    // will be completely removed from its layer. The extrusion multiplier of each segment will be determined by how many layers of infill
    // it represents

    std::size_t combCount = GlobalSettings::InfillCombinationCount.Get();
    if (combCount < 2)
        return;

    Clipper clipper;

    for (std::size_t i = mip->layerCount - 1; i > 0; i -= combCount)
    {
        Paths belowInfill;

        //This will indicate how many layers have been combined
        short combinedmip->layerCount = 1;

        // Then figure out to what extent it intersects with the layers below
        for (std::size_t j = i - 1; j > i - combCount && j > 0; j--)
        {
            // Fist combine all the infill segments for the layer
            Paths combinedInfill;

            for (LayerIsland &isle : mip->layerComponents[i].islandList)
            {
                for (LayerSegment *seg : isle.segments)
                {
                    if (seg->type != SegmentType::InfillSegment)
                        continue;

                    clipper.Clear();
                    clipper.AddPaths(combinedInfill, PolyType::ptSubject, true);
                    clipper.AddPaths(seg->outlinePaths, PolyType::ptClip, true);
                    clipper.Execute(ClipType::ctUnion, combinedInfill);
                }
            }

            if (belowInfill.size() > 0)
            {
                // Then determine which part intersects with the previous layer's infill
                clipper.Clear();
                clipper.AddPaths(belowInfill, PolyType::ptClip, true);
                clipper.AddPaths(combinedInfill, PolyType::ptSubject, true);
                clipper.Execute(ClipType::ctIntersection, combinedInfill);
            }
            else
                belowInfill = combinedInfill;

            combinedmip->layerCount++;
        }

        for (LayerIsland &mainIsle : mip->layerComponents[i].islandList)
        {
            Paths commonInfill;

            // Start by setting all the infill in the current island as the base
            for (LayerSegment *layerSeg : mainIsle.segments)
            {
                if (layerSeg->type != SegmentType::InfillSegment)
                    continue;

                clipper.Clear();
                clipper.AddPaths(commonInfill, PolyType::ptClip, true);
                clipper.AddPaths(layerSeg->outlinePaths, PolyType::ptSubject, true);
                clipper.Execute(ClipType::ctUnion, commonInfill);
            }

            // Then determine which part intersects with the previous layer's infill
            clipper.Clear();
            clipper.AddPaths(commonInfill, PolyType::ptClip, true);
            clipper.AddPaths(belowInfill, PolyType::ptSubject, true);
            clipper.Execute(ClipType::ctIntersection, commonInfill);

            if (commonInfill.size() < 1)
                continue;

            // We should now subtract the common infill from all the affected layers and then add it to the topmost layer
            // again but with it thickness that will account for all the layers

            for (std::size_t j = i; j > i - combCount && j > 0; j--)
            {
                for (LayerIsland &isle : mip->layerComponents[j].islandList)
                {
                    for (LayerSegment *seg : isle.segments)
                    {
                        if (seg->type != SegmentType::InfillSegment)
                            continue;

                        // Remove the common infill from the layersegment
                        clipper.Clear();
                        clipper.AddPaths(seg->outlinePaths, PolyType::ptSubject, true);
                        clipper.AddPaths(commonInfill, PolyType::ptClip, true);
                        clipper.Execute(ClipType::ctDifference, seg->outlinePaths);
                    }
                }
            }

            // Finally add the new segment to the topmost layer
            if (commonInfill.size() < 1)
                continue;

            SegmentWithInfill &infillSegment = mainIsle.segments.emplace<SegmentWithInfill>(SegmentType::InfillSegment);
            infillSegment.infillMultiplier = combCount; // TODO: maybe different variable
            infillSegment.segmentSpeed = GlobalSettings::InfillSpeed.Get();
            infillSegment.outlinePaths = commonInfill;
        }
    }
}
#endif

static inline void GenerateRaft(MeshInfoPtr mip, Progressor &prog)
{
    // TODO: implement this
    SlicerLog("Generating raft");
}

/*static inline void ReserveSkirtIsland(MeshInfoPtr mip)
{
    if (GlobalSettings::SkirtLineCount.Get() == 0)
        return;

    // Create an island at the front of the first layer for the skirt

    mip->layerComponents[0].islandList.emplace_back();
    mip->layerComponents[0].islandList.back().
            segments.emplace_back<LayerSegment>(SegmentType::SkirtSegment);
}*/

static inline void GenerateSkirt(MeshInfoPtr mip, Progressor &prog)
{
    SlicerLog("Generating skirt");

    // TODO: add skirt as a normal island

    if (GlobalSettings::SkirtLineCount.Get() == 0)
        return;

    prog.StartNextStep(2 + GlobalSettings::SkirtLineCount.Get());

    Clipper clipper;
    Paths comboOutline;

    for (const LayerIsland &isle : mip->layerComponents[0].islandList)
        clipper.AddPaths(isle.outlinePaths, PolyType::ptSubject, true);

    clipper.Execute(ClipType::ctUnion, comboOutline);
    OptimizePaths(comboOutline);

    prog.CompleteStepPart();

    IntPoint lastP(0, 0);
    auto &segs = mip->layerComponents[0].skirtSegments;
    mip->layerComponents[0].hasSkirt = true;
    ClipperOffset offset;

    auto &seg = mip->layerComponents[0].islandList[0].segments;

    offset.AddPaths(comboOutline, JoinType::jtSquare, EndType::etClosedPolygon);
    offset.Execute(comboOutline, GlobalSettings::SkirtDistance.Get() * scaleFactor);
    prog.CompleteStepPart();

    for (int a = 0; a < GlobalSettings::SkirtLineCount.Get(); a++)
    {
        offset.Clear();

        offset.AddPaths(comboOutline, JoinType::jtSquare, EndType::etClosedPolygon);
        offset.Execute(comboOutline, NozzleWidth * scaleFactor);

        for (Path path : comboOutline)
        {
            segs.emplace<TravelSegment>(lastP, path.front(), 0, 100);

            for (std::size_t i = 0; i < (path.size() - 1); i++)
                segs.emplace<ExtrudeSegment>(path[i], path[i + 1], 0, 10);

            lastP = path.back();
            segs.emplace<ExtrudeSegment>(lastP, path[0], 0, 10);
        }

        prog.CompleteStepPart();
    }
}

struct SectPoint
{
    IntPoint point;
    const Path *path;

    SectPoint (cInt x, cInt y, const Path *path_)
        : point(x, y), path(path_) {}
};

static inline cInt xOnAxis(const IntPoint &p, bool right)
{
    if (right)
        return p.X - p.Y;
    else
        return p.X + p.Y;
}

static bool CompForY(const SectPoint &a, const SectPoint &b)
{
    return (a.point.Y < b.point.Y);
}

static bool Clockwise(const IntPoint &A, const IntPoint &B, const IntPoint &C)
{
    // http://gamedev.stackexchange.com/questions/45412/understanding-math-used-to-determine-if-vector-is-clockwise-counterclockwise-f
    IntPoint v1 = A - B;
    IntPoint v2 = C - B;

    return (v1.Y*v2.X < v1.X*v2.Y);
}

static void FillInPaths(const Paths &outlines, std::vector<LineSegment> &infillLines,
                        float density, bool right)
{
    // All infill is aligned acording to the x-axis so we work with
    // the diagonal lines through the x-axis spaced according to the
    // divider value
    // This means we project each point of each poin to the axis and work
    // with the infill lines between 2 points that are then projected back
    // to these lines of the path. All paths are stored according to the index
    // of the diagonal line on the x-axis and then connected from bottom
    // to top.

    // By using 45 degrees we can avoid the use of trig functions
    // If other  angles need support these trig values should be
    // cached for densities

    double divider = densityDividers[density];

    // We store all intersections on the same diagonal line as to
    // connect them into lines later
    std::map<cInt, std::vector<SectPoint>> sectMap;

    for (const Path &path : outlines)
    {
        if (path.size() < 3)
            continue;

        for (std::size_t i = 0; i < path.size(); i++)
        {
            // Get the points for each line in the path
            IntPoint p1 = path[i];
            IntPoint p2 = (i < path.size()-1) ? path[i + 1] : path[0];

            double leftMost = xOnAxis(p1, right);
            double rightMost = xOnAxis(p2, right);
            IntPoint leftP, rightP;
            bool swapped;

            if (rightMost < leftMost)
            {
                cInt temp = leftMost;
                leftMost = rightMost;
                rightMost = temp;

                leftP = p2;
                rightP = p1;
                swapped = true;
            }
            else
            {
                leftP = p1;
                rightP = p2;
                swapped = false;
            }

            // We add a small rounding margin to prevent loss of seemingly obvious line
            cInt leftIdx = std::ceil(leftMost / divider);
            cInt rightIdx = std::floor(rightMost / divider);

            double yRise = rightP.Y - leftP.Y;
            double xRise = rightP.X - leftP.X;
            double xDist = rightMost - leftMost;

            // Intersections at the ends of linesegments are corners which are confusing
            // because they create the same point twice and som corners touch a line
            // inside a polygon while others are where it exists a polygon.
            //
            // At an exit corner the direction of the angle between the two lines on the
            // point is the same as the direction to the line through the points whilst
            // for touching corners they differ. We should only place one point for an
            // exit corner and no points for an inside one because an inside one does
            // signal a break in the line.

            // Now get all the points of intersection on this line
            for (cInt idx = leftIdx; idx <= rightIdx; idx++) {
                double xDiff = (idx * divider) - leftMost;
                double xPerc = xDiff / xDist;

                // Skip past perfect intersection on p1 and check
                // if perfect intersection on p2 are inside or outside
                if ((!swapped && (xPerc == 0.0)) || (swapped && (xPerc == 1.0)))
                    continue;
                else if ((swapped && (xPerc == 0.0)) || (!swapped && (xPerc == 1.0)))
                {
                    IntPoint p3;
                    if (i < path.size()-2)
                        p3 = path[i + 2];
                    else if (i < path.size()-1)
                        p3 = path[0];
                    else
                        p3 = path[1];

                    //v1 = p2 to p1;
                    //v2 = p2 to p3;

                    // Get the closest point on the line to p1
                    IntPoint pA;
                    IntPoint v = p1 - p2;
                    if (v.X == 0)
                    {
                        cInt delta = v.Y;
                        pA.Y = p2.Y + delta;
                        pA.X = (right) ? (p2.X + delta) : (p2.X - delta);
                    }
                    else
                    {
                        cInt delta = v.X;
                        pA.X = p2.X + delta;
                        pA.Y = (right) ? (p2.Y + delta) : (p2.Y - delta);
                    }

                    // Determine if it is CW from v1 to v2
                    // and it should the opposite the other way
                    bool clockV1ToV2 = Clockwise(p2, p1, p3);

                    if (Clockwise(p2, p1, pA) != clockV1ToV2)
                        continue;

                    // Get the closest point on the line to p3
                    v = p3 - p2;
                    if (v.X == 0)
                    {
                        cInt delta = v.Y;
                        pA.Y = p2.Y + delta;
                        pA.X = (right) ? (p2.X + delta) : (p2.X - delta);
                    }
                    else
                    {
                        cInt delta = v.X;
                        pA.X = p2.X + delta;
                        pA.Y = (right) ? (p2.Y + delta) : (p2.Y - delta);
                    }

                    // opposite (see above)
                    if (Clockwise(p2, p3, pA) == clockV1ToV2)
                        continue;
                }

                cInt xVal = leftP.X + (cInt)(xPerc * xRise);
                cInt yVal = leftP.Y + (cInt)(xPerc * yRise);
                sectMap[idx].emplace_back(xVal, yVal, &path);
            }
        }
    }

    // Now go through all the points on each line and create the segments

    // We need to group all lines on roughly the same level to avoid jumping
    // up and down between them.
    // Therefore an intial list is kept of all non-bottom lines and then added
    // to the standard list going zig zag
    std::map<std::size_t, std::vector<LineSegment>> higherLines;

    for (auto pair : sectMap)
    {
        std::vector<SectPoint> &points = pair.second;
        if (points.size() < 2)
            continue;

        std::sort(points.begin(), points.end(), CompForY);

        // Add the bottom lines
        infillLines.emplace_back(points[0].point, points[1].point);

        // Store the higher lines for later adding
        for (std::size_t i = 2; i < points.size(); i += 2)
            higherLines[i].emplace_back(points[i].point, points[i + 1].point);
    }

    // Now add back the higher lines from right to left (because we just went from left ro right)
    // and then the other way etc. untill all have been added

    // Reserve enough space for all the lines
    std::size_t fullSize = infillLines.size();
    for (auto pair : higherLines)
        fullSize += pair.second.size();
    infillLines.reserve(fullSize);

    bool rightToLeft = true;
    for (auto pair : higherLines)
    {
        std::vector<LineSegment> &lines = pair.second;

        if (rightToLeft)
        {
            for (std::size_t i = lines.size(); i > 0; i--)
                infillLines.push_back(lines[i - 1]);

            rightToLeft = false;
        }
        else
        {
            std::move(lines.begin(), lines.end(), std::back_inserter(infillLines));

            rightToLeft = true;
        }
    }
}

static void TrimInfillMF(std::size_t startIdx, std::size_t endIdx, bool* doneFlag,
                         MeshInfoPtr mip, Progressor &prog)
{
    SlicerLog(std::string("Trim infill: ") + std::to_string(startIdx) + std::string(" to ") + std::to_string(endIdx));

    // Even layers go right
    bool right = (std::div(startIdx, 2).rem == 0);

    for (std::size_t i = startIdx; i < endIdx; i++)
    {
        SlicerLog("Trim infill: " + std::to_string(i));

        for (LayerIsland &isle : mip->layerComponents[i].islandList)
        {
            for (LayerSegment *segment : isle.segments)
            {
                if (SegmentWithInfill* seg = dynamic_cast<SegmentWithInfill*>(segment))
                {
                    bool goRight = right;
                    float density;

                    switch (seg->type)
                    {
                    case SegmentType::InfillSegment:
                        // If the segment is an infill segment then we need to trim the correlating infill grid to fill it
                        density = 15.0f; // TODO
                        break;
                    case SegmentType::BottomSegment: case SegmentType::TopSegment:
                        // If this is a top or bottom segment then we need to trim the solid infill grid to fill it
                        density = 100.0f;
                        break;
                    case SegmentType::SupportSegment:
                        // If this is a support segment then we need to trim the support infill grid to fill it
                        density = 10.0f; // TODO
                        goRight = false;
                        break;
                    default:
                        std::cout << "Unhandled infill segment of type number: " << (int)seg->type << std::endl;
                        break;
                    }

                    FillInPaths(seg->outlinePaths, seg->fillLines, density, goRight);

                    seg->fillDensity = density;
                }
            }
        }

        right = !right;

        prog.CompleteStepPart();
    }

    *doneFlag = true;
}

static inline void TrimInfill(MeshInfoPtr mip, Progressor &prog)
{
    SlicerLog("Trimming infill");

    MultiRunFunction(TrimInfillMF, 0, mip->layerCount, mip, prog);
}

const cInt MoveHigher = scaleFactor / 10;

static void AddRetractedMove(PMCollection<ToolSegment> &toolSegments,
                            const IntPoint &p1,const IntPoint &p2,
                             int moveSpeed, cInt lastZ)
{
    // Retract filament to avoid stringing if possible and if the distance is long enough
    if (GlobalSettings::RetractionSpeed.Get() > 0 && GlobalSettings::RetractionDistance.Get() > 0)
    {
        const cInt minDist = 10 * scaleFactor;
        const cInt minDist2 = minDist * minDist;

        if (SquaredDist(p1, p2) > minDist2)
            toolSegments.emplace<RetractSegment>(GlobalSettings::RetractionDistance.Get());
    }

    // Create the actual move segment
    toolSegments.emplace<TravelSegment>(p1, p2, lastZ, moveSpeed);
}

// Do a binary search for the closest point on a polygon to a defined other point
// return true if closer distance than the parameter
static bool FindClosestPoint(const Path &outPath, const IntPoint &lastPoint, std::size_t &closestPoint, std::size_t &closestDist)
{
    //std::size_t clos = SquaredDist(lastPoint, outPath[0]);
    std::size_t upIdx = outPath.size() - 1;
    std::size_t lowIdx = 0;
    std::size_t midIdx = 0;

    while (lowIdx != upIdx)
    {
        midIdx = std::div(lowIdx + upIdx, 2).quot;

        if (midIdx == lowIdx)
        {
            if (SquaredDist(lastPoint, outPath[upIdx]) < SquaredDist(lastPoint, outPath[lowIdx]))
                midIdx = upIdx;

            break;
        }

        //if (clos < SquaredDist(lastPoint, outPath[midIdx]))
        if (SquaredDist(lastPoint, outPath[lowIdx]) < SquaredDist(lastPoint, outPath[midIdx]))
            upIdx = midIdx;
        else
            lowIdx = midIdx;
    }

    std::size_t clos = SquaredDist(lastPoint, outPath[midIdx]);
    if (clos < closestDist)
    {
        closestDist = clos;
        closestPoint = midIdx;
        return true;
    }

    return false;
}

// Extrudes a line in a infill segment whilst moving along the outline
static void ExtrudeLine(const std::size_t lineIdx, IntPoint &lastPoint, const cInt lastZ, bool &firstLine,
                        const LayerComponent &curLayer, SegmentWithInfill* infillSeg)
{
    LineSegment &line = infillSeg->fillLines[lineIdx];

    // Create a clean move to the line if needed
    if (firstLine)
        firstLine = false;
    else
    {
        if (SquaredDist(lastPoint, line.p2) < SquaredDist(lastPoint, line.p1))
            line.SwapPoints();

        IntPoint pA, pB;
        const Path *interPath;
        cInt interIdx = 0;

        const Path *closestPath;
        cInt closestIdx = 0;
        cInt closestDist = std::numeric_limits<cInt>::max();

        // Now check between which two outline points we are
        for (const Path &path : infillSeg->outlinePaths)
        {
            // We determine between which two lines the intersection is by checing
            // for colinearity
            for (interIdx = 0; (std::size_t)interIdx < path.size(); interIdx++)
            {
                pA = path[interIdx];
                pB = path[((std::size_t)interIdx == path.size()-1) ? 0 : interIdx + 1];

                if (InALine(pA, lastPoint, pB))
                {
                    interPath = &path;
                    goto FoundP2Intersect;
                }
                else if (SquaredDist(lastPoint, pA) < closestDist)
                {
                    closestDist = SquaredDist(lastPoint, pA);
                    closestPath = &path;
                    closestIdx = interIdx;
                }
            }
        }

        // If not skipped past then we use the closest point
        interPath = closestPath;
        interIdx = closestIdx;

        FoundP2Intersect:

        // Determine if the other point is on the same line
        if (InALine(pA, line.p1, pB))
            infillSeg->toolSegments.emplace<TravelSegment>(lastPoint, line.p1, lastZ, curLayer.moveSpeed);
        else
        {
            // Otherwise check if they are on the same polygon
            bool noInter = true;
            bool forwards = true;

            closestDist = std::numeric_limits<cInt>::max();
            closestIdx = 2;
            bool closestForwards = true;

            // The intersection should be closeish to the first one
            // Do a fanout search for the point from the previous one
            cInt i = 2;
            cInt fullSize = interPath->size();
            cInt halfSize = (fullSize / 2) + 1;
            while (noInter && (i < halfSize))
            {
                // Search a step forward
                cInt aIdx = interIdx + i;
                cInt bIdx = aIdx + 1;

                // Implement wrapping
                if (aIdx >= fullSize)
                {
                    aIdx -= fullSize;
                    bIdx -= fullSize;
                }
                else if (bIdx >= fullSize)
                {
                    bIdx -= fullSize;
                }

                pA = interPath->at(aIdx);
                pB = interPath->at(bIdx);

                if (InALine(pA, line.p1, pB))
                    noInter = false;
                else
                {
                    // Update info for the closest exit point as backup
                    if (SquaredDist(line.p1, pA) < closestDist)
                    {
                        closestDist = SquaredDist(line.p1, pA);
                        closestIdx = i;
                        closestForwards = true;
                    }

                    // Search a step backwards instead
                    aIdx = interIdx - i + 2; // Interidx is already the "left" point of the intersection line
                    bIdx = aIdx - 1;

                    if (aIdx < 0)
                    {
                        aIdx += fullSize;
                        bIdx += fullSize;
                    }
                    else if (bIdx < 0)
                    {
                        bIdx += fullSize;
                    }

                    pA = interPath->at(aIdx);
                    pB = interPath->at(bIdx);

                    if (InALine(pA, line.p1, pB))
                    {
                        noInter = false;
                        forwards = false;
                    }
                    else
                    {
                        // Update info for the closest exit point as backup
                        if (SquaredDist(line.p1, pA) < closestDist)
                        {
                            closestDist = SquaredDist(line.p1, pA);
                            closestIdx = i;
                            closestForwards = false;
                        }

                        i++;
                    }
                }
            }

            if (noInter)
            {
                // Exit from the closest point if no intersection
                i = closestIdx;
                forwards = closestForwards;
            }

            // Move along the outline
            if (forwards)
            {
                cInt idxB = ((interIdx + 1) == fullSize) ? 0 : (interIdx + 1);

                infillSeg->toolSegments.emplace<TravelSegment>(lastPoint, interPath->at(idxB),
                                                               lastZ, curLayer.moveSpeed);

                for (cInt k = interIdx + 1; k < interIdx + i; k++)
                {
                    cInt idxA = k;
                    idxB = k + 1;

                    if (idxA >= fullSize)
                    {
                        idxA -= fullSize;
                        idxB -= fullSize;
                    }
                    else if (idxB >= fullSize)
                        idxB -= fullSize;

                    infillSeg->toolSegments.emplace<TravelSegment>(interPath->at(idxA), interPath->at(idxB),
                                                              lastZ, curLayer.moveSpeed);
                }

                infillSeg->toolSegments.emplace<TravelSegment>(interPath->at(idxB), line.p1,
                                                          lastZ, curLayer.moveSpeed);
            }
            else
            {
                cInt idxB = interIdx;

                infillSeg->toolSegments.emplace<TravelSegment>(lastPoint, interPath->at(idxB),
                                                               lastZ, curLayer.moveSpeed);

                for (cInt k = interIdx; k > (interIdx - i + 2); k--)
                {
                    cInt idxA = k;
                    idxB = k - 1;

                    if (idxA < 0)
                    {
                        idxA += fullSize;
                        idxB += fullSize;
                    }
                    else if (idxB < 0)
                        idxB += fullSize;

                    infillSeg->toolSegments.emplace<TravelSegment>(interPath->at(idxA), interPath->at(idxB),
                                                              lastZ, curLayer.moveSpeed);
                }

                infillSeg->toolSegments.emplace<TravelSegment>(interPath->at(idxB), line.p1,
                                                          lastZ, curLayer.moveSpeed);
            }
        }
    }

    // Extrude the line
    infillSeg->toolSegments.emplace<ExtrudeSegment>(line, lastZ, infillSeg->segmentSpeed);

    lastPoint = line.p2;
}

static IntPoint *LayerLastPoints;

static void CalculateToolpathMF(std::size_t startIdx, std::size_t endIdx, bool* doneFlag,
                                MeshInfoPtr mip, Progressor &prog)
{
    SlicerLog(std::string("Toolpath: ") + std::to_string(startIdx) + std::string(" to ") + std::to_string(endIdx));

    IntPoint lastPoint(0, 0);
    cInt lastZ = std::max((double)0, (GlobalSettings::LayerHeight.Get() * scaleFactor) * ((double)(startIdx) - 0.5));

    for (std::size_t i = startIdx; i < endIdx; i++)
    {
        SlicerLog("Toolpath: " + std::to_string(i));
        LayerComponent &curLayer = mip->layerComponents[i];

        // Move to the new z position
        // We need half a layerheight for the filament
        cInt newZ = (GlobalSettings::LayerHeight.Get() * scaleFactor) * ((double)(i) + 0.5);
        curLayer.initialLayerMoves.emplace_back(lastPoint, lastZ, newZ, curLayer.layerSpeed);
        lastZ = newZ;

        // Create a list of islands left to go to
        std::size_t isleCount = curLayer.islandList.size();
        std::size_t islesLeft = isleCount;
        bool *islesUsed = new bool[isleCount];
        for (std::size_t i = 0; i < isleCount; i++)
            islesUsed[i] = false;

        // Continue to the closest island until all have been moved to
        while (islesLeft > 0)
        {
            std::size_t closestIsle = 0;
            std::size_t closestPoint = 0;
            std::size_t closestDist = std::numeric_limits<std::size_t>::max();

            // Do a binary search for the closest point on each island
            for (std::size_t j = 0; j < isleCount; j++)
            {
                if (islesUsed[j])
                    continue;

                LayerIsland &isle = curLayer.islandList[j];
                LayerSegment *outSeg = isle.segments.begin().operator *();

                // TODO: get rid of these segments
                if (outSeg->outlinePaths.size() == 0)
                {
                    islesUsed[j] = true;
                    islesLeft--;
                    continue;
                }

                Path &outPath = outSeg->outlinePaths[0];

                if (FindClosestPoint(outPath, lastPoint, closestPoint, closestDist))
                    closestIsle = j;
            }

            // Now we can handle this island
            islesUsed[closestIsle] = true;
            islesLeft--;
            LayerIsland &curIsle = curLayer.islandList[closestIsle];

            bool firstSeg = true;

            // Create the commands for the other segments
            for (LayerSegment *seg : curIsle.segments)
            {
                if (seg->outlinePaths.size() == 0)
                    continue;

                if (SegmentWithInfill* infillSeg = dynamic_cast<SegmentWithInfill*>(seg))
                {
                    if (infillSeg->fillLines.size() == 0)
                        continue;

                    // Find the closest line to start from
                    closestDist = std::numeric_limits<std::size_t>::max();
                    std::size_t closIdx = 0;
                    bool closSwapped = false;
                    for (std::size_t k = 0; k < infillSeg->fillLines.size(); k++)
                    {
                        const LineSegment &line = infillSeg->fillLines[k];

                        if ((std::size_t)SquaredDist(lastPoint, line.p1) < closestDist)
                        {
                            closSwapped = false;
                            closestDist = SquaredDist(lastPoint, line.p1);
                            closIdx = k;
                        }

                        // The other point can be even closer
                        if ((std::size_t)SquaredDist(lastPoint, line.p2) < closestDist)
                        {
                            closSwapped = true;
                            closestDist = SquaredDist(lastPoint, line.p2);
                            closIdx = k;
                        }
                    }

                    if (closSwapped)
                        infillSeg->fillLines[closIdx].SwapPoints();

                    // Move to the closest line
                    AddRetractedMove(infillSeg->toolSegments, lastPoint, infillSeg->fillLines[closIdx].p1, curLayer.moveSpeed, lastZ);

                    // Extrude all the lines
                    bool firstLine = true;

                    for (std::size_t k = closIdx; k < infillSeg->fillLines.size(); k++)
                        ExtrudeLine(k, lastPoint, lastZ, firstLine, curLayer, infillSeg);

                    for (std::size_t k = 0; k < closIdx; k++)
                        ExtrudeLine(k, lastPoint, lastZ, firstLine, curLayer, infillSeg);
                }
                else
                {
                    for (const Path &path : seg->outlinePaths)
                    {
                        // Find the closest point to the previous segment if needed
                        std::size_t closIdx = 0;
                        if (firstSeg)
                        {
                            closIdx = closestPoint;
                            firstSeg = false;
                        }
                        else
                        {
                            closestDist = std::numeric_limits<std::size_t>::max();
                            FindClosestPoint(path, lastPoint, closIdx, closestDist);
                        }

                        // Move to the path
                        AddRetractedMove(seg->toolSegments, lastPoint, path[closIdx], curLayer.moveSpeed, lastZ);

                        // Extrude the outline starting with the closest point
                        for (std::size_t k = closIdx; k < path.size()-1; k++)
                           seg->toolSegments.emplace<ExtrudeSegment>(path[k], path[k + 1], lastZ, seg->segmentSpeed);

                        seg->toolSegments.emplace<ExtrudeSegment>(path.back(), path.front(), lastZ, seg->segmentSpeed);

                        for (std::size_t k = 0; k < closIdx; k++)
                           seg->toolSegments.emplace<ExtrudeSegment>(path[k], path[k + 1], lastZ, seg->segmentSpeed);

                        lastPoint = path[closIdx];
                    }
                }
            }
        }

        delete[] islesUsed;

        LayerLastPoints[i] = lastPoint;

        prog.CompleteStepPart();
    }

    *doneFlag = true;
}

static inline void CalculateToolpath(MeshInfoPtr mip, Progressor &prog)
{
    SlicerLog("Calculating toolpath");

    LayerLastPoints = new IntPoint[mip->layerCount];

    MultiRunFunction(CalculateToolpathMF, 0, mip->layerCount, mip, prog);

    // Adjust the starting position of each layer to the end of the last one
    for (std::size_t i = 1; i < mip->layerCount; i++)
    {
        if (mip->layerComponents[i].initialLayerMoves.size() > 0)
            mip->layerComponents[i].initialLayerMoves[0].SetStartPoint(LayerLastPoints[i - 1]);
    }

    delete[] LayerLastPoints;
}

MeshInfoPtr ChopperEngine::SliceMesh(Mesh *inputMesh, Progressor::ProgressCallback callback,
                                     const void *callbackContext)
{
    MeshInfoPtr mip = std::make_shared<MeshInfo>(inputMesh);
    Progressor prog = Progressor(8, callback, callbackContext);

    // Slice the triangles into layers
    SliceTrigsToLayers(mip, prog); // Step 1

    // Calculate islands from the original lines
    CalculateIslandsFromInitialLines(mip, prog); // Step 2

    // Generate the outline segments
    GenerateOutlineSegments(mip, prog); // Step 3

    // Generate the infill grids
    CalculateDensityDividers();

    // The top and bottom segments need to calculated before
    // the infill outlines otherwise the infill will be seen as top or bottom
    // Calculate the top and bottom segments
    CalculateTopBottomSegments(mip, prog); // Step 4

    // Calculate the infill segments
    CalculateInfillSegments(mip, prog); // Step 5

    // Calculate the support segments
    CalculateSupportSegments(mip, prog);

#ifdef COMBINE_INFILL
    // Combine the infill segments
    CombineInfillSegments(mip, prog);
#endif

    // Generate a raft
    GenerateRaft(mip, prog);

    // Generate a skirt
    GenerateSkirt(mip, prog); // Step 6

    // Tim the infill grids to fit the segments
    TrimInfill(mip, prog); // Step 7

    // Calculate the toolpath
    CalculateToolpath(mip, prog); // Step 8

    return mip;
}
