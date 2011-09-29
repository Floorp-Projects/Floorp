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

#ifndef __nsHashSets_h__
#define __nsHashSets_h__

#include "nsDoubleHashtable.h"

/*
 * HASH SETS
 *
 * These hash classes describe hashtables that contain keys without values.
 * This is useful when you want to store things and then just test for their
 * existence later.  We have defined standard ones for string, int and void.
 *
 * nsStringHashSet:  nsAString&
 * nsCStringHashSet: nsACString&
 * nsInt32HashSet:   PRInt32
 * nsVoidHashSet:    void*
 *
 * USAGE:
 * Put(): put a thing of the given type into the set
 * Contains(): whether it contains a thing of the given type
 * To use, you just do: (for example)
 *
 * #include "nsDoubleHashtable.h"
 * nsInt32HashSet mySet;
 * mySet.Init(1);
 * mySet.Put(5);
 * if (mySet.Contains(5)) {
 *   printf("yay\n");
 * }
 *
 * There is a nice convenient macro for declaring empty map classes:
 * DECL_DHASH_SET(CLASSNAME, ENTRY_CLASS, KEY_TYPE)
 * - CLASSNAME: the name of the class
 * - ENTRY_CLASS: the name of the entry class with the key in it
 * - KEY_TYPE: the type of key for Put() and Contains()
 *
 * DHASH_SET(CLASSNAME, ENTRY_CLASS, KEY_TYPE) is the companion macro
 * you must put in the .cpp (implementation) code.
 */

#define DECL_DHASH_SET(CLASSNAME,ENTRY_CLASS,KEY_TYPE)                        \
DECL_DHASH_WRAPPER(CLASSNAME##Super,ENTRY_CLASS,KEY_TYPE)                     \
class DHASH_EXPORT CLASSNAME : public CLASSNAME##Super {                      \
public:                                                                       \
  CLASSNAME() : CLASSNAME##Super() { }                                        \
  ~CLASSNAME() { }                                                            \
  nsresult Put(const KEY_TYPE aKey) {                                         \
    return AddEntry(aKey) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;                   \
  }                                                                           \
  bool Contains(const KEY_TYPE aKey) {                                      \
    return GetEntry(aKey) ? PR_TRUE : PR_FALSE;                               \
  }                                                                           \
};

#define DHASH_SET(CLASSNAME,ENTRY_CLASS,KEY_TYPE)                              \
DHASH_WRAPPER(CLASSNAME##Super,ENTRY_CLASS,KEY_TYPE)

#undef DHASH_EXPORT
#define DHASH_EXPORT

DECL_DHASH_SET(nsStringHashSet, PLDHashStringEntry, nsAString&)
DECL_DHASH_SET(nsCStringHashSet,PLDHashCStringEntry,nsACString&)
DECL_DHASH_SET(nsInt32HashSet,  PLDHashInt32Entry,  PRInt32)
DECL_DHASH_SET(nsVoidHashSet,   PLDHashVoidEntry,   void*)

#undef DHASH_EXPORT
#define DHASH_EXPORT


#endif
