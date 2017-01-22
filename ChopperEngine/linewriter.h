#ifndef LINEWRITER_H
#define LINEWRITER_H

#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <thread>

#include "meshinfo.h"
#include "Printer/gcode.h"

namespace ChopperEngine
{
    // The linewriter is an object that writes a toolpath to text lines in
    // a buffer. The buffer has a certain size and it tries to only keep it
    // full instead of writing everything at once.

    class LineWriter {
    private:
        std::mutex mtx, bufMtx;
        std::condition_variable cv;
        const MeshInfoPtr mip;
        std::atomic_bool done { false };

        const unsigned int TargetBufSize = 100;
        std::queue<GCode> gcodeBuf;

        void WriteLinesFunc();
        std::thread bufferThread;

    public:
        LineWriter(const MeshInfoPtr _mip);
        ~LineWriter();

        std::string ReadNextLine();
        bool HasLineToRead() const;
    };
}

#endif // LINEWRITER_H
