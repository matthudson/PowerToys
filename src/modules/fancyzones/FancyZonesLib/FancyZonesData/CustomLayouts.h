#pragma once

#include <guiddef.h>
#include <map>
#include <memory>
#include <optional>
#include <vector>

#include <FancyZonesLib/FancyZonesData/LayoutData.h>
#include <FancyZonesLib/FancyZonesDataTypes.h>
#include <FancyZonesLib/GuidUtils.h>
#include <FancyZonesLib/ModuleConstants.h>

#include <common/SettingsAPI/FileWatcher.h>
#include <common/SettingsAPI/settings_helpers.h>

namespace NonLocalizable
{
    namespace CustomLayoutsIds
    {
        const static wchar_t* CustomLayoutsArrayID = L"custom-layouts";
        const static wchar_t* UuidID = L"uuid";
        const static wchar_t* NameID = L"name";
        const static wchar_t* InfoID = L"info";
        const static wchar_t* TypeID = L"type";
        const static wchar_t* CanvasID = L"canvas";
        const static wchar_t* GridID = L"grid";
        const static wchar_t* SensitivityRadiusID = L"sensitivity-radius";

        // canvas
        const static wchar_t* RefHeightID = L"ref-height";
        const static wchar_t* RefWidthID = L"ref-width";
        const static wchar_t* ZonesID = L"zones";
        const static wchar_t* XAxisID = L"X";
        const static wchar_t* YAxisID = L"Y";
        const static wchar_t* WidthID = L"width";
        const static wchar_t* HeightID = L"height";

        // grid
        const static wchar_t* RowsID = L"rows";
        const static wchar_t* ColumnsID = L"columns";
        const static wchar_t* RowsPercentageID = L"rows-percentage";
        const static wchar_t* ColumnsPercentageID = L"columns-percentage";
        const static wchar_t* CellChildMapID = L"cell-child-map";
        const static wchar_t* ShowSpacingID = L"show-spacing";
        const static wchar_t* SpacingID = L"spacing";
    }
}

class CustomLayouts
{
public:
    using TCustomLayoutMap = std::unordered_map<GUID, FancyZonesDataTypes::CustomLayoutData>;

    static CustomLayouts& instance();

    inline static std::wstring CustomLayoutsFileName()
    {
        std::wstring saveFolderPath = PTSettingsHelper::get_module_save_folder_location(NonLocalizable::ModuleKey);
#if defined(UNIT_TESTS)
        return saveFolderPath + L"\\test-custom-layouts.json";
#else
        return saveFolderPath + L"\\custom-layouts.json";
#endif
    }

    void LoadData();

    std::optional<LayoutData> GetLayout(const GUID& id) const noexcept;
    std::optional<FancyZonesDataTypes::CustomLayoutData> GetCustomLayoutData(const GUID& id) const noexcept;
    const TCustomLayoutMap& GetAllLayouts() const noexcept;

    /**
     * Updates the row/column track percentages of a stored custom grid layout
     * in memory and serializes the complete layout map back to
     * custom-layouts.json, preserving every unrelated layout. Returns false
     * without writing anything when the layout is missing or is not a grid, or
     * when a vector does not hold exactly `tracks` positive entries summing to
     * LayoutConfigurator::C_MULTIPLIER.
     */
    bool SetGridLayoutTrackPercents(const GUID& id, const std::vector<int>& rowsPercents, const std::vector<int>& columnsPercents) noexcept;

private:
    CustomLayouts();
    ~CustomLayouts() = default;

    TCustomLayoutMap m_layouts;
    std::unique_ptr<FileWatcher> m_fileWatcher;
};
