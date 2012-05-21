/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef _nsZipWriter_h_
#define _nsZipWriter_h_

#include "nsIZipWriter.h"
#include "nsIFileStreams.h"
#include "nsIBufferedStreams.h"
#include "nsIRequestObserver.h"
#include "nsZipHeader.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsTArray.h"
#include "nsDataHashtable.h"

#define ZIPWRITER_CONTRACTID "@mozilla.org/zipwriter;1"
#define ZIPWRITER_CLASSNAME "Zip Writer"
#define ZIPWRITER_CID { 0x430d416c, 0xa722, 0x4ad1, \
           { 0xbe, 0x98, 0xd9, 0xa4, 0x45, 0xf8, 0x5e, 0x3f } }

#define OPERATION_ADD 0
#define OPERATION_REMOVE 1
struct nsZipQueueItem
{
public:
    PRUint32 mOperation;
    nsCString mZipEntry;
    nsCOMPtr<nsIFile> mFile;
    nsCOMPtr<nsIChannel> mChannel;
    nsCOMPtr<nsIInputStream> mStream;
    PRTime mModTime;
    PRInt32 mCompression;
    PRUint32 mPermissions;
};

class nsZipWriter : public nsIZipWriter,
                    public nsIRequestObserver
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIZIPWRITER
    NS_DECL_NSIREQUESTOBSERVER

    nsZipWriter();
    nsresult EntryCompleteCallback(nsZipHeader *aHeader, nsresult aStatus);

private:
    ~nsZipWriter();

    PRUint32 mCDSOffset;
    bool mCDSDirty;
    bool mInQueue;

    nsCOMPtr<nsIFile> mFile;
    nsCOMPtr<nsIRequestObserver> mProcessObserver;
    nsCOMPtr<nsISupports> mProcessContext;
    nsCOMPtr<nsIOutputStream> mStream;
    nsCOMArray<nsZipHeader> mHeaders;
    nsTArray<nsZipQueueItem> mQueue;
    nsDataHashtable<nsCStringHashKey, PRInt32> mEntryHash;
    nsCString mComment;

    nsresult SeekCDS();
    void Cleanup();
    nsresult ReadFile(nsIFile *aFile);
    nsresult InternalAddEntryDirectory(const nsACString & aZipEntry,
                                       PRTime aModTime, PRUint32 aPermissions);
    nsresult BeginProcessingAddition(nsZipQueueItem* aItem, bool* complete);
    nsresult BeginProcessingRemoval(PRInt32 aPos);
    nsresult AddEntryStream(const nsACString & aZipEntry, PRTime aModTime,
                            PRInt32 aCompression, nsIInputStream *aStream,
                            bool aQueue, PRUint32 aPermissions);
    void BeginProcessingNextItem();
    void FinishQueue(nsresult aStatus);
};

#endif
