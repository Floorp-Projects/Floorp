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
 * The Original Code is Zip Writer Component.
 *
 * The Initial Developer of the Original Code is
 * Dave Townsend <dtownsend@oxymoronical.com>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *      Mook <mook.moz+random.code@gmail.com>
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
 * ***** END LICENSE BLOCK *****
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
