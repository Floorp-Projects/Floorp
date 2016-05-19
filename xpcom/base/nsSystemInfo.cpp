/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "nsSystemInfo.h"
#include "prsystem.h"
#include "prio.h"
#include "prprf.h"
#include "mozilla/SSE.h"
#include "mozilla/arm.h"

#ifdef XP_WIN
#include <time.h>
#include <windows.h>
#include <winioctl.h>
#include "base/scoped_handle_win.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsIObserverService.h"
#include "nsWindowsHelpers.h"
#endif

#ifdef XP_MACOSX
#include "MacHelpers.h"
#endif

#ifdef MOZ_WIDGET_GTK
#include <gtk/gtk.h>
#include <dlfcn.h>
#include <unistd.h>
#include <fstream>
#include "mozilla/Tokenizer.h"
#include "nsCharSeparatedTokenizer.h"

#include <map>
#include <string>
#endif

#ifdef MOZ_WIDGET_ANDROID
#include "AndroidBridge.h"
#include "mozilla/dom/ContentChild.h"
#endif

#ifdef MOZ_WIDGET_GONK
#include <sys/system_properties.h>
#include "mozilla/Preferences.h"
#include "nsPrintfCString.h"
#endif

#ifdef ANDROID
extern "C" {
NS_EXPORT int android_sdk_version;
}
#endif

#ifdef XP_MACOSX
#include <sys/sysctl.h>
#endif

#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
#include "mozilla/SandboxInfo.h"
#endif

// Slot for NS_InitXPCOM2 to pass information to nsSystemInfo::Init.
// Only set to nonzero (potentially) if XP_UNIX.  On such systems, the
// system call to discover the appropriate value is not thread-safe,
// so we must call it before going multithreaded, but nsSystemInfo::Init
// only happens well after that point.
uint32_t nsSystemInfo::gUserUmask = 0;

