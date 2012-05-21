/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
