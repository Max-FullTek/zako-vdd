#pragma once

#include <windows.h>
#include <algorithm>
#include <cstring>
#include <string>

namespace VddRegistry
{
// Decode only the bytes returned by Windows, never an unbounded C string.
inline bool DecodeString(DWORD type, const wchar_t* data, DWORD bytes,
                         size_t capacity, std::wstring& value)
{
    if ((type != REG_SZ && type != REG_EXPAND_SZ) || !data ||
        bytes == 0 || bytes % sizeof(wchar_t) != 0 ||
        bytes / sizeof(wchar_t) > capacity)
    {
        return false;
    }
    const size_t count = bytes / sizeof(wchar_t);
    const auto end = std::find(data, data + count, L'\0');
    if (end == data + count)
    {
        return false;
    }
    // REG_EXPAND_SZ retains the existing literal (unexpanded) behavior.
    value.assign(data, static_cast<size_t>(end - data));
    return true;
}

inline bool DecodeDword(DWORD type, const BYTE* data, DWORD bytes, DWORD& value)
{
    if (type != REG_DWORD || !data || bytes != sizeof(DWORD))
    {
        return false;
    }
    std::memcpy(&value, data, sizeof(value));
    return true;
}

inline bool ReadString(HKEY key, const wchar_t* name, std::wstring& value)
{
    wchar_t buffer[MAX_PATH] = {};
    DWORD bytes = sizeof(buffer);
    DWORD type = REG_NONE;
    const LONG result = RegQueryValueExW(key, name, nullptr, &type,
                                        reinterpret_cast<LPBYTE>(buffer), &bytes);
    return result == ERROR_SUCCESS && DecodeString(type, buffer, bytes, MAX_PATH, value);
}

inline bool ReadDword(HKEY key, const wchar_t* name, DWORD& value)
{
    DWORD buffer = 0;
    DWORD bytes = sizeof(buffer);
    DWORD type = REG_NONE;
    const LONG result = RegQueryValueExW(key, name, nullptr, &type,
                                        reinterpret_cast<LPBYTE>(&buffer), &bytes);
    return result == ERROR_SUCCESS &&
           DecodeDword(type, reinterpret_cast<const BYTE*>(&buffer), bytes, value);
}
}
