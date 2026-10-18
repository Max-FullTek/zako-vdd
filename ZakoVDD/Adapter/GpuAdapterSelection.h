#pragma once

#include <string>
#include <windows.h>

class AdapterOption;

bool SameGpuName(const std::wstring &left, const std::wstring &right);
void EnsureUsableRenderAdapter(AdapterOption &adapterOption, const std::wstring &requestedGpuName);
void LogAvailableGPUs();
