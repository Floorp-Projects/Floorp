/* -*- Mode: C++; tab-width: 50; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "nsSystemInfo.h"
#include "prsystem.h"
#include "nsString.h"
#include "prprf.h"
#include "mozilla/SSE.h"
#include "mozilla/arm.h"

#ifdef XP_WIN
#include <windows.h>
#endif

#ifdef MOZ_WIDGET_GTK
#include <gtk/gtk.h>
#endif

#ifdef MOZ_WIDGET_ANDROID
#include "AndroidBridge.h"
#endif

#ifdef MOZ_WIDGET_GONK
#include <sys/system_properties.h>
#endif

#ifdef ANDROID
extern "C" {
NS_EXPORT int android_sdk_version;
}
#endif

using namespace mozilla;

nsSystemInfo::nsSystemInfo()
{
}

nsSystemInfo::~nsSystemInfo()
{
}

// CPU-specific information.
static const struct PropItems {
    const char *name;
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
    // ARM-specific bits.
    { "hasEDSP", mozilla::supports_edsp },
    { "hasARMv6", mozilla::supports_armv6 },
    { "hasARMv7", mozilla::supports_armv7 },
    { "hasNEON", mozilla::supports_neon }
};

nsresult
nsSystemInfo::Init()
{
    nsresult rv = nsHashPropertyBag::Init();
    NS_ENSURE_SUCCESS(rv, rv);

    static const struct {
      PRSysInfo cmd;
      const char *name;
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
        NS_ENSURE_SUCCESS(rv, rv);
      }
      else {
        NS_WARNING("PR_GetSystemInfo failed");
      }
    }

    // Additional informations not available through PR_GetSystemInfo.
    SetInt32Property(NS_LITERAL_STRING("pagesize"), PR_GetPageSize());
    SetInt32Property(NS_LITERAL_STRING("pageshift"), PR_GetPageShift());
    SetInt32Property(NS_LITERAL_STRING("memmapalign"), PR_GetMemMapAlignment());
    SetInt32Property(NS_LITERAL_STRING("cpucount"), PR_GetNumberOfProcessors());
    SetUint64Property(NS_LITERAL_STRING("memsize"), PR_GetPhysicalMemorySize());

    for (uint32_t i = 0; i < ArrayLength(cpuPropItems); i++) {
        rv = SetPropertyAsBool(NS_ConvertASCIItoUTF16(cpuPropItems[i].name),
                               cpuPropItems[i].propfun());
        NS_ENSURE_SUCCESS(rv, rv);
    }

#ifdef XP_WIN
    BOOL isWow64;
    BOOL gotWow64Value = IsWow64Process(GetCurrentProcess(), &isWow64);
    NS_WARN_IF_FALSE(gotWow64Value, "IsWow64Process failed");
    if (gotWow64Value) {
      rv = SetPropertyAsBool(NS_LITERAL_STRING("isWow64"), !!isWow64);
      NS_ENSURE_SUCCESS(rv, rv);
    }
#endif

#if defined(MOZ_WIDGET_GTK)
    // This must be done here because NSPR can only separate OS's when compiled, not libraries.
    char* gtkver = PR_smprintf("GTK %u.%u.%u", gtk_major_version, gtk_minor_version, gtk_micro_version);
    if (gtkver) {
      rv = SetPropertyAsACString(NS_LITERAL_STRING("secondaryLibrary"),
                                 nsDependentCString(gtkver));
      PR_smprintf_free(gtkver);
      NS_ENSURE_SUCCESS(rv, rv);
    }
#endif


#ifdef MOZ_PLATFORM_MAEMO
    char *  line = nullptr;
    size_t  len = 0;
    ssize_t read;
#if MOZ_PLATFORM_MAEMO > 5
    FILE *fp = popen("/usr/bin/sysinfoclient --get /component/product", "r");
#else
    FILE *fp = fopen("/proc/component_version", "r");
#endif
    if (fp) {
      while ((read = getline(&line, &len, fp)) != -1) {
        if (line) {
          if (strstr(line, "RX-51")) {
            SetPropertyAsACString(NS_LITERAL_STRING("device"), NS_LITERAL_CSTRING("Nokia N900"));
            SetPropertyAsACString(NS_LITERAL_STRING("manufacturer"), NS_LITERAL_CSTRING("Nokia"));
            SetPropertyAsACString(NS_LITERAL_STRING("hardware"), NS_LITERAL_CSTRING("RX-51"));
            SetPropertyAsBool(NS_LITERAL_STRING("tablet"), false);
            break;
          } else if (strstr(line, "RX-44") ||
                     strstr(line, "RX-48") ||
                     strstr(line, "RX-32") ) {
            /* not as accurate as we can be, but these devices are deprecated */
            SetPropertyAsACString(NS_LITERAL_STRING("device"), NS_LITERAL_CSTRING("Nokia N8xx"));
            SetPropertyAsACString(NS_LITERAL_STRING("manufacturer"), NS_LITERAL_CSTRING("Nokia"));
            SetPropertyAsACString(NS_LITERAL_STRING("hardware"), NS_LITERAL_CSTRING("N8xx"));
            SetPropertyAsBool(NS_LITERAL_STRING("tablet"), false);
            break;
          } else if (strstr(line, "RM-680")) {
            SetPropertyAsACString(NS_LITERAL_STRING("device"), NS_LITERAL_CSTRING("Nokia N950"));
            SetPropertyAsACString(NS_LITERAL_STRING("manufacturer"), NS_LITERAL_CSTRING("Nokia"));
            SetPropertyAsACString(NS_LITERAL_STRING("hardware"), NS_LITERAL_CSTRING("N9xx"));
            SetPropertyAsBool(NS_LITERAL_STRING("tablet"), false);
            break;
          } else if (strstr(line, "RM-696")) {
            SetPropertyAsACString(NS_LITERAL_STRING("device"), NS_LITERAL_CSTRING("Nokia N9"));
            SetPropertyAsACString(NS_LITERAL_STRING("manufacturer"), NS_LITERAL_CSTRING("Nokia"));
            SetPropertyAsACString(NS_LITERAL_STRING("hardware"), NS_LITERAL_CSTRING("N9xx"));
            SetPropertyAsBool(NS_LITERAL_STRING("tablet"), false);
            break;
          }
        }
      }
      if (line)
        free(line);
