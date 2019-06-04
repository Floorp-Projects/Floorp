/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsVersionComparator_h__
#define nsVersionComparator_h__

#include "mozilla/Char16.h"
#include "nscore.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#if defined(XP_WIN) && !defined(UPDATER_NO_STRING_GLUE_STL)
#  include <wchar.h>
#  include "nsString.h"
#endif

/**
 * In order to compare version numbers in Mozilla, you need to use the
 * mozilla::Version class.  You can construct an object of this type by passing
 * in a string version number to the constructor.  Objects of this type can be
 * compared using the standard comparison operators.
 *
 * For example, let's say that you want to make sure that a given version
 * number is not older than 15.a2.  Here's how you would write a function to
 * do that.
 *
 * bool IsVersionValid(const char* version) {
 *   return mozilla::Version("15.a2") <= mozilla::Version(version);
 * }
 *
 * Or, since Version's constructor is implicit, you can simplify this code:
 *
 * bool IsVersionValid(const char* version) {
 *   return mozilla::Version("15.a2") <= version;
 * }
 */

namespace mozilla {

/**
 * Compares the version strings provided.
 *
 * Returns 0 if the versions match, < 0 if aStrB > aStrA and > 0 if
 * aStrA > aStrB.
 */
int32_t CompareVersions(const char* aStrA, const char* aStrB);

#ifdef XP_WIN
/**
 * As above but for wide character strings.
 */
int32_t CompareVersions(const char16_t* aStrA, const char16_t* aStrB);
#endif

struct Version {
  explicit Version(const char* aVersionString) {
    versionContent = strdup(aVersionString);
  }

  const char* ReadContent() const { return versionContent; }

  ~Version() { free(versionContent); }

  bool operator<(const Version& aRhs) const {
    return CompareVersions(versionContent, aRhs.ReadContent()) < 0;
  }
  bool operator<=(const Version& aRhs) const {
    return CompareVersions(versionContent, aRhs.ReadContent()) < 1;
  }
  bool operator>(const Version& aRhs) const {
    return CompareVersions(versionContent, aRhs.ReadContent()) > 0;
  }
  bool operator>=(const Version& aRhs) const {
    return CompareVersions(versionContent, aRhs.ReadContent()) > -1;
  }
  bool operator==(const Version& aRhs) const {
    return CompareVersions(versionContent, aRhs.ReadContent()) == 0;
  }
  bool operator!=(const Version& aRhs) const {
    return CompareVersions(versionContent, aRhs.ReadContent()) != 0;
  }
  bool operator<(const char* aRhs) const {
    return CompareVersions(versionContent, aRhs) < 0;
  }
  bool operator<=(const char* aRhs) const {
    return CompareVersions(versionContent, aRhs) < 1;
  }
  bool operator>(const char* aRhs) const {
    return CompareVersions(versionContent, aRhs) > 0;
  }
  bool operator>=(const char* aRhs) const {
    return CompareVersions(versionContent, aRhs) > -1;
  }
  bool operator==(const char* aRhs) const {
    return CompareVersions(versionContent, aRhs) == 0;
  }
  bool operator!=(const char* aRhs) const {
    return CompareVersions(versionContent, aRhs) != 0;
  }

 private:
  char* versionContent;
};

}  // namespace mozilla

#endif  // nsVersionComparator_h__
