#include "pch.h"
#include "HtmlExporter.h"
#include <fstream>
#include <sstream>
#include <ctime>
#include <codecvt>
#include <locale>

// ──────────────────────────────────────────────────────────────────────────────
// Export
// ──────────────────────────────────────────────────────────────────────────────
bool HtmlExporter::Export(const wchar_t* path,
                          const std::vector<DestinationResult>& results,
                          const std::wstring& sourceIP,
                          int timeoutMs)
{
    // ── Timestamp ─────────────────────────────────────────────────────────────
    time_t now = time(nullptr);
    struct tm lt {};
    localtime_s(&lt, &now);
    wchar_t ts[64];
    wcsftime(ts, 64, L"%Y-%m-%d %H:%M:%S", &lt);

    // ── Open file for UTF-8 output ────────────────────────────────────────────
    // std::wofstream defaults to the system ANSI codepage; imbue with UTF-8
    // so that accented characters in server names, descriptions, etc. are
    // written correctly regardless of the system locale.
#pragma warning(push)
#pragma warning(disable: 4996)   // codecvt_utf8 deprecated in C++17 but still available
    std::wofstream f(path);
    if (!f.is_open()) return false;
    f.imbue(std::locale(f.getloc(),
                        new std::codecvt_utf8<wchar_t>));
#pragma warning(pop)

    // ── CSS + header ──────────────────────────────────────────────────────────
    f << L"<!DOCTYPE html>\n<html lang=\"es\">\n<head>\n"
      << L"<meta charset=\"UTF-8\">\n"
      << L"<title>NetPortChk Report</title>\n"
      << L"<style>\n"
      << L"body{font-family:Segoe UI,Arial,sans-serif;font-size:13px;background:#f5f5f5;color:#222;}\n"
      << L"h1{background:#1a3a5c;color:#fff;padding:10px 20px;margin:0;font-size:17px;font-weight:400;line-height:1.7;}\n"
      << L"h1 .r1{font-size:17px;font-weight:600;display:block;}\n"
      << L"h1 .r2{font-size:17px;font-weight:400;color:#c0d4e8;display:block;}\n"
      << L"h2{background:#2a5080;color:#fff;padding:6px 16px;margin:16px 0 0;font-size:14px;font-weight:400;}\n"
      << L"table{border-collapse:collapse;width:100%;margin-bottom:16px;table-layout:fixed;}\n"
      << L"col.c-port{width:70px;}\n"
      << L"col.c-proto{width:90px;}\n"
      << L"col.c-desc{width:auto;}\n"
      << L"col.c-status{width:130px;}\n"
      << L"col.c-lat{width:90px;}\n"
      << L"col.c-tx{width:80px;}\n"
      << L"col.c-rx{width:80px;}\n"
      << L"th{background:#1a3a5c;color:#fff;padding:5px 8px;text-align:left;font-weight:400;}\n"
      << L"td{padding:4px 8px;border-bottom:1px solid #ccc;}\n"
      << L"tr:nth-child(even)td{background:#eef1f6;}\n"
      << L".ok{color:#009000;font-weight:600}\n"
      << L".fail{color:#c80000;font-weight:600}\n"
      << L".noresp{color:#4444aa;font-weight:600}\n"
      << L".unk{color:#b47800;font-weight:600}\n"
      << L".pend{color:#969696}\n"
      << L"</style></head><body>\n"
      << L"<h1>"
      << L"<span class=\"r1\">NetPortChk Connectivity Report &nbsp;&mdash;&nbsp; " << ts << L"</span>"
      << L"<span class=\"r2\">IP Origen: "
      << (sourceIP.empty() ? L"(desconocida)" : sourceIP)
      << L" &nbsp;&nbsp;&nbsp;&nbsp; (Timeout: " << timeoutMs << L" ms)"
      << L"</span></h1>\n";

    // ── Results table per destination ─────────────────────────────────────────
    for (const auto& dr : results)
    {
        f << L"<h2>IP Destino: " << dr.config.ip
          << L" &nbsp;&nbsp;&nbsp;&nbsp; Hostname: " << dr.config.name
          << L" &nbsp;&mdash;&nbsp; (" << PortDB::TypeName(dr.config.type) << L")</h2>\n"
          << L"<table>"
          << L"<colgroup><col class=\"c-port\"><col class=\"c-proto\"><col class=\"c-desc\">"
          << L"<col class=\"c-status\"><col class=\"c-lat\"><col class=\"c-tx\"><col class=\"c-rx\"></colgroup>"
          << L"<tr><th>Puerto</th><th>Protocolo</th>"
          << L"<th>Descripci\x00f3n</th><th>Estado</th><th>Latencia (ms)</th>"
          << L"<th>Tx (bytes)</th><th>Rx (bytes)</th></tr>\n";

        // TCP ports first, then UDP
        for (int pass = 0; pass < 2; ++pass)
        {
            Protocol passProto = (pass == 0) ? Protocol::TCP : Protocol::UDP;
            for (const auto& pr : dr.portResults)
            {
                if (pr.entry.protocol != passProto) continue;
                if (!pr.enabled) continue;

                const wchar_t* cls = L"pend";
                switch (pr.status) {
                case ConnectStatus::OK:          cls = L"ok";     break;
                case ConnectStatus::FAILED:      cls = L"fail";   break;
                case ConnectStatus::NO_RESPONSE: cls = L"noresp"; break;
                case ConnectStatus::UNKNOWN:     cls = L"unk";    break;
                default: break;
                }
                f << L"<tr><td>" << pr.entry.port << L"</td><td>"
                  << StrUtil::ProtoText(pr.entry.protocol) << L"</td><td>"
                  << pr.entry.description
                  << L"</td><td class=\"" << cls << L"\">"
                  << StrUtil::StatusText(pr.status) << L"</td><td>"
                  << (pr.status == ConnectStatus::OK ? std::to_wstring(pr.latencyMs) : L"&mdash;")
                  << L"</td><td>"
                  << (pr.bytesSent > 0 ? std::to_wstring(pr.bytesSent) : L"&mdash;")
                  << L"</td><td>"
                  << (pr.bytesRecv > 0 ? std::to_wstring(pr.bytesRecv) : L"&mdash;")
                  << L"</td></tr>\n";
            }
        }
        f << L"</table>\n";
    }

    f << L"</body></html>\n";
    return f.good();
}
