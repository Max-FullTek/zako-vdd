#include "DriverSettings.h"
#include "SettingsXml.h"

#include "..\Core\DriverState.h"
#include "..\Logging\Logger.h"

#include <atlbase.h>
#include <mutex>
#include <shlwapi.h>
#include <string>
#include <xmllite.h>

using namespace std;

namespace
{
mutex s_xmlWriteMutex;

bool UpdateXmlElementText(const wchar_t *targetElement, const wchar_t *replacementText)
{
	lock_guard<mutex> xmlLock(s_xmlWriteMutex);

	const wstring settingsname = confpath + L"\\vdd_settings.xml";
	CComPtr<IStream> pFileStream;
	HRESULT hr = SHCreateStreamOnFileEx(settingsname.c_str(), STGM_READWRITE, FILE_ATTRIBUTE_NORMAL, FALSE, nullptr, &pFileStream);
	if (FAILED(hr))
	{
		VDD_LOG_ERROR("UpdatingXML: XML file could not be opened.");
		return false;
	}

	CComPtr<IXmlReader> pReader;
	hr = CreateXmlReader(__uuidof(IXmlReader), reinterpret_cast<void **>(&pReader), nullptr);
	if (FAILED(hr))
	{
		VDD_LOG_ERROR("UpdatingXML: Failed to create XML reader.");
		return false;
	}
	hr = pReader->SetInput(pFileStream);
	if (FAILED(hr))
	{
		VDD_LOG_ERROR("UpdatingXML: Failed to set XML reader input.");
		return false;
	}

	CComPtr<IStream> pOutFileStream;
	wstring tempFileName = settingsname + L".temp";
	hr = SHCreateStreamOnFileEx(tempFileName.c_str(), STGM_CREATE | STGM_WRITE, FILE_ATTRIBUTE_NORMAL, TRUE, nullptr, &pOutFileStream);
	if (FAILED(hr))
	{
		VDD_LOG_ERROR("UpdatingXML: Failed to create output file stream.");
		return false;
	}

	CComPtr<IXmlWriter> pWriter;
	hr = CreateXmlWriter(__uuidof(IXmlWriter), reinterpret_cast<void **>(&pWriter), nullptr);
	if (FAILED(hr))
	{
		VDD_LOG_ERROR("UpdatingXML: Failed to create XML writer.");
		return false;
	}
	hr = pWriter->SetOutput(pOutFileStream);
	if (FAILED(hr))
	{
		VDD_LOG_ERROR("UpdatingXML: Failed to set XML writer output.");
		return false;
	}
	hr = pWriter->WriteStartDocument(XmlStandalone_Omit);
	if (FAILED(hr))
	{
		VDD_LOG_ERROR("UpdatingXML: Failed to write start of the document.");
		return false;
	}

	if (!VddSettings::ReplaceElementText(pReader, pWriter, targetElement, replacementText) ||
		FAILED(pOutFileStream->Commit(STGC_DEFAULT)))
	{
		VDD_LOG_ERROR("UpdatingXML: Incomplete read/write or setting not found; original file preserved.");
		return false;
	}

	pWriter.Release();
	pReader.Release();
	pOutFileStream.Release();
	pFileStream.Release();

	if (!MoveFileExW(tempFileName.c_str(), settingsname.c_str(), MOVEFILE_REPLACE_EXISTING))
	{
		return false;
	}

	return true;
}
}

bool UpdateXmlToggleSetting(bool toggle, const wchar_t *variable)
{
	return UpdateXmlElementText(variable, toggle ? L"true" : L"false");
}

bool UpdateXmlGpuSetting(const wchar_t *gpuName)
{
	return UpdateXmlElementText(L"friendlyname", gpuName);
}

bool UpdateXmlDisplayCountSetting(int displayCount)
{
	const wstring value = to_wstring(displayCount);
	return UpdateXmlElementText(L"count", value.c_str());
}