#if defined (MOZ_WIDGET_GTK)
static void
SimpleParseKeyValuePairs(const std::string& aFilename,
                         std::map<nsCString, nsCString>& aKeyValuePairs)
{
  std::ifstream input(aFilename.c_str());
  for (std::string line; std::getline(input, line); ) {
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

#if defined(XP_WIN)
namespace {
nsresult
GetHDDInfo(const char* aSpecialDirName, nsAutoCString& aModel,
           nsAutoCString& aRevision)
{
  aModel.Truncate();
  aRevision.Truncate();

  nsCOMPtr<nsIFile> profDir;
  nsresult rv = NS_GetSpecialDirectory(aSpecialDirName,
                                       getter_AddRefs(profDir));
  NS_ENSURE_SUCCESS(rv, rv);
  nsAutoString profDirPath;
  rv = profDir->GetPath(profDirPath);
  NS_ENSURE_SUCCESS(rv, rv);
  wchar_t volumeMountPoint[MAX_PATH] = {L'\\', L'\\', L'.', L'\\'};
  const size_t PREFIX_LEN = 4;
  if (!::GetVolumePathNameW(profDirPath.get(), volumeMountPoint + PREFIX_LEN,
                            mozilla::ArrayLength(volumeMountPoint) -
                            PREFIX_LEN)) {
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
                                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    nullptr, OPEN_EXISTING, 0, nullptr));
  if (!handle.IsValid()) {
    return NS_ERROR_UNEXPECTED;
  }
  STORAGE_PROPERTY_QUERY queryParameters = {
    StorageDeviceProperty, PropertyStandardQuery
  };
  STORAGE_DEVICE_DESCRIPTOR outputHeader = {sizeof(STORAGE_DEVICE_DESCRIPTOR)};
  DWORD bytesRead = 0;
  if (!::DeviceIoControl(handle, IOCTL_STORAGE_QUERY_PROPERTY,
                         &queryParameters, sizeof(queryParameters),
                         &outputHeader, sizeof(outputHeader), &bytesRead,
                         nullptr)) {
    return NS_ERROR_FAILURE;
  }
  PSTORAGE_DEVICE_DESCRIPTOR deviceOutput =
    (PSTORAGE_DEVICE_DESCRIPTOR)malloc(outputHeader.Size);
  if (!::DeviceIoControl(handle, IOCTL_STORAGE_QUERY_PROPERTY,
                         &queryParameters, sizeof(queryParameters),
                         deviceOutput, outputHeader.Size, &bytesRead,
                         nullptr)) {
    free(deviceOutput);
    return NS_ERROR_FAILURE;
  }
  // Some HDDs are including product ID info in the vendor field. Since PNP
  // IDs include vendor info and product ID concatenated together, we'll do
  // that here and interpret the result as a unique ID for the HDD model.
  if (deviceOutput->VendorIdOffset) {
    aModel = reinterpret_cast<char*>(deviceOutput) +
      deviceOutput->VendorIdOffset;
  }
  if (deviceOutput->ProductIdOffset) {
    aModel += reinterpret_cast<char*>(deviceOutput) +
      deviceOutput->ProductIdOffset;
  }
  aModel.CompressWhitespace();
  if (deviceOutput->ProductRevisionOffset) {
    aRevision = reinterpret_cast<char*>(deviceOutput) +
      deviceOutput->ProductRevisionOffset;
    aRevision.CompressWhitespace();
  }
  free(deviceOutput);
  return NS_OK;
}

nsresult GetInstallYear(uint32_t& aYear)
{
  HKEY hKey;
  LONG status = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                              NS_LITERAL_STRING(
                              "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion"
                              ).get(),
                              0, KEY_READ | KEY_WOW64_64KEY, &hKey);

  if (status != ERROR_SUCCESS) {
    return NS_ERROR_UNEXPECTED;
  }

  nsAutoRegKey key(hKey);

  DWORD type = 0;
  time_t raw_time = 0;
  DWORD time_size = sizeof(time_t);

  status = RegQueryValueExW(hKey, L"InstallDate",
                            nullptr, &type, (LPBYTE)&raw_time, &time_size);

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

  aYear = 1900UL + time.tm_year;
  return NS_OK;
}

nsresult GetCountryCode(nsAString& aCountryCode)
{
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
  numChars = GetGeoInfoW(geoid, GEO_ISO2, wwc(aCountryCode.BeginWriting()),
                         aCountryCode.Length(), 0);
  if (!numChars) {
    return NS_ERROR_FAILURE;
  }

  // numChars includes null terminator
  aCountryCode.Truncate(numChars - 1);
  return NS_OK;
}

} // namespace
#endif // defined(XP_WIN)

using namespace mozilla;

nsSystemInfo::nsSystemInfo()
{
}

nsSystemInfo::~nsSystemInfo()
{
}

// CPU-specific information.
static const struct PropItems
{
  const char* name;
  bool (*propfun)(void);
} cpuPropItems[] = {
  // x86-specific bits.
  { "hasMMX", mozilla::supports_mmx },
  { "hasSSE", mozilla::supports_sse },
  { "hasSSE2", mozilla::supports_sse2 },
  { "hasSSE3", mozilla::supports_sse3 },
  { "hasSSSE3", mozilla::supports_ssse3 },
  { "hasSSE4A", mozilla::supports_sse4a },
  { "hasSSE4_1", mozilla::supports_sse4_1 },
  { "hasSSE4_2", mozilla::supports_sse4_2 },
  { "hasAVX", mozilla::supports_avx },
  { "hasAVX2", mozilla::supports_avx2 },
  // ARM-specific bits.
  { "hasEDSP", mozilla::supports_edsp },
  { "hasARMv6", mozilla::supports_armv6 },
  { "hasARMv7", mozilla::supports_armv7 },
  { "hasNEON", mozilla::supports_neon }
};

#ifdef XP_WIN
// Lifted from media/webrtc/trunk/webrtc/base/systeminfo.cc,
// so keeping the _ instead of switching to camel case for now.
typedef BOOL (WINAPI *LPFN_GLPI)(
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION,
    PDWORD);
