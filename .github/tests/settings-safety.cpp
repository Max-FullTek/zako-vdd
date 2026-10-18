#include <windows.h>
#include <atlbase.h>
#include <shlwapi.h>
#include "SettingsXml.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include "../Control/CommandDispatcher.h"
#include "../Control/CommandHandlers.h"

std::string action;
std::wstring argument;
#define FAKE_ACTION(name) void name(wchar_t* param) { action = #name; argument = param ? param : L""; }
FAKE_ACTION(HandleReloadDriverCommand)
FAKE_ACTION(HandleHdrPlusCommand)
FAKE_ACTION(HandleSdr10Command)
FAKE_ACTION(HandleCustomEdidCommand)
FAKE_ACTION(HandlePreventSpoofCommand)
FAKE_ACTION(HandleCeaOverrideCommand)
FAKE_ACTION(HandleEdidProfileCommand)
FAKE_ACTION(HandleVrrCommand)
FAKE_ACTION(HandleHardwareCursorCommand)
FAKE_ACTION(HandleD3DDeviceGpuCommand)
FAKE_ACTION(HandleIddCxVersionCommand)
FAKE_ACTION(HandleGetAssignedGpuCommand)
FAKE_ACTION(HandleGetAllGpusCommand)
FAKE_ACTION(HandleSetGpuCommand)
FAKE_ACTION(HandleSetDisplayCountCommand)
FAKE_ACTION(HandleCreateMonitorCommand)
FAKE_ACTION(HandleDestroyMonitorCommand)
FAKE_ACTION(HandleRefreshModesCommand)
FAKE_ACTION(HandleSetModesCommand)
FAKE_ACTION(HandleUnknownCommand)
#undef FAKE_ACTION

void Check(bool ok, const char* message)
{
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}

CComPtr<IXmlReader> Reader(const std::string& xml)
{
    CComPtr<IStream> stream;
    stream.Attach(SHCreateMemStream(reinterpret_cast<const BYTE*>(xml.data()), static_cast<UINT>(xml.size())));
    Check(stream != nullptr, "memory stream");
    CComPtr<IXmlReader> reader;
    Check(SUCCEEDED(CreateXmlReader(__uuidof(IXmlReader), reinterpret_cast<void**>(&reader), nullptr)), "reader creation");
    Check(SUCCEEDED(reader->SetInput(stream)), "reader input");
    return reader;
}

const std::string valid = "<vdd_settings><monitors><count>2</count></monitors>"
    "<gpu><friendlyname>default</friendlyname></gpu><global><g_refresh_rate>59.94</g_refresh_rate></global>"
    "<resolutions><resolution><width>2868</width><height>1320</height>"
    "<refresh_rate>120</refresh_rate></resolution></resolutions></vdd_settings>";

std::string Replace(std::string text, const std::string& from, const std::string& to)
{
    const auto pos = text.find(from);
    Check(pos != std::string::npos, "test fixture replacement");
    text.replace(pos, from.size(), to);
    return text;
}

