/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla XPCOM.
 *
 * The Initial Developer of the Original Code is
 * Benjamin Smedberg <benjamin@smedbergs.us>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

namespace mozilla {

PRInt32 NS_COM_GLUE
CompareVersions(const char *A, const char *B);

#ifdef XP_WIN
PRInt32 NS_COM_GLUE
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

