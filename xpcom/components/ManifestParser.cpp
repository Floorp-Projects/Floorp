/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "ManifestParser.h"

#include <string.h>

#include "prio.h"
#include "prprf.h"
#if defined(XP_WIN)
#include <windows.h>
#elif defined(MOZ_WIDGET_COCOA)
#include <CoreServices/CoreServices.h>
#elif defined(MOZ_WIDGET_GTK)
#include <gtk/gtk.h>
#endif

#ifdef MOZ_WIDGET_ANDROID
#include "AndroidBridge.h"
#endif

#include "mozilla/Services.h"

#include "nsCRT.h"
#include "nsConsoleMessage.h"
#include "nsTextFormatter.h"
#include "nsVersionComparator.h"
#include "nsXPCOMCIDInternal.h"

#include "nsIConsoleService.h"
#include "nsIScriptError.h"
#include "nsIXULAppInfo.h"
#include "nsIXULRuntime.h"

using namespace mozilla;

struct ManifestDirective
{
  const char* directive;
  int argc;

  // Some directives should only be delivered for NS_COMPONENT_LOCATION
  // manifests.
  bool componentonly;

  bool ischrome;

  bool allowbootstrap;

  // The platform/contentaccessible flags only apply to content directives.
  bool contentflags;

  // Function to handle this directive. This isn't a union because C++ still
  // hasn't learned how to initialize unions in a sane way.
  void (nsComponentManagerImpl::*mgrfunc)
    (nsComponentManagerImpl::ManifestProcessingContext& cx,
     int lineno, char *const * argv);
  void (nsChromeRegistry::*regfunc)
    (nsChromeRegistry::ManifestProcessingContext& cx,
     int lineno, char *const *argv,
     bool platform, bool contentaccessible);

  bool isContract;
};
static const ManifestDirective kParsingTable[] = {
  { "manifest",         1, false, true, true, false,
    &nsComponentManagerImpl::ManifestManifest, nullptr },
  { "binary-component", 1, true, false, false, false,
    &nsComponentManagerImpl::ManifestBinaryComponent, nullptr },
  { "interfaces",       1, true, false, false, false,
    &nsComponentManagerImpl::ManifestXPT, nullptr },
  { "component",        2, true, false, false, false,
    &nsComponentManagerImpl::ManifestComponent, nullptr },
  { "contract",         2, true, false, false, false,
    &nsComponentManagerImpl::ManifestContract, nullptr, true},
  { "category",         3, true, false, false, false,
    &nsComponentManagerImpl::ManifestCategory, nullptr },
  { "content",          2, true, true, true,  true,
    nullptr, &nsChromeRegistry::ManifestContent },
  { "locale",           3, true, true, true,  false,
    nullptr, &nsChromeRegistry::ManifestLocale },
  { "skin",             3, false, true, true,  false,
    nullptr, &nsChromeRegistry::ManifestSkin },
  { "overlay",          2, true, true, false,  false,
    nullptr, &nsChromeRegistry::ManifestOverlay },
  { "style",            2, false, true, false,  false,
    nullptr, &nsChromeRegistry::ManifestStyle },
  { "override",         2, true, true, true,  false,
    nullptr, &nsChromeRegistry::ManifestOverride },
  { "resource",         2, true, true, false,  false,
    nullptr, &nsChromeRegistry::ManifestResource }
};

static const char kWhitespace[] = "\t ";

static bool IsNewline(char c)
{
  return c == '\n' || c == '\r';
}

namespace {
struct AutoPR_smprintf_free
{
  AutoPR_smprintf_free(char* buf)
    : mBuf(buf)
  {
  }

  ~AutoPR_smprintf_free()
  {
    if (mBuf)
      PR_smprintf_free(mBuf);
  }

  operator char*() const {
    return mBuf;
  }

  char* mBuf;
};

} // anonymous namespace

void LogMessage(const char* aMsg, ...)
{
  nsCOMPtr<nsIConsoleService> console =
    do_GetService(NS_CONSOLESERVICE_CONTRACTID);
  if (!console)
    return;

  va_list args;
  va_start(args, aMsg);
  AutoPR_smprintf_free formatted(PR_vsmprintf(aMsg, args));
  va_end(args);

  nsCOMPtr<nsIConsoleMessage> error =
    new nsConsoleMessage(NS_ConvertUTF8toUTF16(formatted).get());
  console->LogMessage(error);
}