#if MOZ_PLATFORM_MAEMO > 5
      pclose(fp);
#else
      fclose(fp);
#endif
    }
#endif

#ifdef MOZ_WIDGET_ANDROID
    if (mozilla::AndroidBridge::Bridge()) {
        nsAutoString str;
        if (mozilla::AndroidBridge::Bridge()->GetStaticStringField("android/os/Build", "MODEL", str))
            SetPropertyAsAString(NS_LITERAL_STRING("device"), str);
        if (mozilla::AndroidBridge::Bridge()->GetStaticStringField("android/os/Build", "MANUFACTURER", str))
            SetPropertyAsAString(NS_LITERAL_STRING("manufacturer"), str);
        if (mozilla::AndroidBridge::Bridge()->GetStaticStringField("android/os/Build$VERSION", "RELEASE", str))
            SetPropertyAsAString(NS_LITERAL_STRING("release_version"), str);
        int32_t version;
        if (!mozilla::AndroidBridge::Bridge()->GetStaticIntField("android/os/Build$VERSION", "SDK_INT", &version))
            version = 0;
        android_sdk_version = version;
        if (version >= 8 && mozilla::AndroidBridge::Bridge()->GetStaticStringField("android/os/Build", "HARDWARE", str))
            SetPropertyAsAString(NS_LITERAL_STRING("hardware"), str);
        bool isTablet = mozilla::AndroidBridge::Bridge()->IsTablet();
        SetPropertyAsBool(NS_LITERAL_STRING("tablet"), isTablet);
        // NSPR "version" is the kernel version. For Android we want the Android version.
        // Rename SDK version to version and put the kernel version into kernel_version.
        rv = GetPropertyAsAString(NS_LITERAL_STRING("version"), str);
        if (NS_SUCCEEDED(rv)) {
            SetPropertyAsAString(NS_LITERAL_STRING("kernel_version"), str);
        }
        SetPropertyAsInt32(NS_LITERAL_STRING("version"), android_sdk_version);
    }
#endif

#ifdef MOZ_WIDGET_GONK
    char sdk[PROP_VALUE_MAX];
    if (__system_property_get("ro.build.version.sdk", sdk))
      android_sdk_version = atoi(sdk);
#endif

    return NS_OK;
}

void
nsSystemInfo::SetInt32Property(const nsAString &aPropertyName,
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
nsSystemInfo::SetUint64Property(const nsAString &aPropertyName,
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
