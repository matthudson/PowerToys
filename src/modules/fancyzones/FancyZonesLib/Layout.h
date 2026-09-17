#pragma once

#include <FancyZonesLib/FancyZonesData/LayoutData.h>
#include <FancyZonesLib/util.h>

#include <FancyZonesLib/LayoutConfigurator.h> // ZonesMap

class Layout
{
public:
    Layout(const LayoutData& data);
    ~Layout() = default;

    bool Init(const FancyZonesUtils::Rect& workAreaRect, HMONITOR monitor) noexcept;

    GUID Id() const noexcept;
    FancyZonesDataTypes::ZoneSetLayoutType Type() const noexcept;

    const ZonesMap& Zones() const noexcept;
    int Spacing() const noexcept;
    /**
     * Atomically replaces the effective zones map, e.g. with a boundary-moved
     * map produced by GridTracks during a linked-resize gesture. Accepted only
     * when the new map keeps the current zone count and ids and every zone
     * reports IsValid(); a rejected map leaves the effective layout untouched.
     * Returns true when replaced.
     */
    bool ReplaceZones(ZonesMap zones) noexcept;
    ZoneIndexSet ZonesFromPoint(POINT pt) const noexcept;
    /**
     * Returns all zones spanned by the minimum bounding rectangle containing the two given zone index sets.
     */
    ZoneIndexSet GetCombinedZoneRange(const ZoneIndexSet& initialZones, const ZoneIndexSet& finalZones) const noexcept; 

    RECT GetCombinedZonesRect(const ZoneIndexSet& zones);
    /**
     * Bounding rect of a zone index set inside an arbitrary zones map, e.g. a
     * prospective boundary-moved map not yet installed through ReplaceZones.
     * Ids missing from the map are skipped.
     */
    static RECT CombinedZonesRect(const ZonesMap& zones, const ZoneIndexSet& indexSet);

private:
    const LayoutData m_data;
    ZonesMap m_zones{};
};
