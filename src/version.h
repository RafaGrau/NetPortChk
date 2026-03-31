#pragma once
// ──────────────────────────────────────────────────────────────────────────────
// version.h  –  Single source of truth for NetPortChk version numbers.
//
// VER_BUILD is incremented automatically by the pre-build script
// (scripts/increment_build.py) on every successful compilation.
//
// To bump the project version manually, edit VER_MAJOR / VER_MINOR / VER_PATCH
// and reset VER_BUILD to 0.
// ──────────────────────────────────────────────────────────────────────────────

#define VER_MAJOR   1
#define VER_MINOR   3
#define VER_PATCH   0
#define VER_BUILD   1

// Helper macros used by the RC file
#define VER_FILEVERSION      VER_MAJOR,VER_MINOR,VER_PATCH,VER_BUILD
#define VER_PRODUCTVERSION   VER_MAJOR,VER_MINOR,VER_PATCH,VER_BUILD

// String versions (used in VS_VERSION_INFO VALUE blocks)
#define _STRINGIZE2(x) #x
#define _STRINGIZE(x)  _STRINGIZE2(x)
#define VER_FILEVERSION_STR    _STRINGIZE(VER_MAJOR) "." _STRINGIZE(VER_MINOR) "." _STRINGIZE(VER_PATCH) "." _STRINGIZE(VER_BUILD)
#define VER_PRODUCTVERSION_STR VER_FILEVERSION_STR
