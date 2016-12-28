#ifndef QTSETTINGS_H
#define QTSETTINGS_H

#include <QObject>
#include "globalsettings.h"

// This class wraps GlobalSettings for use by Qt
class QtSettings : public QObject
{
    Q_OBJECT
public:
    explicit QtSettings(QObject *parent = 0);

#define AUTO_SETTING_PROPERTY(TYPE, NAME, NAMECAP) \
    Q_PROPERTY(TYPE NAME READ NAME WRITE set##NAMECAP NOTIFY NAME##Changed ) \
    TYPE NAME() const { return GlobalSettings::NAMECAP.Get(); }  \
    void set##NAMECAP (TYPE val) { GlobalSettings::NAMECAP.Set(val); } \
    Q_SIGNAL void NAME##Changed();

    AUTO_SETTING_PROPERTY(float, bedWidth, BedWidth)
    AUTO_SETTING_PROPERTY(float, bedHeight, BedHeight)
    AUTO_SETTING_PROPERTY(float, bedLength, BedLength)
    AUTO_SETTING_PROPERTY(float, infillDensity, InfillDensity)
    AUTO_SETTING_PROPERTY(float, layerHeight, LayerHeight)
    AUTO_SETTING_PROPERTY(int, skirtLineCount, SkirtLineCount)
    AUTO_SETTING_PROPERTY(float, skirtDistance, SkirtDistance)
    AUTO_SETTING_PROPERTY(float, printSpeed, PrintSpeed)
    AUTO_SETTING_PROPERTY(float, infillSpeed, InfillSpeed)
    AUTO_SETTING_PROPERTY(float, topBottomSpeed, TopBottomSpeed)
    AUTO_SETTING_PROPERTY(float, firstLineSpeed, FirstLineSpeed)
    AUTO_SETTING_PROPERTY(float, travelSpeed, TravelSpeed)
    AUTO_SETTING_PROPERTY(float, retractionSpeed, RetractionSpeed)
    AUTO_SETTING_PROPERTY(float, retractionDistance, RetractionDistance)
    AUTO_SETTING_PROPERTY(float, shellThickness, ShellThickness)
    AUTO_SETTING_PROPERTY(float, topBottomThickness, TopBottomThickness)
    AUTO_SETTING_PROPERTY(int, printTemperature, PrintTemperature)
#undef AUTO_SETTING_PROPERTY
};

// The global instance
extern QtSettings qtSettings;

#endif // QTSETTINGS_H
