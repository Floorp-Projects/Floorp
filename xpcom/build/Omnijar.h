/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_Omnijar_h
#define mozilla_Omnijar_h

#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIFile.h"
#include "nsZipArchive.h"

#include "mozilla/StaticPtr.h"

namespace mozilla {

class Omnijar {
 private:
  /**
   * Store an nsIFile for an omni.jar. We can store two paths here, one
   * for GRE (corresponding to resource://gre/) and one for APP
   * (corresponding to resource:/// and resource://app/), but only
   * store one when both point to the same location (unified).
   */
  static StaticRefPtr<nsIFile> sPath[2];

  /**
   * Cached nsZipArchives for the corresponding sPath
   */
  static StaticRefPtr<nsZipArchive> sReader[2];

  /**
   * Cached nsZipArchives for the outer jar, when using nested jars.
   * Otherwise nullptr.
   */
  static StaticRefPtr<nsZipArchive> sOuterReader[2];

  /**
   * Has Omnijar::Init() been called?
   */
  static bool sInitialized;

  /**
   * Is using unified GRE/APP jar?
   */
  static bool sIsUnified;

 public:
  enum Type { GRE = 0, APP = 1 };

 private:
  /**
   * Returns whether we are using nested jars.
   */
  static inline bool IsNested(Type aType) {
    MOZ_ASSERT(IsInitialized(), "Omnijar not initialized");
    return !!sOuterReader[aType];
  }

  /**
   * Returns a nsZipArchive pointer for the outer jar file when using nested
   * jars. Returns nullptr in the same cases GetPath() would, or if not using
   * nested jars.
   */
  static inline already_AddRefed<nsZipArchive> GetOuterReader(Type aType) {
    MOZ_ASSERT(IsInitialized(), "Omnijar not initialized");
    RefPtr<nsZipArchive> reader = sOuterReader[aType].get();
    return reader.forget();
  }

 public:
  /**
   * Returns whether SetBase has been called at least once with
   * a valid nsIFile
   */
  static inline bool IsInitialized() { return sInitialized; }

  /**
   * Initializes the Omnijar API with the given directory or file for GRE and
   * APP. Each of the paths given can be:
   * - a file path, pointing to the omnijar file,
   * - a directory path, pointing to a directory containing an "omni.jar" file,
   * - nullptr for autodetection of an "omni.jar" file.
   */
  static void Init(nsIFile* aGrePath = nullptr, nsIFile* aAppPath = nullptr);

  /**
   * Initializes the Omnijar API for a child process, given its argument
   * list, if the `-greomni` flag and optionally also the `-appomni` flag
   * is present.  (`-appomni` is absent in the case of a unified jar.)  If
   * neither flag is present, the Omnijar API is not initialized.  The
   * flags, if present, will be removed from the argument list.
   */
  static void ChildProcessInit(int& aArgc, char** aArgv);

  /**
   * Cleans up the Omnijar API
   */
  static void CleanUp();

  /**
   * Returns an nsIFile pointing to the omni.jar file for GRE or APP.
   * Returns nullptr when there is no corresponding omni.jar.
   * Also returns nullptr for APP in the unified case.
   */
  static inline already_AddRefed<nsIFile> GetPath(Type aType) {
    MOZ_ASSERT(IsInitialized(), "Omnijar not initialized");
    nsCOMPtr<nsIFile> path = sPath[aType].get();
    return path.forget();
  }

  /**
   * Returns whether GRE or APP use an omni.jar. Returns PR_False for
   * APP when using an omni.jar in the unified case.
   */
  static inline bool HasOmnijar(Type aType) {
    MOZ_ASSERT(IsInitialized(), "Omnijar not initialized");
    return !!sPath[aType];
  }

  /**
   * Returns a nsZipArchive pointer for the omni.jar file for GRE or
   * APP. Returns nullptr in the same cases GetPath() would.
   */
  static inline already_AddRefed<nsZipArchive> GetReader(Type aType) {
    MOZ_ASSERT(IsInitialized(), "Omnijar not initialized");
    RefPtr<nsZipArchive> reader = sReader[aType].get();
    return reader.forget();
  }

  /**
   * Returns a nsZipArchive pointer for the given path IAOI the given
   * path is the omni.jar for either GRE or APP.
   */
  static already_AddRefed<nsZipArchive> GetReader(nsIFile* aPath);

  /**
   * In the case of a nested omnijar, this returns the inner reader for the
   * omnijar if aPath points to the outer archive and aEntry is the omnijar
   * entry name. Returns null otherwise.
   * In concrete terms: On Android the omnijar is nested inside the apk archive.
   * GetReader("path/to.apk") returns the outer reader and GetInnerReader(
   * "path/to.apk", "assets/omni.ja") returns the inner reader.
   */
  static already_AddRefed<nsZipArchive> GetInnerReader(
      nsIFile* aPath, const nsACString& aEntry);

  /**
   * Returns the URI string corresponding to the omni.jar or directory
   * for GRE or APP. i.e. jar:/path/to/omni.jar!/ for omni.jar and
   * /path/to/base/dir/ otherwise. Returns an empty string for APP in
   * the unified case.
   * The returned URI is guaranteed to end with a slash.
   */
  static nsresult GetURIString(Type aType, nsACString& aResult);

 private:
  /**
   * Used internally, respectively by Init() and CleanUp()
   */
  static void InitOne(nsIFile* aPath, Type aType);
  static void CleanUpOne(Type aType);
}; /* class Omnijar */

/**
 * Returns whether or not the currently running build is an unpackaged
 * developer build. This check is implemented by looking for omni.ja in the
 * the obj/dist dir. We use this routine to detect when the build dir will
 * use symlinks to the repo and object dir.
 */
inline bool IsPackagedBuild() {
  return Omnijar::HasOmnijar(mozilla::Omnijar::GRE);
}

} /* namespace mozilla */

#endif /* mozilla_Omnijar_h */
