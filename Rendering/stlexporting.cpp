#include "stlexporting.h"

#include <fstream>

bool STLExporting::ExportSTL(std::string path, const std::set<Mesh *> meshes, std::string &error)
{
    std::ofstream os(path, std::ofstream::out | std::ofstream::binary);
    // TODO: check for errors
    if (os.is_open())
    {
        os.seekp(0);

        // Write the header
        char header[80] { ' ' };
#define HEADERTEXT "Written by naai software..."
        const char *headerText = HEADERTEXT;
        memcpy(header, headerText, sizeof HEADERTEXT);
        os.write(header, 80);

        // Write the size
        uint32_t trigCount = 0;
        for (Mesh *mesh : meshes)
            trigCount += mesh->trigCount;
        os.write((char*)&trigCount, 4);

        // Write the triangles
        for (Mesh *mesh : meshes)
        {
            // Get the vertex and normal floats
            const char *verts = (char*)mesh->getFlatVerts();
            const char *norms = (char*)mesh->getFlatNorms();

            for (std::size_t i = 0; i < mesh->trigCount; i++)
            {
                // Write the normal first
                os.write(norms, 12);
                norms += 12;

                // Write the three vertices
                for (uint8_t j = 0; j < 3; j++)
                {
                    os.write(verts, 12);
                    verts += 12;
                }

                // Write the bs 16bit attrib thing
                const char attrib[2] = { 0 };
                os.write(attrib, 2);
            }

            // Dump the temporary arrays
            mesh->dumpFlatVerts();
            mesh->dumpFlatNorms();
        }

        os.close();
    }
    else
        error = "Could not open file";

    return false;
}
