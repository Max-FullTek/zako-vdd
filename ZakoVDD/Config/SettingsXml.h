#pragma once

#include "SettingsValidation.h"
#include "../Util/RefreshRate.h"
#include <windows.h>
#include <objbase.h>
#include <xmllite.h>
#include <set>
#include <tuple>
#include <vector>

namespace VddSettings
{
struct DisplaySettings
{
    unsigned int count = 1;
    std::wstring gpu;
    std::vector<std::tuple<int, int, int, int>> modes;
};

// Publish only after reaching a valid document end. The caller's last good
// snapshot survives malformed XML, invalid values and empty mode lists.
inline bool ReadDisplaySettings(IXmlReader* reader, DisplaySettings& output)
{
    DisplaySettings staged;
    std::wstring element;
    int width = 0, height = 0;
    bool inResolution = false;
    bool needsValue = false;
    std::set<std::tuple<int, int>> resolutions;
    std::vector<float> rates;
    XmlNodeType type;
    HRESULT hr;
    while ((hr = reader->Read(&type)) == S_OK)
    {
        const wchar_t* text = nullptr;
        UINT length = 0;
        if (type == XmlNodeType_Element)
        {
            if (needsValue) return false;
            if (FAILED(reader->GetLocalName(&text, &length))) return false;
            element.assign(text, length);
            if (element == L"resolution")
            {
                if (inResolution || reader->IsEmptyElement()) return false;
                inResolution = true;
                width = 0; height = 0;
            }
            needsValue = element == L"count" || element == L"width" || element == L"height" ||
                element == L"refresh_rate" || element == L"g_refresh_rate";
            if (reader->IsEmptyElement() && needsValue) return false;
        }
        else if (type == XmlNodeType_EndElement)
        {
            if (needsValue) return false;
            if (FAILED(reader->GetLocalName(&text, &length))) return false;
            if (std::wstring(text, length) == L"resolution")
            {
                if (width <= 0 || height <= 0) return false;
                inResolution = false;
            }
            element.clear();
        }
        else if (type == XmlNodeType_Text || type == XmlNodeType_CDATA)
        {
            if (FAILED(reader->GetValue(&text, &length))) return false;
            const std::wstring value(text, length);
            needsValue = false;
            if (element == L"count")
            {
                int count;
                if (!DisplayCount(value.c_str(), count)) return false;
                staged.count = static_cast<unsigned int>(count);
            }
            else if (element == L"friendlyname") staged.gpu = value;
            else if (element == L"width")
            {
                if (!PositiveInteger(value, width)) return false;
            }
            else if (element == L"height")
            {
                if (!PositiveInteger(value, height) || width <= 0) return false;
                resolutions.emplace(width, height);
            }
            else if (element == L"refresh_rate" || element == L"g_refresh_rate")
            {
                float rate;
                if (!RefreshRate(value, rate)) return false;
                if (element == L"g_refresh_rate") rates.push_back(rate);
                else
                {
                    if (width <= 0 || height <= 0) return false;
                    int num, den;
                    float_to_vsync(rate, num, den);
                    staged.modes.emplace_back(width, height, num, den);
                }
            }
        }
    }
    if (hr != S_FALSE) return false;
    for (float rate : rates)
    {
        int num, den;
        float_to_vsync(rate, num, den);
        for (const auto& resolution : resolutions)
            staged.modes.emplace_back(std::get<0>(resolution), std::get<1>(resolution), num, den);
    }
    if (staged.modes.empty()) return false;
    output = std::move(staged);
    return true;
}

// Copy all unrelated XML nodes, including attributes and empty elements.
// A failed read/write or absent target cannot be reported as a saved setting.
inline bool ReplaceElementText(IXmlReader* reader, IXmlWriter* writer,
                               const wchar_t* target, const wchar_t* value)
{
    bool inTarget = false, replaced = false;
    XmlNodeType type;
    HRESULT hr;
    while ((hr = reader->Read(&type)) == S_OK)
    {
        if (type == XmlNodeType_XmlDeclaration) continue;
        if (type == XmlNodeType_Element)
        {
            if (inTarget) return false;
            const wchar_t* name = nullptr;
            if (FAILED(reader->GetLocalName(&name, nullptr))) return false;
            if (wcscmp(name, target) == 0)
            {
                if (FAILED(writer->WriteStartElement(nullptr, name, nullptr)) ||
                    FAILED(writer->WriteAttributes(reader, FALSE)) ||
                    FAILED(writer->WriteString(value))) return false;
                replaced = true;
                if (reader->IsEmptyElement())
                {
                    if (FAILED(writer->WriteEndElement())) return false;
                }
                else inTarget = true;
                continue;
            }
        }
        if (inTarget)
        {
            if (type == XmlNodeType_EndElement)
            {
                if (FAILED(writer->WriteFullEndElement())) return false;
                inTarget = false;
            }
            continue;
        }
        if (FAILED(writer->WriteNodeShallow(reader, FALSE))) return false;
    }
    return hr == S_FALSE && replaced && !inTarget &&
        SUCCEEDED(writer->WriteEndDocument()) && SUCCEEDED(writer->Flush());
}
}
