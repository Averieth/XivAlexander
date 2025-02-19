// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

// ReSharper disable CppClangTidyClangDiagnosticReservedIdMacro
// ReSharper disable CppClangTidyBugproneReservedIdentifier

#pragma once

#ifndef PCH_H
#define PCH_H

// C++ standard library
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <locale>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <numeric>
#include <queue>
#include <ranges>
#include <regex>
#include <set>
#include <span>
#include <string>
#include <type_traits>

// Windows API, part 1
#define NOMINMAX
#define _WINSOCKAPI_   // Prevent <winsock.h> from being included
#include <Windows.h>
#include <winternl.h>

// Windows API, part 2
#include <iphlpapi.h>
#include <mstcpip.h>
#include <PathCch.h>
#include <propvarutil.h>
#include <Psapi.h>
#include <shellapi.h>
#include <ShellScalingApi.h>
#include <ShlObj.h>
#include <Shlwapi.h>
#include <ShObjIdl.h>
#include <windowsx.h>
#include <winhttp.h>
#include <WinSock2.h>
#include <WS2tcpip.h>

// Windows API, part 3
#include <IcmpAPI.h>

// vcpkg dependencies
#pragma warning(push)
#pragma warning(disable: 26439)  // This kind of function may not throw. Declare it 'noexcept' (f.6).
#pragma warning(disable: 26495)  // Variable is uninitialized. Always initialize a member variable (type.6).
#pragma warning(disable: 26819)  // Unannotated fallthrough between switch labels (es.78).
#include <MinHook.h>
#include <argparse/argparse.hpp>
#include <cryptopp/hex.h>
#include <cryptopp/sha.h>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <libzippp/libzippp.h>
#include <nlohmann/json.hpp>
#include <scintilla/Scintilla.h>
#include <srell.hpp>
#include <Zydis/Zydis.h>
#pragma warning(pop)

// COM smart pointer definitions
#include <comdef.h>
_COM_SMARTPTR_TYPEDEF(IFileSaveDialog, __uuidof(IFileSaveDialog));
_COM_SMARTPTR_TYPEDEF(IFileOpenDialog, __uuidof(IFileOpenDialog));
_COM_SMARTPTR_TYPEDEF(IShellItem, __uuidof(IShellItem));
_COM_SMARTPTR_TYPEDEF(IShellItemArray, __uuidof(IShellItemArray));
_COM_SMARTPTR_TYPEDEF(ITaskbarList3, __uuidof(ITaskbarList3));
_COM_SMARTPTR_TYPEDEF(IPropertyStore, __uuidof(IPropertyStore));

// Infrequently changed utility headers
#include <XivAlexanderCommon/XaFormat.h>

#endif //PCH_H
