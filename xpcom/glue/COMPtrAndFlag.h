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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Kyle Huey <me@kylehuey.com>
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

#ifndef mozilla_PtrAndFlag_h__
#define mozilla_PtrAndFlag_h__

#include "nsCOMPtr.h"
#include "mozilla/Types.h"

namespace mozilla {

namespace please_dont_use_this_directly {
template <class T>
class COMPtrAndFlagGetterAddRefs;
}

// A class designed to allow stealing a bit from an nsCOMPtr.
template <class T>
class COMPtrAndFlag
{
  // A RAII class that saves/restores the flag, allowing the COMPtr to be
  // manipulated directly.
  class AutoFlagPersister
  {
  public:
    AutoFlagPersister(uintptr_t* aPtr)
      : mPtr(aPtr), mFlag(*aPtr & 0x1)
    {
      if (mFlag)
        *mPtr &= ~0x1;
    }

    ~AutoFlagPersister()
    {
      if (mFlag)
        *mPtr |= 0x1;
    }
  private:
    uintptr_t *mPtr;
    bool mFlag;
  };

  template <class U>
  friend class please_dont_use_this_directly::COMPtrAndFlagGetterAddRefs;

public:
  COMPtrAndFlag()
  {
    Set(nsnull, false);
  }

  COMPtrAndFlag(T* aPtr, bool aFlag)
  {
    Set(aPtr, aFlag);
  }

  COMPtrAndFlag(const nsQueryInterface aPtr, bool aFlag)
  {
    Set(aPtr, aFlag);
  }

  COMPtrAndFlag(const already_AddRefed<T>& aPtr, bool aFlag)
  {
    Set(aPtr, aFlag);
  }

  ~COMPtrAndFlag()
  {
    // Make sure we unset the flag before nsCOMPtr's dtor tries to
    // Release, or we might explode.
    UnsetFlag();
  }

  COMPtrAndFlag<T>&
  operator= (const COMPtrAndFlag<T>& rhs)
  {
    Set(rhs.Ptr(), rhs.Flag());
    return *this;
  }

  void Set(T* aPtr, bool aFlag)
  {
    SetInternal(aPtr, aFlag);
  }

  void Set(const nsQueryInterface aPtr, bool aFlag)
  {
    SetInternal(aPtr, aFlag);
  }

  void Set(const already_AddRefed<T>& aPtr, bool aFlag)
  {
    SetInternal(aPtr, aFlag);
  }

  void UnsetFlag()
  {
    SetFlag(false);
  }

  void SetFlag(bool aFlag)
  {
    if (aFlag) {
      *VoidPtr() |= 0x1;
    } else {
      *VoidPtr() &= ~0x1;
    }
  }

  bool Flag() const
  {
    return *VoidPtr() & 0x1;
  }

  void SetPtr(T* aPtr)
  {
    SetInternal(aPtr);
  }

  void SetPtr(const nsQueryInterface aPtr)
  {
    SetInternal(aPtr);
  }

  void SetPtr(const already_AddRefed<T>& aPtr)
  {
    SetInternal(aPtr);
  }

  T* Ptr() const
  {
    return reinterpret_cast<T*>(*VoidPtr() & ~0x1);
  }

  void Clear()
  {
    Set(nsnull, false);
  }

private:
  template<class PtrType>
  void SetInternal(PtrType aPtr, bool aFlag)
  {
    UnsetFlag();
    mCOMPtr = aPtr;
    SetFlag(aFlag);
  }

  template<class PtrType>
  void SetInternal(PtrType aPtr)
  {
    AutoFlagPersister saveFlag(VoidPtr());
    mCOMPtr = aPtr;
  }

  uintptr_t* VoidPtr() const {
    return (uintptr_t*)(&mCOMPtr);
  }

  // Don't modify this without using the AutoFlagPersister, or you will explode
  nsCOMPtr<T> mCOMPtr;
};

namespace please_dont_use_this_directly {
/*
  ...

  This class is designed to be used for anonymous temporary objects in the
  argument list of calls that return COM interface pointers, e.g.,

    COMPtrAndFlag<IFoo> fooP;
    ...->GetAddRefedPointer(getter_AddRefs(fooP))

  DO NOT USE THIS TYPE DIRECTLY IN YOUR CODE.  Use |getter_AddRefs()| instead.

  When initialized with a |COMPtrAndFlag|, as in the example above, it returns
  a |void**|, a |T**|, or an |nsISupports**| as needed, that the
  outer call (|GetAddRefedPointer| in this case) can fill in.
*/
template <class T>
class COMPtrAndFlagGetterAddRefs
{
  public:
    explicit
    COMPtrAndFlagGetterAddRefs( COMPtrAndFlag<T>& aSmartPtr )
        : mTargetSmartPtr(aSmartPtr), mFlag(aSmartPtr.Flag())
      {
        if (mFlag)
          aSmartPtr.UnsetFlag();
      }

    ~COMPtrAndFlagGetterAddRefs()
      {
        if (mFlag)
          mTargetSmartPtr.SetFlag(true);
      }

    operator void**()
      {
        return reinterpret_cast<void**>(mTargetSmartPtr.VoidPtr());
      }

    operator T**()
      {
        return reinterpret_cast<T**>(mTargetSmartPtr.VoidPtr());
      }

    T*&
    operator*()
      {
        return *reinterpret_cast<T**>(mTargetSmartPtr.VoidPtr());
      }

  private:
    COMPtrAndFlag<T>& mTargetSmartPtr;
    bool mFlag;
};

} // namespace please_dont_use_this_directly

} // namespace mozilla

template <class T>
inline
mozilla::please_dont_use_this_directly::COMPtrAndFlagGetterAddRefs<T>
getter_AddRefs( mozilla::COMPtrAndFlag<T>& aSmartPtr )
  /*
    Used around a |COMPtrWithFlag| when 
    ...makes the class |COMPtrWithFlag::GetterAddRefs<T>| invisible.
  */
{
  return mozilla::please_dont_use_this_directly::COMPtrAndFlagGetterAddRefs<T>(aSmartPtr);
}


#endif
