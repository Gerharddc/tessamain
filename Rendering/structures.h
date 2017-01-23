#ifndef STRUCTURES
#define STRUCTURES

#include <vector>

#include "ChopperEngine/mesh.h"
#include "ChopperEngine/toolpath.h"
#include "ChopperEngine/meshinfo.h"

enum GType
{
    RapidMove = 0,
    Move = 1,
    Home = 28,
    ToInch = 20,
    ToMM = 21,
    SetAbs = 90,
    SetRel =  91,
    SetPos = 92
};

struct Point2
{
    float x = 0;
    float y = 0;

    Point2() {}
    Point2(float _x, float _y) : x(_x), y(_y) {}
    Point2(double _x, double _y) : x(_x), y(_y) {}

    bool operator== (Point2 &b)
    {
        return (x == b.x) && (y == b.y);
    }

    bool operator!= (Point2 &b)
    {
        return (x != b.x) && (y != b.y);
    }
};

struct RenderInfo
{
    uint16_t milliSecs = 0;
    bool isExtruded = false;

    uint32_t chunkIdx = 0;
    uint32_t idxInChunk = 0;
    uint32_t lineIdxInChunk = 0;
};

class RenderTP;

class TPDataChunk
{
    friend class RenderTP;

private:
    bool indicesCopied = false;
    bool lineIdxsCopied = false;
    ushort *indices = nullptr;
    ushort *lineIdxs = nullptr;

public:
    float *curFloats = nullptr;
    float *prevFloats = nullptr;
    float *nextFloats = nullptr;
    float *sides = nullptr;

    uint32_t curFloatCount = 0;
    uint32_t prevFloatCount = 0;
    uint32_t nextFloatCount = 0;
    uint32_t sideFloatCount = 0;
    uint16_t idxCount = 0;
    uint16_t lineIdxCount = 0;

    ushort *getIndices();
    ushort *getLineIdxs();
    void ShrinkToSize();

    TPDataChunk();
    TPDataChunk(TPDataChunk &&copier);
    ~TPDataChunk();
};

struct RenderTP
{
    ChopperEngine::MeshInfoPtr mip;
    std::vector<TPDataChunk> *CalculateDataChunks();

    // ETA gets calculated when data chunks are generated
    long totalMillis = -1;

    RenderTP(ChopperEngine::MeshInfoPtr _mip) {
        mip = _mip;
    }
};

#endif // STRUCTURES

