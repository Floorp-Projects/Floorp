/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "nsAppRunner.h"
#include "nsSystemInfo.h"
#include "prsystem.h"
#include "prio.h"
#include "mozilla/SSE.h"
#include "mozilla/arm.h"
#include "mozilla/LazyIdleThread.h"
#include "mozilla/Sprintf.h"
#include "jsapi.h"
#include "mozilla/dom/Promise.h"

#ifdef XP_WIN
#  include <comutil.h>
#  include <time.h>
#  ifndef __MINGW32__
#    include <iwscapi.h>
#  endif  // __MINGW32__
#  include <windows.h>
#  include <winioctl.h>
#  ifndef __MINGW32__
#    include <wscapi.h>
#  endif  // __MINGW32__
#  include "base/scoped_handle_win.h"
#  include "nsAppDirectoryServiceDefs.h"
#  include "nsDirectoryServiceDefs.h"
#  include "nsDirectoryServiceUtils.h"
#  include "nsWindowsHelpers.h"

#endif

#ifdef XP_MACOSX
#  include "MacHelpers.h"
#endif

#ifdef MOZ_WIDGET_GTK
#  include <gtk/gtk.h>
#  include <dlfcn.h>
#endif

#if defined(XP_LINUX) && !defined(ANDROID)
#  include <unistd.h>
#  include <fstream>
#  include "mozilla/Tokenizer.h"
#  include "nsCharSeparatedTokenizer.h"

#  include <map>
#  include <string>
#endif

#ifdef MOZ_WIDGET_ANDROID
#  include "AndroidBuild.h"
#  include "GeneratedJNIWrappers.h"
#  include "mozilla/jni/Utils.h"
#endif

#ifdef XP_MACOSX
#  include <sys/sysctl.h>
#endif

#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
#  include "mozilla/SandboxInfo.h"
#endif

// Slot for NS_InitXPCOM to pass information to nsSystemInfo::Init.
// Only set to nonzero (potentially) if XP_UNIX.  On such systems, the
// system call to discover the appropriate value is not thread-safe,
// so we must call it before going multithreaded, but nsSystemInfo::Init
// only happens well after that point.
uint32_t nsSystemInfo::gUserUmask = 0;

using namespace mozilla::dom;

#if defined(XP_LINUX) && !defined(ANDROID)
static void SimpleParseKeyValuePairs(
    const std::string& aFilename,
    std::map<nsCString, nsCString>& aKeyValuePairs) {
  std::ifstream input(aFilename.c_str());
  for (std::string line; std::getline(input, line);) {
    nsAutoCString key, value;

    nsCCharSeparatedTokenizer tokens(nsDependentCString(line.c_str()), ':');
    if (tokens.hasMoreTokens()) {
      key = tokens.nextToken();
      if (tokens.hasMoreTokens()) {
        value = tokens.nextToken();
      }
      // We want the value even if there was just one token, to cover the
      // case where we had the key, and the value was blank (seems to be
      // a valid scenario some files.)
      aKeyValuePairs[key] = value;
    }
  }
}
#endif

#ifdef XP_WIN
// Lifted from media/webrtc/trunk/webrtc/base/systeminfo.cc,
// so keeping the _ instead of switching to camel case for now.
static void GetProcessorInformation(int* physical_cpus, int* cache_size_L2,
                                    int* cache_size_L3) {
  MOZ_ASSERT(physical_cpus && cache_size_L2 && cache_size_L3);

  *physical_cpus = 0;
  *cache_size_L2 = 0;  // This will be in kbytes
  *cache_size_L3 = 0;  // This will be in kbytes

  // Determine buffer size, allocate and get processor information.
  // Size can change between calls (unlikely), so a loop is done.
  SYSTEM_LOGICAL_PROCESSOR_INFORMATION info_buffer[32];
  SYSTEM_LOGICAL_PROCESSOR_INFORMATION* infos = &info_buffer[0];
  DWORD return_length = sizeof(info_buffer);
  while (!::GetLogicalProcessorInformation(infos, &return_length)) {
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER &&
        infos == &info_buffer[0]) {
      infos = new SYSTEM_LOGICAL_PROCESSOR_INFORMATION
          [return_length / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION)];
    } else {
      return;
    }
  }

  for (size_t i = 0;
       i < return_length / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION); ++i) {
    if (infos[i].Relationship == RelationProcessorCore) {
      ++*physical_cpus;
    } else if (infos[i].Relationship == RelationCache) {
      // Only care about L2 and L3 cache
      switch (infos[i].Cache.Level) {
        case 2:
          *cache_size_L2 = static_cast<int>(infos[i].Cache.Size / 1024);
          break;
        case 3:
          *cache_size_L3 = static_cast<int>(infos[i].Cache.Size / 1024);
          break;
        default:
          break;
      }
    }
  }
  if (infos != &info_buffer[0]) {
    delete[] infos;
  }
  return;
}
#endif

