#include "structures.h"

#include <QDebug>
#include <iostream>

Mesh::Mesh(std::size_t size)
{
    trigCount = size;

    // Malloc is used to simply the shrinking of the arrays later on if needed
    vertexFloats = (float*)malloc(sizeof(float) * size * 9); // This will need to be shrunk
    indices = (std::size_t*)malloc(sizeof(std::size_t) * size * 3);
    trigs = (Triangle*)malloc(sizeof(Triangle) * size);

    // The arrays in triangles need to be initialized
    for (std::size_t i = 0; i < size; i++)
        new (trigs + i) Triangle();

    // The std::vectors in the vertices need to be initialized, we also need to keep track of how many there are as to be able to
    // properly destruct them later on
    vertices = (Vertex*)malloc(sizeof(Vertex) * size * 3);  // This will need to be shrunk
    for (std::size_t i = 0; i < size * 3; i++)
        new (vertices + i) Vertex();
    vertArrCount = size * 3;
}

Mesh::~Mesh()
{
    free(vertexFloats);
    free(indices);
    free(trigs);
    delete[] vertFloats;
    delete[] normFloats;

    // Because they contains std::vectors, the vertices have to be destructed first
    for (std::size_t i = 0; i < vertArrCount; i++)
        vertices[i].~Vertex();
    free(vertices);
}

void Mesh::ShrinkVertices(std::size_t vertCount)
{
    // Recreate the arrays that are linked insize to the amount of vertices
    vertexCount = vertCount;

    // We can simply reallocate the memory for the vertex floats
    vertexFloats = (float*)realloc(vertexFloats, sizeof(float) * vertexCount * 3);

    // We need to properly dispose of all the vertices (std::vectors) that will be lost
    // and then reallocate the smaller amount of memory
    // WARNING: this has never failed but the complex nature of std::vector does provide a chance of it
    // perhaps causing strange behaviour when copied directly, please keep this in mind when debugging...
    for (std::size_t i = vertexCount; i < vertArrCount; i++)
    {
        vertices[i].~Vertex();
    }
    vertices = (Vertex*)realloc(vertices, sizeof(Vertex) * vertexCount);
    vertArrCount = vertexCount;
}

void Mesh::ShrinkTrigs(std::size_t newSize)
{
    // Reallocate the arrays with the new size
    trigCount = newSize;
    indices = (std::size_t*)realloc(indices, sizeof(std::size_t) * newSize * 3);
    trigs = (Triangle*)realloc(trigs, sizeof(Triangle) * newSize);
}

const float *Mesh::getFlatVerts()
{
    if (vertexFloats != nullptr)
        delete[] vertFloats;

    vertFloats = new float[trigCount * 9];
    for (std::size_t i = 0; i < trigCount; i++)
    {
        for (uint8_t j = 0; j < 3; j++)
        {
            auto idx = trigs[i].vertIdxs[j];

            for (uint8_t k = 0; k < 3; k++)
            {
                vertFloats[(i * 9) + (j * 3) + k] = vertexFloats[(idx * 3) + k];
            }
        }
    }
    return vertFloats;
}

const float *Mesh::getFlatNorms()
{
    if (normFloats != nullptr)
        delete[] vertexFloats;

    normFloats = new float[trigCount * 9];
    for (std::size_t i = 0; i <trigCount; i++)
    {
        glm::vec3 vecs[3];

        for (uint8_t j = 0; j < 3; j++)
        {
            auto idx = (indices[i * 3 + j]) * 3;
            vecs[j].x = vertexFloats[idx];
            vecs[j].y = vertexFloats[idx + 1];
            vecs[j].z = vertexFloats[idx + 2];
        }

        glm::vec3 e1 = vecs[1] - vecs[0];
        glm::vec3 e2 = vecs[2] - vecs[0];
        glm::vec3 norm = glm::normalize(glm::cross(e1, e2));

        for (uint8_t j = 0; j < 3; j++)
        {
            normFloats[(i * 9) + (j * 3)] = norm.x;
            normFloats[(i * 9) + (j * 3) + 1] = norm.y;
            normFloats[(i * 9) + (j * 3) + 2] = norm.z;
        }
    }
    return normFloats;
}

static inline void AddPointsToArray(float *array, Point2 &p, short count, uint32_t &arrPos)
{
    for (short i = 0; i < count; i++)
    {
        array[arrPos + 0] = p.x;
        array[arrPos + 1] = p.y;
        arrPos += 2;
    }
}

static inline void AddPointZsToArray(float *array, Point2 &p, float z, short count, uint32_t &arrPos)
{
    for (short i = 0; i < count; i++)
    {
        array[arrPos + 0] = p.x;
        array[arrPos + 1] = p.y;
        array[arrPos + 2] = z;
        arrPos += 3;
    }
}

