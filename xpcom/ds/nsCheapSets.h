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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef __nsCheapSets_h__
#define __nsCheapSets_h__

#include "nsHashSets.h"

/**
 * A string set that takes up minimal size when there are 0 or 1 strings in the
 * set.  Use for cases where sizes of 0 and 1 are even slightly common.
 */
class NS_COM nsCheapStringSet {
public:
  nsCheapStringSet() : mValOrHash(nsnull)
  {
  }
  ~nsCheapStringSet();

  /**
   * Put a string into the set
   * @param aVal the value to put in
   */
  nsresult Put(const nsAString& aVal);

  /**
   * Remove a string from the set
   * @param aVal the string to remove
   */
  void Remove(const nsAString& aVal);

  /**
   * Check if the set contains a particular string
   * @param aVal the string to check for
   * @return whether the string is in the set
   */
  PRBool Contains(const nsAString& aVal)
  {
    nsStringHashSet* set = GetHash();
    // Check the value from the hash if the hash is there
    if (set) {
      return set->Contains(aVal);
    }

    // Check whether the value is equal to the string if the string is there
    nsAString* str = GetStr();
    return str && str->Equals(aVal);
  }

private:
  typedef PRUword PtrBits;

  /** Get the hash pointer (or null if we're not a hash) */
  nsStringHashSet* GetHash()
  {
    return (PtrBits(mValOrHash) & 0x1) ? nsnull : (nsStringHashSet*)mValOrHash;
  }
  /** Find out whether it is a string */
  nsAString* GetStr()
  {
    return (PtrBits(mValOrHash) & 0x1)
           ? (nsAString*)(PtrBits(mValOrHash) & ~0x1)
           : nsnull;
  }
  /** Set the single string */
  nsresult SetStr(const nsAString& aVal)
  {
    nsString* str = new nsString(aVal);
    if (!str) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    mValOrHash = (nsAString*)(PtrBits(str) | 0x1);
    return NS_OK;
  }
  /** Initialize the hash */
  nsresult InitHash(nsStringHashSet** aSet);

private:
  /** A hash or string ptr, depending on the lower bit (0=hash, 1=string) */
  void* mValOrHash;
};


/**
 * An integer set that takes up only 4 bytes when there are 0 or 1 integers
 * in the set.  Use for cases where sizes of 0 and 1 are even slightly common.
 */
class NS_COM nsCheapInt32Set {
public:
  nsCheapInt32Set() : mValOrHash(nsnull)
  {
  }
  ~nsCheapInt32Set();

  /**
   * Put an int into the set
   */
  nsresult Put(PRInt32 aVal);
 
  /**
   * Remove a int from the set
   * @param aVal the int to remove
   */
  void Remove(PRInt32 aVal);

  /**
   * Check if the set contains a particular int
   * @param aVal the int to check for
   * @return whether the int is in the set
   */
  PRBool Contains(PRInt32 aVal)
  {
    nsInt32HashSet* set = GetHash();
    if (set) {
      return set->Contains(aVal);
    }
    if (IsInt()) {
      return GetInt() == aVal;
    }
    return PR_FALSE;
  }

private:
  typedef PRUword PtrBits;

  /** Get the hash pointer (or null if we're not a hash) */
  nsInt32HashSet* GetHash()
  {
    return PtrBits(mValOrHash) & 0x1 ? nsnull : (nsInt32HashSet*)mValOrHash;
  }
  /** Find out whether it is an integer */
  PRBool IsInt()
  {
    return !!(PtrBits(mValOrHash) & 0x1);
  }
  /** Get the single integer */
  PRInt32 GetInt()
  {
    return PtrBits(mValOrHash) >> 1;
  }
  /** Set the single integer */
  void SetInt(PRInt32 aInt)
  {
    mValOrHash = (void*)((aInt << 1) | 0x1);
  }
  /** Create the hash and initialize */
  nsresult InitHash(nsInt32HashSet** aSet);

private:
  /** A hash or int, depending on the lower bit (0=hash, 1=int) */
  void* mValOrHash;
};


/**
 * XXX We may want an nsCheapVoidSet and nsCheapCStringSet at some point
 */


#endif
