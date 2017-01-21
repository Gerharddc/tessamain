#ifndef CHOPPERENGINE_H
#define CHOPPERENGINE_H

#include <string>
#include <memory>
#include <queue>

#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "meshinfo.h"
#include "mesh.h"

namespace ChopperEngine
{
    typedef void (*LogDelegate)(std::string message);

    extern MeshInfoPtr SliceMesh(Mesh *inputMesh);
    extern void WriteMeshGcode(std::string outputFile, MeshInfoPtr mip);

    extern void SlicerLog(std::string message);

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
        std::queue<std::string> stringBuf;

        void WriteLinesFunc();
        std::thread bufferThread;

    public:
        LineWriter(const MeshInfoPtr _mip);
        ~LineWriter();

        std::string ReadNextLine();
        bool HasLineToRead() const;
    };

    class Progressor {
    public:
        typedef void (*CallbackFunction)(float);
    private:
        int stepsCount_;
        int stepsDone;

        int stepParts_;
        int partsDone;

        CallbackFunction changeCallback_;

        float CalculateProgress();
    public:
        Progressor(int stepsCount, CallbackFunction changeCallback)
            : stepsCount_(stepsCount), changeCallback_(changeCallback)
        {
            stepsDone = -1;
        }

        void StartNextStep(std::size_t stepParts);
        void CompleteStepPart();
    };
}

#endif // CHOPPERENGINE_H