// An GLES chunk can have a maximum of 2^16(ushort) indices and we need to divide all the data between that
// we will do this by creating a maximum size buffer for each set of data here and then shrinking it to only the
// needed part
static const uint16_t maxIdx = 10000;//UINT16_MAX; // TODO: variable data type

static inline void NewChunk(ushort &idxPos, ushort &saveIdx, ushort &lineIdx, std::vector<TPDataChunk> *chunks,
                            TPDataChunk *&dc, ushort &lastEndIdx, ushort &lastLineIdx)
{
    // We need to shrink the previous chunk to size
    if (dc != nullptr)
    {
        dc->idxCount = idxPos;
        dc->lineIdxCount = lineIdx;
        dc->sideFloatCount = saveIdx;
        dc->ShrinkToSize();
    }

    idxPos = 0;
    saveIdx = 0;
    lineIdx = 0;
    lastEndIdx = 0;
    lastLineIdx = 0;
    chunks->emplace_back();
    dc = &(chunks->back());
}

std::vector<TPDataChunk>* Toolpath::CalculateDataChunks()
{
    ushort idxPos = 0;
    ushort saveIdx = 0;
    ushort lineIdx = 0;
    std::vector<TPDataChunk> *chunks = new std::vector<TPDataChunk>();
    TPDataChunk *dc = nullptr;

    // Data for partial rendering
    ushort lastEndIdx = 0;
    ushort lastLineIdx = 0;

    // Use a MACRO to easily push a new chunk
    #define NEWCHUNK() NewChunk(idxPos, saveIdx, lineIdx, chunks, dc, lastEndIdx, lastLineIdx)
    NEWCHUNK();

    for (Layer layer : layers)
    {
        for (Island isle : layer.islands)
        {
            // We need to determine the maximum amount of points on the island in order to know if we
            // will have to cut it up into pieces
            bool cut = true;
            auto pCount = isle.printPoints.size();

            // Determine how many points we can still fit in this chunk
            uint fitCount = (maxIdx - saveIdx - 2) / 5;
            uint leftCount = 0;

#define MAXFITCOUNT() fitCount = (maxIdx - 2) / 5

            // We need to fit at least 3 points
            if (fitCount < 3)
            {
                NEWCHUNK();
                MAXFITCOUNT();
            }

            if (pCount < fitCount)
            {
                cut = false;
                //fitCount = pCount;
            }
            else
            {
                cut = true;
                leftCount = pCount - fitCount;
            }

            uint fitPos = 0;
            for (uint j = 0; j < pCount; j++)
            {
                // Determine the connecting points
                bool isLast = (j == pCount - 1);
                bool isFirst = (j == 0);

                Point2 curPoint = isle.printPoints[j];
                Point2 prevPoint, nextPoint;
                bool hasNoPrev = false;
                bool hasNoNext = false;

                // Last or first points of open ended shapes need to be treated differently
                // those of closed-ended shapes
                // It is not possible to place the last point alone on the next chunk so we need to ensure it is never needed
                if (isLast)
                {
                    prevPoint = isle.printPoints[j - 1];
                    nextPoint = isle.printPoints[0];

                    if (curPoint == nextPoint)
                    {
                        // Add the first two parts of the first point for the last point to connect to
                        nextPoint = isle.printPoints[1];
                        AddPointZsToArray(dc->curFloats, curPoint, layer.z, 2, dc->curFloatCount);
                        AddPointsToArray(dc->prevFloats, prevPoint, 2, dc->prevFloatCount);
                        AddPointsToArray(dc->nextFloats, nextPoint, 2, dc->nextFloatCount);
                        dc->sides[saveIdx + 0] = 10.0f;
                        dc->sides[saveIdx + 1] = -10.0f;
                        saveIdx += 2;

                        continue;
                    }
                    else
                        hasNoNext = true;
                }
                else
                {
                    nextPoint = isle.printPoints[j + 1];

                    if (isFirst)
                    {
                        prevPoint = isle.printPoints[pCount - 1];

                        if (prevPoint == curPoint)
                            prevPoint = isle.printPoints[pCount - 2]; // TODO: error maybe?
                        else
                            hasNoPrev = true;
                    }
                    else
                        prevPoint = isle.printPoints[j - 1];

                    // Check if we need to move to a new chunk
                    if (cut)
                    {
                        if (fitPos > (fitCount - 2))
                        {
                            // If this point needs to go to a new chunk then we need to add its first 2
                            // vertices for the last one in this chunk to connect to
                            AddPointZsToArray(dc->curFloats, curPoint, layer.z, 2, dc->curFloatCount);
                            AddPointsToArray(dc->prevFloats, prevPoint, 2, dc->prevFloatCount);
                            AddPointsToArray(dc->nextFloats, nextPoint, 2, dc->nextFloatCount);
                            dc->sides[saveIdx + 0] = 10.0f;
                            dc->sides[saveIdx + 1] = -10.0f;
                            saveIdx += 2;

                            // We then need to move to a new chunk
                            NEWCHUNK();

                            // We also need to determine if it will have to be cut again
                            MAXFITCOUNT();
                            if (leftCount < fitCount)
                                cut = false;
                            else
                            {
                                leftCount -= fitCount;
                                fitPos = 0;
                            }
                        }
                        else
                            fitPos++;
                    }
                }

                // We connect the current points with the following ones
                // The 4 current points form the corner and then we connect
                // to the next point to come
                // Only one of the corner triangles will be visible if any because
                // two points of the other will be the same when the rectangle
                // intersections are calculated

                if (hasNoPrev)
                {
                    // Add only 2 points
                    AddPointZsToArray(dc->curFloats, curPoint, layer.z, 2, dc->curFloatCount);
                    AddPointsToArray(dc->prevFloats, prevPoint, 2, dc->prevFloatCount);
                    AddPointsToArray(dc->nextFloats, nextPoint, 2, dc->nextFloatCount);

                    // Point attributes
                    // Forwards only
                    dc->sides[saveIdx + 0] = 40.0f;
                    dc->sides[saveIdx + 1] = -40.0f;

                    // Rectangle only
                    dc->indices[idxPos + 0] = saveIdx + 0;
                    dc->indices[idxPos + 1] = saveIdx + 2;
                    dc->indices[idxPos + 2] = saveIdx + 1;
                    dc->indices[idxPos + 3] = saveIdx + 2;
                    dc->indices[idxPos + 4] = saveIdx + 3;
                    dc->indices[idxPos + 5] = saveIdx + 1;                    

                    // Line
                    dc->lineIdxs[lineIdx] = saveIdx;
                    dc->lineIdxs[lineIdx + 1] = saveIdx + 3;
                    lineIdx += 2;

                    // Mark the rendering start & end point
                    // With the position/count of the index
                    lastEndIdx = idxPos + 6;
                    lastLineIdx = lineIdx + 2;

                    idxPos += 6;
                    saveIdx += 2;
                }
                else if (hasNoNext)
                {
                    // Add only 2 points
                    AddPointZsToArray(dc->curFloats, curPoint, layer.z, 2, dc->curFloatCount);
                    AddPointsToArray(dc->prevFloats, prevPoint, 2, dc->prevFloatCount);
                    AddPointsToArray(dc->nextFloats, nextPoint, 2, dc->nextFloatCount);

                    // Point attributes
                    // Backwards only
                    dc->sides[saveIdx + 0] = 50.0f;
                    dc->sides[saveIdx + 1] = -50.0f;

                    // Nothing for the indices

                    saveIdx += 2;
                }
                else
                {
                    // Add all the position components
                    AddPointZsToArray(dc->curFloats, curPoint, layer.z, 5, dc->curFloatCount);
                    AddPointsToArray(dc->prevFloats, prevPoint, 5, dc->prevFloatCount);
                    AddPointsToArray(dc->nextFloats, nextPoint, 5, dc->nextFloatCount);

                    // Point attributes
                    // Backwards, centre, forwards
                    dc->sides[saveIdx + 0] = 10.0f;
                    dc->sides[saveIdx + 1] = -10.0f;
                    dc->sides[saveIdx + 2] = 20.0f;
                    dc->sides[saveIdx + 3] = 30.0f;
                    dc->sides[saveIdx + 4] = -30.0f;

                    // Connector trigs
                    dc->indices[idxPos + 0] = saveIdx + 0;
                    dc->indices[idxPos + 1] = saveIdx + 3;
                    dc->indices[idxPos + 2] = saveIdx + 2;
                    dc->indices[idxPos + 3] = saveIdx + 1;
                    dc->indices[idxPos + 4] = saveIdx + 2;
                    dc->indices[idxPos + 5] = saveIdx + 4;

                    // Rectangle
                    dc->indices[idxPos + 6] = saveIdx + 3;
                    dc->indices[idxPos + 7] = saveIdx + 5;
                    dc->indices[idxPos + 8] = saveIdx + 4;
                    dc->indices[idxPos + 9] = saveIdx + 4;
                    dc->indices[idxPos + 10] = saveIdx + 5;
                    dc->indices[idxPos + 11] = saveIdx + 6;

                    // Line
                    dc->lineIdxs[lineIdx] = saveIdx;
                    dc->lineIdxs[lineIdx + 1] = saveIdx + 6;
                    lineIdx += 2;

                    // Mark the rendering start & end point
                    // With the position/count of the index
                    lastEndIdx = idxPos + 12;
                    lastLineIdx = lineIdx + 2;

                    idxPos += 12;
                    saveIdx += 5;
                }

                // Add the info and move to the next one
                // -1 means the points signifies the start of a move
                // and isn't relevant
                if (curPoint.lineNum != -1)
                {
                    LineInfo &isleLineInfo = lineInfos[curPoint.lineNum];
                    isleLineInfo.idxInChunk = lastEndIdx;
                    isleLineInfo.lineIdxInChunk = lastLineIdx;
                    isleLineInfo.chunkIdx = chunks->size() - 1;
                }
            }
        }
    }

    // Finish up the last chunk
    dc->idxCount = idxPos;
    dc->lineIdxCount = lineIdx;
    dc->sideFloatCount = saveIdx;
    dc->ShrinkToSize();

    // Fill in all the blank lineinfos
    int chunkIdx = 0;
    int idxInChunk = 0;
    int lineIdxInChunk = 0;
    for (LineInfo &li : lineInfos)
    {
        // Reset the inChunkIdxs after each new block
        if (li.chunkIdx > chunkIdx)
        {
            idxInChunk = 0;
            lineIdxInChunk = 0;
        }

        li.chunkIdx = chunkIdx = std::max(chunkIdx, li.chunkIdx);
        li.idxInChunk = idxInChunk = std::max(idxInChunk, li.idxInChunk);
        li.lineIdxInChunk = lineIdxInChunk = std::max(lineIdxInChunk, li.lineIdxInChunk);
    }

    chunks->shrink_to_fit();
    return chunks;
}

