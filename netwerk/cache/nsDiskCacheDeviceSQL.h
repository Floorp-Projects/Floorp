/* vim:set ts=2 sw=2 sts=2 et cin: */
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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by IBM Corporation are Copyright (C) 2004
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net>
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

#ifndef nsOfflineCacheDevice_h__
#define nsOfflineCacheDevice_h__

#include "nsCacheDevice.h"
#include "nsIApplicationCache.h"
#include "nsIApplicationCacheService.h"
#include "nsILocalFile.h"
#include "nsIObserver.h"
#include "mozIStorageConnection.h"
#include "mozIStorageFunction.h"
#include "nsIFile.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsInterfaceHashtable.h"
#include "nsClassHashtable.h"
#include "nsHashSets.h"
#include "nsWeakReference.h"

class nsIURI;
class nsOfflineCacheDevice;

class nsApplicationCacheNamespace : public nsIApplicationCacheNamespace
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIAPPLICATIONCACHENAMESPACE

  nsApplicationCacheNamespace() : mItemType(0) {}

private:
  PRUint32 mItemType;
  nsCString mNamespaceSpec;
  nsCString mData;
};

class nsOfflineCacheEvictionFunction : public mozIStorageFunction {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGEFUNCTION

  nsOfflineCacheEvictionFunction(nsOfflineCacheDevice *device)
    : mDevice(device)
  {}

  void Reset() { mItems.Clear(); }
  void Apply();

private:
  nsOfflineCacheDevice *mDevice;
  nsCOMArray<nsIFile> mItems;

};

