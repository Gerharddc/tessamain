#include "qtsettings.h"

// The global instance
QtSettings qtSettings;

// Because we can't directly convert a function pointer to a void*,
// we need to create a structure that encapsulates the pointer,
// we can then pass pointers to these structures
struct SignalWrapper {
    typedef void(QtSettings::*signal)();
    signal sig;

    SignalWrapper(signal sig_)
        : sig(sig_) {}

    void operator ()()
    {
        // This signal will be emitted from the global/singleton object
        emit (qtSettings.*sig)();
    }
};

// We use a generic function to envoke the signal contained in the struct
template <typename T>
static void HandleWithSignal(void *context, T)
{
    ((SignalWrapper*)(context))->operator ()();
}

// We need to create a wrapper for each signal
#define AUTO_WRAPPER(NAME) \
static SignalWrapper NAME##Signal(&QtSettings::NAME##Changed);

AUTO_WRAPPER(bedWidth)
AUTO_WRAPPER(bedHeight)
AUTO_WRAPPER(bedLength)
AUTO_WRAPPER(infillDensity)
AUTO_WRAPPER(layerHeight)
AUTO_WRAPPER(skirtLineCount)
AUTO_WRAPPER(skirtDistance)
AUTO_WRAPPER(printSpeed)
AUTO_WRAPPER(infillSpeed)
AUTO_WRAPPER(topBottomSpeed)
AUTO_WRAPPER(firstLineSpeed)
AUTO_WRAPPER(travelSpeed)
AUTO_WRAPPER(retractionSpeed)
AUTO_WRAPPER(retractionDistance)
AUTO_WRAPPER(shellThickness)
AUTO_WRAPPER(topBottomThickness)
AUTO_WRAPPER(printTemperature)
#undef AUTO_WRAPPER

QtSettings::QtSettings(QObject *parent) : QObject(parent)
{
    // Connect the qt signals to the the settings backend
#define AUTO_CONNECT(TYPE, NAME, NAMECAP) \
    GlobalSettings::NAMECAP.RegisterHandler(HandleWithSignal<TYPE>, &NAME##Signal);

    AUTO_CONNECT(float, bedWidth, BedWidth)
    AUTO_CONNECT(float, bedHeight, BedHeight)
    AUTO_CONNECT(float, bedLength, BedLength)
    AUTO_CONNECT(float, infillDensity, InfillDensity)
    AUTO_CONNECT(float, layerHeight, LayerHeight)
    AUTO_CONNECT(int, skirtLineCount, SkirtLineCount)
    AUTO_CONNECT(float, skirtDistance, SkirtDistance)
    AUTO_CONNECT(float, printSpeed, PrintSpeed)
    AUTO_CONNECT(float, infillSpeed, InfillSpeed)
    AUTO_CONNECT(float, topBottomSpeed, TopBottomSpeed)
    AUTO_CONNECT(float, firstLineSpeed, FirstLineSpeed)
    AUTO_CONNECT(float, travelSpeed, TravelSpeed)
    AUTO_CONNECT(float, travelSpeed, TravelSpeed)
    AUTO_CONNECT(float, retractionSpeed, RetractionSpeed)
    AUTO_CONNECT(float, retractionDistance, RetractionDistance)
    AUTO_CONNECT(float, shellThickness, ShellThickness)
    AUTO_CONNECT(float, topBottomThickness, TopBottomThickness)
    AUTO_CONNECT(int, printTemperature, PrintTemperature)
#undef AUTO_CONNECT
}
