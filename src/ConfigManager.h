#pragma once
#include "AppTypes.h"

// ──────────────────────────────────────────────────────────────────────────────
// ConfigManager – reads / writes NetPortChk.config (XML v3 format)
//
// Format example:
//   <NetPortChkConfig version="3">
//     <Source ip="192.168.1.1"/>
//     <Destinations>
//       <Destination name="DC01" ip="10.0.0.1" type="DC">
//         <Ports>
//           <Port port="88" protocol="TCP" description="Kerberos" enabled="1"/>
//         </Ports>
//       </Destination>
//     </Destinations>
//   </NetPortChkConfig>
// ──────────────────────────────────────────────────────────────────────────────
class ConfigManager
{
public:
    ConfigManager() = default;

    // Returns true on success.
    bool Load(const wchar_t* path, AppConfig& out);
    bool Save(const wchar_t* path, const AppConfig& cfg);

    const std::wstring& LastError() const { return m_lastError; }

private:
    std::wstring m_lastError;

    static DestinationType ParseType(const std::wstring& tag);
    static Protocol        ParseProto(const std::wstring& s);
};
