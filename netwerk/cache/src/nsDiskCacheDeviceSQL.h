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

#ifndef nsDiskCacheDeviceSQL_h__
#define nsDiskCacheDeviceSQL_h__

#include "nsCacheDevice.h"
#include "nsILocalFile.h"
#include "nsIObserver.h"
#include "mozIStorageConnection.h"
#include "nsCOMPtr.h"

class nsDiskCacheDevice : public nsCacheDevice
{
public:
  nsDiskCacheDevice();

  /**
   * nsCacheDevice methods
   */
 
  virtual ~nsDiskCacheDevice();

  virtual nsresult        Init();
  virtual nsresult        Shutdown();

  virtual const char *    GetDeviceID(void);
  virtual nsCacheEntry *  FindEntry(nsCString * key);
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
  PRBool   Initialized() { return mDB != nsnull; }
  nsresult EvictDiskCacheEntries(PRUint32 targetCapacity);
  nsresult UpdateEntry(nsCacheEntry *entry);
  nsresult UpdateEntrySize(nsCacheEntry *entry, PRUint32 newSize);
  nsresult DeleteEntry(nsCacheEntry *entry, PRBool deleteData);
  nsresult DeleteData(nsCacheEntry *entry);
  nsresult EnableEvictionObserver();
  nsresult DisableEvictionObserver();

#if 0
  // sqlite function for observing DELETE events
  static void EvictionObserver(struct sqlite3_context *, int, struct Mem **);
#endif

  nsCOMPtr<mozIStorageConnection> mDB;
  nsCOMPtr<nsILocalFile>          mCacheDirectory;
  PRUint32                        mCacheCapacity;     // XXX need soft/hard limits, currentTotal
  PRInt32                         mDeltaCounter;
};

#endif // nsDiskCacheDeviceSQL_h__