void LogMessageWithContext(FileLocation &aFile,
                           uint32_t aLineNumber, const char* aMsg, ...)
{
  va_list args;
  va_start(args, aMsg);
  AutoPR_smprintf_free formatted(PR_vsmprintf(aMsg, args));
  va_end(args);
  if (!formatted)
    return;

  nsCString file;
  aFile.GetURIString(file);

  nsCOMPtr<nsIScriptError> error =
    do_CreateInstance(NS_SCRIPTERROR_CONTRACTID);
  if (!error) {
    // This can happen early in component registration. Fall back to a
    // generic console message.
    LogMessage("Warning: in '%s', line %i: %s", file.get(),
               aLineNumber, (char*) formatted);
    return;
  }

  nsCOMPtr<nsIConsoleService> console =
    do_GetService(NS_CONSOLESERVICE_CONTRACTID);
  if (!console)
    return;

  nsresult rv = error->Init(NS_ConvertUTF8toUTF16(formatted),
			    NS_ConvertUTF8toUTF16(file), EmptyString(),
			    aLineNumber, 0, nsIScriptError::warningFlag,
			    "chrome registration");
  if (NS_FAILED(rv))
    return;

  console->LogMessage(error);
}

/**
 * Check for a modifier flag of the following forms:
 *   "flag"   (same as "true")
 *   "flag=yes|true|1"
 *   "flag="no|false|0"
 * @param aFlag The flag to compare.
 * @param aData The tokenized data to check; this is lowercased
 *              before being passed in.
 * @param aResult If the flag is found, the value is assigned here.
 * @return Whether the flag was handled.
 */
static bool
CheckFlag(const nsSubstring& aFlag, const nsSubstring& aData, bool& aResult)
{
  if (!StringBeginsWith(aData, aFlag))
    return false;

  if (aFlag.Length() == aData.Length()) {
    // the data is simply "flag", which is the same as "flag=yes"
    aResult = true;
    return true;
  }

  if (aData.CharAt(aFlag.Length()) != '=') {
    // the data is "flag2=", which is not anything we care about
    return false;
  }

  if (aData.Length() == aFlag.Length() + 1) {
    aResult = false;
    return true;
  }

  switch (aData.CharAt(aFlag.Length() + 1)) {
  case '1':
  case 't': //true
  case 'y': //yes
    aResult = true;
    return true;

  case '0':
  case 'f': //false
  case 'n': //no
    aResult = false;
    return true;
  }

  return false;
}

enum TriState {
  eUnspecified,
  eBad,
  eOK
};

/**
 * Check for a modifier flag of the following form:
 *   "flag=string"
 *   "flag!=string"
 * @param aFlag The flag to compare.
 * @param aData The tokenized data to check; this is lowercased
 *              before being passed in.
 * @param aValue The value that is expected.
 * @param aResult If this is "ok" when passed in, this is left alone.
 *                Otherwise if the flag is found it is set to eBad or eOK.
 * @return Whether the flag was handled.
 */
static bool
CheckStringFlag(const nsSubstring& aFlag, const nsSubstring& aData,
                const nsSubstring& aValue, TriState& aResult)
{
  if (aData.Length() < aFlag.Length() + 1)
    return false;

  if (!StringBeginsWith(aData, aFlag))
    return false;

  bool comparison = true;
  if (aData[aFlag.Length()] != '=') {
    if (aData[aFlag.Length()] == '!' &&
        aData.Length() >= aFlag.Length() + 2 &&
        aData[aFlag.Length() + 1] == '=')
      comparison = false;
    else
      return false;
  }

  if (aResult != eOK) {
    nsDependentSubstring testdata = Substring(aData, aFlag.Length() + (comparison ? 1 : 2));
    if (testdata.Equals(aValue))
      aResult = comparison ? eOK : eBad;
    else
      aResult = comparison ? eBad : eOK;
  }

  return true;
}