ushort *TPDataChunk::getIndices()
{
    indicesCopied = true;
    return indices;
}

ushort *TPDataChunk::getLineIdxs()
{
    lineIdxsCopied = true;
    return lineIdxs;
}

void TPDataChunk::ShrinkToSize()
{
    curFloats   = (float*) realloc (curFloats, curFloatCount * sizeof(float));
    nextFloats  = (float*) realloc (nextFloats, nextFloatCount * sizeof(float));
    prevFloats  = (float*) realloc (prevFloats, prevFloatCount * sizeof(float));
    sides       = (float*) realloc (sides, sideFloatCount * sizeof(float));
    indices     = (ushort*)realloc (indices, idxCount * sizeof(uint16_t));
    lineIdxs    = (ushort*)realloc (lineIdxs, lineIdxCount * sizeof(uint16_t));
}

TPDataChunk::TPDataChunk()
{
    curFloats  = (float*) malloc (maxIdx * 15 * sizeof(float));
    nextFloats = (float*) malloc (maxIdx * 10 * sizeof(float));
    prevFloats = (float*) malloc (maxIdx * 10 * sizeof(float));
    sides      = (float*) malloc (maxIdx * 5 * sizeof(float));
    indices    = (ushort*)malloc (maxIdx * 12 * sizeof(ushort));
    lineIdxs   = (ushort*)malloc (maxIdx * sizeof(ushort));
}

