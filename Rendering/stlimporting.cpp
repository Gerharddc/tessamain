#include "stlimporting.h"

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <ios>
#include <math.h>
#include <unordered_map>

#include "Misc/strings.h"

// NOTE: float has to be 32bit real number
// TODO: add check

// The plan here to to allocate as few times as possible and to reuse allocated memory space as much as possilbe
// it is therefor needed to make excessive use of pointers and references.
// This needs to be thought through very well because this code has a large workload and needs to be of as high performance
// as possible whilst using as little memory as possible.

// This function reads a float from a binary stream using a preallocated buffer
static float ReadFloat(std::ifstream &is, char* buf)
{
    is.read(buf, 4);
    return *((float*) buf);
}

// This function reads a vertex into a float array
static void ReadVec3(std::ifstream &is, char* buf, float *arr)
{
    arr[0] = ReadFloat(is, buf);
    arr[1] = ReadFloat(is, buf);
    arr[2] = ReadFloat(is, buf);
}

struct HashVertex
{
    // This functor creates a hash value from a vertex
    // NOTE: this has not been optimized for best spreading at all!!!
    std::size_t operator()(const float *arrPos) const
    {
        std::size_t a = *((uint32_t*)(arrPos));
        std::size_t b = *((uint32_t*)(arrPos + 1));
        std::size_t c = *((uint32_t*)(arrPos + 2));
        return a*3 + b*5 + c*7;
    }
};

struct CompVertex
{
    // This functor compares two vertices
    bool operator()(const float *arrPos1, const float *arrPos2) const
    {
        return (arrPos1[0] ==  arrPos2[0]) && (arrPos1[1] ==  arrPos2[1]) && (arrPos1[2] ==  arrPos2[2]);
    }
};

static inline Mesh* ImportBinary(std::ifstream &is, unsigned int trigCount)
{
    Mesh *mesh = new Mesh(trigCount);
    Vec3 MinVec, MaxVec;
    MinVec.ToMax();
    MaxVec.ToMin();

    std::cout << "Trigs: " << std::to_string(trigCount) << std::endl;

    // We allocate this buffer once and resuse it for all the floats that need to be read
    char buf[4];

    // Store a hashtable of the exisiting vertices to easily find indices of exisitng vertices
    std::unordered_map<float*, std::size_t, HashVertex, CompVertex> vecTable;

    std::size_t curIdx = 0; // The first open index in the vertex buffer
    std::size_t saveIdx = 0; // The current index that need to be saved

    for (unsigned int i = 0; i < trigCount; i++)
    {
        // Read past the normal
        is.seekg((std::size_t)is.tellg() + sizeof(float) * 3);

        // We then read the next 3 vertices to the current position in the vertex buffer. If they are all new then
        // they will be able to stay in these position. If they already exisit though then they will be overidden.
        // This should result in the least amount of memory operations.
        float* posV[3];
        posV[0] = &(mesh->vertexFloats[curIdx * 3]);
        posV[1] = &(mesh->vertexFloats[curIdx * 3 + 3]);
        posV[2] = &(mesh->vertexFloats[curIdx * 3 + 6]);

        // Read all 3 the vectors in
        ReadVec3(is, buf, posV[0]);
        ReadVec3(is, buf, posV[1]);
        ReadVec3(is, buf, posV[2]);

        uint8_t fillPos = 0;
        for (uint8_t j = 0; j < 3; j++)
        {
            if (vecTable.count(posV[j]) == 0)
            {
                // We need to move the floats from new vertices to the front of the float array. In ther words,
                // we need to replace the read values with any exisiting vertices with those of new one.
                if (j != fillPos)
                    memcpy(posV[fillPos], posV[j], sizeof(float) * 3);
                vecTable.emplace(posV[fillPos], curIdx);
                mesh->indices[saveIdx] = curIdx;

                mesh->vertices[curIdx].trigIdxs.push_back(i);
                mesh->trigs[i].vertIdxs[j] = curIdx;

                curIdx++;
                fillPos++;

                // Update the min and max values for the mesh with this new vertex
                float* minComps[] = { &MinVec.x, &MinVec.y, &MinVec.z };
                float* maxComps[] = { &MaxVec.x, &MaxVec.y, &MaxVec.z };
                for (uint8_t k = 0; k < 3; k++)
                {
                    if (posV[j][k] < *(minComps[k]))
                        *(minComps[k]) = posV[j][k] ;

                    if (posV[j][k] > *(maxComps[k]))
                        *(maxComps[k]) = posV[j][k] ;
                }
            }
            else
            {
                // For existing vertices we reuse the indices of the first occurences of said verties
                auto pos = vecTable[posV[j]];
                mesh->indices[saveIdx] = pos;

                mesh->vertices[pos].trigIdxs.push_back(i);
                mesh->trigs[i].vertIdxs[j] = pos;
            }
            saveIdx++;
        }

        // Skip past the empty two bytes at the end
        is.seekg((std::size_t)is.tellg() + 2);
    }

    is.close();

    mesh->MinVec = MinVec;
    mesh->MaxVec = MaxVec;
    mesh->ShrinkVertices(curIdx); // Shrink the vertex storage to what is needed

    return mesh;
}