#if defined(XP_WIN)
namespace {
static nsresult GetFolderDiskInfo(nsIFile* file, FolderDiskInfo& info) {
  info.model.Truncate();
  info.revision.Truncate();
  info.isSSD = false;

  nsAutoString filePath;
  nsresult rv = file->GetPath(filePath);
  NS_ENSURE_SUCCESS(rv, rv);
  wchar_t volumeMountPoint[MAX_PATH] = {L'\\', L'\\', L'.', L'\\'};
  const size_t PREFIX_LEN = 4;
  if (!::GetVolumePathNameW(
          filePath.get(), volumeMountPoint + PREFIX_LEN,
          mozilla::ArrayLength(volumeMountPoint) - PREFIX_LEN)) {
    return NS_ERROR_UNEXPECTED;
  }
  size_t volumeMountPointLen = wcslen(volumeMountPoint);
  // Since we would like to open a drive and not a directory, we need to
  // remove any trailing backslash. A drive handle is valid for
  // DeviceIoControl calls, a directory handle is not.
  if (volumeMountPoint[volumeMountPointLen - 1] == L'\\') {
    volumeMountPoint[volumeMountPointLen - 1] = L'\0';
  }
  ScopedHandle handle(::CreateFileW(volumeMountPoint, 0,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                    OPEN_EXISTING, 0, nullptr));
  if (!handle.IsValid()) {
    return NS_ERROR_UNEXPECTED;
  }
  STORAGE_PROPERTY_QUERY queryParameters = {StorageDeviceProperty,
                                            PropertyStandardQuery};
  STORAGE_DEVICE_DESCRIPTOR outputHeader = {sizeof(STORAGE_DEVICE_DESCRIPTOR)};
  DWORD bytesRead = 0;
  if (!::DeviceIoControl(handle, IOCTL_STORAGE_QUERY_PROPERTY, &queryParameters,
                         sizeof(queryParameters), &outputHeader,
                         sizeof(outputHeader), &bytesRead, nullptr)) {
    return NS_ERROR_FAILURE;
  }
  PSTORAGE_DEVICE_DESCRIPTOR deviceOutput =
      (PSTORAGE_DEVICE_DESCRIPTOR)malloc(outputHeader.Size);
  if (!::DeviceIoControl(handle, IOCTL_STORAGE_QUERY_PROPERTY, &queryParameters,
                         sizeof(queryParameters), deviceOutput,
                         outputHeader.Size, &bytesRead, nullptr)) {
    free(deviceOutput);
    return NS_ERROR_FAILURE;
  }

  queryParameters.PropertyId = StorageDeviceTrimProperty;
  bytesRead = 0;
  bool isSSD = false;
  DEVICE_TRIM_DESCRIPTOR trimDescriptor = {sizeof(DEVICE_TRIM_DESCRIPTOR)};
  if (::DeviceIoControl(handle, IOCTL_STORAGE_QUERY_PROPERTY, &queryParameters,
                        sizeof(queryParameters), &trimDescriptor,
                        sizeof(trimDescriptor), &bytesRead, nullptr)) {
    if (trimDescriptor.TrimEnabled) {
      isSSD = true;
    }
  }

  if (isSSD) {
    // Get Seek Penalty
    queryParameters.PropertyId = StorageDeviceSeekPenaltyProperty;
    bytesRead = 0;
    DEVICE_SEEK_PENALTY_DESCRIPTOR seekPenaltyDescriptor = {
        sizeof(DEVICE_SEEK_PENALTY_DESCRIPTOR)};
    if (::DeviceIoControl(handle, IOCTL_STORAGE_QUERY_PROPERTY,
                          &queryParameters, sizeof(queryParameters),
                          &seekPenaltyDescriptor, sizeof(seekPenaltyDescriptor),
                          &bytesRead, nullptr)) {
      // It is possible that the disk has TrimEnabled, but also
      // IncursSeekPenalty; In this case, this is an HDD
      if (seekPenaltyDescriptor.IncursSeekPenalty) {
        isSSD = false;
      }
    }
  }

  // Some HDDs are including product ID info in the vendor field. Since PNP
  // IDs include vendor info and product ID concatenated together, we'll do
  // that here and interpret the result as a unique ID for the HDD model.
  if (deviceOutput->VendorIdOffset) {
    info.model =
        reinterpret_cast<char*>(deviceOutput) + deviceOutput->VendorIdOffset;
  }
  if (deviceOutput->ProductIdOffset) {
    info.model +=
        reinterpret_cast<char*>(deviceOutput) + deviceOutput->ProductIdOffset;
  }
  info.model.CompressWhitespace();
  if (deviceOutput->ProductRevisionOffset) {
    info.revision = reinterpret_cast<char*>(deviceOutput) +
                    deviceOutput->ProductRevisionOffset;
    info.revision.CompressWhitespace();
  }
  info.isSSD = isSSD;
  free(deviceOutput);
  return NS_OK;
}

static nsresult CollectDiskInfo(nsIFile* greDir, nsIFile* winDir,
                                nsIFile* profDir, DiskInfo& info) {
  nsresult rv = GetFolderDiskInfo(greDir, info.binary);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = GetFolderDiskInfo(winDir, info.system);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return GetFolderDiskInfo(profDir, info.profile);
}

static nsresult CollectOSInfo(OSInfo& info) {
  HKEY installYearHKey;
  LONG status = RegOpenKeyExW(
      HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0,
      KEY_READ | KEY_WOW64_64KEY, &installYearHKey);

  if (status != ERROR_SUCCESS) {
    return NS_ERROR_UNEXPECTED;
  }

  nsAutoRegKey installYearKey(installYearHKey);

  DWORD type = 0;
  time_t raw_time = 0;
  DWORD time_size = sizeof(time_t);

  status = RegQueryValueExW(installYearHKey, L"InstallDate", nullptr, &type,
                            (LPBYTE)&raw_time, &time_size);

  if (status != ERROR_SUCCESS) {
    return NS_ERROR_UNEXPECTED;
  }

  if (type != REG_DWORD) {
    return NS_ERROR_UNEXPECTED;
  }

  tm time;
  if (localtime_s(&time, &raw_time) != 0) {
    return NS_ERROR_UNEXPECTED;
  }

  info.installYear = 1900UL + time.tm_year;

  nsAutoServiceHandle scm(
      OpenSCManager(nullptr, SERVICES_ACTIVE_DATABASE, SC_MANAGER_CONNECT));

  if (!scm) {
    return NS_ERROR_UNEXPECTED;
  }

  bool superfetchServiceRunning = false;

  // Superfetch was introduced in Windows Vista as a service with the name
  // SysMain. The service display name was also renamed to SysMain after Windows
  // 10 build 1809.
  nsAutoServiceHandle hService(OpenService(scm, L"SysMain", GENERIC_READ));

  if (hService) {
    SERVICE_STATUS superfetchStatus;
    LPSERVICE_STATUS pSuperfetchStatus = &superfetchStatus;

    if (!QueryServiceStatus(hService, pSuperfetchStatus)) {
      return NS_ERROR_UNEXPECTED;
    }

    superfetchServiceRunning =
        superfetchStatus.dwCurrentState == SERVICE_RUNNING;
  }

  // If the SysMain (Superfetch) service is available, but not configured using
  // the defaults, then it's disabled for our purposes, since it's not going to
  // be operating as expected.
  bool superfetchUsingDefaultParams = true;
  bool prefetchUsingDefaultParams = true;

  static const WCHAR prefetchParamsKeyName[] =
      L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Memory "
      L"Management\\PrefetchParameters";
  static const DWORD SUPERFETCH_DEFAULT_PARAM = 3;
  static const DWORD PREFETCH_DEFAULT_PARAM = 3;

  HKEY prefetchParamsHKey;

  LONG prefetchParamsStatus =
      RegOpenKeyExW(HKEY_LOCAL_MACHINE, prefetchParamsKeyName, 0,
                    KEY_READ | KEY_WOW64_64KEY, &prefetchParamsHKey);

  if (prefetchParamsStatus == ERROR_SUCCESS) {
    DWORD valueSize = sizeof(DWORD);
    DWORD superfetchValue = 0;
    nsAutoRegKey prefetchParamsKey(prefetchParamsHKey);
    LONG superfetchParamStatus = RegQueryValueExW(
        prefetchParamsHKey, L"EnableSuperfetch", nullptr, &type,
        reinterpret_cast<LPBYTE>(&superfetchValue), &valueSize);

    // If the EnableSuperfetch registry key doesn't exist, then it's using the
    // default configuration.
    if (superfetchParamStatus == ERROR_SUCCESS &&
        superfetchValue != SUPERFETCH_DEFAULT_PARAM) {
      superfetchUsingDefaultParams = false;
    }

    DWORD prefetchValue = 0;

    LONG prefetchParamStatus = RegQueryValueExW(
        prefetchParamsHKey, L"EnablePrefetcher", nullptr, &type,
        reinterpret_cast<LPBYTE>(&prefetchValue), &valueSize);

    // If the EnablePrefetcher registry key doesn't exist, then we interpret
    // that as the Prefetcher being disabled (since Prefetch behaviour when
    // the key is not available appears to be undefined).
    if (prefetchParamStatus != ERROR_SUCCESS ||
        prefetchValue != PREFETCH_DEFAULT_PARAM) {
      prefetchUsingDefaultParams = false;
    }
  }

  info.hasSuperfetch = superfetchServiceRunning && superfetchUsingDefaultParams;
  info.hasPrefetch = prefetchUsingDefaultParams;

  return NS_OK;
}

nsresult CollectCountryCode(nsAString& aCountryCode) {
  GEOID geoid = GetUserGeoID(GEOCLASS_NATION);
  if (geoid == GEOID_NOT_AVAILABLE) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  // Get required length
  int numChars = GetGeoInfoW(geoid, GEO_ISO2, nullptr, 0, 0);
  if (!numChars) {
    return NS_ERROR_FAILURE;
  }
  // Now get the string for real
  aCountryCode.SetLength(numChars);
  numChars =
      GetGeoInfoW(geoid, GEO_ISO2, char16ptr_t(aCountryCode.BeginWriting()),
                  aCountryCode.Length(), 0);
  if (!numChars) {
    return NS_ERROR_FAILURE;
  }

  // numChars includes null terminator
  aCountryCode.Truncate(numChars - 1);
  return NS_OK;
}

}  // namespace

