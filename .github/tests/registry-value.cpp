// Offline API mock: never opens or changes a real registry key.
#define NOMINMAX
#include <windows.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

static DWORD queryType = REG_NONE;
static std::vector<BYTE> queryData;
static LONG queryError = ERROR_SUCCESS;

static LONG FakeQuery(HKEY, LPCWSTR, LPDWORD, LPDWORD type, LPBYTE data, LPDWORD bytes)
{
    if (queryError != ERROR_SUCCESS) return queryError;
    *type = queryType;
    const DWORD available = *bytes;
    *bytes = static_cast<DWORD>(queryData.size());
    if (available < *bytes) return ERROR_MORE_DATA;
    if (*bytes) std::memcpy(data, queryData.data(), *bytes);
    return ERROR_SUCCESS;
}

#define RegQueryValueExW FakeQuery
#include "RegistryValue.h"
#undef RegQueryValueExW

static void Require(bool ok, const char* message)
{
    if (!ok) throw std::runtime_error(message);
}

static void SetString(DWORD type, const wchar_t* text, size_t count)
{
    queryType = type;
    const auto bytes = reinterpret_cast<const BYTE*>(text);
    queryData.assign(bytes, bytes + count * sizeof(wchar_t));
}

int main()
{
    try
    {
        std::wstring value = L"unchanged";
        SetString(REG_SZ, L"true", 5);
        Require(VddRegistry::ReadString(nullptr, L"test", value) && value == L"true", "valid string");
        SetString(REG_EXPAND_SZ, L"%TEMP%", 7);
        Require(VddRegistry::ReadString(nullptr, L"test", value) && value == L"%TEMP%", "literal expand string");
        SetString(REG_SZ, L"", 1);
        Require(VddRegistry::ReadString(nullptr, L"test", value) && value.empty(), "empty string");

        // Full-size, nonterminated input previously escaped the stack buffer.
        std::vector<wchar_t> full(MAX_PATH, L'x');
        value = L"unchanged";
        SetString(REG_SZ, full.data(), full.size());
        Require(!VddRegistry::ReadString(nullptr, L"test", value) && value == L"unchanged", "unterminated full buffer");
        full.back() = L'\0';
        SetString(REG_SZ, full.data(), full.size());
        Require(VddRegistry::ReadString(nullptr, L"test", value) && value.size() == MAX_PATH - 1, "maximum terminated string");

        for (const DWORD type : {REG_BINARY, REG_MULTI_SZ, REG_DWORD})
        {
            value = L"unchanged";
            SetString(type, L"true", 5);
            Require(!VddRegistry::ReadString(nullptr, L"test", value) && value == L"unchanged", "wrong string type");
        }
        SetString(REG_SZ, L"true", 5);
        queryData.pop_back();
        Require(!VddRegistry::ReadString(nullptr, L"test", value), "odd byte count");
        queryData.clear();
        Require(!VddRegistry::ReadString(nullptr, L"test", value), "zero byte count");
        queryData.resize((MAX_PATH + 1) * sizeof(wchar_t));
        Require(!VddRegistry::ReadString(nullptr, L"test", value), "oversized input");
        queryError = ERROR_FILE_NOT_FOUND;
        Require(!VddRegistry::ReadString(nullptr, L"test", value), "missing value");
        queryError = ERROR_ACCESS_DENIED;
        Require(!VddRegistry::ReadString(nullptr, L"test", value), "access denied");
        queryError = ERROR_SUCCESS;

        const DWORD expected = 123456;
        queryType = REG_DWORD;
        const auto data = reinterpret_cast<const BYTE*>(&expected);
        queryData.assign(data, data + sizeof(expected));
        DWORD number = 99;
        Require(VddRegistry::ReadDword(nullptr, L"test", number) && number == expected, "valid DWORD");
        for (const DWORD type : {REG_BINARY, REG_SZ, REG_QWORD})
        {
            queryType = type;
            number = 99;
            Require(!VddRegistry::ReadDword(nullptr, L"test", number) && number == 99, "wrong DWORD type");
        }
        queryType = REG_DWORD;
        for (const size_t length : {size_t(0), size_t(1), size_t(3), size_t(5), size_t(8)})
        {
            queryData.resize(length);
            number = 99;
            Require(!VddRegistry::ReadDword(nullptr, L"test", number) && number == 99, "wrong DWORD size");
        }
        std::cout << "Registry boundary/type/API-failure regression checks passed; no registry access.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
