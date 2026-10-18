#pragma once

#include <cmath>
#include <cwctype>
#include <limits>
#include <string>

namespace VddSettings
{
inline bool CompleteNumber(const std::wstring& text, size_t end)
{
    while (end < text.size() && std::iswspace(text[end])) ++end;
    return end == text.size();
}

inline bool PositiveInteger(const std::wstring& text, int& value)
{
    try {
        size_t end = 0;
        const int parsed = std::stoi(text, &end);
        if (parsed <= 0 || !CompleteNumber(text, end)) return false;
        value = parsed;
        return true;
    } catch (...) { return false; }
}

inline bool RefreshRate(const std::wstring& text, float& value)
{
    try {
        size_t end = 0;
        const float parsed = std::stof(text, &end);
        // Match float_to_vsync's float multiplication and rounding before its
        // conversion to int. Tiny positive rates must not round down to zero.
        const double numerator = std::round(static_cast<double>(parsed * 10000));
        if (!CompleteNumber(text, end) || !std::isfinite(numerator) ||
            numerator < 1 || numerator > (std::numeric_limits<int>::max)()) return false;
        value = parsed;
        return true;
    } catch (...) { return false; }
}

inline bool DisplayCount(const wchar_t* text, int& value)
{
    return text && PositiveInteger(text, value) && value <= 16;
}

inline bool QuotedGpu(const wchar_t* text, std::wstring& value)
{
    if (!text) return false;
    const std::wstring input(text);
    if (input.size() < 3 || input.front() != L'"' || input.back() != L'"') return false;
    const auto name = input.substr(1, input.size() - 2);
    if (name.find_first_of(L"\"\r\n") != std::wstring::npos ||
        name.find_first_not_of(L" \t") == std::wstring::npos) return false;
    value = name;
    return true;
}
}
