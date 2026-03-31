#include "pch.h"
#include "ConfigManager.h"
#include "XmlLite.h"
#include "AppTypes.h"   // PortDB::PortDefaultDesc

// ──────────────────────────────────────────────────────────────────────────────
// Static helpers
// ──────────────────────────────────────────────────────────────────────────────
DestinationType ConfigManager::ParseType(const std::wstring& tag)
{
    if (tag == L"DC")          return DestinationType::DC;
    if (tag == L"PrintServer") return DestinationType::PrintServer;
    if (tag == L"SCCM_Full")   return DestinationType::SCCM_Full;
    if (tag == L"SCCM_DP")     return DestinationType::SCCM_DP;
    if (tag == L"DNS")         return DestinationType::DNS;
    if (tag == L"DHCP")        return DestinationType::DHCP;
    if (tag == L"Custom")      return DestinationType::Custom;
    return DestinationType::DC;
}

Protocol ConfigManager::ParseProto(const std::wstring& s)
{
    return (s == L"UDP") ? Protocol::UDP : Protocol::TCP;
}

// ──────────────────────────────────────────────────────────────────────────────
// Load
// ──────────────────────────────────────────────────────────────────────────────
bool ConfigManager::Load(const wchar_t* path, AppConfig& out)
{
    out = {};
    auto root = XmlParseFile(path);
    if (!root) {
        m_lastError = L"Cannot open or parse: ";
        m_lastError += path;
        return false;
    }

    // <Settings timeout="..."/>
    if (auto sNode = root->FirstChild(L"Settings"))
        out.timeoutMs = sNode->AttrInt(L"timeout", 1000);

    // <Destinations>
    auto destsNode = root->FirstChild(L"Destinations");
    if (!destsNode) return true;

    for (auto& destNode : destsNode->Children(L"Destination"))
    {
        DestinationConfig dc;
        dc.name = destNode->Attr(L"name");
        dc.ip   = destNode->Attr(L"ip");
        dc.type = ParseType(destNode->Attr(L"type"));

        auto portsNode = destNode->FirstChild(L"Ports");
        if (portsNode)
        {
            for (auto& portNode : portsNode->Children(L"Port"))
            {
                PortEntry pe;
                pe.port        = portNode->AttrInt(L"port");
                pe.protocol    = ParseProto(portNode->Attr(L"protocol"));
                pe.enabled     = portNode->AttrBool(L"enabled", true);
                // Description is not persisted; resolve from well-known table
                const wchar_t* d = PortDB::PortDefaultDesc(pe.port, pe.protocol);
                pe.description = d ? d : L"";
                dc.ports.push_back(pe);
            }
        }
        out.destinations.push_back(std::move(dc));
    }
    return true;
}

// ──────────────────────────────────────────────────────────────────────────────
// Save
// ──────────────────────────────────────────────────────────────────────────────
bool ConfigManager::Save(const wchar_t* path, const AppConfig& cfg)
{
    XmlWriter w;
    w.Open(L"NetPortChkConfig", {{L"version", L"3"}});

    // Settings
    w.EmptyElement(L"Settings", {{L"timeout", std::to_wstring(cfg.timeoutMs)}});

    // Destinations
    w.Open(L"Destinations");
    for (const auto& dest : cfg.destinations)
    {
        w.Open(L"Destination", {
            {L"name", dest.name},
            {L"ip",   dest.ip},
            {L"type", PortDB::TypeTag(dest.type)}
        });

        w.Open(L"Ports");
        for (const auto& pe : dest.ports)
        {
            w.EmptyElement(L"Port", {
                {L"port",     std::to_wstring(pe.port)},
                {L"protocol", pe.protocol == Protocol::TCP ? L"TCP" : L"UDP"},
                {L"enabled",  pe.enabled ? L"1" : L"0"}
            });
        }
        w.Close(L"Ports");
        w.Close(L"Destination");
    }
    w.Close(L"Destinations");
    w.Close(L"NetPortChkConfig");

    if (!w.WriteFile(path)) {
        m_lastError = L"Cannot write: ";
        m_lastError += path;
        return false;
    }
    return true;
}