TPDataChunk::TPDataChunk(TPDataChunk &&copier)
{
    curFloats = copier.curFloats;
    prevFloats = copier.prevFloats;
    nextFloats = copier.nextFloats;
    sides = copier.sides;
    indices = copier.indices;
    lineIdxs = copier.lineIdxs;

    curFloatCount = copier.curFloatCount;
    prevFloatCount = copier.prevFloatCount;
    nextFloatCount = copier.nextFloatCount;
    sideFloatCount = copier.sideFloatCount;
    idxCount = copier.idxCount;
    lineIdxCount = copier.lineIdxCount;
    indicesCopied = copier.indicesCopied;
    lineIdxsCopied = copier.lineIdxsCopied;

    copier.curFloats = nullptr;
    copier.prevFloats = nullptr;
    copier.nextFloats = nullptr;
    copier.sides = nullptr;
    copier.indices = nullptr;
    copier.lineIdxs = nullptr;
}

TPDataChunk::~TPDataChunk()
{
    if (curFloats != nullptr)
        free (curFloats);
    if (prevFloats != nullptr)
        free (prevFloats);
    if (nextFloats != nullptr)
        free (nextFloats);
    if (sides != nullptr)
        free (sides);

    // The indices won't be copied over to the gpu and will need to remain in existence
    // the renderer will be responsible for freeing the memory used by that array
    if (!indicesCopied && indices != nullptr)
        free (indices);
    if (!lineIdxsCopied && lineIdxs != nullptr)
        free (lineIdxs);
}