class nsOfflineCacheDevice : public nsCacheDevice
                           , public nsIApplicationCacheService
{
public:
  nsOfflineCacheDevice();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIAPPLICATIONCACHESERVICE

  /**
   * nsCacheDevice methods
   */

  virtual ~nsOfflineCacheDevice();

  static nsOfflineCacheDevice *GetInstance();

  virtual nsresult        Init();
  virtual nsresult        Shutdown();

  virtual const char *    GetDeviceID(void);
  virtual nsCacheEntry *  FindEntry(nsCString * key, bool *collision);
  virtual nsresult        DeactivateEntry(nsCacheEntry * entry);
  virtual nsresult        BindEntry(nsCacheEntry * entry);
  virtual void            DoomEntry( nsCacheEntry * entry );

  virtual nsresult OpenInputStreamForEntry(nsCacheEntry *    entry,
                                           nsCacheAccessMode mode,
                                           PRUint32          offset,
                                           nsIInputStream ** result);

  virtual nsresult OpenOutputStreamForEntry(nsCacheEntry *     entry,
                                            nsCacheAccessMode  mode,
                                            PRUint32           offset,
                                            nsIOutputStream ** result);

  virtual nsresult        GetFileForEntry(nsCacheEntry *    entry,
                                          nsIFile **        result);

  virtual nsresult        OnDataSizeChange(nsCacheEntry * entry, PRInt32 deltaSize);
  
  virtual nsresult        Visit(nsICacheVisitor * visitor);

  virtual nsresult        EvictEntries(const char * clientID);

  /* Entry ownership */
  nsresult                GetOwnerDomains(const char *        clientID,
                                          PRUint32 *          count,
                                          char ***            domains);
  nsresult                GetOwnerURIs(const char *           clientID,
                                       const nsACString &     ownerDomain,
                                       PRUint32 *             count,
                                       char ***               uris);
  nsresult                SetOwnedKeys(const char *           clientID,
                                       const nsACString &     ownerDomain,
                                       const nsACString &     ownerUrl,
                                       PRUint32               count,
                                       const char **          keys);
  nsresult                GetOwnedKeys(const char *           clientID,
                                       const nsACString &     ownerDomain,
                                       const nsACString &     ownerUrl,
                                       PRUint32 *             count,
                                       char ***               keys);
  nsresult                AddOwnedKey(const char *            clientID,
                                      const nsACString &      ownerDomain,
                                      const nsACString &      ownerURI,
                                      const nsACString &      key);
  nsresult                RemoveOwnedKey(const char *         clientID,
                                         const nsACString &   ownerDomain,
                                         const nsACString &   ownerURI,
                                         const nsACString &   key);
  nsresult                KeyIsOwned(const char *             clientID,
                                     const nsACString &       ownerDomain,
                                     const nsACString &       ownerURI,
                                     const nsACString &       key,
                                     bool *                 isOwned);

  nsresult                ClearKeysOwnedByDomain(const char *clientID,
                                                 const nsACString &ownerDomain);
  nsresult                EvictUnownedEntries(const char *clientID);

  nsresult                ActivateCache(const nsCSubstring &group,
                                        const nsCSubstring &clientID);
  bool                    IsActiveCache(const nsCSubstring &group,
                                        const nsCSubstring &clientID);
  nsresult                GetGroupForCache(const nsCSubstring &clientID,
                                           nsCString &out);

  /**
   * Preference accessors
   */

  void                    SetCacheParentDirectory(nsILocalFile * parentDir);
  void                    SetCapacity(PRUint32  capacity);

  nsILocalFile *          CacheDirectory() { return mCacheDirectory; }
  PRUint32                CacheCapacity() { return mCacheCapacity; }
  PRUint32                CacheSize();
  PRUint32                EntryCount();
  
private:
  friend class nsApplicationCache;

  static PLDHashOperator ShutdownApplicationCache(const nsACString &key,
                                                  nsIWeakReference *weakRef,
                                                  void *ctx);

  static bool GetStrictFileOriginPolicy();

  bool     Initialized() { return mDB != nsnull; }

  nsresult InitActiveCaches();
  nsresult UpdateEntry(nsCacheEntry *entry);
  nsresult UpdateEntrySize(nsCacheEntry *entry, PRUint32 newSize);
  nsresult DeleteEntry(nsCacheEntry *entry, bool deleteData);
  nsresult DeleteData(nsCacheEntry *entry);
  nsresult EnableEvictionObserver();
  nsresult DisableEvictionObserver();

  bool CanUseCache(nsIURI *keyURI, const nsCString &clientID);

  nsresult MarkEntry(const nsCString &clientID,
                     const nsACString &key,
                     PRUint32 typeBits);
  nsresult UnmarkEntry(const nsCString &clientID,
                       const nsACString &key,
                       PRUint32 typeBits);

  nsresult CacheOpportunistically(const nsCString &clientID,
                                  const nsACString &key);
  nsresult GetTypes(const nsCString &clientID,
                    const nsACString &key,
                    PRUint32 *typeBits);

  nsresult GetMatchingNamespace(const nsCString &clientID,
                                const nsACString &key,
                                nsIApplicationCacheNamespace **out);
  nsresult GatherEntries(const nsCString &clientID,
                         PRUint32 typeBits,
                         PRUint32 *count,
                         char *** values);
  nsresult AddNamespace(const nsCString &clientID,
                        nsIApplicationCacheNamespace *ns);

  nsresult GetUsage(const nsACString &clientID,
                    PRUint32 *usage);

  nsresult RunSimpleQuery(mozIStorageStatement *statment,
                          PRUint32 resultIndex,
                          PRUint32 * count,
                          char *** values);

  nsCOMPtr<mozIStorageConnection>          mDB;
  nsRefPtr<nsOfflineCacheEvictionFunction> mEvictionFunction;

  nsCOMPtr<mozIStorageStatement>  mStatement_CacheSize;
  nsCOMPtr<mozIStorageStatement>  mStatement_ApplicationCacheSize;
  nsCOMPtr<mozIStorageStatement>  mStatement_EntryCount;
  nsCOMPtr<mozIStorageStatement>  mStatement_UpdateEntry;
  nsCOMPtr<mozIStorageStatement>  mStatement_UpdateEntrySize;
  nsCOMPtr<mozIStorageStatement>  mStatement_UpdateEntryFlags;
  nsCOMPtr<mozIStorageStatement>  mStatement_DeleteEntry;
  nsCOMPtr<mozIStorageStatement>  mStatement_FindEntry;
  nsCOMPtr<mozIStorageStatement>  mStatement_BindEntry;
  nsCOMPtr<mozIStorageStatement>  mStatement_ClearDomain;
  nsCOMPtr<mozIStorageStatement>  mStatement_MarkEntry;
  nsCOMPtr<mozIStorageStatement>  mStatement_UnmarkEntry;
  nsCOMPtr<mozIStorageStatement>  mStatement_GetTypes;
  nsCOMPtr<mozIStorageStatement>  mStatement_FindNamespaceEntry;
  nsCOMPtr<mozIStorageStatement>  mStatement_InsertNamespaceEntry;
  nsCOMPtr<mozIStorageStatement>  mStatement_CleanupUnmarked;
  nsCOMPtr<mozIStorageStatement>  mStatement_GatherEntries;
  nsCOMPtr<mozIStorageStatement>  mStatement_ActivateClient;
  nsCOMPtr<mozIStorageStatement>  mStatement_DeactivateGroup;
  nsCOMPtr<mozIStorageStatement>  mStatement_FindClient;
  nsCOMPtr<mozIStorageStatement>  mStatement_FindClientByNamespace;
  nsCOMPtr<mozIStorageStatement>  mStatement_EnumerateGroups;

  nsCOMPtr<nsILocalFile>          mCacheDirectory;
  PRUint32                        mCacheCapacity; // in bytes
  PRInt32                         mDeltaCounter;

  nsInterfaceHashtable<nsCStringHashKey, nsIWeakReference> mCaches;
  nsClassHashtable<nsCStringHashKey, nsCString> mActiveCachesByGroup;
  nsCStringHashSet mActiveCaches;

  nsCOMPtr<nsIThread> mInitThread;
};

#endif // nsOfflineCacheDevice_h__
