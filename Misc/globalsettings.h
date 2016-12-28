#ifndef GLOBALSETTINGS_H
#define GLOBALSETTINGS_H

#include <set>
#include <string>

// This is a helper class used to define settings with finite types.
// This is important because the sotrage mechanism is inherintly not typesafe.
template<typename T> class GlobalSetting
{
public:
    GlobalSetting(const std::string name, T defVal);
    void Set(T value);
    T Get();

    // The changed handler is a pointer to a C function that handles a change to the settings,
    // this function gets the new value and pointer to the "context" associated with it.
    // This could be a pointer to a class that the function needs. It should be passed along
    // when the handler is registered or unregistered
    typedef void(*ChangedHandler)(void*, T);
    void RegisterHandler(ChangedHandler handler, void *context);
    void UnregisterHandler(ChangedHandler handler, void *context);

private:
    std::string Name;

    struct StoredHandler
    {
        ChangedHandler m_handler;
        void *m_context;

        StoredHandler(ChangedHandler handler, void *context)
            : m_handler(handler), m_context(context) {}

        bool operator ==(const StoredHandler &o) const {
            return ((m_handler == o.m_handler) && (m_context == o.m_context));
        }

        bool operator <(const StoredHandler &o) const {
            return ((m_handler < o.m_handler) && (m_context < o.m_context));
        }
    };

    std::set<StoredHandler> storedHandlers;
};

// As static class is used instead of a namespace because the compiler does not
// like to combine the nampesace option with this funny template class that is
// implemented in the cpp file
class GlobalSettings
{
public:
    static void LoadSettings();
    static void SaveSettings();
    static void StopSavingLoop();

    // The actual settings
    static GlobalSetting<float> BedWidth;
    static GlobalSetting<float> BedLength;
    static GlobalSetting<float> BedHeight;
    static GlobalSetting<float> InfillDensity;
    static GlobalSetting<float> LayerHeight;
    static GlobalSetting<int> SkirtLineCount;
    static GlobalSetting<float> SkirtDistance;
    static GlobalSetting<float> PrintSpeed;
    static GlobalSetting<float> InfillSpeed;
    static GlobalSetting<float> TopBottomSpeed;
    static GlobalSetting<float> FirstLineSpeed;
    static GlobalSetting<float> TravelSpeed;
    static GlobalSetting<float> RetractionSpeed;
    static GlobalSetting<float> RetractionDistance;
    static GlobalSetting<float> ShellThickness;
    static GlobalSetting<float> TopBottomThickness;
    static GlobalSetting<int> PrintTemperature;
    static GlobalSetting<int> InfillCombinationCount;
};

#endif // GLOBALSETTINGS_H