#  ifndef __MINGW32__

static HRESULT EnumWSCProductList(nsAString& aOutput,
                                  NotNull<IWSCProductList*> aProdList) {
  MOZ_ASSERT(aOutput.IsEmpty());

  LONG count;
  HRESULT hr = aProdList->get_Count(&count);
  if (FAILED(hr)) {
    return hr;
  }

  for (LONG index = 0; index < count; ++index) {
    RefPtr<IWscProduct> product;
    hr = aProdList->get_Item(index, getter_AddRefs(product));
    if (FAILED(hr)) {
      return hr;
    }

    WSC_SECURITY_PRODUCT_STATE state;
    hr = product->get_ProductState(&state);
    if (FAILED(hr)) {
      return hr;
    }

    // We only care about products that are active
    if (state == WSC_SECURITY_PRODUCT_STATE_OFF ||
        state == WSC_SECURITY_PRODUCT_STATE_SNOOZED) {
      continue;
    }

    _bstr_t bName;
    hr = product->get_ProductName(bName.GetAddress());
    if (FAILED(hr)) {
      return hr;
    }

    if (!aOutput.IsEmpty()) {
      aOutput.AppendLiteral(u";");
    }

    aOutput.Append((wchar_t*)bName, bName.length());
  }

  return S_OK;
}

static nsresult GetWindowsSecurityCenterInfo(nsAString& aAVInfo,
                                             nsAString& aAntiSpyInfo,
                                             nsAString& aFirewallInfo) {
  aAVInfo.Truncate();
  aAntiSpyInfo.Truncate();
  aFirewallInfo.Truncate();

  if (!XRE_IsParentProcess()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  const CLSID clsid = __uuidof(WSCProductList);
  const IID iid = __uuidof(IWSCProductList);

  // NB: A separate instance of IWSCProductList is needed for each distinct
  // security provider type; MSDN says that we cannot reuse the same object
  // and call Initialize() to pave over the previous data.

  WSC_SECURITY_PROVIDER providerTypes[] = {WSC_SECURITY_PROVIDER_ANTIVIRUS,
                                           WSC_SECURITY_PROVIDER_ANTISPYWARE,
                                           WSC_SECURITY_PROVIDER_FIREWALL};

  // Each output must match the corresponding entry in providerTypes.
  nsAString* outputs[] = {&aAVInfo, &aAntiSpyInfo, &aFirewallInfo};

  static_assert(ArrayLength(providerTypes) == ArrayLength(outputs),
                "Length of providerTypes and outputs arrays must match");

  for (uint32_t index = 0; index < ArrayLength(providerTypes); ++index) {
    RefPtr<IWSCProductList> prodList;
    HRESULT hr = ::CoCreateInstance(clsid, nullptr, CLSCTX_INPROC_SERVER, iid,
                                    getter_AddRefs(prodList));
    if (FAILED(hr)) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    hr = prodList->Initialize(providerTypes[index]);
    if (FAILED(hr)) {
      return NS_ERROR_UNEXPECTED;
    }

    hr = EnumWSCProductList(*outputs[index], WrapNotNull(prodList.get()));
    if (FAILED(hr)) {
      return NS_ERROR_UNEXPECTED;
    }
  }

  return NS_OK;
}

#  endif  // __MINGW32__

#endif  // defined(XP_WIN)

#ifdef XP_MACOSX
static nsresult GetAppleModelId(nsAutoCString& aModelId) {
  size_t numChars = 0;
  size_t result = sysctlbyname("hw.model", nullptr, &numChars, nullptr, 0);
  if (result != 0 || !numChars) {
    return NS_ERROR_FAILURE;
  }
  aModelId.SetLength(numChars);
  result =
      sysctlbyname("hw.model", aModelId.BeginWriting(), &numChars, nullptr, 0);
  if (result != 0) {
    return NS_ERROR_FAILURE;
  }
  // numChars includes null terminator
  aModelId.Truncate(numChars - 1);
  return NS_OK;
}
#endif

using namespace mozilla;

nsSystemInfo::nsSystemInfo() = default;

nsSystemInfo::~nsSystemInfo() = default;

// CPU-specific information.
static const struct PropItems {
  const char* name;
  bool (*propfun)(void);
} cpuPropItems[] = {
    // x86-specific bits.
    {"hasMMX", mozilla::supports_mmx},
    {"hasSSE", mozilla::supports_sse},
    {"hasSSE2", mozilla::supports_sse2},
    {"hasSSE3", mozilla::supports_sse3},
    {"hasSSSE3", mozilla::supports_ssse3},
    {"hasSSE4A", mozilla::supports_sse4a},
    {"hasSSE4_1", mozilla::supports_sse4_1},
    {"hasSSE4_2", mozilla::supports_sse4_2},
    {"hasAVX", mozilla::supports_avx},
    {"hasAVX2", mozilla::supports_avx2},
    {"hasAES", mozilla::supports_aes},
    // ARM-specific bits.
    {"hasEDSP", mozilla::supports_edsp},
    {"hasARMv6", mozilla::supports_armv6},
    {"hasARMv7", mozilla::supports_armv7},
    {"hasNEON", mozilla::supports_neon}};

nsresult CollectProcessInfo(ProcessInfo& info) {
  nsAutoCString cpuVendor;
  int cpuSpeed = -1;
  int cpuFamily = -1;
  int cpuModel = -1;
  int cpuStepping = -1;
  int logicalCPUs = -1;
  int physicalCPUs = -1;
  int cacheSizeL2 = -1;
  int cacheSizeL3 = -1;

#if defined(XP_WIN)
  // IsWow64Process2 is only available on Windows 10+, so we have to dynamically
  // check for its existence.
  typedef BOOL(WINAPI * LPFN_IWP2)(HANDLE, USHORT*, USHORT*);
  LPFN_IWP2 iwp2 = reinterpret_cast<LPFN_IWP2>(
      GetProcAddress(GetModuleHandle(L"kernel32"), "IsWow64Process2"));
  BOOL isWow64 = FALSE;
  USHORT processMachine = IMAGE_FILE_MACHINE_UNKNOWN;
  USHORT nativeMachine = IMAGE_FILE_MACHINE_UNKNOWN;
  BOOL gotWow64Value;
  if (iwp2) {
    gotWow64Value = iwp2(GetCurrentProcess(), &processMachine, &nativeMachine);
    if (gotWow64Value) {
      isWow64 = (processMachine != IMAGE_FILE_MACHINE_UNKNOWN);
    }
  } else {
    gotWow64Value = IsWow64Process(GetCurrentProcess(), &isWow64);
    // The function only indicates a WOW64 environment if it's 32-bit x86
    // running on x86-64, so emulate what IsWow64Process2 would have given.
    if (gotWow64Value && isWow64) {
      processMachine = IMAGE_FILE_MACHINE_I386;
      nativeMachine = IMAGE_FILE_MACHINE_AMD64;
    }
  }
  NS_WARNING_ASSERTION(gotWow64Value, "IsWow64Process failed");
  if (gotWow64Value) {
    // Set this always, even for the x86-on-arm64 case.
    info.isWow64 = !!isWow64;
    // Additional information if we're running x86-on-arm64
    info.isWowARM64 = (processMachine == IMAGE_FILE_MACHINE_I386 &&
                       nativeMachine == IMAGE_FILE_MACHINE_ARM64);
  }

  // CPU speed
  HKEY key;
  static const WCHAR keyName[] =
      L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0";

  if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, keyName, 0, KEY_QUERY_VALUE, &key) ==
      ERROR_SUCCESS) {
    DWORD data, len, vtype;
    len = sizeof(data);

    if (RegQueryValueEx(key, L"~Mhz", 0, 0, reinterpret_cast<LPBYTE>(&data),
                        &len) == ERROR_SUCCESS) {
      cpuSpeed = static_cast<int>(data);
    }

    // Limit to 64 double byte characters, should be plenty, but create
    // a buffer one larger as the result may not be null terminated. If
    // it is more than 64, we will not get the value.
    wchar_t cpuVendorStr[64 + 1];
    len = sizeof(cpuVendorStr) - 2;
    if (RegQueryValueExW(key, L"VendorIdentifier", 0, &vtype,
                         reinterpret_cast<LPBYTE>(cpuVendorStr),
                         &len) == ERROR_SUCCESS &&
        vtype == REG_SZ && len % 2 == 0 && len > 1) {
      cpuVendorStr[len / 2] = 0;  // In case it isn't null terminated
      CopyUTF16toUTF8(nsDependentString(cpuVendorStr), cpuVendor);
    }

    RegCloseKey(key);
  }

  // Other CPU attributes:
  SYSTEM_INFO si;
  GetNativeSystemInfo(&si);
  logicalCPUs = si.dwNumberOfProcessors;
  GetProcessorInformation(&physicalCPUs, &cacheSizeL2, &cacheSizeL3);
  if (physicalCPUs <= 0) {
    physicalCPUs = logicalCPUs;
  }
  cpuFamily = si.wProcessorLevel;
  cpuModel = si.wProcessorRevision >> 8;
  cpuStepping = si.wProcessorRevision & 0xFF;