static void
GetProcessorInformation(int* physical_cpus, int* cache_size_L2, int* cache_size_L3)
{
  MOZ_ASSERT(physical_cpus && cache_size_L2 && cache_size_L3);

  *physical_cpus = 0;
  *cache_size_L2 = 0; // This will be in kbytes
  *cache_size_L3 = 0; // This will be in kbytes

  // GetLogicalProcessorInformation() is available on Windows XP SP3 and beyond.
  LPFN_GLPI glpi = reinterpret_cast<LPFN_GLPI>(GetProcAddress(
      GetModuleHandle(L"kernel32"),
      "GetLogicalProcessorInformation"));
  if (nullptr == glpi) {
    return;
  }
  // Determine buffer size, allocate and get processor information.
  // Size can change between calls (unlikely), so a loop is done.
  SYSTEM_LOGICAL_PROCESSOR_INFORMATION info_buffer[32];
  SYSTEM_LOGICAL_PROCESSOR_INFORMATION* infos = &info_buffer[0];
  DWORD return_length = sizeof(info_buffer);
  while (!glpi(infos, &return_length)) {
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && infos == &info_buffer[0]) {
      infos = new SYSTEM_LOGICAL_PROCESSOR_INFORMATION[return_length / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION)];
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
          *cache_size_L2 = static_cast<int>(infos[i].Cache.Size/1024);
          break;
        case 3:
          *cache_size_L3 = static_cast<int>(infos[i].Cache.Size/1024);
          break;
        default:
          break;
      }
    }
  }
  if (infos != &info_buffer[0]) {
    delete [] infos;
  }
  return;
}
#endif

