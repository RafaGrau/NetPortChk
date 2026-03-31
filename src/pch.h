#pragma once

// ─── Prevent winsock.h being dragged in by windows.h ────────────────────────
#define _WINSOCKAPI_

// ─── Winsock2 / IP helpers (must come before any Windows / MFC header) ───────
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

// ─── MFC dynamic-link ────────────────────────────────────────────────────────
// NOTE: _AFXDLL is defined in the project preprocessor settings (vcxproj),
//       NOT here, to avoid C4005 macro-redefinition warnings.
#include <afxwin.h>           // Core MFC + Windows
#include <htmlhelp.h>
#include <afxext.h>           // MFC Extensions (CToolBar, CStatusBar, …)
#include <afxcmn.h>           // Common Controls (CListCtrl, CProgressCtrl, …)
#include <afxdlgs.h>          // Common dialogs (CFileDialog, …)
#include <afxdialogex.h>      // CDialogEx (themed dialogs with background image)

// ─── STL ─────────────────────────────────────────────────────────────────────
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <memory>
#include <chrono>

// ─── Pragma comments (ws2_32 and iphlpapi are also in .vcxproj) ──────────────
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
