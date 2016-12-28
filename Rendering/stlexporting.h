#ifndef STLEXPORTING_H
#define STLEXPORTING_H

#include <string>
#include <set>
#include "structures.h"

namespace STLExporting {
    bool ExportSTL(std::string path, const std::set<Mesh*> meshes, std::string &error);
}

#endif // STLEXPORTING_H
