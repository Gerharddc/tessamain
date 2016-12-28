#ifndef GRIDGENERATION_H
#define GRIDGENERATION_H

#include <cstdlib>

namespace GridGeneration {

    struct Grid
    {
        float* floats = nullptr;
        std::size_t floatCount = 0;

        Grid(std::size_t cnt)
        {
            floats = new float[cnt];
            floatCount = cnt;
        }

        ~Grid()
        {
            delete[] floats;
        }
    };

    inline Grid* GenerateGrids(unsigned int xSize, unsigned int ySize, unsigned int zSize, unsigned int interval)
    {   
        // When calculate the amount of blocks and then add the final line
        unsigned int xCount = xSize / interval;
        unsigned int yCount = ySize / interval;
        unsigned int zCount = zSize / interval;
        float xInt = xSize / xCount;
        float yInt = ySize / yCount;
        float zInt = zSize / zCount;
        xCount++;
        yCount++;
        zCount++;

        // Each line has 2 points and each point has 3 floats (x,y,z)
        Grid *grid = new Grid((xCount + yCount + zCount) * 2 * 6);

        unsigned int idx = 0;

        // Calculate the vertical bottom lines
        for (unsigned int i = 0; i < xCount; i++)
        {
            float x = xInt * i;

            grid->floats[idx    ] = x; // x1
            grid->floats[idx + 1] = 0; // y1
            grid->floats[idx + 2] = 0; // z1

            grid->floats[idx + 3] = x;     // x2
            grid->floats[idx + 4] = ySize; // y2
            grid->floats[idx + 5] = 0;     // z2

            idx += 6;
        }

        // Calculate the horizontal bottom lines
        for (unsigned int i = 0; i < yCount; i++)
        {
            float y = yInt * i;

            grid->floats[idx    ] = 0; // x1
            grid->floats[idx + 1] = y; // y1
            grid->floats[idx + 2] = 0; // z1

            grid->floats[idx + 3] = xSize; // x2
            grid->floats[idx + 4] = y;     // y2
            grid->floats[idx + 5] = 0;     // z2

            idx += 6;
        }

        // Calculate the vertical side lines
        for (unsigned int i = 0; i < xCount; i++)
        {
            float x = xInt * i;

            grid->floats[idx    ] = x; // x1
            grid->floats[idx + 1] = ySize; // y1
            grid->floats[idx + 2] = 0; // z1

            grid->floats[idx + 3] = x;     // x2
            grid->floats[idx + 4] = ySize; // y2
            grid->floats[idx + 5] = zSize;     // z2

            idx += 6;
        }

        // Calculate the horizontal side lines
        for (unsigned int i = 0; i < zCount; i++)
        {
            float z = zInt * i;

            grid->floats[idx    ] = 0; // x1
            grid->floats[idx + 1] = ySize; // y1
            grid->floats[idx + 2] = z; // z1

            grid->floats[idx + 3] = xSize;     // x2
            grid->floats[idx + 4] = ySize; // y2
            grid->floats[idx + 5] = z;     // z2

            idx += 6;
        }

        // Calculate the vertical back lines
        for (unsigned int i = 0; i < yCount; i++)
        {
            float y = yInt * i;

            grid->floats[idx    ] = xSize; // x1
            grid->floats[idx + 1] = y; // y1
            grid->floats[idx + 2] = 0; // z1

            grid->floats[idx + 3] = xSize;     // x2
            grid->floats[idx + 4] = y; // y2
            grid->floats[idx + 5] = zSize;     // z2

            idx += 6;
        }

        // Calculate the horizontal back lines
        for (unsigned int i = 0; i < zCount; i++)
        {
            float z = zInt * i;

            grid->floats[idx    ] = xSize; // x1
            grid->floats[idx + 1] = 0; // y1
            grid->floats[idx + 2] = z; // z1

            grid->floats[idx + 3] = xSize;     // x2
            grid->floats[idx + 4] = ySize; // y2
            grid->floats[idx + 5] = z;     // z2

            idx += 6;
        }

        return grid;
    }

}

#endif // GRIDGENERATION_H
