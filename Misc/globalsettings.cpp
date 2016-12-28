#include "globalsettings.h"

#include <cstddef>
#include <cstring>
#include <memory>
#include <fstream>
#include <deque>
#include <algorithm>
#include <exception>
#include <iostream>
#include <map>
#include <thread>

struct SettingValue
{
    // We use a shared ptr because std::map likes to copy this thing like crazy
    // and it's not worth the effort of doing anything more advanced to combat that here
    std::shared_ptr<char> bytes;
    uint32_t byteCnt = 0;

    SettingValue() { }

    SettingValue(const void *_bytes, std::size_t _byteCnt)
    {
        bytes = std::shared_ptr<char>(new char(_byteCnt));
        memcpy(bytes.get(), _bytes, _byteCnt);
        byteCnt = _byteCnt;
    }

    SettingValue& operator =(const SettingValue& o)
    {
        bytes = o.bytes;
        byteCnt = o.byteCnt;
        return *this;
    }
};

// By keeping track of the position and size of settings loaded from the file
// we can avoid rewriting the file if the changed value is the same size as
// the old one
struct LoadedSetting {
    std::size_t posInFile;
    uint32_t valueSize;

    LoadedSetting(std::size_t PosInFile, uint32_t ValueSize)
    {
        posInFile = PosInFile;
        valueSize = ValueSize;
    }

    LoadedSetting() {}
};

struct QueuedSetting {
    std::string name;
    bool inFile = false;

    QueuedSetting(std::string Name)
    {
        name = Name;
    }

    bool operator ==(const QueuedSetting &o)
    {
        return (o.name == name);
    }
};

static std::deque<QueuedSetting> queuedSettings;
// TODO: maybe a hashed map will deliver better performance
static std::map<std::string, SettingValue> settingsMap;
static std::map<std::string, LoadedSetting> lSettingsMap;

static const std::string SettingsFilePath = "/home/Simon/.Settings/settings.bin";
static std::size_t LoadedFileLength = 0;
static bool readFile = false;

static void WriteSetting(std::ofstream &os, std::string name)
{
    // Write the name length
    uint16_t nl = name.length();
    os.write((char*)(&nl), 2);

    // Write the name
    os.write(name.c_str(), nl);

    // Write the value length
    uint32_t vl = settingsMap[name].byteCnt;
    os.write((char*)(&vl), 4);

    // Write the value
    os.write(settingsMap[name].bytes.get(), vl);

    // Store the metadata if needed & move the counter
    LoadedFileLength += 6 + nl;
    lSettingsMap[name] = LoadedSetting(LoadedFileLength, vl);
    LoadedFileLength += vl;
}

void GlobalSettings::SaveSettings()
{
    // Flag for the rewriting of the whole file
    bool rewrite;

    if (readFile)
    {
        rewrite = false;

        // Determine if the exisitng file is compatible with the new settings
        for (auto &qs : queuedSettings)
        {
            std::string name = qs.name;

            if (lSettingsMap.count(name) == 1)
            {
                if (lSettingsMap[name].valueSize != settingsMap[name].byteCnt)
                {
                    rewrite = true;
                    break;
                }
                else
                    qs.inFile = true;
            }
        }
    }
    else
        rewrite = true;

    if (rewrite)
    {
        LoadedFileLength = 0;

        std::ofstream os(SettingsFilePath, std::ios::binary | std::ios::out);
        if (!os.fail())
        {
            while (!queuedSettings.empty()) {
                auto qs = queuedSettings.front();
                WriteSetting(os, qs.name);
                queuedSettings.pop_front();
            }

            os.close();
        }
        else
            std::cout << "Could not rewrite settings." << std::endl;
    }
    else
    {
        std::ofstream os(SettingsFilePath, std::ios::binary | std::ios::in | std::ios::out);
        if (!os.fail())
        {
            // Write all the settings
            while (!queuedSettings.empty()) {
                auto qs = queuedSettings.front();

                if (qs.inFile)
                {
                    // Modify the value
                    os.seekp(lSettingsMap[qs.name].posInFile);
                    os.write(settingsMap[qs.name].bytes.get(), settingsMap[qs.name].byteCnt);
                }
                else
                {
                    // Append to the end of the file
                    os.seekp(LoadedFileLength);
                    WriteSetting(os, qs.name);
                }

                queuedSettings.pop_front();
            }

            os.close();
        }
        else
            std::cout << "Could not append-write settings." << std::endl;
    }
}

static volatile bool shouldSaveLoop = true;

