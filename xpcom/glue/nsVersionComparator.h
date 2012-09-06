/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsVersionComparator_h__
#define nsVersionComparator_h__

#include "nscore.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#if defined(XP_WIN) && !defined(UPDATER_NO_STRING_GLUE_STL)
#include <wchar.h>
#include "nsStringGlue.h"
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
 *
 * On Windows, if your version strings are wide characters, you should use the
 * mozilla::VersionW variant instead.  The semantics of that class is the same
 * as Version.
 */

namespace mozilla {

int32_t NS_COM_GLUE
CompareVersions(const char *A, const char *B);

#ifdef XP_WIN
int32_t NS_COM_GLUE
CompareVersions(const PRUnichar *A, const PRUnichar *B);
#endif

struct NS_COM_GLUE Version
{
  Version(const char* versionString)
  {
    versionContent = strdup(versionString);
  }

  const char* ReadContent() const
  {
    return versionContent;
  }

  ~Version()
  {
    free(versionContent);
  }

  bool operator< (const Version& rhs) const
  {
    return CompareVersions(versionContent, rhs.ReadContent()) == -1;
  }
  bool operator<= (const Version& rhs) const
  {
    return CompareVersions(versionContent, rhs.ReadContent()) < 1;
  }
  bool operator> (const Version& rhs) const
  {
    return CompareVersions(versionContent, rhs.ReadContent()) == 1;
  }
  bool operator>= (const Version& rhs) const
  {
    return CompareVersions(versionContent, rhs.ReadContent()) > -1;
  }
  bool operator== (const Version& rhs) const
  {
    return CompareVersions(versionContent, rhs.ReadContent()) == 0;
  }
  bool operator!= (const Version& rhs) const
  {
    return CompareVersions(versionContent, rhs.ReadContent()) != 0;
  }

 private:
  char* versionContent;
};

#ifdef XP_WIN
struct NS_COM_GLUE VersionW
{
  VersionW(const PRUnichar *versionStringW)
  {
    versionContentW = wcsdup(versionStringW);
  }

  const PRUnichar* ReadContentW() const
  {
    return versionContentW;
  }

  ~VersionW()
  {
    free(versionContentW);
  }

  bool operator< (const VersionW& rhs) const
  {
    return CompareVersions(versionContentW, rhs.ReadContentW()) == -1;
  }
  bool operator<= (const VersionW& rhs) const
  {
    return CompareVersions(versionContentW, rhs.ReadContentW()) < 1;
  }
  bool operator> (const VersionW& rhs) const
  {
    return CompareVersions(versionContentW, rhs.ReadContentW()) == 1;
  }
  bool operator>= (const VersionW& rhs) const
  {
    return CompareVersions(versionContentW, rhs.ReadContentW()) > -1;
  }
  bool operator== (const VersionW& rhs) const
  {
    return CompareVersions(versionContentW, rhs.ReadContentW()) == 0;
  }
  bool operator!= (const VersionW& rhs) const
  {
    return CompareVersions(versionContentW, rhs.ReadContentW()) != 0;
  }

 private:
  PRUnichar* versionContentW;
};
#endif

} // namespace mozilla

#endif // nsVersionComparator_h__

