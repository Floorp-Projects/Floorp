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

#include "nsCheapSets.h"

nsCheapStringSet::~nsCheapStringSet()
{
  nsStringHashSet* set = GetHash();
  if (set) {
    delete set;
  } else {
    delete GetStr();
  }
}

/**
 * Put a string into the table
 */
nsresult
nsCheapStringSet::Put(const nsAString& aVal)
{
  // Add the value to the hash if it is there
  nsStringHashSet* set = GetHash();
  if (set) {
    return set->Put(aVal);
  }

  // If a string is already there, create a hashtable and both of these to it
  if (GetStr()) {
    nsAString* oldStr = GetStr();
    nsresult rv = InitHash(&set);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = set->Put(*oldStr);
    delete oldStr;
    NS_ENSURE_SUCCESS(rv, rv);

    return set->Put(aVal);
  }

  // Nothing exists in the hash right now, so just set the single string
  return SetStr(aVal);
}

void
nsCheapStringSet::Remove(const nsAString& aVal)
{
  // Remove from the hash if the hash is there
  nsStringHashSet* set = GetHash();
  if (set) {
    set->Remove(aVal);
    return;
  }

  // Remove the string if there is just a string
  nsAString* str = GetStr();
  if (str && str->Equals(aVal)) {
    delete str;
    mValOrHash = nsnull;
  }
}

nsresult
nsCheapStringSet::InitHash(nsStringHashSet** aSet)
{
  nsStringHashSet* newSet = new nsStringHashSet();
  if (!newSet) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = newSet->Init(10);
  NS_ENSURE_SUCCESS(rv, rv);

  mValOrHash = newSet;
  *aSet = newSet;
  return NS_OK;
}


nsCheapInt32Set::~nsCheapInt32Set()
{
  delete GetHash();
}

nsresult
nsCheapInt32Set::Put(PRInt32 aVal)
{
  // Add the value to the hash or set the pointer as an int
  nsInt32HashSet* set = GetHash();
  if (set) {
    return set->Put(aVal);
  }

  // Create the hash and add the value to it if there is an int already
  if (IsInt()) {
    PRInt32 oldInt = GetInt();

    nsresult rv = InitHash(&set);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = set->Put(oldInt);
    NS_ENSURE_SUCCESS(rv, rv);

    return set->Put(aVal);
  }

  // Create the hash anyway if the int is negative (negative numbers cannot
  // fit into our PtrBits abstraction)
  if (aVal < 0) {
    nsresult rv = InitHash(&set);
    NS_ENSURE_SUCCESS(rv, rv);

    return set->Put(aVal);
  }

  // Finally, just set the int if we can't do anything with hashes
  SetInt(aVal);
  return NS_OK;
}
 
void
nsCheapInt32Set::Remove(PRInt32 aVal)
{
  nsInt32HashSet* set = GetHash();
  if (set) {
    set->Remove(aVal);
  } else if (IsInt() && GetInt() == aVal) {
    mValOrHash = nsnull;
  }
}

nsresult
nsCheapInt32Set::InitHash(nsInt32HashSet** aSet)
{
  nsInt32HashSet* newSet = new nsInt32HashSet();
  if (!newSet) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = newSet->Init(10);
  NS_ENSURE_SUCCESS(rv, rv);

  mValOrHash = newSet;
  *aSet = newSet;
  return NS_OK;
}