#elif defined(XP_MACOSX)
  // CPU speed
  uint64_t sysctlValue64 = 0;
  uint32_t sysctlValue32 = 0;
  size_t len = 0;
  len = sizeof(sysctlValue64);
  if (!sysctlbyname("hw.cpufrequency_max", &sysctlValue64, &len, NULL, 0)) {
    cpuSpeed = static_cast<int>(sysctlValue64 / 1000000);
  }
  MOZ_ASSERT(sizeof(sysctlValue64) == len);

  len = sizeof(sysctlValue32);
  if (!sysctlbyname("hw.physicalcpu_max", &sysctlValue32, &len, NULL, 0)) {
    physicalCPUs = static_cast<int>(sysctlValue32);
  }
  MOZ_ASSERT(sizeof(sysctlValue32) == len);

  len = sizeof(sysctlValue32);
  if (!sysctlbyname("hw.logicalcpu_max", &sysctlValue32, &len, NULL, 0)) {
    logicalCPUs = static_cast<int>(sysctlValue32);
  }
  MOZ_ASSERT(sizeof(sysctlValue32) == len);

  len = sizeof(sysctlValue64);
  if (!sysctlbyname("hw.l2cachesize", &sysctlValue64, &len, NULL, 0)) {
    cacheSizeL2 = static_cast<int>(sysctlValue64 / 1024);
  }
  MOZ_ASSERT(sizeof(sysctlValue64) == len);

  len = sizeof(sysctlValue64);
  if (!sysctlbyname("hw.l3cachesize", &sysctlValue64, &len, NULL, 0)) {
    cacheSizeL3 = static_cast<int>(sysctlValue64 / 1024);
  }
  MOZ_ASSERT(sizeof(sysctlValue64) == len);

  if (!sysctlbyname("machdep.cpu.vendor", NULL, &len, NULL, 0)) {
    char* cpuVendorStr = new char[len];
    if (!sysctlbyname("machdep.cpu.vendor", cpuVendorStr, &len, NULL, 0)) {
      cpuVendor = cpuVendorStr;
    }
    delete[] cpuVendorStr;
  }

  len = sizeof(sysctlValue32);
  if (!sysctlbyname("machdep.cpu.family", &sysctlValue32, &len, NULL, 0)) {
    cpuFamily = static_cast<int>(sysctlValue32);
  }
  MOZ_ASSERT(sizeof(sysctlValue32) == len);

  len = sizeof(sysctlValue32);
  if (!sysctlbyname("machdep.cpu.model", &sysctlValue32, &len, NULL, 0)) {
    cpuModel = static_cast<int>(sysctlValue32);
  }
  MOZ_ASSERT(sizeof(sysctlValue32) == len);

  len = sizeof(sysctlValue32);
  if (!sysctlbyname("machdep.cpu.stepping", &sysctlValue32, &len, NULL, 0)) {
    cpuStepping = static_cast<int>(sysctlValue32);
  }
  MOZ_ASSERT(sizeof(sysctlValue32) == len);

