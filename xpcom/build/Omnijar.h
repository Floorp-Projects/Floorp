/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_Omnijar_h
#define mozilla_Omnijar_h

#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIFile.h"
#include "nsZipArchive.h"

class nsIURI;

namespace mozilla {

class Omnijar
{
private:
  /**
   * Store an nsIFile for an omni.jar. We can store two paths here, one
   * for GRE (corresponding to resource://gre/) and one for APP
   * (corresponding to resource:/// and resource://app/), but only
   * store one when both point to the same location (unified).
   */
  static nsIFile* sPath[2];

  /**
   * Cached nsZipArchives for the corresponding sPath
   */
  static nsZipArchive* sReader[2];

  /**
   * Has Omnijar::Init() been called?
   */
  static bool sInitialized;

public:
  enum Type
  {
    GRE = 0,
    APP = 1
  };

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
   * Cleans up the Omnijar API
   */
  static void CleanUp();

  /**
   * Returns an nsIFile pointing to the omni.jar file for GRE or APP.
   * Returns nullptr when there is no corresponding omni.jar.
   * Also returns nullptr for APP in the unified case.
   */
  static inline already_AddRefed<nsIFile> GetPath(Type aType)
  {
    MOZ_ASSERT(IsInitialized(), "Omnijar not initialized");
    nsCOMPtr<nsIFile> path = sPath[aType];
    return path.forget();
  }

  /**
   * Returns whether GRE or APP use an omni.jar. Returns PR_False for
   * APP when using an omni.jar in the unified case.
   */
  static inline bool HasOmnijar(Type aType)
  {
    MOZ_ASSERT(IsInitialized(), "Omnijar not initialized");
    return !!sPath[aType];
  }

  /**
   * Returns a nsZipArchive pointer for the omni.jar file for GRE or
   * APP. Returns nullptr in the same cases GetPath() would.
   */
  static inline already_AddRefed<nsZipArchive> GetReader(Type aType)
  {
    MOZ_ASSERT(IsInitialized(), "Omnijar not initialized");
    nsRefPtr<nsZipArchive> reader = sReader[aType];
    return reader.forget();
  }

  /**
   * Returns a nsZipArchive pointer for the given path IAOI the given
   * path is the omni.jar for either GRE or APP.
   */
  static already_AddRefed<nsZipArchive> GetReader(nsIFile* aPath);

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

} /* namespace mozilla */

#endif /* mozilla_Omnijar_h */
