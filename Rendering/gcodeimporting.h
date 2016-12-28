#ifndef GCODEIMPORTING_H
#define GCODEIMPORTING_H

#include "structures.h"

namespace GCodeImporting {

extern Toolpath* ImportGCode(const char* path);

}

#endif // GCODEIMPORTING_H
