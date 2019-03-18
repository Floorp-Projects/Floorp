/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAppDirectoryServiceDefs_h___
#define nsAppDirectoryServiceDefs_h___

//========================================================================================
//
// Defines property names for directories available from standard
// nsIDirectoryServiceProviders. These keys are not guaranteed to exist because
// the nsIDirectoryServiceProviders which provide them are optional.
//
// Keys whose definition ends in "DIR" or "FILE" return a single nsIFile (or
// subclass). Keys whose definition ends in "LIST" return an nsISimpleEnumerator
// which enumerates a list of file objects.
//
// System and XPCOM level properties are defined in nsDirectoryServiceDefs.h.
//
//========================================================================================

// --------------------------------------------------------------------------------------
// Files and directories which exist on a per-product basis
// --------------------------------------------------------------------------------------

#define NS_APP_APPLICATION_REGISTRY_FILE "AppRegF"
#define NS_APP_APPLICATION_REGISTRY_DIR "AppRegD"

#define NS_APP_DEFAULTS_50_DIR "DefRt"  // The root dir of all defaults dirs
#define NS_APP_PREF_DEFAULTS_50_DIR "PrfDef"

#define NS_APP_USER_PROFILES_ROOT_DIR \
  "DefProfRt"  // The dir where user profile dirs live.
#define NS_APP_USER_PROFILES_LOCAL_ROOT_DIR \
  "DefProfLRt"  // The dir where user profile temp dirs live.

#define NS_APP_RES_DIR "ARes"
#define NS_APP_CHROME_DIR "AChrom"
#define NS_APP_PLUGINS_DIR \
  "APlugns"  // Deprecated - use NS_APP_PLUGINS_DIR_LIST

#define NS_APP_CHROME_DIR_LIST "AChromDL"
#define NS_APP_PLUGINS_DIR_LIST "APluginsDL"
#define NS_APP_DISTRIBUTION_SEARCH_DIR_LIST "SrchPluginsDistDL"

// --------------------------------------------------------------------------------------
// Files and directories which exist on a per-profile basis
// These locations are typically provided by the profile mgr
// --------------------------------------------------------------------------------------

// In a shared profile environment, prefixing a profile-relative
// key with NS_SHARED returns a location that is shared by
// other users of the profile. Without this prefix, the consumer
// has exclusive access to this location.

#define NS_SHARED "SHARED"

#define NS_APP_PREFS_50_DIR "PrefD"  // Directory which contains user prefs
#define NS_APP_PREFS_50_FILE "PrefF"
#define NS_APP_PREFS_DEFAULTS_DIR_LIST "PrefDL"
#define NS_APP_PREFS_OVERRIDE_DIR \
  "PrefDOverride"  // Directory for per-profile defaults

#define NS_APP_USER_PROFILE_50_DIR "ProfD"
#define NS_APP_USER_PROFILE_LOCAL_50_DIR "ProfLD"

#define NS_APP_USER_CHROME_DIR "UChrm"

#define NS_APP_USER_PANELS_50_FILE "UPnls"
#define NS_APP_CACHE_PARENT_DIR "cachePDir"

#define NS_APP_INSTALL_CLEANUP_DIR \
  "XPIClnupD"  // location of xpicleanup.dat xpicleanup.exe

#define NS_APP_INDEXEDDB_PARENT_DIR "indexedDBPDir"

#define NS_APP_PERMISSION_PARENT_DIR "permissionDBPDir"

#if defined(MOZ_SANDBOX)
//
// NS_APP_CONTENT_PROCESS_TEMP_DIR refers to a directory that is read and
// write accessible from a sandboxed content process. The key may be used in
// either process, but the directory is intended to be used for short-lived
// files that need to be saved to the filesystem by the content process and
// don't need to survive browser restarts. The directory is reset on startup.
// The key is only valid when MOZ_SANDBOX is defined. When
// MOZ_SANDBOX is defined, the directory the key refers to differs
// depending on whether or not content sandboxing is enabled.
//
// When MOZ_SANDBOX is defined and sandboxing is enabled (versus
// manually disabled via prefs), the content process replaces NS_OS_TEMP_DIR
// with NS_APP_CONTENT_PROCESS_TEMP_DIR so that legacy code in content
// attempting to write to NS_OS_TEMP_DIR will write to
// NS_APP_CONTENT_PROCESS_TEMP_DIR instead. When MOZ_SANDBOX is
// defined but sandboxing is disabled, NS_APP_CONTENT_PROCESS_TEMP_DIR
// falls back to NS_OS_TEMP_DIR in both content and chrome processes.
//
// New code should avoid writing to the filesystem from the content process
// and should instead proxy through the parent process whenever possible.
//
// At present, all sandboxed content processes use the same directory for
// NS_APP_CONTENT_PROCESS_TEMP_DIR, but that should not be relied upon.
//
#  define NS_APP_CONTENT_PROCESS_TEMP_DIR "ContentTmpD"
#else
// Otherwise NS_APP_CONTENT_PROCESS_TEMP_DIR must match NS_OS_TEMP_DIR.
#  define NS_APP_CONTENT_PROCESS_TEMP_DIR "TmpD"
#endif  // defined(MOZ_SANDBOX)

#if defined(MOZ_SANDBOX)
#  define NS_APP_PLUGIN_PROCESS_TEMP_DIR "PluginTmpD"
#else
#  define NS_APP_PLUGIN_PROCESS_TEMP_DIR "TmpD"
#endif

#endif  // nsAppDirectoryServiceDefs_h___