/**
 * Check for a modifier flag of the following form:
 *   "flag=version"
 *   "flag<=version"
 *   "flag<version"
 *   "flag>=version"
 *   "flag>version"
 * @param aFlag The flag to compare.
 * @param aData The tokenized data to check; this is lowercased
 *              before being passed in.
 * @param aValue The value that is expected. If this is empty then no
 *               comparison will match.
 * @param aResult If this is eOK when passed in, this is left alone.
 *                Otherwise if the flag is found it is set to eBad or eOK.
 * @return Whether the flag was handled.
 */

#define COMPARE_EQ    1 << 0
#define COMPARE_LT    1 << 1
#define COMPARE_GT    1 << 2

static bool
CheckVersionFlag(const nsString& aFlag, const nsString& aData,
                 const nsString& aValue, TriState& aResult)
{
  if (aData.Length() < aFlag.Length() + 2)
    return false;

  if (!StringBeginsWith(aData, aFlag))
    return false;

  if (aValue.Length() == 0) {
    if (aResult != eOK)
      aResult = eBad;
    return true;
  }

  uint32_t comparison;
  nsAutoString testdata;

  switch (aData[aFlag.Length()]) {
  case '=':
    comparison = COMPARE_EQ;
    testdata = Substring(aData, aFlag.Length() + 1);
    break;

  case '<':
    if (aData[aFlag.Length() + 1] == '=') {
      comparison = COMPARE_EQ | COMPARE_LT;
      testdata = Substring(aData, aFlag.Length() + 2);
    }
    else {
      comparison = COMPARE_LT;
      testdata = Substring(aData, aFlag.Length() + 1);
    }
    break;

  case '>':
    if (aData[aFlag.Length() + 1] == '=') {
      comparison = COMPARE_EQ | COMPARE_GT;
      testdata = Substring(aData, aFlag.Length() + 2);
    }
    else {
      comparison = COMPARE_GT;
      testdata = Substring(aData, aFlag.Length() + 1);
    }
    break;

  default:
    return false;
  }

  if (testdata.Length() == 0)
    return false;

  if (aResult != eOK) {
    int32_t c = mozilla::CompareVersions(NS_ConvertUTF16toUTF8(aValue).get(),
                                         NS_ConvertUTF16toUTF8(testdata).get());
    if ((c == 0 && comparison & COMPARE_EQ) ||
	(c < 0 && comparison & COMPARE_LT) ||
	(c > 0 && comparison & COMPARE_GT))
      aResult = eOK;
    else
      aResult = eBad;
  }

  return true;
}

// In-place conversion of ascii characters to lower case
static void
ToLowerCase(char* token)
{
  for (; *token; ++token)
    *token = NS_ToLower(*token);
}

namespace {

struct CachedDirective
{
  int lineno;
  char* argv[4];
};

} // anonymous namespace