static void ReadVec3(std::string &line, std::size_t &lastPos, float* arr)
{
    // This cuurently assumes the line is properly formatted (unsafe)
    for (uint8_t i = 0; i < 3; i++)
    {
        lastPos = line.find(' ', lastPos);
        std::size_t nextPos = lastPos;

        // We need to have support for multiple spaces next to each other
        bool needNext = true;
        while (needNext)
        {
            nextPos = line.find(' ', nextPos + 1);
            if (nextPos == std::string::npos)
            {
                nextPos = line.length() + 1;
                needNext = false;
            }
            else if (line[nextPos - 1] != ' ' && line[nextPos - 1] != ' ') // Space and tab
                needNext = false;
        }

        std::string sub = line.substr(lastPos + 1, nextPos - lastPos - 1);
        lastPos = nextPos;
        veccomp val = std::stof(sub);
        arr[i] = val;
    }
}

// This is a rough (hopefully safe) estimation of the amount of bytes that an ASCII facet
// defination can take up and is used to calculate an overly large buffer
static const uint minSize = 100;

static inline Mesh* ImportASCII(const char* path, std::size_t fileSize)
{
    // TODO: this thing does not cope well with invalid files

    // Allocate a large enough mesh and shrink it later
    Mesh* mesh = new Mesh(fileSize / minSize);
    std::ifstream is(path);

    // Check for a valid file
    if (is)
    {
        Vec3 MinVec, MaxVec;
        MinVec.ToMax();
        MaxVec.ToMin();

        std::string line;
        std::size_t i = 0;
        bool valid = true;

        // Store a hashtable of the exisiting vertices to easily find indices of exisitng vertices
        std::unordered_map<float*, int, HashVertex, CompVertex> vecTable;

        uint16_t curIdx = 0; // The first open index in the vertex buffer
        uint16_t saveIdx = 0; // The current index that need to be saved

        while (valid && std::getline(is, line))
        {
            if (line.find("facet") != std::string::npos)
            {
                std::size_t normalPos = line.find("normal");
                if (normalPos == std::string::npos)
                    valid = false;
                else
                {
                    // Ignore the nromal as it will be calculated manually for safety reasons

                    // Read "outer loop"
                    if (!std::getline(is, line))
                    {
                        valid = false;
                        break;
                    }

                    // We then read the next 3 vertices to the current position in the vertex buffer. If they are all new then
                    // they will be able to stay in these position. If they already exisit though then they will be overidden.
                    // This should result in the least amount of memory operations.
                    float* posV[3];
                    posV[0] = &(mesh->vertexFloats[curIdx * 3]);
                    posV[1] = &(mesh->vertexFloats[curIdx * 3 + 3]);
                    posV[2] = &(mesh->vertexFloats[curIdx * 3 + 6]);

                    for (uint8_t j = 0; j < 3; j++)
                    {
                        if (std::getline(is, line))
                        {
                            std::size_t vertexPos = line.find("vertex");
                            if (vertexPos == std::string::npos)
                            {
                                valid = false;
                                break;
                            }
                            else
                            {
                                ReadVec3(line, vertexPos, posV[j]);
                            }
                        }
                        else
                        {
                            valid = false;
                            break;
                        }
                    }

                    // Read "endloop"
                    if (!std::getline(is, line))
                    {
                        valid = false;
                        break;
                    }

                    // Read "endfacet"
                    if (!std::getline(is, line))
                    {
                        valid = false;
                        break;
                    }

                    // Determine which vertices are new and store them
                    uint8_t fillPos = 0;
                    for (uint8_t j = 0; j < 3; j++)
                    {
                        if (vecTable.count(posV[j]) == 0)
                        {
                            // We need to move the floats from new vertices to the front of the float array. In ther words,
                            // we need to replace the read values with any exisiting vertices with those of new one.
                            if (j != fillPos)
                                memcpy(posV[fillPos], posV[j], sizeof(float) * 3);
                            vecTable.emplace(posV[fillPos], curIdx);
                            mesh->indices[saveIdx] = curIdx;

                            mesh->vertices[curIdx].trigIdxs.push_back(i);
                            mesh->trigs[i].vertIdxs[j] = curIdx;

                            curIdx++;
                            fillPos++;

                            // Update the min and max values for the mesh with this new vertex
                            float* minComps[] = { &MinVec.x, &MinVec.y, &MinVec.z };
                            float* maxComps[] = { &MaxVec.x, &MaxVec.y, &MaxVec.z };
                            for (uint8_t k = 0; k < 3; k++)
                            {
                                if (posV[j][k] < *(minComps[k]))
                                    *(minComps[k]) = posV[j][k] ;

                                if (posV[j][k] > *(maxComps[k]))
                                    *(maxComps[k]) = posV[j][k] ;
                            }
                        }
                        else
                        {
                            // For existing vertices we reuse the indices of the first occurences of said verties
                            auto pos = vecTable[posV[j]];
                            mesh->indices[saveIdx] = pos;

                            mesh->vertices[pos].trigIdxs.push_back(i);
                            mesh->trigs[i].vertIdxs[j] = pos;

                        }
                        saveIdx++;
                    }
                }

                i++;
            }
        }

        if (valid)
        {
            mesh->MinVec = MinVec;
            mesh->MaxVec = MaxVec;
            mesh->ShrinkVertices(curIdx);
            mesh->ShrinkTrigs(i);
        }
        else
        {
            throw std::runtime_error("The ASCII file is invalid");
        }
    }
    else
        throw std::runtime_error(format_string("Could not open ASCII stl file with path: %s", path));

    is.close();

    return mesh;
}

Mesh* STLImporting::ImportSTL(const char *path)
{
    // TODO: better error handling
    std::ifstream is(path, std::ios::binary);
    if (!is.fail())
    {
        // get length of file:
        is.seekg (0, is.end);
        std::size_t length = is.tellg();

        // move past the header
        is.seekg(80);

        // read the triangle count
        char trigCountChars[4];
        is.read(trigCountChars, 4);
        uint trigCount = *((uint*) trigCountChars);

        Mesh *mesh;

        // check if the length is correct for binary
        if (length == (84 + (trigCount * 50)))
        {
            mesh = ImportBinary(is, trigCount);
        }
        else
        {
            is.close();
            mesh = ImportASCII(path, length);
        }

        // Shrink the triangle index vectors for each vertex as they will never have to grow again
        // and don't need to waste memory
        for (std::size_t i = 0; i < mesh->vertexCount; i++)
        {
            mesh->vertices[i].trigIdxs.shrink_to_fit();
        }

        return mesh;
    }
    else
        throw std::runtime_error(format_string("Could not open stl file with path: %s", path));
}
