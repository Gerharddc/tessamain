#ifndef TOOLPATH_H
#define TOOLPATH_H

#include <vector>
#include <cmath>
#include <iostream>

#include "clipper.hpp"
#include "pmvector.h"

#include "Misc/globalsettings.h"

using namespace ClipperLib;

// Scale double to ints with this factor
static double scaleFactor = 100000.0;
const double PI = 3.14159265358979323846;
const float NozzleWidth = 0.5f;
const float FilamentWidth = 2.8f;

struct TrigLineSegment
{
    // This is a linesegment that is linked to a triangle face

    IntPoint p1, p2;
    bool usedInPolygon = false;
    std::size_t trigIdx;

    TrigLineSegment(const IntPoint &_p1, const IntPoint &_p2, std::size_t _trigIdx) :
        p1(_p1), p2(_p2), trigIdx(_trigIdx) {}

    void SwapPoints()
    {
        IntPoint temp = p1;
        p1 = p2;
        p2 = temp;
    }
};

struct LineSegment
{
    IntPoint p1, p2;

    LineSegment(const IntPoint &_p1, const IntPoint &_p2) :
        p1(_p1), p2(_p2) {}

    void SwapPoints()
    {
        IntPoint temp = p1;
        p1 = p2;
        p2 = temp;
    }
};

typedef std::vector<LineSegment> LineList;

enum class ToolSegType
{
    Retraction,
    Travel,
    Extruded
};

struct ToolSegment
{
    ToolSegType type;

    ToolSegment(ToolSegType _type)
        : type(_type) {}

    virtual ~ToolSegment() {}
};

struct RetractSegment : public ToolSegment
{
    cInt distance;

    RetractSegment(cInt dist)
        : ToolSegment(ToolSegType::Retraction), distance(dist) {}
};

struct IntPoint3
{
    cInt X, Y, Z;

    IntPoint3(cInt x, cInt y, cInt z) :
        X(x), Y(y), Z(z) {}

    IntPoint3(const IntPoint &ip, cInt z) :
        X(ip.X), Y(ip.Y), Z(z) {}

    bool operator ==(const IntPoint3 &b)
    {
        return (X == b.X) && (Y == b.Y) && (Z == b.Z);
    }
};

template <typename T>
static void FreeInfoOfType(void *info)
{
    delete (T)info;
}

class MovingSegment : public ToolSegment
{
private:
    void (*freeRenderInfo)(void*);

public:
    IntPoint3 p1, p2;
    int speed;

    MovingSegment(ToolSegType _type, const IntPoint3& _p1, const IntPoint3& _p2, const int _speed)
        : ToolSegment(_type), p1(_p1), p2(_p2), speed(_speed) {}

    MovingSegment(ToolSegType _type, const IntPoint& _p1, const IntPoint& _p2, cInt Z, const int _speed)
        : ToolSegment(_type), p1(_p1, Z), p2(_p2, Z), speed(_speed) {}

    cInt MoveDistance()
    {
        return (cInt)(std::sqrt(std::pow((long)p2.X - (long)p1.X, 2) +
                         std::pow((long)p2.Y - (long)p1.Y, 2) + std::pow((long)p2.Z - (long)p1.Z, 2)));
    }

    void SetStartPoint(IntPoint p)
    {
        p1.X = p.X;
        p1.Y = p.Y;
    }

    // TODO: renderInfo is not move safe
    void *renderInfo = nullptr;

    template <typename T>
    void setRenderInfo(T info)
    {
        renderInfo = info;
        freeRenderInfo = &FreeInfoOfType<T>;     
    }

    uint16_t calcMillis()
    {
        // Calculate the time for the info
        double x = (double)(p2.X - p1.X) / scaleFactor;
        double y = (double)(p2.Y - p1.Y) / scaleFactor;
        double z = (double)(p2.Z - p1.Z) / scaleFactor;
        double dist = std::sqrt(x*x + y*y + z*z);

        // Speed in mm/sec
        return std::round(dist * 1000 / speed);
    }

    ~MovingSegment() {
        if (renderInfo != nullptr)
            freeRenderInfo(renderInfo);
    }
};

struct TravelSegment : public MovingSegment
{
    TravelSegment(const IntPoint3& _p1, const IntPoint3& _p2, const int _speed)
        : MovingSegment(ToolSegType::Travel, _p1, _p2, _speed) {}

    TravelSegment(const IntPoint& _p1, const IntPoint& _p2, cInt Z, const int _speed)
        : MovingSegment(ToolSegType::Travel, _p1, _p2, Z, _speed) {}

    TravelSegment(const IntPoint& P, cInt Z1, cInt Z2, const int _speed)
        : MovingSegment(ToolSegType::Travel, IntPoint3(P, Z1), IntPoint3(P, Z2), _speed) {}
};

struct ExtrudeSegment : public MovingSegment
{
    ExtrudeSegment(const IntPoint3& _p1, const IntPoint3& _p2, const int _speed)
        : MovingSegment(ToolSegType::Extruded, _p1, _p2, _speed) {}

    ExtrudeSegment(const IntPoint& _p1, const IntPoint& _p2, cInt Z, const int _speed)
        : MovingSegment(ToolSegType::Extruded, _p1, _p2, Z, _speed) {}

    ExtrudeSegment(const LineSegment& line, cInt Z, const int _speed)
        : MovingSegment(ToolSegType::Extruded, IntPoint3(line.p1, Z), IntPoint3(line.p2, Z), _speed) {}

    double ExtrusionDistance()
    {
        if (GlobalSettings::LayerHeight.Get() == 0)
            return 0;

        // First we need to calculate the volume of the segment
        double volume = (MoveDistance() / scaleFactor) * GlobalSettings::LayerHeight.Get() / NozzleWidth;

        // We then need to calculate how much smaller the extrusion is from the filament so that
        // we know how much filament to use to get the desired amount of extrusion
        double filamentToTip =  FilamentWidth / NozzleWidth;

        // We can then return the amount of filament needed for the extrusion of the move
        return volume / filamentToTip / 5; //Not sure why the 5 is needed
        // TODO: the above is not 100% correct
    }
};

enum class SegmentType
{
    OutlineSegment,
    InfillSegment,
    TopSegment,
    BottomSegment,
    SupportSegment,
    SkirtSegment,
    RaftSegment
};

struct LayerSegment
{
    Paths outlinePaths;
    SegmentType type;
    int segmentSpeed;
    PMCollection<ToolSegment> toolSegments;

    LayerSegment(SegmentType _type) :
        type(_type) {}

    virtual ~LayerSegment() {}
};

struct SegmentWithInfill : public LayerSegment
{
    float infillMultiplier = 1.0f;
    float fillDensity = 1.0;
    std::vector<LineSegment> fillLines;

    SegmentWithInfill(SegmentType _type) :
        LayerSegment(_type) {}
};

struct LayerIsland
{
    Paths outlinePaths;
    PMCollection<LayerSegment> segments;
};

struct LayerComponent
{
    std::vector<TrigLineSegment> initialLineList;
    std::map<std::size_t, std::size_t> faceToLineIdxs;
    std::vector<LayerIsland> islandList;
    int layerSpeed = GlobalSettings::PrintSpeed.Get(); // TODO
    int moveSpeed = GlobalSettings::TravelSpeed.Get(); // TODO
    std::vector<TravelSegment> initialLayerMoves;

    bool hasSkirt = false;
    PMCollection<ToolSegment> skirtSegments;
};

#endif // TOOLPATH_H