static void SettingSaveLoop()
{
    while (shouldSaveLoop)
    {
        if (queuedSettings.size() > 0) {
            GlobalSettings::SaveSettings();
            std::cout << "Autosaved settings" << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

static std::thread saveThread;

void GlobalSettings::StopSavingLoop()
{
    shouldSaveLoop = false;
}

void GlobalSettings::LoadSettings()
{
    std::ifstream is(SettingsFilePath, std::ios::binary);
    if (!is.fail())
    {
        is.seekg(0, is.end);
        LoadedFileLength = is.tellg();
        is.seekg(0, is.beg);
        std::size_t pos = 0;

        try
        {
            if (LoadedFileLength < 6)
                throw std::runtime_error("Settings file too small");

            while (pos < LoadedFileLength)
            {
                // Read the amount of chars in the setting name
                char nameLengthBuf[2];
                is.read(nameLengthBuf, 2);
                uint16_t nameLength = *(uint16_t*)nameLengthBuf;

                // Read the name
                // Add the null terminator
                char name[nameLength + 1];
                is.read(name, nameLength);
                name[nameLength] = '\0';

                // Read the byte count of the value
                char valLengthBuf[4];
                is.read(valLengthBuf, 4);
                uint32_t valLength = *(uint32_t*)valLengthBuf;

                // Read the value
                char value[valLength];
                is.read(value, valLength);

                // Load the value
                std::string sName(name);
                settingsMap[sName] = SettingValue(&value, valLength);

                // Store its file metadata for improved rewriting
                pos += 6 + nameLength;
                lSettingsMap[sName] = LoadedSetting(pos, valLength);

                pos += valLength;
            }

            readFile = true;
        }
        catch (std::exception& e)
        {
            std::cout << "Settings loading exception: " << e.what() << std::endl;
        }
        catch (...)
        {
            std::cout << "Exception loading settings." << std::endl;
        }

        is.close();
    }
    else
        std::cout << "Settings file does not exist." << std::endl;

    saveThread = std::thread(SettingSaveLoop);
    saveThread.detach();
}

// This function reads a setting by converting it from a byte array
template<typename T> T GetGlobalSetting(std::string name)
{
    if (settingsMap.count(name) == 0)
        return T();

    // NOTE: This thing is seriously unsafe and should only be used by GlobalSetting objects

    return *reinterpret_cast<T*>(settingsMap[name].bytes.get());
}

// This function saves a setting by converting it to a byte array
template<typename T> void SetGlobalSetting(std::string name, T value, bool enqueue = true)
{
    // Check for change
    if (GetGlobalSetting<T>(name) == value)
        return;

    // Assign the settings
    settingsMap[name] = SettingValue(&value, sizeof(value));

    // Queue long-term storage saving of the setting
    // if it has not already been queued
    auto qs = QueuedSetting(name);
    if (enqueue && std::find(queuedSettings.begin(), queuedSettings.end(), qs) == queuedSettings.end())
        queuedSettings.push_front(qs);
}

template<typename T> GlobalSetting<T>::GlobalSetting(std::string name, T defVal)
{
    Name = name;
    SetGlobalSetting(Name, defVal, false);
}

template<typename T> void GlobalSetting<T>::Set(T value)
{
    // Check for change first
    if (value != Get())
    {
        SetGlobalSetting(Name, value);

        // Alert the handlers of the change
        for (const StoredHandler &handler : storedHandlers)
            handler.m_handler(handler.m_context, value);
    }
}

template<typename T> T GlobalSetting<T>::Get()
{
    return GetGlobalSetting<T>(Name);
}

template<typename T> void GlobalSetting<T>::RegisterHandler(ChangedHandler handler, void *context)
{
    storedHandlers.insert(StoredHandler(handler, context));
}

template<typename T> void GlobalSetting<T>::UnregisterHandler(ChangedHandler handler, void *context)
{
    storedHandlers.erase(StoredHandler(handler, context));
}

// Init the settings
// This needs to be at the end to prevent a crash
#define AUTO_SET(NAME, TYPE, VALUE) \
    GlobalSetting<TYPE> GlobalSettings::NAME = GlobalSetting<TYPE>(#NAME, VALUE);

AUTO_SET(BedWidth, float, 100.0f)
AUTO_SET(BedLength, float, 100.0f)
AUTO_SET(BedHeight, float, 100.0f)
AUTO_SET(InfillDensity, float, 20.0f)
AUTO_SET(LayerHeight, float, 0.2f)
AUTO_SET(SkirtLineCount, int, 3)
AUTO_SET(SkirtDistance, float, 5.0f)
AUTO_SET(PrintSpeed, float, 60.0f)
AUTO_SET(InfillSpeed, float, 100.0f)
AUTO_SET(TopBottomSpeed, float, 15.0f)
AUTO_SET(FirstLineSpeed, float, 15.0f)
AUTO_SET(TravelSpeed, float, 80.0f)
AUTO_SET(RetractionSpeed, float, 45.0f)
AUTO_SET(RetractionDistance, float, 3.5f)
AUTO_SET(ShellThickness, float, 1.5f)
AUTO_SET(TopBottomThickness, float, 1.2f)
AUTO_SET(PrintTemperature, int, 200)
AUTO_SET(InfillCombinationCount, int, 1)
#undef AUTO_SET

// Explicitly specialize the GS classes
template class GlobalSetting<float>;
template class GlobalSetting<int>;
