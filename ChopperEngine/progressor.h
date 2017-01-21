#ifndef PROGRESSOR_H
#define PROGRESSOR_H

namespace ChopperEngine
{
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

        void StartNextStep(int stepParts);
        void CompleteStepPart();
    };
}

#endif // PROGRESSOR_H