nsresult
nsSystemInfo::Init()
{
  nsresult rv;

  static const struct
  {
    PRSysInfo cmd;
    const char* name;
  } items[] = {
    { PR_SI_SYSNAME, "name" },
    { PR_SI_HOSTNAME, "host" },
    { PR_SI_ARCHITECTURE, "arch" },
    { PR_SI_RELEASE, "version" }
  };

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
  nsAutoCString cpuVendor;
  int cpuSpeed = -1;
  int cpuFamily = -1;
  int cpuModel = -1;
  int cpuStepping = -1;
  int logicalCPUs = -1;
  int physicalCPUs = -1;
  int cacheSizeL2 = -1;
  int cacheSizeL3 = -1;

#if defined (XP_WIN)
  // Virtual memory:
  MEMORYSTATUSEX memStat;
  memStat.dwLength = sizeof(memStat);
  if (GlobalMemoryStatusEx(&memStat)) {
    virtualMem = memStat.ullTotalVirtual;
  }

  // CPU speed
  HKEY key;
  static const WCHAR keyName[] =
    L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0";

  if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, keyName , 0, KEY_QUERY_VALUE, &key)
      == ERROR_SUCCESS) {
    DWORD data, len, vtype;
    len = sizeof(data);

    if (RegQueryValueEx(key, L"~Mhz", 0, 0, reinterpret_cast<LPBYTE>(&data),
                        &len) == ERROR_SUCCESS) {
      cpuSpeed = static_cast<int>(data);
    }

    // Limit to 64 double byte characters, should be plenty, but create
    // a buffer one larger as the result may not be null terminated. If
    // it is more than 64, we will not get the value.
    wchar_t cpuVendorStr[64+1];
    len = sizeof(cpuVendorStr)-2;
    if (RegQueryValueExW(key, L"VendorIdentifier",
                         0, &vtype,
                         reinterpret_cast<LPBYTE>(cpuVendorStr),
                         &len) == ERROR_SUCCESS &&
        vtype == REG_SZ && len % 2 == 0 && len > 1) {
      cpuVendorStr[len/2] = 0; // In case it isn't null terminated
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
#elif defined (XP_MACOSX)
  // CPU speed
  uint64_t sysctlValue64 = 0;
  uint32_t sysctlValue32 = 0;
  size_t len = 0;
  len = sizeof(sysctlValue64);
  if (!sysctlbyname("hw.cpufrequency_max", &sysctlValue64, &len, NULL, 0)) {
    cpuSpeed = static_cast<int>(sysctlValue64/1000000);
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
    cacheSizeL2 = static_cast<int>(sysctlValue64/1024);
  }
  MOZ_ASSERT(sizeof(sysctlValue64) == len);

  len = sizeof(sysctlValue64);
  if (!sysctlbyname("hw.l3cachesize", &sysctlValue64, &len, NULL, 0)) {
    cacheSizeL3 = static_cast<int>(sysctlValue64/1024);
  }
  MOZ_ASSERT(sizeof(sysctlValue64) == len);

  if (!sysctlbyname("machdep.cpu.vendor", NULL, &len, NULL, 0)) {
    char* cpuVendorStr = new char[len];
    if (!sysctlbyname("machdep.cpu.vendor", cpuVendorStr, &len, NULL, 0)) {
      cpuVendor = cpuVendorStr;
    }
    delete [] cpuVendorStr;
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

#elif defined (MOZ_WIDGET_GTK)
  // Get vendor, family, model, stepping, physical cores, L3 cache size
  // from /proc/cpuinfo file
  {
    std::map<nsCString, nsCString> keyValuePairs;
    SimpleParseKeyValuePairs("/proc/cpuinfo", keyValuePairs);

    // cpuVendor from "vendor_id"
    cpuVendor.Assign(keyValuePairs[NS_LITERAL_CSTRING("vendor_id")]);

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

    {
      // cacheSizeL3 from "cache size"
      Tokenizer::Token t;
      Tokenizer p(keyValuePairs[NS_LITERAL_CSTRING("cache size")]);
      if (p.Next(t) && t.Type() == Tokenizer::TOKEN_INTEGER &&
          t.AsInteger() <= INT32_MAX) {
        cacheSizeL3 = static_cast<int>(t.AsInteger());
        if (p.Next(t) && t.Type() == Tokenizer::TOKEN_WORD &&
            t.AsString() != NS_LITERAL_CSTRING("KB")) {
          // If we get here, there was some text after the cache size value
          // and that text was not KB.  For now, just don't report the
          // L3 cache.
          cacheSizeL3 = -1;
        }
      }
    }
  }

  {
    // Get cpuSpeed from another file.
    std::ifstream input("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq");
    std::string line;
    if (getline(input, line)) {
      Tokenizer::Token t;
      Tokenizer p(line.c_str());
      if (p.Next(t) && t.Type() == Tokenizer::TOKEN_INTEGER &&
          t.AsInteger() <= INT32_MAX) {
        cpuSpeed = static_cast<int>(t.AsInteger()/1000);
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

  SetInt32Property(NS_LITERAL_STRING("cpucount"), PR_GetNumberOfProcessors());
#else
  SetInt32Property(NS_LITERAL_STRING("cpucount"), PR_GetNumberOfProcessors());
#endif

  if (virtualMem) SetUint64Property(NS_LITERAL_STRING("virtualmemsize"), virtualMem);
  if (cpuSpeed >= 0) SetInt32Property(NS_LITERAL_STRING("cpuspeed"), cpuSpeed);
  if (!cpuVendor.IsEmpty()) SetPropertyAsACString(NS_LITERAL_STRING("cpuvendor"), cpuVendor);
  if (cpuFamily >= 0) SetInt32Property(NS_LITERAL_STRING("cpufamily"), cpuFamily);
  if (cpuModel >= 0) SetInt32Property(NS_LITERAL_STRING("cpumodel"), cpuModel);
  if (cpuStepping >= 0) SetInt32Property(NS_LITERAL_STRING("cpustepping"), cpuStepping);

  if (logicalCPUs >= 0) SetInt32Property(NS_LITERAL_STRING("cpucount"), logicalCPUs);
  if (physicalCPUs >= 0) SetInt32Property(NS_LITERAL_STRING("cpucores"), physicalCPUs);

  if (cacheSizeL2 >= 0) SetInt32Property(NS_LITERAL_STRING("cpucachel2"), cacheSizeL2);
  if (cacheSizeL3 >= 0) SetInt32Property(NS_LITERAL_STRING("cpucachel3"), cacheSizeL3);

  for (uint32_t i = 0; i < ArrayLength(cpuPropItems); i++) {
    rv = SetPropertyAsBool(NS_ConvertASCIItoUTF16(cpuPropItems[i].name),
                           cpuPropItems[i].propfun());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

#ifdef XP_WIN
  BOOL isWow64;
  BOOL gotWow64Value = IsWow64Process(GetCurrentProcess(), &isWow64);
  NS_WARN_IF_FALSE(gotWow64Value, "IsWow64Process failed");
  if (gotWow64Value) {
    rv = SetPropertyAsBool(NS_LITERAL_STRING("isWow64"), !!isWow64);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
  if (NS_FAILED(GetProfileHDDInfo())) {
    // We might have been called before profile-do-change. We'll observe that
    // event so that we can fill this in later.
    nsCOMPtr<nsIObserverService> obsService =
      do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    rv = obsService->AddObserver(this, "profile-do-change", false);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  nsAutoCString hddModel, hddRevision;
  if (NS_SUCCEEDED(GetHDDInfo(NS_GRE_DIR, hddModel, hddRevision))) {
    rv = SetPropertyAsACString(NS_LITERAL_STRING("binHDDModel"), hddModel);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = SetPropertyAsACString(NS_LITERAL_STRING("binHDDRevision"),
                               hddRevision);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  if (NS_SUCCEEDED(GetHDDInfo(NS_WIN_WINDOWS_DIR, hddModel, hddRevision))) {
    rv = SetPropertyAsACString(NS_LITERAL_STRING("winHDDModel"), hddModel);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = SetPropertyAsACString(NS_LITERAL_STRING("winHDDRevision"),
                               hddRevision);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsAutoString countryCode;
  if (NS_SUCCEEDED(GetCountryCode(countryCode))) {
    rv = SetPropertyAsAString(NS_LITERAL_STRING("countryCode"), countryCode);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  uint32_t installYear = 0;
  if (NS_SUCCEEDED(GetInstallYear(installYear))) {
    rv = SetPropertyAsUint32(NS_LITERAL_STRING("installYear"), installYear);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
#endif

#if defined(XP_MACOSX)
  nsAutoString countryCode;
  if (NS_SUCCEEDED(GetSelectedCityInfo(countryCode))) {
    rv = SetPropertyAsAString(NS_LITERAL_STRING("countryCode"), countryCode);
    NS_ENSURE_SUCCESS(rv, rv);
  }
#endif

#if defined(MOZ_WIDGET_GTK)
  // This must be done here because NSPR can only separate OS's when compiled, not libraries.
  // 64 bytes is going to be well enough for "GTK " followed by 3 integers
  // separated with dots.
  char gtkver[64];
  ssize_t gtkver_len = 0;

#if MOZ_WIDGET_GTK == 2
  extern int gtk_read_end_of_the_pipe;

  if (gtk_read_end_of_the_pipe != -1) {
    do {
      gtkver_len = read(gtk_read_end_of_the_pipe, &gtkver, sizeof(gtkver));
    } while (gtkver_len < 0 && errno == EINTR);
    close(gtk_read_end_of_the_pipe);
  }
#endif

  if (gtkver_len <= 0) {
    gtkver_len = snprintf(gtkver, sizeof(gtkver), "GTK %u.%u.%u",
                          gtk_major_version, gtk_minor_version,
                          gtk_micro_version);
  }

  nsAutoCString secondaryLibrary;
  if (gtkver_len > 0) {
    secondaryLibrary.Append(nsDependentCSubstring(gtkver, gtkver_len));
  }

  void* libpulse = dlopen("libpulse.so.0", RTLD_LAZY);
  const char* libpulseVersion = "not-available";
  if (libpulse) {
    auto pa_get_library_version = reinterpret_cast<const char* (*)()>
      (dlsym(libpulse, "pa_get_library_version"));

    if (pa_get_library_version) {
      libpulseVersion = pa_get_library_version();
    }
  }

  secondaryLibrary.AppendPrintf(",libpulse %s", libpulseVersion);

  if (libpulse) {
    dlclose(libpulse);
  }

  rv = SetPropertyAsACString(NS_LITERAL_STRING("secondaryLibrary"),
                             secondaryLibrary);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
#endif

#ifdef MOZ_WIDGET_ANDROID
  AndroidSystemInfo info;
  if (XRE_IsContentProcess()) {
    dom::ContentChild* child = dom::ContentChild::GetSingleton();
    if (child) {
      child->SendGetAndroidSystemInfo(&info);
      SetupAndroidInfo(info);
    }
  } else {
    GetAndroidSystemInfo(&info);
    SetupAndroidInfo(info);
  }
#endif

#ifdef MOZ_WIDGET_GONK
  char sdk[PROP_VALUE_MAX];
  if (__system_property_get("ro.build.version.sdk", sdk)) {
    android_sdk_version = atoi(sdk);
    SetPropertyAsInt32(NS_LITERAL_STRING("sdk_version"), android_sdk_version);

    SetPropertyAsACString(NS_LITERAL_STRING("secondaryLibrary"),
                          nsPrintfCString("SDK %u", android_sdk_version));
  }

  char characteristics[PROP_VALUE_MAX];
  if (__system_property_get("ro.build.characteristics", characteristics)) {
    if (!strcmp(characteristics, "tablet")) {
      SetPropertyAsBool(NS_LITERAL_STRING("tablet"), true);
    } else if (!strcmp(characteristics, "tv")) {
      SetPropertyAsBool(NS_LITERAL_STRING("tv"), true);
    }
  }

  nsAutoString str;
  rv = GetPropertyAsAString(NS_LITERAL_STRING("version"), str);
  if (NS_SUCCEEDED(rv)) {
    SetPropertyAsAString(NS_LITERAL_STRING("kernel_version"), str);
  }

  const nsAdoptingString& b2g_os_name =
    mozilla::Preferences::GetString("b2g.osName");
  if (b2g_os_name) {
    SetPropertyAsAString(NS_LITERAL_STRING("name"), b2g_os_name);
  }

  const nsAdoptingString& b2g_version =
    mozilla::Preferences::GetString("b2g.version");
  if (b2g_version) {
    SetPropertyAsAString(NS_LITERAL_STRING("version"), b2g_version);
  }
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
#endif // XP_LINUX && MOZ_SANDBOX

  return NS_OK;
}

#ifdef MOZ_WIDGET_ANDROID
/* static */
void
nsSystemInfo::GetAndroidSystemInfo(AndroidSystemInfo* aInfo)
{
  MOZ_ASSERT(XRE_IsParentProcess());

  if (!mozilla::AndroidBridge::Bridge()) {
    aInfo->sdk_version() = 0;
    return;
  }

  nsAutoString str;
  if (mozilla::AndroidBridge::Bridge()->GetStaticStringField(
      "android/os/Build", "MODEL", str)) {
    aInfo->device() = str;
  }
  if (mozilla::AndroidBridge::Bridge()->GetStaticStringField(
      "android/os/Build", "MANUFACTURER", str)) {
    aInfo->manufacturer() = str;
  }
  if (mozilla::AndroidBridge::Bridge()->GetStaticStringField(
      "android/os/Build$VERSION", "RELEASE", str)) {
    aInfo->release_version() = str;
  }
  if (mozilla::AndroidBridge::Bridge()->GetStaticStringField(
      "android/os/Build", "HARDWARE", str)) {
    aInfo->hardware() = str;
  }
  int32_t sdk_version;
  if (!mozilla::AndroidBridge::Bridge()->GetStaticIntField(
      "android/os/Build$VERSION", "SDK_INT", &sdk_version)) {
    sdk_version = 0;
  }
  aInfo->sdk_version() = sdk_version;
  aInfo->isTablet() = mozilla::widget::GeckoAppShell::IsTablet();
}

void
nsSystemInfo::SetupAndroidInfo(const AndroidSystemInfo& aInfo)
{
  if (!aInfo.device().IsEmpty()) {
    SetPropertyAsAString(NS_LITERAL_STRING("device"), aInfo.device());
  }
  if (!aInfo.manufacturer().IsEmpty()) {
    SetPropertyAsAString(NS_LITERAL_STRING("manufacturer"), aInfo.manufacturer());
  }
  if (!aInfo.release_version().IsEmpty()) {
    SetPropertyAsAString(NS_LITERAL_STRING("release_version"), aInfo.release_version());
  }
  SetPropertyAsBool(NS_LITERAL_STRING("tablet"), aInfo.isTablet());
  // NSPR "version" is the kernel version. For Android we want the Android version.
  // Rename SDK version to version and put the kernel version into kernel_version.
  nsAutoString str;
  nsresult rv = GetPropertyAsAString(NS_LITERAL_STRING("version"), str);
  if (NS_SUCCEEDED(rv)) {
    SetPropertyAsAString(NS_LITERAL_STRING("kernel_version"), str);
  }
  // When AndroidBridge is not available (eg. in xpcshell tests), sdk_version is 0.
  if (aInfo.sdk_version() != 0) {
    android_sdk_version = aInfo.sdk_version();
    if (android_sdk_version >= 8 && !aInfo.hardware().IsEmpty()) {
      SetPropertyAsAString(NS_LITERAL_STRING("hardware"), aInfo.hardware());
    }
    SetPropertyAsInt32(NS_LITERAL_STRING("version"), android_sdk_version);
  }
}
#endif // MOZ_WIDGET_ANDROID

void
nsSystemInfo::SetInt32Property(const nsAString& aPropertyName,
                               const int32_t aValue)
{
  NS_WARN_IF_FALSE(aValue > 0, "Unable to read system value");
  if (aValue > 0) {
#ifdef DEBUG
    nsresult rv =
#endif
      SetPropertyAsInt32(aPropertyName, aValue);
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Unable to set property");
  }
}

void
nsSystemInfo::SetUint32Property(const nsAString& aPropertyName,
                                const uint32_t aValue)
{
  // Only one property is currently set via this function.
  // It may legitimately be zero.
#ifdef DEBUG
  nsresult rv =
#endif
    SetPropertyAsUint32(aPropertyName, aValue);
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Unable to set property");
}

void
nsSystemInfo::SetUint64Property(const nsAString& aPropertyName,
                                const uint64_t aValue)
{
  NS_WARN_IF_FALSE(aValue > 0, "Unable to read system value");
  if (aValue > 0) {
#ifdef DEBUG
    nsresult rv =
#endif
      SetPropertyAsUint64(aPropertyName, aValue);
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Unable to set property");
  }
}

#if defined(XP_WIN)
NS_IMETHODIMP
nsSystemInfo::Observe(nsISupports* aSubject, const char* aTopic,
                      const char16_t* aData)
{
  if (!strcmp(aTopic, "profile-do-change")) {
    nsresult rv;
    nsCOMPtr<nsIObserverService> obsService =
      do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = obsService->RemoveObserver(this, "profile-do-change");
    if (NS_FAILED(rv)) {
      return rv;
    }
    return GetProfileHDDInfo();
  }
  return NS_OK;
}

nsresult
nsSystemInfo::GetProfileHDDInfo()
{
  nsAutoCString hddModel, hddRevision;
  nsresult rv = GetHDDInfo(NS_APP_USER_PROFILE_50_DIR, hddModel, hddRevision);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = SetPropertyAsACString(NS_LITERAL_STRING("profileHDDModel"), hddModel);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = SetPropertyAsACString(NS_LITERAL_STRING("profileHDDRevision"),
                             hddRevision);
  return rv;
}

NS_IMPL_ISUPPORTS_INHERITED(nsSystemInfo, nsHashPropertyBag, nsIObserver)
#endif // defined(XP_WIN)