class FullStream final : public ISequentialStream
{
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override
    {
        if (!out) return E_POINTER;
        *out = nullptr;
        if (iid != IID_IUnknown && iid != IID_ISequentialStream) return E_NOINTERFACE;
        *out = static_cast<ISequentialStream*>(this); AddRef(); return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override { const auto n = --refs; if (!n) delete this; return n; }
    HRESULT STDMETHODCALLTYPE Read(void*, ULONG, ULONG*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Write(const void*, ULONG, ULONG* written) override
    { if (written) *written = 0; return STG_E_MEDIUMFULL; }
private:
    ULONG refs = 1;
};

bool Transform(const std::string& xml, const wchar_t* target, const wchar_t* value,
               std::string& result, bool full = false)
{
    auto reader = Reader(xml);
    CComPtr<IXmlWriter> writer;
    Check(SUCCEEDED(CreateXmlWriter(__uuidof(IXmlWriter), reinterpret_cast<void**>(&writer), nullptr)), "writer creation");
    CComPtr<IStream> stream;
    Check(SUCCEEDED(CreateStreamOnHGlobal(nullptr, TRUE, &stream)), "output memory stream");
    CComPtr<ISequentialStream> failing;
    failing.Attach(new FullStream());
    Check(SUCCEEDED(writer->SetOutput(full ? static_cast<IUnknown*>(failing) : static_cast<IUnknown*>(stream))), "writer output");
    Check(SUCCEEDED(writer->WriteStartDocument(XmlStandalone_Omit)), "document start");
    if (!VddSettings::ReplaceElementText(reader, writer, target, value)) return false;
    HGLOBAL memory = nullptr;
    Check(SUCCEEDED(GetHGlobalFromStream(stream, &memory)), "output handle");
    STATSTG stat{};
    Check(SUCCEEDED(stream->Stat(&stat, STATFLAG_NONAME)), "output size");
    const auto* bytes = static_cast<const char*>(GlobalLock(memory));
    Check(bytes != nullptr, "output bytes");
    result.assign(bytes, stat.cbSize.LowPart);
    GlobalUnlock(memory);
    return true;
}

int main()
{
    for (const auto* bad : {L"RELOAD_DRIVERjunk", L"DESTROYMONITORx", L"HDRPLUStrue", L""})
    {
        std::wstring command(bad);
        DispatchVddCommandBuffer(command.data());
        Check(action == "HandleUnknownCommand", "command prefix accepted as action");
    }
    DispatchVddCommandBuffer(nullptr);
    std::wstring command = L"HDRPLUS true";
    DispatchVddCommandBuffer(command.data());
    Check(action == "HandleHdrPlusCommand" && argument == L"true", "valid command dispatch");
    command = L"RELOAD_DRIVER";
    DispatchVddCommandBuffer(command.data());
    Check(action == "HandleReloadDriverCommand" && argument.empty(), "parameterless command");
    VddSettings::DisplaySettings saved;
    auto reader = Reader(valid);
    Check(VddSettings::ReadDisplaySettings(reader, saved), "valid configuration rejected");
    Check(saved.count == 2 && saved.gpu == L"default" && saved.modes.size() == 2, "snapshot values");
    Check(saved.modes.front() == std::make_tuple(2868, 1320, 120, 1), "preferred mode order");
    const auto good = saved;
    for (const auto& bad : {
        valid.substr(0, valid.size() - 5),
        Replace(valid, "<count>2</count>", "<count>-1</count>"),
        Replace(valid, "<count>2</count>", "<count>17</count>"),
        Replace(valid, "<count>2</count>", "<count>2junk</count>"),
        Replace(valid, "<count>2</count>", "<count> </count>"),
        Replace(valid, "<width>2868</width>", "<width>0</width>"),
        Replace(valid, "<height>1320</height>", "<height/>"),
        Replace(valid, "59.94", "nan"), Replace(valid, "59.94", "inf"),
        Replace(valid, "59.94", "999999999"), Replace(valid, "59.94", "0.000001"),
        Replace(valid, "59.94", "60junk"), std::string("<vdd_settings/>") })
    {
        reader = Reader(bad);
        Check(!VddSettings::ReadDisplaySettings(reader, saved), "bad configuration accepted");
        Check(saved.count == good.count && saved.gpu == good.gpu && saved.modes == good.modes,
              "bad XML changed last good snapshot");
    }
    int count = 0;
    Check(VddSettings::DisplayCount(L"16", count) && count == 16, "max count");
    for (const auto* bad : {L"", L"0", L"-1", L"17", L"1junk", L"9999999999999999999"})
        Check(!VddSettings::DisplayCount(bad, count), "bad command count");
    Check(!VddSettings::DisplayCount(nullptr, count), "null count");
    std::wstring gpu;
    Check(VddSettings::QuotedGpu(L"\"NVIDIA GeForce RTX 4090\"", gpu) && gpu == L"NVIDIA GeForce RTX 4090", "GPU name");
    for (const auto* bad : {L"", L"x", L"\"\"", L"\" \"", L"unquoted", L"\"bad\"extra\""})
        Check(!VddSettings::QuotedGpu(bad, gpu), "bad GPU name");
    Check(!VddSettings::QuotedGpu(nullptr, gpu), "null GPU name");

    std::string output;
    Check(Transform(valid, L"friendlyname", L"GPU & test", output), "GPU XML update");
    reader = Reader(output);
    Check(VddSettings::ReadDisplaySettings(reader, saved) && saved.gpu == L"GPU & test" && saved.modes == good.modes,
          "GPU update changed modes or lost escaping");
    Check(Transform("<root keep=\"yes\"><empty/><count/></root>", L"count", L"3", output), "empty target update");
    Check(output.find("keep=\"yes\"") != std::string::npos && output.find("<empty") != std::string::npos &&
          output.find("<count>3</count>") != std::string::npos, "unrelated XML lost");
    Check(!Transform(valid, L"missing", L"3", output), "missing target reported saved");
    Check(!Transform(valid.substr(0, valid.size() - 5), L"count", L"3", output), "malformed source reported saved");
    Check(!Transform(valid, L"count", L"3", output, true), "full output stream reported saved");
    std::cout << "Settings safety regressions passed (memory-only; no registry, devices or installed files).\n";
}
