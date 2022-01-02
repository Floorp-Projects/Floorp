/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXREAppData_h
#define nsXREAppData_h

#include <stdint.h>
#include "mozilla/Attributes.h"
#include "mozilla/UniquePtrExtensions.h"
#include "nsCOMPtr.h"
#include "nsCRTGlue.h"
#include "nsIFile.h"

#if defined(XP_WIN) && defined(MOZ_SANDBOX)
namespace sandbox {
class BrokerServices;
}
namespace mozilla {
namespace sandboxing {
class PermissionsService;
}
}  // namespace mozilla
#endif

namespace mozilla {

struct StaticXREAppData;

/**
 * Application-specific data needed to start the apprunner.
 */
class XREAppData {
 public:
  XREAppData() = default;
  ~XREAppData() = default;
  XREAppData(const XREAppData& aOther) { *this = aOther; }

  explicit XREAppData(const StaticXREAppData& aOther) { *this = aOther; }

  XREAppData& operator=(const StaticXREAppData& aOther);
  XREAppData& operator=(const XREAppData& aOther);
  XREAppData& operator=(XREAppData&& aOther) = default;

  // Lots of code reads these fields directly like a struct, so rather
  // than using UniquePtr directly, use an auto-converting wrapper.
  class CharPtr {
   public:
    explicit CharPtr() = default;
    explicit CharPtr(const char* v) { *this = v; }
    CharPtr(CharPtr&&) = default;
    ~CharPtr() = default;

    CharPtr& operator=(const char* v) {
      if (v) {
        mValue.reset(NS_xstrdup(v));
      } else {
        mValue = nullptr;
      }
      return *this;
    }
    CharPtr& operator=(const CharPtr& v) {
      *this = (const char*)v;
      return *this;
    }

    operator const char*() const { return mValue.get(); }

   private:
    UniqueFreePtr<const char> mValue;
  };

  /**
   * The directory of the application to be run. May be null if the
   * xulrunner and the app are installed into the same directory.
   */
  nsCOMPtr<nsIFile> directory;

  /**
   * The name of the application vendor. This must be ASCII, and is normally
   * mixed-case, e.g. "Mozilla". Optional (may be null), but highly
   * recommended. Must not be the empty string.
   */
  CharPtr vendor;

  /**
   * The name of the application. This must be ASCII, and is normally
   * mixed-case, e.g. "Firefox". Required (must not be null or an empty
   * string).
   */
  CharPtr name;

  /**
   * The internal name of the application for remoting purposes. When left
   * unspecified, "name" is used instead. This must be ASCII, and is normally
   * lowercase, e.g. "firefox". Optional (may be null but not an empty string).
   */
  CharPtr remotingName;

  /**
   * The major version, e.g. "0.8.0+". Optional (may be null), but
   * required for advanced application features such as the extension
   * manager and update service. Must not be the empty string.
   */
  CharPtr version;

  /**
   * The application's build identifier, e.g. "2004051604"
   */
  CharPtr buildID;

  /**
   * The application's UUID. Used by the extension manager to determine
   * compatible extensions. Optional, but required for advanced application
   * features such as the extension manager and update service.
   *
   * This has traditionally been in the form
   * "{AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE}" but for new applications
   * a more readable form is encouraged: "appname@vendor.tld". Only
   * the following characters are allowed: a-z A-Z 0-9 - . @ _ { } *
   */
  CharPtr ID;

  /**
   * The copyright information to print for the -h commandline flag,
   * e.g. "Copyright (c) 2003 mozilla.org".
   */
  CharPtr copyright;

  /**
   * Combination of NS_XRE_ prefixed flags (defined below).
   */
  uint32_t flags = 0;

  /**
   * The location of the XRE. XRE_main may not be able to figure this out
   * programatically.
   */
  nsCOMPtr<nsIFile> xreDirectory;

  /**
   * The minimum/maximum compatible XRE version.
   */
  CharPtr minVersion;
  CharPtr maxVersion;

  /**
   * The server URL to send crash reports to.
   */
  CharPtr crashReporterURL;

  /**
   * The profile directory that will be used. Optional (may be null). Must not
   * be the empty string, must be ASCII. The path is split into components
   * along the path separator characters '/' and '\'.
   *
   * The application data directory ("UAppData", see below) is normally
   * composed as follows, where $HOME is platform-specific:
   *
   *   UAppData = $HOME[/$vendor]/$name
   *
   * If present, the 'profile' string will be used instead of the combination of
   * vendor and name as follows:
   *
   *   UAppData = $HOME/$profile
   */
  CharPtr profile;

  /**
   * The application name to use in the User Agent string.
   */
  CharPtr UAName;

  /**
   * The URL to the source revision for this build of the application.
   */
  CharPtr sourceURL;

  /**
   * The URL to use to check for updates.
   */
  CharPtr updateURL;

#if defined(XP_WIN) && defined(MOZ_SANDBOX)
  /**
   * Chromium sandbox BrokerServices.
   */
  sandbox::BrokerServices* sandboxBrokerServices = nullptr;
  mozilla::sandboxing::PermissionsService* sandboxPermissionsService;
#endif
};

/**
 * Indicates whether or not the profile migrator service may be
 * invoked at startup when creating a profile.
 */
#define NS_XRE_ENABLE_PROFILE_MIGRATOR (1 << 1)

/**
 * Indicates whether or not to use Breakpad crash reporting.
 */
#define NS_XRE_ENABLE_CRASH_REPORTER (1 << 3)

/**
 * A static version of the XRE app data is compiled into the application
 * so that it is not necessary to read application.ini at startup.
 *
 * This structure is initialized into and matches nsXREAppData
 */
struct StaticXREAppData {
  const char* vendor;
  const char* name;
  const char* remotingName;
  const char* version;
  const char* buildID;
  const char* ID;
  const char* copyright;
  uint32_t flags;
  const char* minVersion;
  const char* maxVersion;
  const char* crashReporterURL;
  const char* profile;
  const char* UAName;
  const char* sourceURL;
  const char* updateURL;
};

}  // namespace mozilla

#endif  // XREAppData_h
