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
 * The Original Code is an RAII helper classes for Windows development.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian R. Bondy <netzen@gmail.com>
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

#ifndef nsWindowsHelpers_h
#define nsWindowsHelpers_h

#include "nsAutoRef.h"
#include "nscore.h"

template<>
class nsAutoRefTraits<HKEY>
{
public:
  typedef HKEY RawRef;
  static HKEY Void() 
  { 
    return NULL; 
  }

  static void Release(RawRef aFD) 
  { 
    if (aFD != Void()) {
      RegCloseKey(aFD);
    }
  }
};

template<>
class nsAutoRefTraits<SC_HANDLE>
{
public:
  typedef SC_HANDLE RawRef;
  static SC_HANDLE Void() 
  { 
    return NULL; 
  }

  static void Release(RawRef aFD) 
  { 
    if (aFD != Void()) {
      CloseServiceHandle(aFD);
    }
  }
};

template<>
class nsSimpleRef<HANDLE>
{
protected:
  typedef HANDLE RawRef;

  nsSimpleRef() : mRawRef(NULL) 
  {
  }

  nsSimpleRef(RawRef aRawRef) : mRawRef(aRawRef) 
  {
  }

  bool HaveResource() const 
  {
    return mRawRef != NULL && mRawRef != INVALID_HANDLE_VALUE;
  }

public:
  RawRef get() const 
  {
    return mRawRef;
  }

  static void Release(RawRef aRawRef) 
  {
    if (aRawRef != NULL && aRawRef != INVALID_HANDLE_VALUE) {
      CloseHandle(aRawRef);
    }
  }
  RawRef mRawRef;
};

typedef nsAutoRef<HKEY> nsAutoRegKey;
typedef nsAutoRef<SC_HANDLE> nsAutoServiceHandle;
typedef nsAutoRef<HANDLE> nsAutoHandle;

#endif
