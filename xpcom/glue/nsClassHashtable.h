/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is C++ hashtable templates.
 *
 * The Initial Developer of the Original Code is
 * Benjamin Smedberg.
 * Portions created by the Initial Developer are Copyright (C) 2002
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

#ifndef nsClassHashtable_h__
#define nsClassHashtable_h__

#include "nsBaseHashtable.h"
#include "nsHashKeys.h"
#include "nsAutoPtr.h"

/**
 * templated hashtable class maps keys to C++ object pointers.
 * See nsBaseHashtable for complete declaration.
 * @param KeyClass a wrapper-class for the hashtable key, see nsHashKeys.h
 *   for a complete specification.
 * @param Class the class-type being wrapped
 * @see nsInterfaceHashtable, nsClassHashtable
 */
template<class KeyClass,class T>
class nsClassHashtable :
  public nsBaseHashtable< KeyClass, nsAutoPtr<T>, T* >
{
public:
  typedef typename KeyClass::KeyType KeyType;
  typedef T* UserDataType;
  typedef nsBaseHashtable< KeyClass, nsAutoPtr<T>, T* > base_type;

  /**
   * @copydoc nsBaseHashtable::Get
   * @param pData if the key doesn't exist, pData will be set to nsnull.
   */
  bool Get(KeyType aKey, UserDataType* pData) const;

  /**
   * @copydoc nsBaseHashtable::Get
   * @returns NULL if the key is not present.
   */
  UserDataType Get(KeyType aKey) const;

  /**
   * Remove the entry for the given key from the hashtable and return it in
   * aOut.  If the key is not in the hashtable, aOut's pointer is set to NULL.
   *
   * Normally, an entry is deleted when it's removed from an nsClassHashtable,
   * but this function transfers ownership of the entry back to the caller
   * through aOut -- the entry will be deleted when aOut goes out of scope.
   *
   * @param aKey the key to get and remove from the hashtable
   */
  void RemoveAndForget(KeyType aKey, nsAutoPtr<T> &aOut);
};


/**
 * Thread-safe version of nsClassHashtable
 * @param KeyClass a wrapper-class for the hashtable key, see nsHashKeys.h
 *   for a complete specification.
 * @param Class the class-type being wrapped
 * @see nsInterfaceHashtable, nsClassHashtable
 */
template<class KeyClass,class T>
class nsClassHashtableMT :
  public nsBaseHashtableMT< KeyClass, nsAutoPtr<T>, T* >
{
public:
  typedef typename KeyClass::KeyType KeyType;
  typedef T* UserDataType;
  typedef nsBaseHashtableMT< KeyClass, nsAutoPtr<T>, T* > base_type;

  /**
   * @copydoc nsBaseHashtable::Get
   * @param pData if the key doesn't exist, pData will be set to nsnull.
   */
  bool Get(KeyType aKey, UserDataType* pData) const;
};


//
// nsClassHashtable definitions
//

template<class KeyClass,class T>
bool
nsClassHashtable<KeyClass,T>::Get(KeyType aKey, T** retVal) const
{
  typename base_type::EntryType* ent = this->GetEntry(aKey);

  if (ent)
  {
    if (retVal)
      *retVal = ent->mData;

    return PR_TRUE;
  }

  if (retVal)
    *retVal = nsnull;

  return PR_FALSE;
}

template<class KeyClass,class T>
T*
nsClassHashtable<KeyClass,T>::Get(KeyType aKey) const
{
  typename base_type::EntryType* ent = this->GetEntry(aKey);

  if (!ent)
    return NULL;

  return ent->mData;
}

template<class KeyClass,class T>
void
nsClassHashtable<KeyClass,T>::RemoveAndForget(KeyType aKey, nsAutoPtr<T> &aOut)
{
  aOut = nsnull;
  nsAutoPtr<T> ptr;

  typename base_type::EntryType *ent = this->GetEntry(aKey);
  if (!ent)
    return;

  // Transfer ownership from ent->mData into aOut.
  aOut = ent->mData;

  this->Remove(aKey);
}


//
// nsClassHashtableMT definitions
//

template<class KeyClass,class T>
bool
nsClassHashtableMT<KeyClass,T>::Get(KeyType aKey, T** retVal) const
{
  PR_Lock(this->mLock);

  typename base_type::EntryType* ent = this->GetEntry(aKey);

  if (ent)
  {
    if (retVal)
      *retVal = ent->mData;

    PR_Unlock(this->mLock);

    return PR_TRUE;
  }

  if (retVal)
    *retVal = nsnull;

  PR_Unlock(this->mLock);

  return PR_FALSE;
}

#endif // nsClassHashtable_h__