#elif defined(XP_LINUX) && !defined(ANDROID)
  // Get vendor, family, model, stepping, physical cores
  // from /proc/cpuinfo file
  {
    std::map<nsCString, nsCString> keyValuePairs;
    SimpleParseKeyValuePairs("/proc/cpuinfo", keyValuePairs);

    // cpuVendor from "vendor_id"
    info.cpuVendor.Assign(keyValuePairs[NS_LITERAL_CSTRING("vendor_id")]);

    {
      // cpuFamily from "cpu family"
      Tokenizer::Token t;
      Tokenizer p(keyValuePairs[NS_LITERAL_CSTRING("cpu family")]);
      if (p.Next(t) && t.Type() == Tokenizer::TOKEN_INTEGER &&
          t.AsInteger() <= INT32_MAX) {
        cpuFamily = static_cast<int>(t.AsInteger());
      }
    }

    {
      // cpuModel from "model"
      Tokenizer::Token t;
      Tokenizer p(keyValuePairs[NS_LITERAL_CSTRING("model")]);
      if (p.Next(t) && t.Type() == Tokenizer::TOKEN_INTEGER &&
          t.AsInteger() <= INT32_MAX) {
        cpuModel = static_cast<int>(t.AsInteger());
      }
    }

    {
      // cpuStepping from "stepping"
      Tokenizer::Token t;
      Tokenizer p(keyValuePairs[NS_LITERAL_CSTRING("stepping")]);
      if (p.Next(t) && t.Type() == Tokenizer::TOKEN_INTEGER &&
          t.AsInteger() <= INT32_MAX) {
        cpuStepping = static_cast<int>(t.AsInteger());
      }
    }

    {
      // physicalCPUs from "cpu cores"
      Tokenizer::Token t;
      Tokenizer p(keyValuePairs[NS_LITERAL_CSTRING("cpu cores")]);
      if (p.Next(t) && t.Type() == Tokenizer::TOKEN_INTEGER &&
          t.AsInteger() <= INT32_MAX) {
        physicalCPUs = static_cast<int>(t.AsInteger());
      }
    }
  }

  {
    // Get cpuSpeed from another file.
    std::ifstream input(
        "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq");
    std::string line;
    if (getline(input, line)) {
      Tokenizer::Token t;
      Tokenizer p(line.c_str());
      if (p.Next(t) && t.Type() == Tokenizer::TOKEN_INTEGER &&
          t.AsInteger() <= INT32_MAX) {
        cpuSpeed = static_cast<int>(t.AsInteger() / 1000);
      }
    }
  }

  {
    // Get cacheSizeL2 from yet another file
    std::ifstream input("/sys/devices/system/cpu/cpu0/cache/index2/size");
    std::string line;
    if (getline(input, line)) {
      Tokenizer::Token t;
      Tokenizer p(line.c_str(), nullptr, "K");
      if (p.Next(t) && t.Type() == Tokenizer::TOKEN_INTEGER &&
          t.AsInteger() <= INT32_MAX) {
        cacheSizeL2 = static_cast<int>(t.AsInteger());
      }
    }
  }

  {
    // Get cacheSizeL3 from yet another file
    std::ifstream input("/sys/devices/system/cpu/cpu0/cache/index3/size");
    std::string line;
    if (getline(input, line)) {
      Tokenizer::Token t;
      Tokenizer p(line.c_str(), nullptr, "K");
      if (p.Next(t) && t.Type() == Tokenizer::TOKEN_INTEGER &&
          t.AsInteger() <= INT32_MAX) {
        cacheSizeL3 = static_cast<int>(t.AsInteger());
      }
    }
  }

  info.cpuCount = PR_GetNumberOfProcessors();
#else
  info.cpuCount = PR_GetNumberOfProcessors();
#endif

  if (cpuSpeed >= 0) {
    info.cpuSpeed = cpuSpeed;
  } else {
    info.cpuSpeed = 0;
  }
  if (!cpuVendor.IsEmpty()) {
    info.cpuVendor = cpuVendor;
  }
  if (cpuFamily >= 0) {
    info.cpuFamily = cpuFamily;
  }
  if (cpuModel >= 0) {
    info.cpuModel = cpuModel;
  }
  if (cpuStepping >= 0) {
    info.cpuStepping = cpuStepping;
  }

  if (logicalCPUs >= 0) {
    info.cpuCount = logicalCPUs;
  }
  if (physicalCPUs >= 0) {
    info.cpuCores = physicalCPUs;
  }

  if (cacheSizeL2 >= 0) {
    info.l2cacheKB = cacheSizeL2;
  }
  if (cacheSizeL3 >= 0) {
    info.l3cacheKB = cacheSizeL3;
  }

  return NS_OK;
}

