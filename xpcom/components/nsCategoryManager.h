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
 * The Original Code is mozilla.org code.
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


#ifndef NSCATEGORYMANAGER_H
#define NSCATEGORYMANAGER_H

#include "prio.h"
#include "prlock.h"
#include "plarena.h"
#include "nsClassHashtable.h"
#include "nsICategoryManager.h"

#define NS_CATEGORYMANAGER_CLASSNAME     "Category Manager"

/* 16d222a6-1dd2-11b2-b693-f38b02c021b2 */
#define NS_CATEGORYMANAGER_CID \
{ 0x16d222a6, 0x1dd2, 0x11b2, \
  {0xb6, 0x93, 0xf3, 0x8b, 0x02, 0xc0, 0x21, 0xb2} }

/**
 * a "leaf-node", managed by the nsCategoryNode hashtable.
 *
 * we need to keep a "persistent value" (which will be written to the registry)
 * and a non-persistent value (for the current runtime): these are usually
 * the same, except when aPersist==PR_FALSE. The strings are permanently arena-
 * allocated, and will never go away.
 */
class CategoryLeaf : public nsDepCharHashKey
{
public:
  CategoryLeaf(const char* aKey)
    : nsDepCharHashKey(aKey),
      pValue(nsnull),
      nonpValue(nsnull) { }
  const char* pValue;
  const char* nonpValue;
};


/**
 * CategoryNode keeps a hashtable of it's entries.
 * the CategoryNode itself is permanently allocated in
 * the arena.
 */
class CategoryNode
{
public:
  NS_METHOD GetLeaf(const char* aEntryName,
                    char** _retval);

  NS_METHOD AddLeaf(const char* aEntryName,
                    const char* aValue,
                    PRBool aPersist,
                    PRBool aReplace,
                    char** _retval,
                    PLArenaPool* aArena);

  NS_METHOD DeleteLeaf(const char* aEntryName,
                       PRBool aDontPersist);

  void Clear() {
    PR_Lock(mLock);
    mTable.Clear();
    PR_Unlock(mLock);
  }

  PRUint32 Count() {
    PR_Lock(mLock);
    PRUint32 tCount = mTable.Count();
    PR_Unlock(mLock);
    return tCount;
  }

  NS_METHOD Enumerate(nsISimpleEnumerator** _retval);

  PRBool WritePersistentEntries(PRFileDesc* fd, const char* aCategoryName);

  // CategoryNode is arena-allocated, with the strings
  static CategoryNode* Create(PLArenaPool* aArena);
  ~CategoryNode();
  void operator delete(void*) { }

private:
  CategoryNode() { }
  void* operator new(size_t aSize, PLArenaPool* aArena);

  nsTHashtable<CategoryLeaf> mTable;
  PRLock* mLock;
};


/**
 * The main implementation of nsICategoryManager.
 *
 * This implementation is thread-safe.
 */
class nsCategoryManager
  : public nsICategoryManager
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICATEGORYMANAGER

  /**
   * Write the categories to the XPCOM persistent registry.
   * This is to be used by nsComponentManagerImpl (and NO ONE ELSE).
   */
  NS_METHOD WriteCategoryManagerToRegistry(PRFileDesc* fd);

  nsCategoryManager() { }
private:
  friend class nsCategoryManagerFactory;
  static nsCategoryManager* Create();

  ~nsCategoryManager();

  CategoryNode* get_category(const char* aName);

  PLArenaPool mArena;
  nsClassHashtable<nsDepCharHashKey, CategoryNode> mTable;
  PRLock* mLock;
};

#endif
