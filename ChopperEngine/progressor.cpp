#include "progressor.h"

using namespace ChopperEngine;

void Progressor::StartNextStep(int stepParts)
{
    stepsDone++;
    stepParts_ = stepParts;
    partsDone = 0;

    changeCallback_(CalculateProgress());
}

void Progressor::CompleteStepPart()
{
    partsDone++;

    changeCallback_(CalculateProgress());
}

float Progressor::CalculateProgress()
{
    if (stepsCount_ == 0)
        return 100.0f;
    else
    {
        float progress = ((float)stepsDone / (float)stepsCount_) * 100.0f;

        if (stepParts_ != 0)
            progress += (1 / (float)stepsCount_) * ((float)partsDone / (float)stepParts_) * 100.0f;

        return progress;
    }
}