void
ParseManifest(NSLocationType type, FileLocation &file, char* buf, bool aChromeOnly)
{
  nsComponentManagerImpl::ManifestProcessingContext mgrcx(type, file, aChromeOnly);
  nsChromeRegistry::ManifestProcessingContext chromecx(type, file);
  nsresult rv;

  NS_NAMED_LITERAL_STRING(kPlatform, "platform");
  NS_NAMED_LITERAL_STRING(kContentAccessible, "contentaccessible");
  NS_NAMED_LITERAL_STRING(kApplication, "application");
  NS_NAMED_LITERAL_STRING(kAppVersion, "appversion");
  NS_NAMED_LITERAL_STRING(kGeckoVersion, "platformversion");
  NS_NAMED_LITERAL_STRING(kOs, "os");
  NS_NAMED_LITERAL_STRING(kOsVersion, "osversion");
  NS_NAMED_LITERAL_STRING(kABI, "abi");
#if defined(MOZ_WIDGET_ANDROID)
  NS_NAMED_LITERAL_STRING(kTablet, "tablet");
#endif

  // Obsolete
  NS_NAMED_LITERAL_STRING(kXPCNativeWrappers, "xpcnativewrappers");

  nsAutoString appID;
  nsAutoString appVersion;
  nsAutoString geckoVersion;
  nsAutoString osTarget;
  nsAutoString abi;

  nsCOMPtr<nsIXULAppInfo> xapp (do_GetService(XULAPPINFO_SERVICE_CONTRACTID));
  if (xapp) {
    nsAutoCString s;
    rv = xapp->GetID(s);
    if (NS_SUCCEEDED(rv))
      CopyUTF8toUTF16(s, appID);

    rv = xapp->GetVersion(s);
    if (NS_SUCCEEDED(rv))
      CopyUTF8toUTF16(s, appVersion);

    rv = xapp->GetPlatformVersion(s);
    if (NS_SUCCEEDED(rv))
      CopyUTF8toUTF16(s, geckoVersion);

    nsCOMPtr<nsIXULRuntime> xruntime (do_QueryInterface(xapp));
    if (xruntime) {
      rv = xruntime->GetOS(s);
      if (NS_SUCCEEDED(rv)) {
        ToLowerCase(s);
        CopyUTF8toUTF16(s, osTarget);
      }

      rv = xruntime->GetXPCOMABI(s);
      if (NS_SUCCEEDED(rv) && osTarget.Length()) {
        ToLowerCase(s);
        CopyUTF8toUTF16(s, abi);
        abi.Insert(PRUnichar('_'), 0);
        abi.Insert(osTarget, 0);
      }
    }
  }

  nsAutoString osVersion;
#if defined(XP_WIN)
  OSVERSIONINFO info = { sizeof(OSVERSIONINFO) };
  if (GetVersionEx(&info)) {
    nsTextFormatter::ssprintf(osVersion, NS_LITERAL_STRING("%ld.%ld").get(),
                                         info.dwMajorVersion,
                                         info.dwMinorVersion);
  }
#elif defined(MOZ_WIDGET_COCOA)
  SInt32 majorVersion, minorVersion;
  if ((Gestalt(gestaltSystemVersionMajor, &majorVersion) == noErr) &&
      (Gestalt(gestaltSystemVersionMinor, &minorVersion) == noErr)) {
    nsTextFormatter::ssprintf(osVersion, NS_LITERAL_STRING("%ld.%ld").get(),
                                         majorVersion,
                                         minorVersion);
  }
#elif defined(MOZ_WIDGET_GTK)
  nsTextFormatter::ssprintf(osVersion, NS_LITERAL_STRING("%ld.%ld").get(),
                                       gtk_major_version,
                                       gtk_minor_version);
#elif defined(MOZ_WIDGET_ANDROID)
  bool isTablet = false;
  if (mozilla::AndroidBridge::Bridge()) {
    mozilla::AndroidBridge::Bridge()->GetStaticStringField("android/os/Build$VERSION", "RELEASE", osVersion);
    isTablet = mozilla::AndroidBridge::Bridge()->IsTablet();
  }
#endif

  // Because contracts must be registered after CIDs, we save and process them
  // at the end.
  nsTArray<CachedDirective> contracts;

  char *token;
  char *newline = buf;
  uint32_t line = 0;

  // outer loop tokenizes by newline
  while (*newline) {
    while (*newline && IsNewline(*newline)) {
      ++newline;
      ++line;
    }
    if (!*newline)
      break;

    token = newline;
    while (*newline && !IsNewline(*newline))
      ++newline;

    if (*newline) {
      *newline = '\0';
      ++newline;
    }
    ++line;

    if (*token == '#') // ignore lines that begin with # as comments
      continue;

    char *whitespace = token;
    token = nsCRT::strtok(whitespace, kWhitespace, &whitespace);
    if (!token) continue;

    const ManifestDirective* directive = nullptr;
    for (const ManifestDirective* d = kParsingTable;
	 d < ArrayEnd(kParsingTable);
	 ++d) {
      if (!strcmp(d->directive, token)) {
	directive = d;
	break;
      }
    }

    if (!directive) {
      LogMessageWithContext(file, line,
                            "Ignoring unrecognized chrome manifest directive '%s'.",
                            token);
      continue;
    }

    if (!directive->allowbootstrap && NS_BOOTSTRAPPED_LOCATION == type) {
      LogMessageWithContext(file, line,
                            "Bootstrapped manifest not allowed to use '%s' directive.",
                            token);
      continue;
    }

    if (directive->componentonly && NS_SKIN_LOCATION == type) {
      LogMessageWithContext(file, line,
                            "Skin manifest not allowed to use '%s' directive.",
                            token);
      continue;
    }

    NS_ASSERTION(directive->argc < 4, "Need to reset argv array length");
    char* argv[4];
    for (int i = 0; i < directive->argc; ++i)
      argv[i] = nsCRT::strtok(whitespace, kWhitespace, &whitespace);

    if (!argv[directive->argc - 1]) {
      LogMessageWithContext(file, line,
                            "Not enough arguments for chrome manifest directive '%s', expected %i.",
                            token, directive->argc);
      continue;
    }

    bool ok = true;
    TriState stAppVersion = eUnspecified;
    TriState stGeckoVersion = eUnspecified;
    TriState stApp = eUnspecified;
    TriState stOsVersion = eUnspecified;
    TriState stOs = eUnspecified;
    TriState stABI = eUnspecified;
#if defined(MOZ_WIDGET_ANDROID)
    TriState stTablet = eUnspecified;
#endif
    bool platform = false;
    bool contentAccessible = false;

    while (nullptr != (token = nsCRT::strtok(whitespace, kWhitespace, &whitespace)) && ok) {
      ToLowerCase(token);
      NS_ConvertASCIItoUTF16 wtoken(token);

      if (CheckStringFlag(kApplication, wtoken, appID, stApp) ||
          CheckStringFlag(kOs, wtoken, osTarget, stOs) ||
          CheckStringFlag(kABI, wtoken, abi, stABI) ||
          CheckVersionFlag(kOsVersion, wtoken, osVersion, stOsVersion) ||
          CheckVersionFlag(kAppVersion, wtoken, appVersion, stAppVersion) ||
          CheckVersionFlag(kGeckoVersion, wtoken, geckoVersion, stGeckoVersion))
        continue;

#if defined(MOZ_WIDGET_ANDROID)
      bool tablet = false;
      if (CheckFlag(kTablet, wtoken, tablet)) {
        stTablet = (tablet == isTablet) ? eOK : eBad;
        continue;
      }
#endif

      if (directive->contentflags &&
          (CheckFlag(kPlatform, wtoken, platform) ||
           CheckFlag(kContentAccessible, wtoken, contentAccessible)))
        continue;

      bool xpcNativeWrappers = true; // Dummy for CheckFlag.
      if (CheckFlag(kXPCNativeWrappers, wtoken, xpcNativeWrappers)) {
        LogMessageWithContext(file, line,
                              "Ignoring obsolete chrome registration modifier '%s'.",
                              token);
        continue;
      }

      LogMessageWithContext(file, line,
                            "Unrecognized chrome manifest modifier '%s'.",
                            token);
      ok = false;
    }

    if (!ok ||
        stApp == eBad ||
        stAppVersion == eBad ||
        stGeckoVersion == eBad ||
        stOs == eBad ||
        stOsVersion == eBad ||
#ifdef MOZ_WIDGET_ANDROID
        stTablet == eBad ||
#endif
        stABI == eBad)
      continue;

    if (directive->regfunc) {
      if (GeckoProcessType_Default != XRE_GetProcessType())
        continue;

      if (!nsChromeRegistry::gChromeRegistry) {
        nsCOMPtr<nsIChromeRegistry> cr =
          mozilla::services::GetChromeRegistryService();
        if (!nsChromeRegistry::gChromeRegistry) {
          LogMessageWithContext(file, line,
                                "Chrome registry isn't available yet.");
          continue;
        }
      }

      (nsChromeRegistry::gChromeRegistry->*(directive->regfunc))
	(chromecx, line, argv, platform, contentAccessible);
    }
    else if (directive->ischrome || !aChromeOnly) {
      if (directive->isContract) {
        CachedDirective* cd = contracts.AppendElement();
        cd->lineno = line;
        cd->argv[0] = argv[0];
        cd->argv[1] = argv[1];
      }
      else
        (nsComponentManagerImpl::gComponentManager->*(directive->mgrfunc))
          (mgrcx, line, argv);
    }
  }

  for (uint32_t i = 0; i < contracts.Length(); ++i) {
    CachedDirective& d = contracts[i];
    nsComponentManagerImpl::gComponentManager->ManifestContract
      (mgrcx, d.lineno, d.argv);
  }
}
