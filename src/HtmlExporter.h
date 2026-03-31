#pragma once
#include "AppTypes.h"
#include <vector>

class HtmlExporter
{
public:
    // Returns true on success.
    // timeoutMs: connect timeout used during the test run (shown in report header).
    bool Export(const wchar_t* path,
                const std::vector<DestinationResult>& results,
                const std::wstring& sourceIP,
                int timeoutMs = 1000);
};