nsresult nsSystemInfo::Init() {
  // check that it is called from the main thread on all platforms.
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv;

  static const struct {
    PRSysInfo cmd;
    const char* name;
  } items[] = {{PR_SI_SYSNAME, "name"},
               {PR_SI_ARCHITECTURE, "arch"},
               {PR_SI_RELEASE, "version"}};

  for (uint32_t i = 0; i < (sizeof(items) / sizeof(items[0])); i++) {
    char buf[SYS_INFO_BUFFER_LENGTH];
    if (PR_GetSystemInfo(items[i].cmd, buf, sizeof(buf)) == PR_SUCCESS) {
      rv = SetPropertyAsACString(NS_ConvertASCIItoUTF16(items[i].name),
                                 nsDependentCString(buf));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else {
      NS_WARNING("PR_GetSystemInfo failed");
    }
  }

  rv = SetPropertyAsBool(NS_ConvertASCIItoUTF16("hasWindowsTouchInterface"),
                         false);
  NS_ENSURE_SUCCESS(rv, rv);

  // Additional informations not available through PR_GetSystemInfo.
  SetInt32Property(NS_LITERAL_STRING("pagesize"), PR_GetPageSize());
  SetInt32Property(NS_LITERAL_STRING("pageshift"), PR_GetPageShift());
  SetInt32Property(NS_LITERAL_STRING("memmapalign"), PR_GetMemMapAlignment());
  SetUint64Property(NS_LITERAL_STRING("memsize"), PR_GetPhysicalMemorySize());
  SetUint32Property(NS_LITERAL_STRING("umask"), nsSystemInfo::gUserUmask);

  uint64_t virtualMem = 0;

#if defined(XP_WIN)
  // Virtual memory:
  MEMORYSTATUSEX memStat;
  memStat.dwLength = sizeof(memStat);
  if (GlobalMemoryStatusEx(&memStat)) {
    virtualMem = memStat.ullTotalVirtual;
  }
#endif
  if (virtualMem)
    SetUint64Property(NS_LITERAL_STRING("virtualmemsize"), virtualMem);

  for (uint32_t i = 0; i < ArrayLength(cpuPropItems); i++) {
    rv = SetPropertyAsBool(NS_ConvertASCIItoUTF16(cpuPropItems[i].name),
                           cpuPropItems[i].propfun());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

#ifdef XP_WIN
  bool isMinGW =
#  ifdef __MINGW32__
      true;
#  else
      false;
#  endif
  rv = SetPropertyAsBool(NS_LITERAL_STRING("isMinGW"), !!isMinGW);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

#  ifndef __MINGW32__
  nsAutoString avInfo, antiSpyInfo, firewallInfo;
  if (NS_SUCCEEDED(
          GetWindowsSecurityCenterInfo(avInfo, antiSpyInfo, firewallInfo))) {
    if (!avInfo.IsEmpty()) {
      rv = SetPropertyAsAString(NS_LITERAL_STRING("registeredAntiVirus"),
                                avInfo);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    if (!antiSpyInfo.IsEmpty()) {
      rv = SetPropertyAsAString(NS_LITERAL_STRING("registeredAntiSpyware"),
                                antiSpyInfo);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    if (!firewallInfo.IsEmpty()) {
      rv = SetPropertyAsAString(NS_LITERAL_STRING("registeredFirewall"),
                                firewallInfo);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }
#  endif  // __MINGW32__
#endif

#if defined(XP_MACOSX)
  nsAutoCString modelId;
  if (NS_SUCCEEDED(GetAppleModelId(modelId))) {
    rv = SetPropertyAsACString(NS_LITERAL_STRING("appleModelId"), modelId);
    NS_ENSURE_SUCCESS(rv, rv);
  }
#endif

#if defined(MOZ_WIDGET_GTK)
  // This must be done here because NSPR can only separate OS's when compiled,
  // not libraries. 64 bytes is going to be well enough for "GTK " followed by 3
  // integers separated with dots.
  char gtkver[64];
  ssize_t gtkver_len = 0;

  if (gtkver_len <= 0) {
    gtkver_len = SprintfLiteral(gtkver, "GTK %u.%u.%u", gtk_major_version,
                                gtk_minor_version, gtk_micro_version);
  }

  nsAutoCString secondaryLibrary;
  if (gtkver_len > 0 && gtkver_len < int(sizeof(gtkver))) {
    secondaryLibrary.Append(nsDependentCSubstring(gtkver, gtkver_len));
  }

#  ifndef MOZ_TSAN
  // With TSan, avoid loading libpulse here because we cannot unload it
  // afterwards due to restrictions from TSan about unloading libraries
  // matched by the suppression list.
  void* libpulse = dlopen("libpulse.so.0", RTLD_LAZY);
  const char* libpulseVersion = "not-available";
  if (libpulse) {
    auto pa_get_library_version = reinterpret_cast<const char* (*)()>(
        dlsym(libpulse, "pa_get_library_version"));

    if (pa_get_library_version) {
      libpulseVersion = pa_get_library_version();
    }
  }

  secondaryLibrary.AppendPrintf(",libpulse %s", libpulseVersion);

  if (libpulse) {
    dlclose(libpulse);
  }
#  endif

  rv = SetPropertyAsACString(NS_LITERAL_STRING("secondaryLibrary"),
                             secondaryLibrary);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
#endif

#ifdef MOZ_WIDGET_ANDROID
  AndroidSystemInfo info;
  GetAndroidSystemInfo(&info);
  SetupAndroidInfo(info);
#endif

#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
  SandboxInfo sandInfo = SandboxInfo::Get();

  SetPropertyAsBool(NS_LITERAL_STRING("hasSeccompBPF"),
                    sandInfo.Test(SandboxInfo::kHasSeccompBPF));
  SetPropertyAsBool(NS_LITERAL_STRING("hasSeccompTSync"),
                    sandInfo.Test(SandboxInfo::kHasSeccompTSync));
  SetPropertyAsBool(NS_LITERAL_STRING("hasUserNamespaces"),
                    sandInfo.Test(SandboxInfo::kHasUserNamespaces));
  SetPropertyAsBool(NS_LITERAL_STRING("hasPrivilegedUserNamespaces"),
                    sandInfo.Test(SandboxInfo::kHasPrivilegedUserNamespaces));

  if (sandInfo.Test(SandboxInfo::kEnabledForContent)) {
    SetPropertyAsBool(NS_LITERAL_STRING("canSandboxContent"),
                      sandInfo.CanSandboxContent());
  }

  if (sandInfo.Test(SandboxInfo::kEnabledForMedia)) {
    SetPropertyAsBool(NS_LITERAL_STRING("canSandboxMedia"),
                      sandInfo.CanSandboxMedia());
  }
#endif  // XP_LINUX && MOZ_SANDBOX

  return NS_OK;
}

#ifdef MOZ_WIDGET_ANDROID
// Prerelease versions of Android use a letter instead of version numbers.
// Unfortunately this breaks websites due to the user agent.
// Chrome works around this by hardcoding an Android version when a
// numeric version can't be obtained. We're doing the same.
// This version will need to be updated whenever there is a new official
// Android release. Search for "kDefaultAndroidMajorVersion" in:
// https://source.chromium.org/chromium/chromium/src/+/master:base/system/sys_info_android.cc
#  define DEFAULT_ANDROID_VERSION "10.0.99"

/* static */
void nsSystemInfo::GetAndroidSystemInfo(AndroidSystemInfo* aInfo) {
  if (!jni::IsAvailable()) {
    // called from xpcshell etc.
    aInfo->sdk_version() = 0;
    return;
  }

  jni::String::LocalRef model = java::sdk::Build::MODEL();
  aInfo->device() = model->ToString();

  jni::String::LocalRef manufacturer =
      mozilla::java::sdk::Build::MANUFACTURER();
  aInfo->manufacturer() = manufacturer->ToString();

  jni::String::LocalRef hardware = java::sdk::Build::HARDWARE();
  aInfo->hardware() = hardware->ToString();

  jni::String::LocalRef release = java::sdk::VERSION::RELEASE();
  nsString str(release->ToString());
  int major_version;
  int minor_version;
  int bugfix_version;
  int num_read = sscanf(NS_ConvertUTF16toUTF8(str).get(), "%d.%d.%d",
                        &major_version, &minor_version, &bugfix_version);
  if (num_read == 0) {
    aInfo->release_version() = NS_LITERAL_STRING(DEFAULT_ANDROID_VERSION);
  } else {
    aInfo->release_version() = str;
  }

  aInfo->sdk_version() = jni::GetAPIVersion();
  aInfo->isTablet() = java::GeckoAppShell::IsTablet();
}

void nsSystemInfo::SetupAndroidInfo(const AndroidSystemInfo& aInfo) {
  if (!aInfo.device().IsEmpty()) {
    SetPropertyAsAString(NS_LITERAL_STRING("device"), aInfo.device());
  }
  if (!aInfo.manufacturer().IsEmpty()) {
    SetPropertyAsAString(NS_LITERAL_STRING("manufacturer"),
                         aInfo.manufacturer());
  }
  if (!aInfo.release_version().IsEmpty()) {
    SetPropertyAsAString(NS_LITERAL_STRING("release_version"),
                         aInfo.release_version());
  }
  SetPropertyAsBool(NS_LITERAL_STRING("tablet"), aInfo.isTablet());
  // NSPR "version" is the kernel version. For Android we want the Android
  // version. Rename SDK version to version and put the kernel version into
  // kernel_version.
  nsAutoString str;
  nsresult rv = GetPropertyAsAString(NS_LITERAL_STRING("version"), str);
  if (NS_SUCCEEDED(rv)) {
    SetPropertyAsAString(NS_LITERAL_STRING("kernel_version"), str);
  }
  // When JNI is not available (eg. in xpcshell tests), sdk_version is 0.
  if (aInfo.sdk_version() != 0) {
    if (!aInfo.hardware().IsEmpty()) {
      SetPropertyAsAString(NS_LITERAL_STRING("hardware"), aInfo.hardware());
    }
    SetPropertyAsInt32(NS_LITERAL_STRING("version"), aInfo.sdk_version());
  }
}
#endif  // MOZ_WIDGET_ANDROID

void nsSystemInfo::SetInt32Property(const nsAString& aPropertyName,
                                    const int32_t aValue) {
  NS_WARNING_ASSERTION(aValue > 0, "Unable to read system value");
  if (aValue > 0) {
#ifdef DEBUG
    nsresult rv =
#endif
        SetPropertyAsInt32(aPropertyName, aValue);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Unable to set property");
  }
}

void nsSystemInfo::SetUint32Property(const nsAString& aPropertyName,
                                     const uint32_t aValue) {
  // Only one property is currently set via this function.
  // It may legitimately be zero.
#ifdef DEBUG
  nsresult rv =
#endif
      SetPropertyAsUint32(aPropertyName, aValue);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Unable to set property");
}

void nsSystemInfo::SetUint64Property(const nsAString& aPropertyName,
                                     const uint64_t aValue) {
  NS_WARNING_ASSERTION(aValue > 0, "Unable to read system value");
  if (aValue > 0) {
#ifdef DEBUG
    nsresult rv =
#endif
        SetPropertyAsUint64(aPropertyName, aValue);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Unable to set property");
  }
}

#ifdef XP_WIN

static bool GetJSObjForDiskInfo(JSContext* aCx, JS::Handle<JSObject*> aParent,
                                const FolderDiskInfo& info,
                                const char* propName) {
  JS::Rooted<JSObject*> jsInfo(aCx, JS_NewPlainObject(aCx));
  if (!jsInfo) {
    return false;
  }

  JSString* strModel =
      JS_NewStringCopyN(aCx, info.model.get(), info.model.Length());
  if (!strModel) {
    return false;
  }
  JS::Rooted<JS::Value> valModel(aCx, JS::StringValue(strModel));
  if (!JS_SetProperty(aCx, jsInfo, "model", valModel)) {
    return false;
  }

  JSString* strRevision =
      JS_NewStringCopyN(aCx, info.revision.get(), info.revision.Length());
  if (!strRevision) {
    return false;
  }
  JS::Rooted<JS::Value> valRevision(aCx, JS::StringValue(strRevision));
  if (!JS_SetProperty(aCx, jsInfo, "revision", valRevision)) {
    return false;
  }

  JSString* strSSD = JS_NewStringCopyZ(aCx, info.isSSD ? "SSD" : "HDD");
  if (!strSSD) {
    return false;
  }
  JS::Rooted<JS::Value> valSSD(aCx, JS::StringValue(strSSD));
  if (!JS_SetProperty(aCx, jsInfo, "type", valSSD)) {
    return false;
  }

  JS::Rooted<JS::Value> val(aCx, JS::ObjectValue(*jsInfo));
  return JS_SetProperty(aCx, aParent, propName, val);
}

JSObject* GetJSObjForOSInfo(JSContext* aCx, const OSInfo& info) {
  JS::Rooted<JSObject*> jsInfo(aCx, JS_NewPlainObject(aCx));

  JS::Rooted<JS::Value> valInstallYear(aCx, JS::Int32Value(info.installYear));
  JS_SetProperty(aCx, jsInfo, "installYear", valInstallYear);

  JS::Rooted<JS::Value> valHasSuperfetch(aCx,
                                         JS::BooleanValue(info.hasSuperfetch));
  JS_SetProperty(aCx, jsInfo, "hasSuperfetch", valHasSuperfetch);

  JS::Rooted<JS::Value> valHasPrefetch(aCx, JS::BooleanValue(info.hasPrefetch));
  JS_SetProperty(aCx, jsInfo, "hasPrefetch", valHasPrefetch);

  return jsInfo;
}

#endif

JSObject* GetJSObjForProcessInfo(JSContext* aCx, const ProcessInfo& info) {
  JS::Rooted<JSObject*> jsInfo(aCx, JS_NewPlainObject(aCx));

#if defined(XP_WIN)
  JS::Rooted<JS::Value> valisWow64(aCx, JS::BooleanValue(info.isWow64));
  JS_SetProperty(aCx, jsInfo, "isWow64", valisWow64);

  JS::Rooted<JS::Value> valisWowARM64(aCx, JS::BooleanValue(info.isWowARM64));
  JS_SetProperty(aCx, jsInfo, "isWowARM64", valisWowARM64);
#endif

  JS::Rooted<JS::Value> valCountInfo(aCx, JS::Int32Value(info.cpuCount));
  JS_SetProperty(aCx, jsInfo, "count", valCountInfo);

  JS::Rooted<JS::Value> valCoreInfo(aCx, JS::Int32Value(info.cpuCores));
  JS_SetProperty(aCx, jsInfo, "cores", valCoreInfo);

  JSString* strVendor =
      JS_NewStringCopyN(aCx, info.cpuVendor.get(), info.cpuVendor.Length());
  JS::Rooted<JS::Value> valVendor(aCx, JS::StringValue(strVendor));
  JS_SetProperty(aCx, jsInfo, "vendor", valVendor);

  JS::Rooted<JS::Value> valFamilyInfo(aCx, JS::Int32Value(info.cpuFamily));
  JS_SetProperty(aCx, jsInfo, "family", valFamilyInfo);

  JS::Rooted<JS::Value> valModelInfo(aCx, JS::Int32Value(info.cpuModel));
  JS_SetProperty(aCx, jsInfo, "model", valModelInfo);

  JS::Rooted<JS::Value> valSteppingInfo(aCx, JS::Int32Value(info.cpuStepping));
  JS_SetProperty(aCx, jsInfo, "stepping", valSteppingInfo);

  JS::Rooted<JS::Value> valL2CacheInfo(aCx, JS::Int32Value(info.l2cacheKB));
  JS_SetProperty(aCx, jsInfo, "l2cacheKB", valL2CacheInfo);

  JS::Rooted<JS::Value> valL3CacheInfo(aCx, JS::Int32Value(info.l3cacheKB));
  JS_SetProperty(aCx, jsInfo, "l3cacheKB", valL3CacheInfo);

  JS::Rooted<JS::Value> valSpeedInfo(aCx, JS::Int32Value(info.cpuSpeed));
  JS_SetProperty(aCx, jsInfo, "speedMHz", valSpeedInfo);

  return jsInfo;
}

RefPtr<nsISerialEventTarget> nsSystemInfo::GetBackgroundTarget() {
  if (!mBackgroundET) {
    MOZ_ALWAYS_SUCCEEDS(NS_CreateBackgroundTaskQueue(
        "SystemInfoThread", getter_AddRefs(mBackgroundET)));
  }
  return mBackgroundET;
}

NS_IMETHODIMP
nsSystemInfo::GetOsInfo(JSContext* aCx, Promise** aResult) {
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = nullptr;
  if (!XRE_IsParentProcess()) {
    return NS_ERROR_FAILURE;
  }
#if defined(XP_WIN)
  nsIGlobalObject* global = xpc::CurrentNativeGlobal(aCx);
  if (NS_WARN_IF(!global)) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult erv;
  RefPtr<Promise> promise = Promise::Create(global, erv);
  if (NS_WARN_IF(erv.Failed())) {
    return erv.StealNSResult();
  }

  if (!mOSInfoPromise) {
    RefPtr<nsISerialEventTarget> backgroundET = GetBackgroundTarget();

    mOSInfoPromise = InvokeAsync(backgroundET, __func__, []() {
      OSInfo info;
      nsresult rv = CollectOSInfo(info);
      if (NS_SUCCEEDED(rv)) {
        return OSInfoPromise::CreateAndResolve(info, __func__);
      }
      return OSInfoPromise::CreateAndReject(rv, __func__);
    });
  };

  // Chain the new promise to the extant mozpromise
  RefPtr<Promise> capturedPromise = promise;
  mOSInfoPromise->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [capturedPromise](const OSInfo& info) {
        AutoJSAPI jsapi;
        if (NS_WARN_IF(!jsapi.Init(capturedPromise->GetGlobalObject()))) {
          capturedPromise->MaybeReject(NS_ERROR_UNEXPECTED);
          return;
        }
        JSContext* cx = jsapi.cx();
        JS::Rooted<JS::Value> val(
            cx, JS::ObjectValue(*GetJSObjForOSInfo(cx, info)));
        capturedPromise->MaybeResolve(val);
      },
      [capturedPromise](const nsresult rv) {
        // Resolve with null when installYear is not available from the system
        capturedPromise->MaybeResolve(JS::NullHandleValue);
      });

  promise.forget(aResult);
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsSystemInfo::GetDiskInfo(JSContext* aCx, Promise** aResult) {
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = nullptr;
  if (!XRE_IsParentProcess()) {
    return NS_ERROR_FAILURE;
  }
#ifdef XP_WIN
  nsIGlobalObject* global = xpc::CurrentNativeGlobal(aCx);
  if (NS_WARN_IF(!global)) {
    return NS_ERROR_FAILURE;
  }
  ErrorResult erv;
  RefPtr<Promise> promise = Promise::Create(global, erv);
  if (NS_WARN_IF(erv.Failed())) {
    return erv.StealNSResult();
  }

  if (!mDiskInfoPromise) {
    RefPtr<nsISerialEventTarget> backgroundET = GetBackgroundTarget();
    nsCOMPtr<nsIFile> greDir;
    nsCOMPtr<nsIFile> winDir;
    nsCOMPtr<nsIFile> profDir;
    nsresult rv = NS_GetSpecialDirectory(NS_GRE_DIR, getter_AddRefs(greDir));
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = NS_GetSpecialDirectory(NS_WIN_WINDOWS_DIR, getter_AddRefs(winDir));
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                getter_AddRefs(profDir));
    if (NS_FAILED(rv)) {
      return rv;
    }

    mDiskInfoPromise =
        InvokeAsync(backgroundET, __func__, [greDir, winDir, profDir]() {
          DiskInfo info;
          nsresult rv = CollectDiskInfo(greDir, winDir, profDir, info);
          if (NS_SUCCEEDED(rv)) {
            return DiskInfoPromise::CreateAndResolve(info, __func__);
          }
          return DiskInfoPromise::CreateAndReject(rv, __func__);
        });
  }

  // Chain the new promise to the extant mozpromise.
  RefPtr<Promise> capturedPromise = promise;
  mDiskInfoPromise->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [capturedPromise](const DiskInfo& info) {
        AutoJSAPI jsapi;
        if (NS_WARN_IF(!jsapi.Init(capturedPromise->GetGlobalObject()))) {
          capturedPromise->MaybeReject(NS_ERROR_UNEXPECTED);
          return;
        }
        JSContext* cx = jsapi.cx();
        JS::Rooted<JSObject*> jsInfo(cx, JS_NewPlainObject(cx));
        // Store data in the rv:
        bool succeededSettingAllObjects =
            jsInfo && GetJSObjForDiskInfo(cx, jsInfo, info.binary, "binary") &&
            GetJSObjForDiskInfo(cx, jsInfo, info.profile, "profile") &&
            GetJSObjForDiskInfo(cx, jsInfo, info.system, "system");
        // The above can fail due to OOM
        if (!succeededSettingAllObjects) {
          JS_ClearPendingException(cx);
          capturedPromise->MaybeReject(NS_ERROR_FAILURE);
          return;
        }

        JS::Rooted<JS::Value> val(cx, JS::ObjectValue(*jsInfo));
        capturedPromise->MaybeResolve(val);
      },
      [capturedPromise](const nsresult rv) {
        capturedPromise->MaybeReject(rv);
      });

  promise.forget(aResult);
#endif
  return NS_OK;
}

NS_IMPL_ISUPPORTS_INHERITED(nsSystemInfo, nsHashPropertyBag, nsISystemInfo)

NS_IMETHODIMP
nsSystemInfo::GetCountryCode(JSContext* aCx, Promise** aResult) {
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = nullptr;

  if (!XRE_IsParentProcess()) {
    return NS_ERROR_FAILURE;
  }
#if defined(XP_MACOSX) || defined(XP_WIN)
  nsIGlobalObject* global = xpc::CurrentNativeGlobal(aCx);
  if (NS_WARN_IF(!global)) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult erv;
  RefPtr<Promise> promise = Promise::Create(global, erv);
  if (NS_WARN_IF(erv.Failed())) {
    return erv.StealNSResult();
  }

  if (!mCountryCodePromise) {
    RefPtr<nsISerialEventTarget> backgroundET = GetBackgroundTarget();

    mCountryCodePromise = InvokeAsync(backgroundET, __func__, []() {
      nsAutoString countryCode;
#  ifdef XP_MACOSX
      nsresult rv = GetSelectedCityInfo(countryCode);
#  endif
#  ifdef XP_WIN
      nsresult rv = CollectCountryCode(countryCode);
#  endif

      if (NS_SUCCEEDED(rv)) {
        return CountryCodePromise::CreateAndResolve(countryCode, __func__);
      }
      return CountryCodePromise::CreateAndReject(rv, __func__);
    });
  }

  RefPtr<Promise> capturedPromise = promise;
  mCountryCodePromise->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [capturedPromise](const nsString& countryCode) {
        AutoJSAPI jsapi;
        if (NS_WARN_IF(!jsapi.Init(capturedPromise->GetGlobalObject()))) {
          capturedPromise->MaybeReject(NS_ERROR_UNEXPECTED);
          return;
        }
        JSContext* cx = jsapi.cx();
        JS::Rooted<JSString*> jsCountryCode(
            cx, JS_NewUCStringCopyZ(cx, countryCode.get()));

        JS::Rooted<JS::Value> val(cx, JS::StringValue(jsCountryCode));
        capturedPromise->MaybeResolve(val);
      },
      [capturedPromise](const nsresult rv) {
        // Resolve with null when countryCode is not available from the system
        capturedPromise->MaybeResolve(JS::NullHandleValue);
      });

  promise.forget(aResult);
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsSystemInfo::GetProcessInfo(JSContext* aCx, Promise** aResult) {
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = nullptr;

  if (!XRE_IsParentProcess()) {
    return NS_ERROR_FAILURE;
  }

  nsIGlobalObject* global = xpc::CurrentNativeGlobal(aCx);
  if (NS_WARN_IF(!global)) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult erv;
  RefPtr<Promise> promise = Promise::Create(global, erv);
  if (NS_WARN_IF(erv.Failed())) {
    return erv.StealNSResult();
  }

  if (!mProcessInfoPromise) {
    RefPtr<nsISerialEventTarget> backgroundET = GetBackgroundTarget();

    mProcessInfoPromise = InvokeAsync(backgroundET, __func__, []() {
      ProcessInfo info;
      nsresult rv = CollectProcessInfo(info);
      if (NS_SUCCEEDED(rv)) {
        return ProcessInfoPromise::CreateAndResolve(info, __func__);
      }
      return ProcessInfoPromise::CreateAndReject(rv, __func__);
    });
  };

  // Chain the new promise to the extant mozpromise
  RefPtr<Promise> capturedPromise = promise;
  mProcessInfoPromise->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [capturedPromise](const ProcessInfo& info) {
        AutoJSAPI jsapi;
        if (NS_WARN_IF(!jsapi.Init(capturedPromise->GetGlobalObject()))) {
          capturedPromise->MaybeReject(NS_ERROR_UNEXPECTED);
          return;
        }
        JSContext* cx = jsapi.cx();
        JS::Rooted<JS::Value> val(
            cx, JS::ObjectValue(*GetJSObjForProcessInfo(cx, info)));
        capturedPromise->MaybeResolve(val);
      },
      [capturedPromise](const nsresult rv) {
        // Resolve with null when installYear is not available from the system
        capturedPromise->MaybeResolve(JS::NullHandleValue);
      });

  promise.forget(aResult);

  return NS_OK;
}
