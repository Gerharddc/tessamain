#ifndef PROGRESSOR_H
#define PROGRESSOR_H

namespace ChopperEngine
{
    class Progressor {
    public:
        typedef void (*ProgressCallback)(float, const void*);
    private:
        int stepsCount_;
        int stepsDone;

        int stepParts_;
        int partsDone;

        const ProgressCallback changeCallback_;
        const void * const callbackContext_;

        float CalculateProgress();
    public:
        Progressor(int stepsCount, ProgressCallback changeCallback, const void* callbackContext)
            : stepsCount_(stepsCount), changeCallback_(changeCallback),
              callbackContext_(callbackContext)
        {
            stepsDone = -1;
        }

        void StartNextStep(int stepParts);
        void CompleteStepPart();
    };
}

#endif // PROGRESSOR_H
