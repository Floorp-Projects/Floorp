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

#ifndef nsTHashKeys_h__
#define nsTHashKeys_h__

#include "nsAString.h"
#include "nsString.h"
#include "nsID.h"
#include "nsCRT.h"
#include "nsReadableUtils.h"
#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "pldhash.h"
#include NEW_H

/** @file nsHashKeys.h
 * standard HashKey classes for nsBaseHashtable and relatives. Each of these
 * classes follows the nsTHashtable::EntryType specification
 *
 * Lightweight keytypes provided here:
 * nsStringHashKey
 * nsCStringHashKey
 * nsUint32HashKey
 * nsISupportsHashKey
 * nsIDHashKey
 */

/**
 * hashkey wrapper using nsAString KeyType
 *
 * @see nsTHashtable::EntryType for specification
 */
class NS_COM nsStringHashKey : public PLDHashEntryHdr
{
public:
  typedef const nsAString& KeyType;
  typedef const nsAString* KeyTypePointer;

  nsStringHashKey(KeyTypePointer aStr) : mStr(*aStr) { }
  nsStringHashKey(const nsStringHashKey& toCopy) : mStr(toCopy.mStr) { }
  ~nsStringHashKey() { }

  KeyType GetKey() const { return mStr; }
  KeyTypePointer GetKeyPointer() const { return &mStr; }
  PRBool KeyEquals(const KeyTypePointer aKey) const
  {
    return mStr.Equals(*aKey);
  }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
  static PLDHashNumber HashKey(const KeyTypePointer aKey)
  {
    return HashString(*aKey);
  }
  enum { ALLOW_MEMMOVE = PR_TRUE };

private:
  const nsString mStr;
};

/**
 * hashkey wrapper using nsACString KeyType
 *
 * @see nsTHashtable::EntryType for specification
 */
class NS_COM nsCStringHashKey : public PLDHashEntryHdr
{
public:
  typedef const nsACString& KeyType;
  typedef const nsACString* KeyTypePointer;
  
  nsCStringHashKey(const nsACString* aStr) : mStr(*aStr) { }
  nsCStringHashKey(const nsCStringHashKey& toCopy) : mStr(toCopy.mStr) { }
  ~nsCStringHashKey() { }

  KeyType GetKey() const { return mStr; }
  KeyTypePointer GetKeyPointer() const { return &mStr; }

  PRBool KeyEquals(KeyTypePointer aKey) const { return mStr.Equals(*aKey); }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey)
  {
    return HashString(*aKey);
  }
  enum { ALLOW_MEMMOVE = PR_TRUE };

private:
  const nsCString mStr;
};

/**
 * hashkey wrapper using PRUint32 KeyType
 *
 * @see nsTHashtable::EntryType for specification
 */
class NS_COM nsUint32HashKey : public PLDHashEntryHdr
{
public:
  typedef const PRUint32& KeyType;
  typedef const PRUint32* KeyTypePointer;
  
  nsUint32HashKey(KeyTypePointer aKey) : mValue(*aKey) { }
  nsUint32HashKey(const nsUint32HashKey& toCopy) : mValue(toCopy.mValue) { }
  ~nsUint32HashKey() { }

  KeyType GetKey() const { return mValue; }
  KeyTypePointer GetKeyPointer() const { return &mValue; }
  PRBool KeyEquals(KeyTypePointer aKey) const { return *aKey == mValue; }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey) { return *aKey; }
  enum { ALLOW_MEMMOVE = PR_TRUE };

private:
  const PRUint32 mValue;
};

/**
 * hashkey wrapper using nsISupports* KeyType
 *
 * @see nsTHashtable::EntryType for specification
 */
class NS_COM nsISupportsHashKey : public PLDHashEntryHdr
{
public:
  typedef nsISupports* KeyType;
  typedef const nsISupports* KeyTypePointer;

  nsISupportsHashKey(const nsISupports* key) :
    mSupports(NS_CONST_CAST(nsISupports*,key)) { }
  nsISupportsHashKey(const nsISupportsHashKey& toCopy) :
    mSupports(toCopy.mSupports) { }
  ~nsISupportsHashKey() { }

  KeyType GetKey() const { return mSupports; }
  KeyTypePointer GetKeyPointer() const { return mSupports; }
  
  PRBool KeyEquals(KeyTypePointer aKey) const { return aKey == mSupports; }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey)
  {
    return NS_PTR_TO_INT32(aKey) >>2;
  }
  enum { ALLOW_MEMMOVE = PR_TRUE };

private:
  nsCOMPtr<nsISupports> mSupports;
};

/**
 * hashkey wrapper using nsID KeyType
 *
 * @see nsTHashtable::EntryType for specification
 */
class NS_COM nsIDHashKey : public PLDHashEntryHdr
{
public:
  typedef const nsID& KeyType;
  typedef const nsID* KeyTypePointer;
  
  nsIDHashKey(const nsID* id) : mID(*id) { }
  nsIDHashKey(const nsIDHashKey& toCopy) : mID(toCopy.mID) { }
  ~nsIDHashKey() { }

  KeyType GetKey() const { return mID; }
  KeyTypePointer GetKeyPointer() const { return &mID; }
  PRBool operator==(KeyTypePointer aKey) const { return aKey->Equals(mID); }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey);
  enum { ALLOW_MEMMOVE = PR_TRUE };

private:
  const nsID mID;
};

#endif // nsTHashKeys_h__
