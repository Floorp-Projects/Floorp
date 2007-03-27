/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Mozilla FastLoad code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brendan Eich <brendan@mozilla.org> (original author)
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

#include <string.h>
#include "prtypes.h"
#include "nscore.h"
#include "nsDebug.h"
#include "nsEnumeratorUtils.h"
#include "nsMemory.h"
#include "nsXPIDLString.h"
#include "nsString.h"
#include "nsReadableUtils.h"

#include "nsIComponentManager.h"
#include "nsIFile.h"
#include "nsILocalFile.h"
#include "nsISeekableStream.h"
#include "nsISerializable.h"
#include "nsIStreamBufferAccess.h"
#include "nsIClassInfo.h"

#include "nsBinaryStream.h"
#include "nsFastLoadFile.h"
#include "nsInt64.h"

#ifdef DEBUG_brendan
# define METERING
# define DEBUG_MUX
#endif

#ifdef METERING
# define METER(x)       x
#else
# define METER(x)       /* nothing */
#endif

#ifdef DEBUG_MUX
# include <stdio.h>
# include <stdarg.h>

static void trace_mux(char mode, const char *format, ...)
{
    va_list ap;
    static FILE *tfp;
    if (!tfp) {
        char tfn[16];
        sprintf(tfn, "/tmp/mux.%ctrace", mode);
        tfp = fopen(tfn, "w");
        if (!tfp)
            return;
        setvbuf(tfp, NULL, _IOLBF, 0);
    }
    va_start(ap, format);
    vfprintf(tfp, format, ap);
    va_end(ap);
}

# define TRACE_MUX(args) trace_mux args
#else
# define TRACE_MUX(args) /* nothing */
#endif

/*
 * Fletcher's 16-bit checksum, using 32-bit two's-complement arithmetic.
 */
#define FOLD_ONES_COMPLEMENT_CARRY(X)   ((X) = ((X) & 0xffff) + ((X) >> 16))
#define ONES_COMPLEMENT_ACCUMULATE(X,Y) (X) += (Y); if ((X) & 0x80000000)     \
                                        FOLD_ONES_COMPLEMENT_CARRY(X)
#define FLETCHER_ACCUMULATE(A,B,U)      ONES_COMPLEMENT_ACCUMULATE(A, U);     \
                                        ONES_COMPLEMENT_ACCUMULATE(B, A)

PRUint32
NS_AccumulateFastLoadChecksum(PRUint32 *aChecksum,
                              const PRUint8* aBuffer,
                              PRUint32 aLength,
                              PRBool aLastBuffer)
{
    PRUint32 C = *aChecksum;
    PRUint32 A = C & 0xffff;
    PRUint32 B = C >> 16;

    PRUint16 U = 0;
    if (aLength >= 4) {
        PRBool odd = PRWord(aBuffer) & 1;
        switch (PRWord(aBuffer) & 3) {
          case 3:
            U = (aBuffer[0] << 8) | aBuffer[1];
            FLETCHER_ACCUMULATE(A, B, U);
            U = aBuffer[2];
            aBuffer += 3;
            aLength -= 3;
            break;

          case 2:
            U = (aBuffer[0] << 8) | aBuffer[1];
            FLETCHER_ACCUMULATE(A, B, U);
            U = 0;
            aBuffer += 2;
            aLength -= 2;
            break;

          case 1:
            U = *aBuffer++;
            aLength--;
            break;
        }

        PRUint32 W;
        if (odd) {
            while (aLength > 3) {
                W = *NS_REINTERPRET_CAST(const PRUint32*, aBuffer);
                U <<= 8;
#ifdef IS_BIG_ENDIAN
                U |= W >> 24;
                FLETCHER_ACCUMULATE(A, B, U);
                U = PRUint16(W >> 8);
                FLETCHER_ACCUMULATE(A, B, U);
                U = W & 0xff;
#else
                U |= W & 0xff;
                FLETCHER_ACCUMULATE(A, B, U);
                U = PRUint16(W >> 8);
                U = NS_SWAP16(U);
                FLETCHER_ACCUMULATE(A, B, U);
                U = W >> 24;
#endif
                aBuffer += 4;
                aLength -= 4;
            }
            aBuffer--;      // we're odd, we didn't checksum the last byte
            aLength++;
        } else {
            while (aLength > 3) {
                W = *NS_REINTERPRET_CAST(const PRUint32*, aBuffer);
#ifdef IS_BIG_ENDIAN
                U = W >> 16;
                FLETCHER_ACCUMULATE(A, B, U);
                U = PRUint16(W);
                FLETCHER_ACCUMULATE(A, B, U);
#else
                U = NS_SWAP16(W);
                FLETCHER_ACCUMULATE(A, B, U);
                U = W >> 16;
                U = NS_SWAP16(W);
                FLETCHER_ACCUMULATE(A, B, U);
#endif
                aBuffer += 4;
                aLength -= 4;
            }
        }
    }

    if (aLastBuffer) {
        NS_ASSERTION(aLength <= 4, "aLength botch");
        switch (aLength) {
          case 4:
            U = (aBuffer[0] << 8) | aBuffer[1];
            FLETCHER_ACCUMULATE(A, B, U);
            U = (aBuffer[2] << 8) | aBuffer[3];
            FLETCHER_ACCUMULATE(A, B, U);
            break;

          case 3:
            U = (aBuffer[0] << 8) | aBuffer[1];
            FLETCHER_ACCUMULATE(A, B, U);
            U = aBuffer[2];
            FLETCHER_ACCUMULATE(A, B, U);
            break;

          case 2:
            U = (aBuffer[0] << 8) | aBuffer[1];
            FLETCHER_ACCUMULATE(A, B, U);
            break;

          case 1:
            U = aBuffer[0];
            FLETCHER_ACCUMULATE(A, B, U);
            break;
        }

        aLength = 0;
    }

    while (A >> 16)
        FOLD_ONES_COMPLEMENT_CARRY(A);
    while (B >> 16)
        FOLD_ONES_COMPLEMENT_CARRY(B);

    *aChecksum = (B << 16) | A;
    return aLength;
}

PRUint32
NS_AddFastLoadChecksums(PRUint32 sum1, PRUint32 sum2, PRUint32 sum2ByteCount)
{
    PRUint32 A1 = sum1 & 0xffff;
    PRUint32 B1 = sum1 >> 16;

    PRUint32 A2 = sum2 & 0xffff;
    PRUint32 B2 = sum2 >> 16;

    PRUint32 A = A1 + A2;
    while (A >> 16)
        FOLD_ONES_COMPLEMENT_CARRY(A);

    PRUint32 B = B2;
    for (PRUint32 n = (sum2ByteCount + 1) / 2; n != 0; n--)
        ONES_COMPLEMENT_ACCUMULATE(B, B1);
    while (B >> 16)
        FOLD_ONES_COMPLEMENT_CARRY(B);

    return (B << 16) | A;
}

#undef FOLD_ONES_COMPLEMENT_CARRY
#undef ONES_COMPLEMENT_ACCUMULATE
#undef FLETCHER_ACCUMULATE

static const char magic[] = MFL_FILE_MAGIC;

// -------------------------- nsFastLoadFileReader --------------------------

nsID nsFastLoadFileReader::nsFastLoadFooter::gDummyID;
nsFastLoadFileReader::nsObjectMapEntry
    nsFastLoadFileReader::nsFastLoadFooter::gDummySharpObjectEntry;

NS_IMPL_ISUPPORTS_INHERITED5(nsFastLoadFileReader,
                             nsBinaryInputStream,
                             nsIObjectInputStream,
                             nsIFastLoadFileControl,
                             nsIFastLoadReadControl,
                             nsISeekableStream,
                             nsIFastLoadFileReader)

nsresult
nsFastLoadFileReader::ReadHeader(nsFastLoadHeader *aHeader)
{
    nsresult rv;
    PRUint32 bytesRead;

    rv = Read(NS_REINTERPRET_CAST(char*, aHeader), sizeof *aHeader, &bytesRead);
    if (NS_FAILED(rv))
        return rv;

    if (bytesRead != sizeof *aHeader ||
        memcmp(aHeader->mMagic, magic, MFL_FILE_MAGIC_SIZE)) {
        return NS_ERROR_UNEXPECTED;
    }

    aHeader->mChecksum     = NS_SWAP32(aHeader->mChecksum);
    aHeader->mVersion      = NS_SWAP32(aHeader->mVersion);
    aHeader->mFooterOffset = NS_SWAP32(aHeader->mFooterOffset);
    aHeader->mFileSize     = NS_SWAP32(aHeader->mFileSize);

    return NS_OK;
}

// nsIFastLoadFileControl methods:

NS_IMETHODIMP
nsFastLoadFileReader::GetChecksum(PRUint32 *aChecksum)
{
    *aChecksum = mHeader.mChecksum;
    return NS_OK;
}

NS_IMETHODIMP
nsFastLoadFileReader::SetChecksum(PRUint32 aChecksum)
{
    mHeader.mChecksum = aChecksum;
    return NS_OK;
}

struct nsStringMapEntry : public PLDHashEntryHdr {
    const char*     mString;            // key, must come first
    nsISupports*    mURI;               // for SelectMuxedDocument return value
};

struct nsDocumentMapEntry : public nsStringMapEntry {
    PRUint32    mInitialSegmentOffset;  // offset of URI's first segment in file
};

struct nsDocumentMapReadEntry : public nsDocumentMapEntry {
    PRUint32    mNextSegmentOffset;     // offset of URI's next segment to read
    PRUint32    mBytesLeft : 31,        // bytes remaining in current segment
                mNeedToSeek : 1;        // flag to defer Seek from Select to
                                        // Read, in case there is no Read before
                                        // another entry is Selected (to improve
                                        // input stream buffer utilization)
    PRInt64     mSaveOffset;            // in case demux schedule differs from
                                        // mux schedule
};

PR_STATIC_CALLBACK(void)
strmap_ClearEntry(PLDHashTable *aTable, PLDHashEntryHdr *aHdr)
{
    nsStringMapEntry* entry = NS_STATIC_CAST(nsStringMapEntry*, aHdr);

    if (entry->mString)
        nsMemory::Free((void*) entry->mString);
    NS_IF_RELEASE(entry->mURI);
    PL_DHashClearEntryStub(aTable, aHdr);
}

static const PLDHashTableOps strmap_DHashTableOps = {
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    PL_DHashStringKey,
    PL_DHashMatchStringKey,
    PL_DHashMoveEntryStub,
    strmap_ClearEntry,
    PL_DHashFinalizeStub,
    NULL
};

// An nsObjectMapEntry holds a strong reference to an XPCOM object, unless the
// mObject member, when cast to NSFastLoadOID, has its MFL_OBJECT_DEF_TAG bit
// set.  NB: we rely on the fact that an nsISupports* is never an odd pointer.
struct nsObjectMapEntry : public PLDHashEntryHdr {
    nsISupports*            mObject;        // key, must come first
};

// Fast mapping from URI object pointer back to spec-indexed document info.
struct nsURIMapReadEntry : public nsObjectMapEntry {
    nsDocumentMapReadEntry* mDocMapEntry;
};

PR_STATIC_CALLBACK(void)
objmap_ClearEntry(PLDHashTable *aTable, PLDHashEntryHdr *aHdr)
{
    nsObjectMapEntry* entry = NS_STATIC_CAST(nsObjectMapEntry*, aHdr);

    // Ignore tagged object ids stored as object pointer keys (the updater
    // code does this).
    if ((NS_PTR_TO_INT32(entry->mObject) & MFL_OBJECT_DEF_TAG) == 0)
        NS_IF_RELEASE(entry->mObject);
    PL_DHashClearEntryStub(aTable, aHdr);
}

static const PLDHashTableOps objmap_DHashTableOps = {
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    PL_DHashVoidPtrKeyStub,
    PL_DHashMatchEntryStub,
    PL_DHashMoveEntryStub,
    objmap_ClearEntry,
    PL_DHashFinalizeStub,
    NULL
};

NS_IMETHODIMP
nsFastLoadFileReader::HasMuxedDocument(const char* aURISpec, PRBool *aResult)
{
    nsDocumentMapReadEntry* docMapEntry =
        NS_STATIC_CAST(nsDocumentMapReadEntry*,
                       PL_DHashTableOperate(&mFooter.mDocumentMap, aURISpec,
                                            PL_DHASH_LOOKUP));

    *aResult = PL_DHASH_ENTRY_IS_BUSY(docMapEntry);
    return NS_OK;
}

NS_IMETHODIMP
nsFastLoadFileReader::StartMuxedDocument(nsISupports* aURI, const char* aURISpec)
{
    nsDocumentMapReadEntry* docMapEntry =
        NS_STATIC_CAST(nsDocumentMapReadEntry*,
                       PL_DHashTableOperate(&mFooter.mDocumentMap, aURISpec,
                                            PL_DHASH_LOOKUP));

    // If the spec isn't in the map, return NS_ERROR_NOT_AVAILABLE so the
    // FastLoad service can try for a file update.
    if (PL_DHASH_ENTRY_IS_FREE(docMapEntry))
        return NS_ERROR_NOT_AVAILABLE;

    nsCOMPtr<nsISupports> key(do_QueryInterface(aURI));
    nsURIMapReadEntry* uriMapEntry =
        NS_STATIC_CAST(nsURIMapReadEntry*,
                       PL_DHashTableOperate(&mFooter.mURIMap, key,
                                            PL_DHASH_ADD));
    if (!uriMapEntry)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ASSERTION(uriMapEntry->mDocMapEntry == nsnull,
                 "URI mapped to two different specs?");
    if (uriMapEntry->mDocMapEntry)
        return NS_ERROR_UNEXPECTED;

    docMapEntry->mURI = aURI;
    NS_ADDREF(docMapEntry->mURI);
    uriMapEntry->mObject = key;
    NS_ADDREF(uriMapEntry->mObject);
    uriMapEntry->mDocMapEntry = docMapEntry;
    TRACE_MUX(('r', "start %p (%p) %s\n", aURI, key.get(), aURISpec));
    return NS_OK;
}

NS_IMETHODIMP
nsFastLoadFileReader::SelectMuxedDocument(nsISupports* aURI,
                                          nsISupports** aResult)
{
    nsresult rv;

    // Find the given URI's entry and select it for more reading.
    nsCOMPtr<nsISupports> key(do_QueryInterface(aURI));
    nsURIMapReadEntry* uriMapEntry =
        NS_STATIC_CAST(nsURIMapReadEntry*,
                       PL_DHashTableOperate(&mFooter.mURIMap, key,
                                            PL_DHASH_LOOKUP));

    // If the URI isn't in the map, return NS_ERROR_NOT_AVAILABLE so the
    // FastLoad service can try selecting the file updater.
    if (PL_DHASH_ENTRY_IS_FREE(uriMapEntry))
        return NS_ERROR_NOT_AVAILABLE;

    // If we're interrupting another document's segment, save its offset so
    // we can seek back when it's reselected.  If prevDocMapEntry->mNeedToSeek
    // is set, that means the stream is not positioned for prevDocMapEntry, to
    // avoid buffer thrashing.  See below in this function for more.
    nsDocumentMapReadEntry* prevDocMapEntry = mCurrentDocumentMapEntry;
    if (prevDocMapEntry &&
        prevDocMapEntry->mBytesLeft &&
        !prevDocMapEntry->mNeedToSeek) {
        rv = Tell(&prevDocMapEntry->mSaveOffset);
        if (NS_FAILED(rv))
            return rv;
    }

    // It turns out we get a fair amount of redundant select calls, thanks to
    // non-blocking hunks of data from the parser that are devoid of scripts.
    // As more data gets FastLoaded, the number of these useless selects will
    // decline.
    nsDocumentMapReadEntry* docMapEntry = uriMapEntry->mDocMapEntry;
    if (docMapEntry == prevDocMapEntry) {
        TRACE_MUX(('r', "select prev %s same as current!\n",
                   docMapEntry->mString));
    }

    // Invariant: docMapEntry->mBytesLeft implies docMapEntry->mSaveOffset has
    // been set non-zero by the Tell call above.
    else if (docMapEntry->mBytesLeft) {
        NS_ASSERTION(docMapEntry->mSaveOffset != 0,
                     "reselecting from multiplex at unsaved offset?");

        // Defer Seek till Read, in case of "ping-pong" Selects without any
        // intervening Reads, to avoid dumping the underlying mInputStream's
        // input buffer for cases where alternate "pongs" fall in the same
        // buffer.
        docMapEntry->mNeedToSeek = PR_TRUE;
    }

    *aResult = prevDocMapEntry ? prevDocMapEntry->mURI : nsnull;
    NS_IF_ADDREF(*aResult);

    mCurrentDocumentMapEntry = docMapEntry;
#ifdef DEBUG_MUX
    PRInt64 currentSegmentOffset;
    Tell(&currentSegmentOffset);
    trace_mux('r', "select %p (%p) offset %ld\n",
              aURI, key.get(), (long) currentSegmentOffset);
#endif
    return NS_OK;
}

NS_IMETHODIMP
nsFastLoadFileReader::EndMuxedDocument(nsISupports* aURI)
{
    nsCOMPtr<nsISupports> key(do_QueryInterface(aURI));
    nsURIMapReadEntry* uriMapEntry =
        NS_STATIC_CAST(nsURIMapReadEntry*,
                       PL_DHashTableOperate(&mFooter.mURIMap, key,
                                            PL_DHASH_LOOKUP));

    // If the URI isn't in the map, return NS_ERROR_NOT_AVAILABLE so the
    // FastLoad service can try to end a select on its file updater.
    if (PL_DHASH_ENTRY_IS_FREE(uriMapEntry))
        return NS_ERROR_NOT_AVAILABLE;

    // Drop our ref to the URI object that was passed to StartMuxedDocument,
    // we no longer need it, and we do not want to extend its lifetime.
    if (uriMapEntry->mDocMapEntry)
        NS_RELEASE(uriMapEntry->mDocMapEntry->mURI);

    // Shrink the table if half the entries are removed sentinels.
    PRUint32 size = PL_DHASH_TABLE_SIZE(&mFooter.mURIMap);
    if (mFooter.mURIMap.removedCount >= (size >> 2))
        PL_DHashTableOperate(&mFooter.mURIMap, key, PL_DHASH_REMOVE);
    else
        PL_DHashTableRawRemove(&mFooter.mURIMap, uriMapEntry);

    TRACE_MUX(('r', "end %p (%p)\n", aURI, key.get()));
    return NS_OK;
}

NS_IMETHODIMP
nsFastLoadFileReader::Read(char* aBuffer, PRUint32 aCount, PRUint32 *aBytesRead)
{
    nsresult rv;

    nsDocumentMapReadEntry* entry = mCurrentDocumentMapEntry;
    if (entry) {
        // Don't call our Seek wrapper, as it clears mCurrentDocumentMapEntry.
        if (entry->mNeedToSeek) {
            rv = mSeekableInput->Seek(nsISeekableStream::NS_SEEK_SET,
                                      entry->mSaveOffset);
            if (NS_FAILED(rv))
                return rv;

            entry->mNeedToSeek = PR_FALSE;
        }

        // Loop to handle empty segments, which may be generated by the
        // writer, given Start A; Start B; Select A; Select B; write B data;
        // multiplexing schedules, which do tend to occur given non-blocking
        // i/o with LIFO scheduling.  XXXbe investigate LIFO issues
        while (entry->mBytesLeft == 0) {
            // Check for unexpected end of multiplexed stream.
            NS_ASSERTION(entry->mNextSegmentOffset != 0,
                         "document demuxed from FastLoad file more than once?");
            if (entry->mNextSegmentOffset == 0)
                return NS_ERROR_UNEXPECTED;

            rv = mSeekableInput->Seek(nsISeekableStream::NS_SEEK_SET,
                                      entry->mNextSegmentOffset);
            if (NS_FAILED(rv))
                return rv;

            // Clear mCurrentDocumentMapEntry temporarily to avoid recursion.
            mCurrentDocumentMapEntry = nsnull;

            rv = Read32(&entry->mNextSegmentOffset);
            if (NS_SUCCEEDED(rv)) {
                PRUint32 bytesLeft = 0;
                rv = Read32(&bytesLeft);
                entry->mBytesLeft = bytesLeft;
            }

            mCurrentDocumentMapEntry = entry;
            if (NS_FAILED(rv))
                return rv;

            NS_ASSERTION(entry->mBytesLeft >= 8, "demux segment length botch!");
            entry->mBytesLeft -= 8;
        }
    }

    rv = mInputStream->Read(aBuffer, aCount, aBytesRead);

    if (NS_SUCCEEDED(rv) && entry) {
        NS_ASSERTION(entry->mBytesLeft >= *aBytesRead, "demux Read underflow!");
        entry->mBytesLeft -= *aBytesRead;

#ifdef NS_DEBUG
        // Invariant: !entry->mBytesLeft implies entry->mSaveOffset == 0.
        if (entry->mBytesLeft == 0)
            entry->mSaveOffset = 0;
#endif
    }
    return rv;
}

NS_IMETHODIMP
nsFastLoadFileReader::ReadSegments(nsWriteSegmentFun aWriter, void* aClosure,
                                   PRUint32 aCount, PRUint32 *aResult)
{
    nsDocumentMapReadEntry* entry = mCurrentDocumentMapEntry;

    NS_ASSERTION(!entry || (!entry->mNeedToSeek && entry->mBytesLeft != 0),
                 "ReadSegments called from above nsFastLoadFileReader layer?!");

    nsresult rv = nsBinaryInputStream::ReadSegments(aWriter, aClosure, aCount,
                                                    aResult);
    if (NS_SUCCEEDED(rv) && entry) {
        NS_ASSERTION(entry->mBytesLeft >= *aResult,
                     "demux ReadSegments underflow!");
        entry->mBytesLeft -= *aResult;

#ifdef NS_DEBUG
        // Invariant: !entry->mBytesLeft implies entry->mSaveOffset == 0.
        if (entry->mBytesLeft == 0)
            entry->mSaveOffset = 0;
#endif
    }
    return rv;
}

NS_IMETHODIMP
nsFastLoadFileReader::SetInputStream(nsIInputStream *aInputStream)
{
    nsresult rv = nsBinaryInputStream::SetInputStream(aInputStream);
    mSeekableInput = do_QueryInterface(aInputStream);
    NS_ASSERTION(!mInputStream || mSeekableInput,
                 "FastLoad requires a seekable input stream");
    return rv;
}

/**
 * XXX tuneme
 */
#define MFL_CHECKSUM_BUFSIZE    8192

NS_IMETHODIMP
nsFastLoadFileReader::ComputeChecksum(PRUint32 *aResult)
{
    nsCOMPtr<nsIInputStream> stream = mInputStream;
    nsCOMPtr<nsISeekableStream> seekable = mSeekableInput;

    PRInt64 saveOffset;
    nsresult rv = seekable->Tell(&saveOffset);
    if (NS_FAILED(rv))
        return rv;

    if (mBufferAccess) {
        rv = mBufferAccess->GetUnbufferedStream(getter_AddRefs(stream));
        if (NS_FAILED(rv))
            return rv;

        seekable = do_QueryInterface(stream);
        if (!seekable)
            return NS_ERROR_UNEXPECTED;
    }

    rv = seekable->Seek(nsISeekableStream::NS_SEEK_SET, 0);
    if (NS_FAILED(rv))
        return rv;

    char buf[MFL_CHECKSUM_BUFSIZE];
    PRUint32 len, rem;

    rem = offsetof(nsFastLoadHeader, mChecksum);
    rv = stream->Read(buf, rem, &len);
    if (NS_FAILED(rv))
        return rv;
    if (len != rem)
        return NS_ERROR_UNEXPECTED;

    rv = seekable->Seek(nsISeekableStream::NS_SEEK_CUR, 4);
    if (NS_FAILED(rv))
        return rv;
    memset(buf + rem, 0, 4);
    rem += 4;

    PRUint32 checksum = 0;
    while (NS_SUCCEEDED(rv = stream->Read(buf + rem, sizeof buf - rem, &len)) &&
           len) {
        len += rem;
        rem = NS_AccumulateFastLoadChecksum(&checksum,
                                            NS_REINTERPRET_CAST(PRUint8*, buf),
                                            len,
                                            PR_FALSE);
        if (rem)
            memcpy(buf, buf + len - rem, rem);
    }
    if (NS_FAILED(rv))
        return rv;

    if (rem) {
        NS_AccumulateFastLoadChecksum(&checksum,
                                      NS_REINTERPRET_CAST(PRUint8*, buf),
                                      rem,
                                      PR_TRUE);
    }

    rv = seekable->Seek(nsISeekableStream::NS_SEEK_SET, saveOffset);
    if (NS_FAILED(rv))
        return rv;

    *aResult = checksum;
    return NS_OK;
}

NS_IMETHODIMP
nsFastLoadFileReader::GetDependencies(nsISimpleEnumerator* *aDependencies)
{
    return NS_NewArrayEnumerator(aDependencies, mFooter.mDependencies);
}

nsresult
nsFastLoadFileReader::ReadFooter(nsFastLoadFooter *aFooter)
{
    nsresult rv;

    rv = ReadFooterPrefix(aFooter);
    if (NS_FAILED(rv))
        return rv;

    aFooter->mIDMap = new nsID[aFooter->mNumIDs];
    if (!aFooter->mIDMap)
        return NS_ERROR_OUT_OF_MEMORY;

    PRUint32 i, n;
    for (i = 0, n = aFooter->mNumIDs; i < n; i++) {
        rv = ReadSlowID(&aFooter->mIDMap[i]);
        if (NS_FAILED(rv))
            return rv;
    }

    aFooter->mObjectMap = new nsObjectMapEntry[aFooter->mNumSharpObjects];
    if (!aFooter->mObjectMap)
        return NS_ERROR_OUT_OF_MEMORY;

    for (i = 0, n = aFooter->mNumSharpObjects; i < n; i++) {
        nsObjectMapEntry* entry = &aFooter->mObjectMap[i];

        rv = ReadSharpObjectInfo(entry);
        if (NS_FAILED(rv))
            return rv;

        entry->mReadObject = nsnull;
        entry->mSkipOffset = 0;
        entry->mSaveStrongRefCnt = entry->mStrongRefCnt;
        entry->mSaveWeakRefCnt = entry->mWeakRefCnt;
    }

    if (!PL_DHashTableInit(&aFooter->mDocumentMap, &strmap_DHashTableOps,
                           (void *)this, sizeof(nsDocumentMapReadEntry),
                           aFooter->mNumMuxedDocuments)) {
        aFooter->mDocumentMap.ops = nsnull;
        return NS_ERROR_OUT_OF_MEMORY;
    }

    if (!PL_DHashTableInit(&aFooter->mURIMap, &objmap_DHashTableOps,
                           (void *)this, sizeof(nsURIMapReadEntry),
                           aFooter->mNumMuxedDocuments)) {
        aFooter->mURIMap.ops = nsnull;
        return NS_ERROR_OUT_OF_MEMORY;
    }

    for (i = 0, n = aFooter->mNumMuxedDocuments; i < n; i++) {
        nsFastLoadMuxedDocumentInfo info;

        rv = ReadMuxedDocumentInfo(&info);
        if (NS_FAILED(rv))
            return rv;

        nsDocumentMapReadEntry* entry =
            NS_STATIC_CAST(nsDocumentMapReadEntry*,
                           PL_DHashTableOperate(&aFooter->mDocumentMap,
                                                info.mURISpec,
                                                PL_DHASH_ADD));
        if (!entry) {
            nsMemory::Free((void*) info.mURISpec);
            return NS_ERROR_OUT_OF_MEMORY;
        }

        NS_ASSERTION(!entry->mString, "duplicate URISpec in MuxedDocumentMap");
        entry->mString = info.mURISpec;
        entry->mURI = nsnull;
        entry->mInitialSegmentOffset = info.mInitialSegmentOffset;
        entry->mNextSegmentOffset = info.mInitialSegmentOffset;
        entry->mBytesLeft = 0;
        entry->mNeedToSeek = PR_FALSE;
        entry->mSaveOffset = 0;
    }

    nsCOMPtr<nsISupportsArray> readDeps;
    rv = NS_NewISupportsArray(getter_AddRefs(readDeps));
    if (NS_FAILED(rv))
        return rv;

    nsCAutoString filename;
    for (i = 0, n = aFooter->mNumDependencies; i < n; i++) {
        rv = ReadCString(filename);
        if (NS_FAILED(rv))
            return rv;

        PRInt64 fastLoadMtime;
        rv = Read64(NS_REINTERPRET_CAST(PRUint64*, &fastLoadMtime));
        if (NS_FAILED(rv))
            return rv;

        nsCOMPtr<nsILocalFile> file;
        rv = NS_NewNativeLocalFile(filename, PR_TRUE, getter_AddRefs(file));
        if (NS_FAILED(rv))
            return rv;

        PRInt64 currentMtime;
        rv = file->GetLastModifiedTime(&currentMtime);
        if (NS_FAILED(rv))
            return rv;

        if (LL_NE(fastLoadMtime, currentMtime)) {
#ifdef DEBUG
            nsCAutoString path;
            file->GetNativePath(path);
            printf("%s mtime changed, invalidating FastLoad file\n",
                   path.get());
#endif
            return NS_ERROR_FAILURE;
        }

        rv = readDeps->AppendElement(file);
        if (NS_FAILED(rv))
            return rv;
    }

    aFooter->mDependencies = readDeps;
    return NS_OK;
}

nsresult
nsFastLoadFileReader::ReadFooterPrefix(nsFastLoadFooterPrefix *aFooterPrefix)
{
    nsresult rv;

    rv = Read32(&aFooterPrefix->mNumIDs);
    if (NS_FAILED(rv))
        return rv;

    rv = Read32(&aFooterPrefix->mNumSharpObjects);
    if (NS_FAILED(rv))
        return rv;

    rv = Read32(&aFooterPrefix->mNumMuxedDocuments);
    if (NS_FAILED(rv))
        return rv;

    rv = Read32(&aFooterPrefix->mNumDependencies);
    if (NS_FAILED(rv))
        return rv;

    return NS_OK;
}

nsresult
nsFastLoadFileReader::ReadSlowID(nsID *aID)
{
    nsresult rv;

    rv = Read32(&aID->m0);
    if (NS_FAILED(rv))
        return rv;

    rv = Read16(&aID->m1);
    if (NS_FAILED(rv))
        return rv;

    rv = Read16(&aID->m2);
    if (NS_FAILED(rv))
        return rv;

    PRUint32 bytesRead;
    rv = Read(NS_REINTERPRET_CAST(char*, aID->m3), sizeof aID->m3, &bytesRead);
    if (NS_FAILED(rv))
        return rv;

    if (bytesRead != sizeof aID->m3)
        return NS_ERROR_FAILURE;
    return NS_OK;
}

nsresult
nsFastLoadFileReader::ReadFastID(NSFastLoadID *aID)
{
    nsresult rv = Read32(aID);
    if (NS_SUCCEEDED(rv))
        *aID ^= MFL_ID_XOR_KEY;
    return rv;
}

nsresult
nsFastLoadFileReader::ReadSharpObjectInfo(nsFastLoadSharpObjectInfo *aInfo)
{
    nsresult rv;

    rv = Read32(&aInfo->mCIDOffset);
    if (NS_FAILED(rv))
        return rv;

    NS_ASSERTION(aInfo->mCIDOffset != 0,
                 "fastload reader: mCIDOffset cannot be zero!");

    rv = Read16(&aInfo->mStrongRefCnt);
    if (NS_FAILED(rv))
        return rv;

    rv = Read16(&aInfo->mWeakRefCnt);
    if (NS_FAILED(rv))
        return rv;

    return NS_OK;
}

nsresult
nsFastLoadFileReader::ReadMuxedDocumentInfo(nsFastLoadMuxedDocumentInfo *aInfo)
{
    nsresult rv;

    nsCAutoString spec;
    rv = ReadCString(spec);
    if (NS_FAILED(rv))
        return rv;

    rv = Read32(&aInfo->mInitialSegmentOffset);
    if (NS_FAILED(rv))
        return rv;

    aInfo->mURISpec = ToNewCString(spec);
    return NS_OK;
}

nsresult
nsFastLoadFileReader::Open()
{
    nsresult rv;

    // Don't bother buffering the header, as we immediately seek to EOF.
    if (mBufferAccess)
        mBufferAccess->DisableBuffering();

    rv = ReadHeader(&mHeader);

    if (mBufferAccess)
        mBufferAccess->EnableBuffering();
    if (NS_FAILED(rv))
        return rv;

    if (mHeader.mVersion != MFL_FILE_VERSION)
        return NS_ERROR_UNEXPECTED;
    if (mHeader.mFooterOffset == 0)
        return NS_ERROR_UNEXPECTED;

    rv = mSeekableInput->Seek(nsISeekableStream::NS_SEEK_END, 0);
    if (NS_FAILED(rv))
        return rv;

    PRInt64 fileSize;
    rv = mSeekableInput->Tell(&fileSize);
    if (NS_FAILED(rv))
        return rv;

    nsInt64 fileSize64 = fileSize;
    const nsInt64 maxUint32 = PR_UINT32_MAX;
    NS_ASSERTION(fileSize64 <= maxUint32, "fileSize must fit in 32 bits");
    if ((PRUint32) fileSize64 != mHeader.mFileSize)
        return NS_ERROR_UNEXPECTED;

    rv = mSeekableInput->Seek(nsISeekableStream::NS_SEEK_SET,
                              PRInt32(mHeader.mFooterOffset));
    if (NS_FAILED(rv))
        return rv;

    rv = ReadFooter(&mFooter);
    if (NS_FAILED(rv))
        return rv;

    return mSeekableInput->Seek(nsISeekableStream::NS_SEEK_SET,
                                sizeof(nsFastLoadHeader));
}

NS_IMETHODIMP
nsFastLoadFileReader::Close()
{
    // Give up our strong "keepalive" references, in case not all objects that
    // were deserialized were fully re-connected.
    //
    // This happens for sure when an nsFastLoadFileUpdater is created and wraps
    // an nsFastLoadFileReader whose data was already deserialized by an earlier
    // FastLoad episode.  The reader is useful in the second such episode during
    // a session not so much for reading objects as for its footer information,
    // which primes the updater's tables so that after the update completes, the
    // FastLoad file has a superset footer.

    for (PRUint32 i = 0, n = mFooter.mNumSharpObjects; i < n; i++) {
        nsObjectMapEntry* entry = &mFooter.mObjectMap[i];
        entry->mReadObject = nsnull;
    }

    return mInputStream->Close();
}

nsresult
nsFastLoadFileReader::DeserializeObject(nsISupports* *aObject)
{
    nsresult rv;
    NSFastLoadID fastCID;

    rv = ReadFastID(&fastCID);
    if (NS_FAILED(rv))
        return rv;

    const nsID& slowCID = mFooter.GetID(fastCID);
    nsCOMPtr<nsISupports> object(do_CreateInstance(slowCID, &rv));
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsISerializable> serializable(do_QueryInterface(object));
    if (!serializable)
        return NS_ERROR_FAILURE;

    rv = serializable->Read(this);
    if (NS_FAILED(rv))
        return rv;

    *aObject = object;
    NS_ADDREF(*aObject);
    return NS_OK;
}

nsresult
nsFastLoadFileReader::ReadObject(PRBool aIsStrongRef, nsISupports* *aObject)
{
    nsresult rv;
    NSFastLoadOID oid;

    rv = Read32(&oid);
    if (NS_FAILED(rv))
        return rv;
    oid ^= MFL_OID_XOR_KEY;

    nsCOMPtr<nsISupports> object;

    if (oid == MFL_DULL_OBJECT_OID) {
        // A very dull object, defined at point of single (strong) reference.
        NS_ASSERTION(aIsStrongRef, "dull object read via weak ref!");

        rv = DeserializeObject(getter_AddRefs(object));
        if (NS_FAILED(rv))
            return rv;
    } else {
        NS_ASSERTION((oid & MFL_WEAK_REF_TAG) ==
                     (aIsStrongRef ? 0 : MFL_WEAK_REF_TAG),
                     "strong vs. weak ref deserialization mismatch!");

        nsObjectMapEntry* entry = &mFooter.GetSharpObjectEntry(oid);

        // Check whether we've already deserialized the object for this OID.
        object = entry->mReadObject;
        if (!object) {
            PRInt64 saveOffset;
            nsDocumentMapReadEntry* saveDocMapEntry = nsnull;

            rv = mSeekableInput->Tell(&saveOffset);
            if (NS_FAILED(rv))
                return rv;

            PRUint32 saveOffset32 = saveOffset;
            if (entry->mCIDOffset != saveOffset32) {
                // We skipped deserialization of this object from its position
                // earlier in the input stream, presumably due to the reference
                // there being an nsFastLoadPtr, or (more likely) because the
                // object was muxed in another document, and deserialization
                // order does not match serialization order.  So we must seek
                // back and read it now.
                NS_ASSERTION(entry->mCIDOffset < saveOffset32,
                             "out of order object?!");

                // Ape our Seek wrapper by clearing mCurrentDocumentMapEntry.
                // This allows for a skipped object to be referenced from two
                // or more multiplexed documents in the FastLoad file.
                saveDocMapEntry = mCurrentDocumentMapEntry;
                mCurrentDocumentMapEntry = nsnull;
                rv = mSeekableInput->Seek(nsISeekableStream::NS_SEEK_SET,
                                          entry->mCIDOffset);
                if (NS_FAILED(rv))
                    return rv;
            }

            rv = DeserializeObject(getter_AddRefs(object));
            if (NS_FAILED(rv))
                return rv;

            if (entry->mCIDOffset != saveOffset32) {
                // Save the "skip offset" in case we need to skip this object
                // definition when reading forward, later on.
                rv = mSeekableInput->Tell(&entry->mSkipOffset);
                if (NS_FAILED(rv))
                    return rv;

                // Restore stream offset and mCurrentDocumentMapEntry in case
                // we're still reading forward through a part of the multiplex
                // to get object definitions eagerly.
                rv = mSeekableInput->Seek(nsISeekableStream::NS_SEEK_SET,
                                          saveOffset);
                if (NS_FAILED(rv))
                    return rv;
                mCurrentDocumentMapEntry = saveDocMapEntry;
            }

            // Save object until all refs have been deserialized.
            entry->mReadObject = object;
        } else {
            // What if we are at a definition that's already been read?  This
            // case arises when a sharp object's def is serialized before its
            // refs, while a non-defining ref is deserialized before the def.
            // We must skip over the object definition.
            if (oid & MFL_OBJECT_DEF_TAG) {
                NS_ASSERTION(entry->mSkipOffset != 0, "impossible! see above");

                // Since we are seeking within a muxed segment, we must adjust
                // mBytesLeft, so that Seek called from Read will be triggered
                // when mBytesLeft goes to zero.
                PRInt64 currentOffset;
                rv = mSeekableInput->Tell(&currentOffset);
                if (NS_FAILED(rv))
                    return rv;

                NS_ASSERTION(entry->mSkipOffset > (PRUint32)currentOffset,
                             "skipping backwards from object?!");
                NS_ASSERTION(mCurrentDocumentMapEntry->mBytesLeft >=
                             entry->mSkipOffset - (PRUint32)currentOffset,
                             "skipped object buffer underflow!");

                mCurrentDocumentMapEntry->mBytesLeft -=
                    entry->mSkipOffset - (PRUint32)currentOffset;

                rv = mSeekableInput->Seek(nsISeekableStream::NS_SEEK_SET,
                                          entry->mSkipOffset);
                if (NS_FAILED(rv))
                    return rv;
            }
        }

        if (aIsStrongRef) {
            NS_ASSERTION(entry->mStrongRefCnt != 0,
                         "mStrongRefCnt underflow!");
            --entry->mStrongRefCnt;
        } else {
            NS_ASSERTION(MFL_GET_WEAK_REFCNT(entry) != 0,
                         "mWeakRefCnt underflow!");
            MFL_DROP_WEAK_REFCNT(entry);
        }

        if (entry->mStrongRefCnt == 0 && MFL_GET_WEAK_REFCNT(entry) == 0)
            entry->mReadObject = nsnull;
    }

    if (oid & MFL_QUERY_INTERFACE_TAG) {
        NSFastLoadID iid;
        rv = ReadFastID(&iid);
        if (NS_FAILED(rv))
            return rv;

        rv = object->QueryInterface(mFooter.GetID(iid),
                                    NS_REINTERPRET_CAST(void**, aObject));
        if (NS_FAILED(rv))
            return rv;
    } else {
        *aObject = object;
        NS_ADDREF(*aObject);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsFastLoadFileReader::ReadID(nsID *aResult)
{
    nsresult rv;
    NSFastLoadID fastID;

    rv = ReadFastID(&fastID);
    if (NS_FAILED(rv))
        return rv;

    *aResult = mFooter.GetID(fastID);
    return NS_OK;
}

NS_IMETHODIMP
nsFastLoadFileReader::Seek(PRInt32 aWhence, PRInt64 aOffset)
{
    mCurrentDocumentMapEntry = nsnull;
    return mSeekableInput->Seek(aWhence, aOffset);
}

NS_IMETHODIMP
nsFastLoadFileReader::Tell(PRInt64 *aResult)
{
    return mSeekableInput->Tell(aResult);
}

NS_IMETHODIMP
nsFastLoadFileReader::SetEOF()
{
    return mSeekableInput->SetEOF();
}

NS_COM nsresult
NS_NewFastLoadFileReader(nsIObjectInputStream* *aResult,
                         nsIInputStream* aSrcStream)
{
    nsFastLoadFileReader* reader = new nsFastLoadFileReader(aSrcStream);
    if (!reader)
        return NS_ERROR_OUT_OF_MEMORY;

    // Stabilize reader's refcnt.
    nsCOMPtr<nsIObjectInputStream> stream(reader);

    nsresult rv = reader->Open();
    if (NS_FAILED(rv))
        return rv;

    *aResult = stream;
    NS_ADDREF(*aResult);
    return NS_OK;
}

// -------------------------- nsFastLoadFileWriter --------------------------

NS_IMPL_ISUPPORTS_INHERITED4(nsFastLoadFileWriter,
                             nsBinaryOutputStream,
                             nsIObjectOutputStream,
                             nsIFastLoadFileControl,
                             nsIFastLoadWriteControl,
                             nsISeekableStream)

struct nsIDMapEntry : public PLDHashEntryHdr {
    NSFastLoadID    mFastID;            // 1 + nsFastLoadFooter::mIDMap index
    nsID            mSlowID;            // key, used by PLDHashTableOps below
};

PR_STATIC_CALLBACK(PLDHashNumber)
idmap_HashKey(PLDHashTable *aTable, const void *aKey)
{
    const nsID *idp = NS_REINTERPRET_CAST(const nsID*, aKey);

    return idp->m0;
}

PR_STATIC_CALLBACK(PRBool)
idmap_MatchEntry(PLDHashTable *aTable,
                const PLDHashEntryHdr *aHdr,
                const void *aKey)
{
    const nsIDMapEntry* entry = NS_STATIC_CAST(const nsIDMapEntry*, aHdr);
    const nsID *idp = NS_REINTERPRET_CAST(const nsID*, aKey);

    return memcmp(&entry->mSlowID, idp, sizeof(nsID)) == 0;
}

static const PLDHashTableOps idmap_DHashTableOps = {
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    idmap_HashKey,
    idmap_MatchEntry,
    PL_DHashMoveEntryStub,
    PL_DHashClearEntryStub,
    PL_DHashFinalizeStub,
    NULL
};

nsresult
nsFastLoadFileWriter::MapID(const nsID& aSlowID, NSFastLoadID *aResult)
{
    nsIDMapEntry* entry =
        NS_STATIC_CAST(nsIDMapEntry*,
                       PL_DHashTableOperate(&mIDMap, &aSlowID, PL_DHASH_ADD));
    if (!entry)
        return NS_ERROR_OUT_OF_MEMORY;

    if (entry->mFastID == 0) {
        entry->mFastID = mIDMap.entryCount;
        entry->mSlowID = aSlowID;
    }

    *aResult = entry->mFastID;
    return NS_OK;
}

nsresult
nsFastLoadFileWriter::WriteHeader(nsFastLoadHeader *aHeader)
{
    nsresult rv;
    PRUint32 bytesWritten;

    rv = Write(aHeader->mMagic, MFL_FILE_MAGIC_SIZE, &bytesWritten);
    if (NS_FAILED(rv))
        return rv;

    if (bytesWritten != MFL_FILE_MAGIC_SIZE)
        return NS_ERROR_FAILURE;

    rv = Write32(aHeader->mChecksum);
    if (NS_FAILED(rv))
        return rv;

    rv = Write32(aHeader->mVersion);
    if (NS_FAILED(rv))
        return rv;

    rv = Write32(aHeader->mFooterOffset);
    if (NS_FAILED(rv))
        return rv;

    rv = Write32(aHeader->mFileSize);
    if (NS_FAILED(rv))
        return rv;

    return NS_OK;
}

// nsIFastLoadFileControl methods:

NS_IMETHODIMP
nsFastLoadFileWriter::GetChecksum(PRUint32 *aChecksum)
{
    if (mHeader.mChecksum == 0)
        return NS_ERROR_NOT_AVAILABLE;
    *aChecksum = mHeader.mChecksum;
    return NS_OK;
}

NS_IMETHODIMP
nsFastLoadFileWriter::SetChecksum(PRUint32 aChecksum)
{
    mHeader.mChecksum = aChecksum;
    return NS_OK;
}

struct nsDocumentMapWriteEntry : public nsDocumentMapEntry {
    PRUint32    mCurrentSegmentOffset;      // last written segment's offset
};

// Fast mapping from URI object pointer back to spec-indexed document info.
// We also may need the slow mapping from mURISpec to nsDocumentMapWriteEntry,
// because the writer's mDocumentMap double hash table may grow "behind the
// back of" each mURIMap entry's mDocMapEntry member.
struct nsURIMapWriteEntry : public nsObjectMapEntry {
    nsDocumentMapWriteEntry* mDocMapEntry;
    PRUint32                 mGeneration;
    const char*              mURISpec;
};

NS_IMETHODIMP
nsFastLoadFileWriter::HasMuxedDocument(const char* aURISpec, PRBool *aResult)
{
    nsDocumentMapWriteEntry* docMapEntry =
        NS_STATIC_CAST(nsDocumentMapWriteEntry*,
                       PL_DHashTableOperate(&mDocumentMap, aURISpec,
                                            PL_DHASH_LOOKUP));

    *aResult = PL_DHASH_ENTRY_IS_BUSY(docMapEntry);
    return NS_OK;
}

NS_IMETHODIMP
nsFastLoadFileWriter::StartMuxedDocument(nsISupports* aURI,
                                         const char* aURISpec)
{
    // Save mDocumentMap table generation and mCurrentDocumentMapEntry key in
    // case the hash table grows during the PL_DHASH_ADD operation.
    PRUint32 saveGeneration = mDocumentMap.generation;
    const char* saveURISpec = mCurrentDocumentMapEntry
                              ? mCurrentDocumentMapEntry->mString
                              : nsnull;

    nsDocumentMapWriteEntry* docMapEntry =
        NS_STATIC_CAST(nsDocumentMapWriteEntry*,
                       PL_DHashTableOperate(&mDocumentMap, aURISpec,
                                            PL_DHASH_ADD));
    if (!docMapEntry)
        return NS_ERROR_OUT_OF_MEMORY;

    // If the generation number changed, refresh mCurrentDocumentMapEntry.
    if (mCurrentDocumentMapEntry && mDocumentMap.generation != saveGeneration) {
        mCurrentDocumentMapEntry =
            NS_STATIC_CAST(nsDocumentMapWriteEntry*,
                           PL_DHashTableOperate(&mDocumentMap, saveURISpec,
                                                PL_DHASH_LOOKUP));
        NS_ASSERTION(PL_DHASH_ENTRY_IS_BUSY(mCurrentDocumentMapEntry),
                     "mCurrentDocumentMapEntry lost during table growth?!");

        // Refresh saveGeneration for use below when initializing uriMapEntry.
        saveGeneration = mDocumentMap.generation;
    }

    NS_ASSERTION(docMapEntry->mString == nsnull,
                 "redundant multiplexed document?");
    if (docMapEntry->mString)
        return NS_ERROR_UNEXPECTED;

    void* spec = nsMemory::Clone(aURISpec, strlen(aURISpec) + 1);
    if (!spec)
        return NS_ERROR_OUT_OF_MEMORY;
    docMapEntry->mString = NS_REINTERPRET_CAST(const char*, spec);
    docMapEntry->mURI = aURI;
    NS_ADDREF(docMapEntry->mURI);

    nsCOMPtr<nsISupports> key(do_QueryInterface(aURI));
    nsURIMapWriteEntry* uriMapEntry =
        NS_STATIC_CAST(nsURIMapWriteEntry*,
                       PL_DHashTableOperate(&mURIMap, key, PL_DHASH_ADD));
    if (!uriMapEntry)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ASSERTION(uriMapEntry->mDocMapEntry == nsnull,
                 "URI mapped to two different specs?");
    if (uriMapEntry->mDocMapEntry)
        return NS_ERROR_UNEXPECTED;

    uriMapEntry->mObject = key;
    NS_ADDREF(uriMapEntry->mObject);
    uriMapEntry->mDocMapEntry = docMapEntry;
    uriMapEntry->mGeneration = saveGeneration;
    uriMapEntry->mURISpec = NS_REINTERPRET_CAST(const char*, spec);
    TRACE_MUX(('w', "start %p (%p) %s\n", aURI, key.get(), aURISpec));
    return NS_OK;
}

NS_IMETHODIMP
nsFastLoadFileWriter::SelectMuxedDocument(nsISupports* aURI,
                                          nsISupports** aResult)
{
    // Capture the current file offset (XXXbe maintain our own via Write?)
    nsresult rv;
    PRInt64 currentSegmentOffset;
    rv = mSeekableOutput->Tell(&currentSegmentOffset);
    if (NS_FAILED(rv))
        return rv;

    PRUint32 currentSegmentOffset32 = currentSegmentOffset;
    // Look for an existing entry keyed by aURI, added by StartMuxedDocument.
    nsCOMPtr<nsISupports> key(do_QueryInterface(aURI));
    nsURIMapWriteEntry* uriMapEntry =
        NS_STATIC_CAST(nsURIMapWriteEntry*,
                       PL_DHashTableOperate(&mURIMap, key, PL_DHASH_LOOKUP));
    NS_ASSERTION(PL_DHASH_ENTRY_IS_BUSY(uriMapEntry),
                 "SelectMuxedDocument without prior StartMuxedDocument?");
    if (PL_DHASH_ENTRY_IS_FREE(uriMapEntry))
        return NS_ERROR_UNEXPECTED;

    // Beware that uriMapEntry->mDocMapEntry may be stale, if an mDocumentMap
    // addition caused that table to grow.  We save the mDocumentMap generation
    // in each uriMapEntry and compare it to the current generation, rehashing
    // uriMapEntry->mURISpec if necessary.

    nsDocumentMapWriteEntry* docMapEntry = uriMapEntry->mDocMapEntry;
    if (uriMapEntry->mGeneration != mDocumentMap.generation) {
        docMapEntry =
            NS_STATIC_CAST(nsDocumentMapWriteEntry*,
                           PL_DHashTableOperate(&mDocumentMap,
                                                uriMapEntry->mURISpec,
                                                PL_DHASH_LOOKUP));
        NS_ASSERTION(PL_DHASH_ENTRY_IS_BUSY(docMapEntry), "lost mDocMapEntry!?");
        uriMapEntry->mDocMapEntry = docMapEntry;
        uriMapEntry->mGeneration = mDocumentMap.generation;
    }

    // If there is a muxed document segment open, close it now by setting its
    // length, stored in the second PRUint32 of the segment.
    nsDocumentMapWriteEntry* prevDocMapEntry = mCurrentDocumentMapEntry;
    if (prevDocMapEntry) {
        if (prevDocMapEntry == docMapEntry) {
            TRACE_MUX(('w', "select prev %s same as current!\n",
                       prevDocMapEntry->mString));
            *aResult = docMapEntry->mURI;
            NS_ADDREF(*aResult);
            return NS_OK;
        }

        PRUint32 prevSegmentOffset = prevDocMapEntry->mCurrentSegmentOffset;
        TRACE_MUX(('w', "select prev %s offset %lu\n",
                   prevDocMapEntry->mString, prevSegmentOffset));

        rv = mSeekableOutput->Seek(nsISeekableStream::NS_SEEK_SET,
                                   prevSegmentOffset + 4);
        if (NS_FAILED(rv))
            return rv;

        // The length counts all bytes in the segment, including the header
        // that contains [nextSegmentOffset, length].
        rv = Write32(currentSegmentOffset32 - prevSegmentOffset);
        if (NS_FAILED(rv))
            return rv;

        // Seek back to the current offset only if we are not going to seek
        // back to *this* entry's last "current" segment offset and write its
        // next segment offset at the first PRUint32 of the segment.
        if (!docMapEntry->mInitialSegmentOffset) {
            rv = mSeekableOutput->Seek(nsISeekableStream::NS_SEEK_SET,
                                       currentSegmentOffset);
            if (NS_FAILED(rv))
                return rv;
        }
    }

    // If this entry was newly added, set its key and initial segment offset.
    // Otherwise, seek back to write the next segment offset of the previous
    // segment for this document in the multiplex.
    if (!docMapEntry->mInitialSegmentOffset) {
        docMapEntry->mInitialSegmentOffset = currentSegmentOffset32;
    } else {
        rv = mSeekableOutput->Seek(nsISeekableStream::NS_SEEK_SET,
                                   docMapEntry->mCurrentSegmentOffset);
        if (NS_FAILED(rv))
            return rv;

        rv = Write32(currentSegmentOffset32);
        if (NS_FAILED(rv))
            return rv;

        rv = mSeekableOutput->Seek(nsISeekableStream::NS_SEEK_SET,
                                   currentSegmentOffset);
        if (NS_FAILED(rv))
            return rv;
    }

    // Update this document's current segment offset so we can later fix its
    // next segment offset (unless it is last, in which case we leave the zero
    // placeholder as a terminator).
    docMapEntry->mCurrentSegmentOffset = currentSegmentOffset32;

    rv = Write32(0);    // nextSegmentOffset placeholder
    if (NS_FAILED(rv))
        return rv;

    rv = Write32(0);    // length placeholder
    if (NS_FAILED(rv))
        return rv;

    *aResult = prevDocMapEntry ? prevDocMapEntry->mURI : nsnull;
    NS_IF_ADDREF(*aResult);

    mCurrentDocumentMapEntry = docMapEntry;
    TRACE_MUX(('w', "select %p (%p) offset %lu\n",
               aURI, key.get(), currentSegmentOffset));
    return NS_OK;
}

NS_IMETHODIMP
nsFastLoadFileWriter::EndMuxedDocument(nsISupports* aURI)
{
    nsCOMPtr<nsISupports> key(do_QueryInterface(aURI));
    nsURIMapWriteEntry* uriMapEntry =
        NS_STATIC_CAST(nsURIMapWriteEntry*,
                       PL_DHashTableOperate(&mURIMap, key, PL_DHASH_LOOKUP));

    // If the URI isn't in the map, nsFastLoadFileWriter::StartMuxedDocument
    // must have been called with a redundant URI, *and* its caller must have
    // ignored the NS_ERROR_UNEXPECTED it returned in that case.
    if (PL_DHASH_ENTRY_IS_FREE(uriMapEntry)) {
        TRACE_MUX(('w', "bad end %p (%p)\n", aURI, key.get()));
        return NS_ERROR_UNEXPECTED;
    }

    // Drop our ref to the URI object that was passed to StartMuxedDocument,
    // we no longer need it, and we do not want to extend its lifetime.
    if (uriMapEntry->mDocMapEntry)
        NS_RELEASE(uriMapEntry->mDocMapEntry->mURI);

    // Shrink the table if half the entries are removed sentinels.
    PRUint32 size = PL_DHASH_TABLE_SIZE(&mURIMap);
    if (mURIMap.removedCount >= (size >> 2))
        PL_DHashTableOperate(&mURIMap, key, PL_DHASH_REMOVE);
    else
        PL_DHashTableRawRemove(&mURIMap, uriMapEntry);

    TRACE_MUX(('w', "end %p (%p)\n", aURI, key.get()));
    return NS_OK;
}

struct nsDependencyMapEntry : public nsStringMapEntry {
    PRInt64 mLastModified;
};

NS_IMETHODIMP
nsFastLoadFileWriter::AddDependency(nsIFile* aFile)
{
    nsCAutoString path;
    nsresult rv = aFile->GetNativePath(path);
    if (NS_FAILED(rv))
        return rv;

    nsDependencyMapEntry* entry =
        NS_STATIC_CAST(nsDependencyMapEntry*,
                       PL_DHashTableOperate(&mDependencyMap, path.get(),
                                            PL_DHASH_ADD));
    if (!entry)
        return NS_ERROR_OUT_OF_MEMORY;

    if (!entry->mString) {
        const char *tmp = ToNewCString(path);
        if (!tmp)
            return NS_ERROR_OUT_OF_MEMORY;
        entry->mString = tmp;

        // If we can't get the last modified time from aFile, assume it does
        // not exist, or is otherwise inaccessible to us (due to permissions),
        // remove the dependency, and suppress the failure.
        //
        // Otherwise, we would end up aborting the fastload process due to a
        // missing .js or .xul or other file on every startup.

        rv = aFile->GetLastModifiedTime(&entry->mLastModified);
        if (NS_FAILED(rv)) {
            PL_DHashTableOperate(&mDependencyMap, path.get(), PL_DHASH_REMOVE);
            rv = NS_OK;
        }
    }
    return rv;
}

nsresult
nsFastLoadFileWriter::WriteFooterPrefix(const nsFastLoadFooterPrefix& aFooterPrefix)
{
    nsresult rv;

    rv = Write32(aFooterPrefix.mNumIDs);
    if (NS_FAILED(rv))
        return rv;

    rv = Write32(aFooterPrefix.mNumSharpObjects);
    if (NS_FAILED(rv))
        return rv;

    rv = Write32(aFooterPrefix.mNumMuxedDocuments);
    if (NS_FAILED(rv))
        return rv;

    rv = Write32(aFooterPrefix.mNumDependencies);
    if (NS_FAILED(rv))
        return rv;

    return NS_OK;
}

nsresult
nsFastLoadFileWriter::WriteSlowID(const nsID& aID)
{
    nsresult rv;

    rv = Write32(aID.m0);
    if (NS_FAILED(rv))
        return rv;

    rv = Write16(aID.m1);
    if (NS_FAILED(rv))
        return rv;

    rv = Write16(aID.m2);
    if (NS_FAILED(rv))
        return rv;

    PRUint32 bytesWritten;
    rv = Write(NS_REINTERPRET_CAST(const char*, aID.m3), sizeof aID.m3,
               &bytesWritten);
    if (NS_FAILED(rv))
        return rv;

    if (bytesWritten != sizeof aID.m3)
        return NS_ERROR_FAILURE;
    return NS_OK;
}

nsresult
nsFastLoadFileWriter::WriteFastID(NSFastLoadID aID)
{
    return Write32(aID ^ MFL_ID_XOR_KEY);
}

nsresult
nsFastLoadFileWriter::WriteSharpObjectInfo(const nsFastLoadSharpObjectInfo& aInfo)
{
    nsresult rv;

    NS_ASSERTION(aInfo.mCIDOffset != 0,
                 "fastload writer: mCIDOffset cannot be zero!");

    rv = Write32(aInfo.mCIDOffset);
    if (NS_FAILED(rv))
        return rv;

    rv = Write16(aInfo.mStrongRefCnt);
    if (NS_FAILED(rv))
        return rv;

    rv = Write16(aInfo.mWeakRefCnt);
    if (NS_FAILED(rv))
        return rv;

    return NS_OK;
}

nsresult
nsFastLoadFileWriter::WriteMuxedDocumentInfo(const nsFastLoadMuxedDocumentInfo& aInfo)
{
    nsresult rv;

    rv = WriteStringZ(aInfo.mURISpec);
    if (NS_FAILED(rv))
        return rv;

    rv = Write32(aInfo.mInitialSegmentOffset);
    if (NS_FAILED(rv))
        return rv;

    return NS_OK;
}

PLDHashOperator PR_CALLBACK
nsFastLoadFileWriter::IDMapEnumerate(PLDHashTable *aTable,
                                     PLDHashEntryHdr *aHdr,
                                     PRUint32 aNumber,
                                     void *aData)
{
    nsIDMapEntry* entry = NS_STATIC_CAST(nsIDMapEntry*, aHdr);
    PRUint32 index = entry->mFastID - 1;
    nsID* vector = NS_REINTERPRET_CAST(nsID*, aData);

    NS_ASSERTION(index < aTable->entryCount, "bad nsIDMap index!");
    vector[index] = entry->mSlowID;
    return PL_DHASH_NEXT;
}

struct nsSharpObjectMapEntry : public nsObjectMapEntry {
    NSFastLoadOID               mOID;
    nsFastLoadSharpObjectInfo   mInfo;
};

PLDHashOperator PR_CALLBACK
nsFastLoadFileWriter::ObjectMapEnumerate(PLDHashTable *aTable,
                                         PLDHashEntryHdr *aHdr,
                                         PRUint32 aNumber,
                                         void *aData)
{
    nsSharpObjectMapEntry* entry = NS_STATIC_CAST(nsSharpObjectMapEntry*, aHdr);
    PRUint32 index = MFL_OID_TO_SHARP_INDEX(entry->mOID);
    nsFastLoadSharpObjectInfo* vector =
        NS_REINTERPRET_CAST(nsFastLoadSharpObjectInfo*, aData);

    NS_ASSERTION(index < aTable->entryCount, "bad nsObjectMap index!");
    vector[index] = entry->mInfo;

    NS_ASSERTION(entry->mInfo.mStrongRefCnt, "no strong ref in serialization!");

    // Ignore tagged object ids stored as object pointer keys (the updater
    // code does this).
    if ((NS_PTR_TO_INT32(entry->mObject) & MFL_OBJECT_DEF_TAG) == 0)
        NS_RELEASE(entry->mObject);

    return PL_DHASH_NEXT;
}

PLDHashOperator PR_CALLBACK
nsFastLoadFileWriter::DocumentMapEnumerate(PLDHashTable *aTable,
                                           PLDHashEntryHdr *aHdr,
                                           PRUint32 aNumber,
                                           void *aData)
{
    nsFastLoadFileWriter* writer =
        NS_REINTERPRET_CAST(nsFastLoadFileWriter*, aTable->data);
    nsDocumentMapWriteEntry* entry =
        NS_STATIC_CAST(nsDocumentMapWriteEntry*, aHdr);
    nsresult* rvp = NS_REINTERPRET_CAST(nsresult*, aData);

    nsFastLoadMuxedDocumentInfo info;
    info.mURISpec = entry->mString;
    info.mInitialSegmentOffset = entry->mInitialSegmentOffset;
    *rvp = writer->WriteMuxedDocumentInfo(info);

    return NS_FAILED(*rvp) ? PL_DHASH_STOP : PL_DHASH_NEXT;
}

PLDHashOperator PR_CALLBACK
nsFastLoadFileWriter::DependencyMapEnumerate(PLDHashTable *aTable,
                                             PLDHashEntryHdr *aHdr,
                                             PRUint32 aNumber,
                                             void *aData)
{
    nsFastLoadFileWriter* writer =
        NS_REINTERPRET_CAST(nsFastLoadFileWriter*, aTable->data);
    nsDependencyMapEntry* entry = NS_STATIC_CAST(nsDependencyMapEntry*, aHdr);
    nsresult* rvp = NS_REINTERPRET_CAST(nsresult*, aData);

    *rvp = writer->WriteStringZ(entry->mString);
    if (NS_SUCCEEDED(*rvp))
        *rvp = writer->Write64(entry->mLastModified);

    return NS_FAILED(*rvp) ? PL_DHASH_STOP :PL_DHASH_NEXT;
}

nsresult
nsFastLoadFileWriter::WriteFooter()
{
    nsresult rv;
    PRUint32 i, count;

    nsFastLoadFooterPrefix footerPrefix;
    footerPrefix.mNumIDs = mIDMap.entryCount;
    footerPrefix.mNumSharpObjects = mObjectMap.entryCount;
    footerPrefix.mNumMuxedDocuments = mDocumentMap.entryCount;
    footerPrefix.mNumDependencies = mDependencyMap.entryCount;

    rv = WriteFooterPrefix(footerPrefix);
    if (NS_FAILED(rv))
        return rv;

    // Enumerate mIDMap into a vector indexed by mFastID and write it.
    nsID* idvec = new nsID[footerPrefix.mNumIDs];
    if (!idvec)
        return NS_ERROR_OUT_OF_MEMORY;

    count = PL_DHashTableEnumerate(&mIDMap, IDMapEnumerate, idvec);
    NS_ASSERTION(count == footerPrefix.mNumIDs, "bad mIDMap enumeration!");
    for (i = 0; i < count; i++) {
        rv = WriteSlowID(idvec[i]);
        if (NS_FAILED(rv)) break;
    }

    delete[] idvec;
    if (NS_FAILED(rv))
        return rv;

    // Enumerate mObjectMap into a vector indexed by mOID and write it.
    nsFastLoadSharpObjectInfo* objvec =
        new nsFastLoadSharpObjectInfo[footerPrefix.mNumSharpObjects];
    if (!objvec)
        return NS_ERROR_OUT_OF_MEMORY;
#ifdef NS_DEBUG
    memset(objvec, 0, footerPrefix.mNumSharpObjects *
                      sizeof(nsFastLoadSharpObjectInfo));
#endif

    count = PL_DHashTableEnumerate(&mObjectMap, ObjectMapEnumerate, objvec);
    NS_ASSERTION(count == footerPrefix.mNumSharpObjects,
                 "bad mObjectMap enumeration!");
    for (i = 0; i < count; i++) {
        rv = WriteSharpObjectInfo(objvec[i]);
        if (NS_FAILED(rv)) break;
    }

    delete[] objvec;
    if (NS_FAILED(rv))
        return rv;

    // Enumerate mDocumentMap, writing nsFastLoadMuxedDocumentInfo records
    count = PL_DHashTableEnumerate(&mDocumentMap, DocumentMapEnumerate, &rv);
    if (NS_FAILED(rv))
        return rv;

    NS_ASSERTION(count == footerPrefix.mNumMuxedDocuments,
                 "bad mDocumentMap enumeration!");

    // Write out make-like file dependencies.
    count = PL_DHashTableEnumerate(&mDependencyMap, DependencyMapEnumerate, &rv);
    if (NS_FAILED(rv))
        return rv;

    return NS_OK;
}

nsresult
nsFastLoadFileWriter::Init()
{
    if (!PL_DHashTableInit(&mIDMap, &idmap_DHashTableOps, (void *)this,
                           sizeof(nsIDMapEntry), PL_DHASH_MIN_SIZE)) {
        mIDMap.ops = nsnull;
        return NS_ERROR_OUT_OF_MEMORY;
    }

    if (!PL_DHashTableInit(&mObjectMap, &objmap_DHashTableOps, (void *)this,
                           sizeof(nsSharpObjectMapEntry), PL_DHASH_MIN_SIZE)) {
        mObjectMap.ops = nsnull;
        return NS_ERROR_OUT_OF_MEMORY;
    }

    if (!PL_DHashTableInit(&mDocumentMap, &strmap_DHashTableOps, (void *)this,
                           sizeof(nsDocumentMapWriteEntry),
                           PL_DHASH_MIN_SIZE)) {
        mDocumentMap.ops = nsnull;
        return NS_ERROR_OUT_OF_MEMORY;
    }

    if (!PL_DHashTableInit(&mURIMap, &objmap_DHashTableOps, (void *)this,
                           sizeof(nsURIMapWriteEntry), PL_DHASH_MIN_SIZE)) {
        mURIMap.ops = nsnull;
        return NS_ERROR_OUT_OF_MEMORY;
    }

    if (!PL_DHashTableInit(&mDependencyMap, &strmap_DHashTableOps, (void *)this,
                           sizeof(nsDependencyMapEntry), PL_DHASH_MIN_SIZE)) {
        mDependencyMap.ops = nsnull;
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return NS_OK;
}

nsresult
nsFastLoadFileWriter::Open()
{
    nsresult rv;

    rv = mSeekableOutput->Seek(nsISeekableStream::NS_SEEK_SET,
                               sizeof(nsFastLoadHeader));
    if (NS_FAILED(rv))
        return rv;

    return Init();
}

NS_IMETHODIMP
nsFastLoadFileWriter::Close()
{
    nsresult rv;

    memcpy(mHeader.mMagic, magic, MFL_FILE_MAGIC_SIZE);
    mHeader.mChecksum = 0;
    mHeader.mVersion = MFL_FILE_VERSION;

    PRInt64 footerOffset;
    rv = mSeekableOutput->Tell(&footerOffset);

    LL_L2UI(mHeader.mFooterOffset, footerOffset);
    if (NS_FAILED(rv))
        return rv;

    // If there is a muxed document segment open, close it now by setting its
    // length, stored in the second PRUint32 of the segment.
    if (mCurrentDocumentMapEntry) {
        PRUint32 currentSegmentOffset =
            mCurrentDocumentMapEntry->mCurrentSegmentOffset;
        rv = mSeekableOutput->Seek(nsISeekableStream::NS_SEEK_SET,
                                   currentSegmentOffset + 4);
        if (NS_FAILED(rv))
            return rv;

        rv = Write32(mHeader.mFooterOffset - currentSegmentOffset);
        if (NS_FAILED(rv))
            return rv;

        // Seek back to the current offset to write the footer.
        rv = mSeekableOutput->Seek(nsISeekableStream::NS_SEEK_SET,
                                   mHeader.mFooterOffset);
        if (NS_FAILED(rv))
            return rv;

        mCurrentDocumentMapEntry = nsnull;
    }

    rv = WriteFooter();
    if (NS_FAILED(rv))
        return rv;
    PRInt64 fileSize;
    rv = mSeekableOutput->Tell(&fileSize);
    LL_L2UI(mHeader.mFileSize, fileSize);
    if (NS_FAILED(rv))
        return rv;

    rv = mSeekableOutput->Seek(nsISeekableStream::NS_SEEK_SET, 0);
    if (NS_FAILED(rv))
        return rv;

    rv = WriteHeader(&mHeader);
    if (NS_FAILED(rv))
        return rv;

    // Now compute the checksum, using mFileIO to get an input stream on the
    // underlying FastLoad file.
    if (mFileIO) {
        // Get the unbuffered output stream, which flushes the buffered header
        // so we can read and checksum it along with the rest of the file, and
        // which allows us to write the checksum directly.
        nsCOMPtr<nsIOutputStream> output;
        rv = mBufferAccess->GetUnbufferedStream(getter_AddRefs(output));
        if (NS_FAILED(rv) || !output)
            return NS_ERROR_UNEXPECTED;

        nsCOMPtr<nsIInputStream> input;
        rv = mFileIO->GetInputStream(getter_AddRefs(input));
        if (NS_FAILED(rv))
            return rv;

        // Get the unbuffered input stream, to avoid copying overhead and to
        // keep our view of the file coherent with the writer -- we don't want
        // to hit a stale buffer in the reader's underlying stream.
        nsCOMPtr<nsIStreamBufferAccess> bufferAccess =
            do_QueryInterface(input);
        rv = bufferAccess->GetUnbufferedStream(getter_AddRefs(input));
        if (NS_FAILED(rv) || !input)
            return NS_ERROR_UNEXPECTED;

        // Seek the input stream to offset 0, in case it's a reader who has
        // already been used to consume some of the FastLoad file.
        nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(input);
        rv = seekable->Seek(nsISeekableStream::NS_SEEK_SET, 0);
        if (NS_FAILED(rv))
            return rv;

        char buf[MFL_CHECKSUM_BUFSIZE];
        PRUint32 len, rem = 0;
        PRUint32 checksum = 0;

        // Ok, we're finally ready to checksum the FastLoad file we just wrote!
        while (NS_SUCCEEDED(rv =
                            input->Read(buf + rem, sizeof buf - rem, &len)) &&
               len) {
            len += rem;
            rem = NS_AccumulateFastLoadChecksum(&checksum,
                                                NS_REINTERPRET_CAST(PRUint8*,
                                                                    buf),
                                                len,
                                                PR_FALSE);
            if (rem)
                memcpy(buf, buf + len - rem, rem);
        }
        if (NS_FAILED(rv))
            return rv;

        if (rem) {
            NS_AccumulateFastLoadChecksum(&checksum,
                                          NS_REINTERPRET_CAST(PRUint8*, buf),
                                          rem,
                                          PR_TRUE);
        }

        // Store the checksum in the FastLoad file header and remember it via
        // mHeader.mChecksum, for GetChecksum.
        seekable = do_QueryInterface(output);
        rv = seekable->Seek(nsISeekableStream::NS_SEEK_SET,
                            offsetof(nsFastLoadHeader, mChecksum));
        if (NS_FAILED(rv))
            return rv;

        mHeader.mChecksum = checksum;
        checksum = NS_SWAP32(checksum);
        PRUint32 bytesWritten;
        rv = output->Write(NS_REINTERPRET_CAST(char*, &checksum),
                           sizeof checksum,
                           &bytesWritten);
        if (NS_FAILED(rv))
            return rv;
        if (bytesWritten != sizeof checksum)
            return NS_ERROR_FAILURE;
    }

    return mOutputStream->Close();
}

// Pseudo-tag used as flag between WriteSingleRefObject and WriteObjectCommon.
#define MFL_SINGLE_REF_PSEUDO_TAG       PR_BIT(MFL_OBJECT_TAG_BITS)

nsresult
nsFastLoadFileWriter::WriteObjectCommon(nsISupports* aObject,
                                        PRBool aIsStrongRef,
                                        PRUint32 aTags)
{
    nsrefcnt rc;
    nsresult rv;

    NS_ASSERTION((NS_PTR_TO_INT32(aObject) & MFL_OBJECT_DEF_TAG) == 0,
                 "odd nsISupports*, oh no!");

    // Here be manual refcounting dragons!
    rc = aObject->AddRef();
    NS_ASSERTION(rc != 0, "bad refcnt when writing aObject!");

    NSFastLoadOID oid;
    nsCOMPtr<nsIClassInfo> classInfo;

    if (rc == 2 && (aTags & MFL_SINGLE_REF_PSEUDO_TAG)) {
        // Dull object: only one strong ref and no weak refs in serialization.
        // Conservative: we don't trust the caller if there are more than two
        // refs (one from the AddRef above, one from the data structure that's
        // being serialized).
        oid = MFL_DULL_OBJECT_OID;
        aObject->Release();
    } else {
        // Object is presumed to be multiply connected through some combo of
        // strong and weak refs.  Hold onto it via mObjectMap.
        nsSharpObjectMapEntry* entry =
            NS_STATIC_CAST(nsSharpObjectMapEntry*,
                           PL_DHashTableOperate(&mObjectMap, aObject,
                                                PL_DHASH_ADD));
        if (!entry) {
            aObject->Release();
            return NS_ERROR_OUT_OF_MEMORY;
        }

        if (!entry->mObject) {
            // First time we've seen this object address: add it to mObjectMap
            // and serialize the object at the current stream offset.
            PRInt64 thisOffset;
            rv = Tell(&thisOffset);
            if (NS_FAILED(rv)) {
                aObject->Release();
                return rv;
            }

            // NB: aObject was already held, and mObject is a raw nsISupports*.
            entry->mObject = aObject;

            oid = (mObjectMap.entryCount << MFL_OBJECT_TAG_BITS);
            entry->mOID = oid;

            // NB: the (32-bit, fast) CID and object data follow the OID.
            entry->mInfo.mCIDOffset = thisOffset + sizeof(oid);
            entry->mInfo.mStrongRefCnt = aIsStrongRef ? 1 : 0;
            entry->mInfo.mWeakRefCnt   = aIsStrongRef ? 0 : 1;

            // Record in oid the fact that we're defining this object in the
            // stream, and get the object's class info here, so we can take
            // note of singletons in order to avoid reserializing them when
            // updating after reading.
            oid |= MFL_OBJECT_DEF_TAG;
            classInfo = do_QueryInterface(aObject);
            if (!classInfo) {
                NS_NOTREACHED("aObject must implement nsIClassInfo");
                return NS_ERROR_FAILURE;
            }

            PRUint32 flags;
            if (NS_SUCCEEDED(classInfo->GetFlags(&flags)) &&
                (flags & nsIClassInfo::SINGLETON)) {
                MFL_SET_SINGLETON_FLAG(&entry->mInfo);
            }
        } else {
            // Already serialized, recover oid and update the desired refcnt.
            oid = entry->mOID;
            if (aIsStrongRef) {
                ++entry->mInfo.mStrongRefCnt;
                NS_ASSERTION(entry->mInfo.mStrongRefCnt != 0,
                             "mStrongRefCnt overflow");
            } else {
                MFL_BUMP_WEAK_REFCNT(&entry->mInfo);
                NS_ASSERTION(MFL_GET_WEAK_REFCNT(&entry->mInfo) != 0,
                             "mWeakRefCnt overflow");
            }

            aObject->Release();
        }
    }

    if (!aIsStrongRef)
        oid |= MFL_WEAK_REF_TAG;
    oid |= (aTags & MFL_QUERY_INTERFACE_TAG);

    rv = Write32(oid ^ MFL_OID_XOR_KEY);
    if (NS_FAILED(rv))
        return rv;

    if (oid & MFL_OBJECT_DEF_TAG) {
        nsCOMPtr<nsISerializable> serializable(do_QueryInterface(aObject));
        if (!serializable) {
            NS_NOTREACHED("aObject must implement nsISerializable");
            return NS_ERROR_FAILURE;
        }

        nsCID slowCID;
        rv = classInfo->GetClassIDNoAlloc(&slowCID);
        if (NS_FAILED(rv))
            return rv;

        NSFastLoadID fastCID;
        rv = MapID(slowCID, &fastCID);
        if (NS_FAILED(rv))
            return rv;

        rv = WriteFastID(fastCID);
        if (NS_FAILED(rv))
            return rv;

        rv = serializable->Write(this);
        if (NS_FAILED(rv))
            return rv;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsFastLoadFileWriter::WriteObject(nsISupports* aObject, PRBool aIsStrongRef)
{
#ifdef NS_DEBUG
    nsCOMPtr<nsISupports> rootObject(do_QueryInterface(aObject));

    NS_ASSERTION(rootObject.get() == aObject,
                 "bad call to WriteObject -- call WriteCompoundObject!");
#endif

    return WriteObjectCommon(aObject, aIsStrongRef, 0);
}

NS_IMETHODIMP
nsFastLoadFileWriter::WriteSingleRefObject(nsISupports* aObject)
{
#ifdef NS_DEBUG
    nsCOMPtr<nsISupports> rootObject(do_QueryInterface(aObject));

    NS_ASSERTION(rootObject.get() == aObject,
                 "bad call to WriteSingleRefObject -- call WriteCompoundObject!");
#endif

    return WriteObjectCommon(aObject, PR_TRUE, MFL_SINGLE_REF_PSEUDO_TAG);
}

NS_IMETHODIMP
nsFastLoadFileWriter::WriteCompoundObject(nsISupports* aObject,
                                          const nsIID& aIID,
                                          PRBool aIsStrongRef)
{
    nsresult rv;
    nsCOMPtr<nsISupports> rootObject(do_QueryInterface(aObject));
    
    // We could assert that |rootObject != aObject|, but that would prevent
    // callers who don't know whether they're dealing with the primary
    // nsISupports pointer (e.g., they don't know which implementation of
    // nsIURI they have) from using this function.

#ifdef NS_DEBUG
    nsCOMPtr<nsISupports> roundtrip;
    rootObject->QueryInterface(aIID, getter_AddRefs(roundtrip));
    NS_ASSERTION(roundtrip.get() == aObject,
                 "bad aggregation or multiple inheritance detected by call to "
                 "WriteCompoundObject!");
#endif

    rv = WriteObjectCommon(rootObject, aIsStrongRef, MFL_QUERY_INTERFACE_TAG);
    if (NS_FAILED(rv))
        return rv;

    NSFastLoadID iid;
    rv = MapID(aIID, &iid);
    if (NS_FAILED(rv))
        return rv;

    return WriteFastID(iid);
}

NS_IMETHODIMP
nsFastLoadFileWriter::WriteID(const nsID& aID)
{
    nsresult rv;
    NSFastLoadID fastID;

    rv = MapID(aID, &fastID);
    if (NS_FAILED(rv))
        return rv;

    return WriteFastID(fastID);
}

NS_IMETHODIMP
nsFastLoadFileWriter::Seek(PRInt32 aWhence, PRInt64 aOffset)
{
    mCurrentDocumentMapEntry = nsnull;
    return mSeekableOutput->Seek(aWhence, aOffset);
}

NS_IMETHODIMP
nsFastLoadFileWriter::Tell(PRInt64 *aResult)
{
    return mSeekableOutput->Tell(aResult);
}

NS_IMETHODIMP
nsFastLoadFileWriter::SetEOF()
{
    return mSeekableOutput->SetEOF();
}

NS_IMETHODIMP
nsFastLoadFileWriter::SetOutputStream(nsIOutputStream *aStream)
{
    nsresult rv = nsBinaryOutputStream::SetOutputStream(aStream);
    mSeekableOutput = do_QueryInterface(mOutputStream);
    return rv;
}

NS_COM nsresult
NS_NewFastLoadFileWriter(nsIObjectOutputStream* *aResult,
                         nsIOutputStream* aDestStream,
                         nsIFastLoadFileIO* aFileIO)
{
    nsFastLoadFileWriter* writer =
        new nsFastLoadFileWriter(aDestStream, aFileIO);
    if (!writer)
        return NS_ERROR_OUT_OF_MEMORY;

    // Stabilize writer's refcnt.
    nsCOMPtr<nsIObjectOutputStream> stream(writer);

    nsresult rv = writer->Open();
    if (NS_FAILED(rv))
        return rv;

    *aResult = stream;
    NS_ADDREF(*aResult);
    return NS_OK;
}

// -------------------------- nsFastLoadFileUpdater --------------------------

NS_IMPL_ISUPPORTS_INHERITED1(nsFastLoadFileUpdater,
                             nsFastLoadFileWriter,
                             nsIFastLoadFileIO)

NS_IMETHODIMP
nsFastLoadFileUpdater::GetInputStream(nsIInputStream** aResult)
{
    *aResult = mInputStream;
    NS_IF_ADDREF(*aResult);
    return NS_OK;
}

NS_IMETHODIMP
nsFastLoadFileUpdater::GetOutputStream(nsIOutputStream** aResult)
{
    *aResult = nsnull;
    return NS_OK;
}

PLDHashOperator PR_CALLBACK
nsFastLoadFileUpdater::CopyReadDocumentMapEntryToUpdater(PLDHashTable *aTable,
                                                         PLDHashEntryHdr *aHdr,
                                                         PRUint32 aNumber,
                                                         void *aData)
{
    nsDocumentMapReadEntry* readEntry =
        NS_STATIC_CAST(nsDocumentMapReadEntry*, aHdr);
    nsFastLoadFileUpdater* updater =
        NS_REINTERPRET_CAST(nsFastLoadFileUpdater*, aData);

    void* spec = nsMemory::Clone(readEntry->mString,
                                 strlen(readEntry->mString) + 1);
    if (!spec)
        return PL_DHASH_STOP;

    nsDocumentMapWriteEntry* writeEntry =
        NS_STATIC_CAST(nsDocumentMapWriteEntry*,
                       PL_DHashTableOperate(&updater->mDocumentMap, spec,
                                            PL_DHASH_ADD));
    if (!writeEntry) {
        nsMemory::Free(spec);
        return PL_DHASH_STOP;
    }

    writeEntry->mString = NS_REINTERPRET_CAST(const char*, spec);
    writeEntry->mURI = nsnull;
    writeEntry->mInitialSegmentOffset = readEntry->mInitialSegmentOffset;
    writeEntry->mCurrentSegmentOffset = 0;
    return PL_DHASH_NEXT;
}

nsresult
nsFastLoadFileUpdater::Open(nsFastLoadFileReader* aReader)
{
    nsresult rv;
    rv = nsFastLoadFileWriter::Init();
    if (NS_FAILED(rv))
        return rv;

    PRUint32 i, n;

    // Map from dense, zero-based, uint32 NSFastLoadID in reader to 16-byte
    // nsID in updater.
    nsID* readIDMap = aReader->mFooter.mIDMap;
    for (i = 0, n = aReader->mFooter.mNumIDs; i < n; i++) {
        NSFastLoadID fastID;
        rv = MapID(readIDMap[i], &fastID);
        NS_ASSERTION(fastID == i + 1, "huh?");
        if (NS_FAILED(rv))
            return rv;
    }

    // Map from reader dense, zero-based MFL_OID_TO_SHARP_INDEX(oid) to sharp
    // object offset and refcnt information in updater.
    nsFastLoadFileReader::nsObjectMapEntry* readObjectMap =
        aReader->mFooter.mObjectMap;

    // Prepare to save aReader state in case we need to seek back and read a
    // singleton object that might otherwise get written by this updater.
    nsDocumentMapReadEntry* saveDocMapEntry = nsnull;
    nsISeekableStream* inputSeekable = nsnull;
    PRInt64 saveOffset = 0;

    for (i = 0, n = aReader->mFooter.mNumSharpObjects; i < n; i++) {
        nsFastLoadFileReader::nsObjectMapEntry* readEntry = &readObjectMap[i];

        NS_ASSERTION(readEntry->mCIDOffset != 0,
                     "fastload updater: mCIDOffset cannot be zero!");

        // If the reader didn't read this object but it's a singleton, we must
        // "deserialize" it now, to discover its one and only root nsISupports
        // address.  The object already exists in memory if it was created at
        // startup without resort to the FastLoad file.  The canonical example
        // is the system principal object held by all XUL JS scripts.

        nsISupports* obj = readEntry->mReadObject;
        if (!obj && MFL_GET_SINGLETON_FLAG(readEntry)) {
            if (!saveDocMapEntry) {
                inputSeekable = aReader->mSeekableInput;
                rv = inputSeekable->Tell(&saveOffset);
                if (NS_FAILED(rv))
                    return rv;

                saveDocMapEntry = aReader->mCurrentDocumentMapEntry;
                aReader->mCurrentDocumentMapEntry = nsnull;
            }

            rv = inputSeekable->Seek(nsISeekableStream::NS_SEEK_SET,
                                     readEntry->mCIDOffset);
            if (NS_FAILED(rv))
                return rv;

            rv = aReader
                 ->DeserializeObject(getter_AddRefs(readEntry->mReadObject));
            if (NS_FAILED(rv))
                return rv;
            obj = readEntry->mReadObject;

            // Don't forget to set mSkipOffset in case someone calls the reader
            // to "deserialize" (yet again) the object we just read.
            //
            // Say the singleton is the system principal, and the FastLoad file
            // contains data for navigator.xul including scripts and functions.
            // If we update the FastLoad file to contain data for messenger.xul
            // in a separate session started via mozilla -mail, *and during the
            // same FastLoad episode in this session* race to open a navigator
            // window, we will attempt to read all objects serialized in the
            // navigator.xul portion of the FastLoad file.
            //
            // mSkipOffset must be set in such a case so the reader can skip
            // the system principal's serialized data, because the updater for
            // messenger.xul being opened here has already read it.

            rv = inputSeekable->Tell(&readEntry->mSkipOffset);
            if (NS_FAILED(rv))
                return rv;
        }

        NSFastLoadOID oid = MFL_SHARP_INDEX_TO_OID(i);
        void* key = obj
                    ? NS_REINTERPRET_CAST(void*, obj)
                    : NS_REINTERPRET_CAST(void*, (oid | MFL_OBJECT_DEF_TAG));

        nsSharpObjectMapEntry* writeEntry =
            NS_STATIC_CAST(nsSharpObjectMapEntry*,
                           PL_DHashTableOperate(&mObjectMap, key,
                                                PL_DHASH_ADD));
        if (!writeEntry)
            return NS_ERROR_OUT_OF_MEMORY;

        // Hold the object if there is one, so that objmap_ClearEntry can
        // release the reference.
        NS_IF_ADDREF(obj);
        writeEntry->mObject = NS_REINTERPRET_CAST(nsISupports*, key);
        writeEntry->mOID = oid;
        writeEntry->mInfo.mCIDOffset = readEntry->mCIDOffset;
        writeEntry->mInfo.mStrongRefCnt = readEntry->mSaveStrongRefCnt;
        writeEntry->mInfo.mWeakRefCnt = readEntry->mSaveWeakRefCnt;
    }

    // If we had to read any singletons, restore aReader's saved state.
    if (saveDocMapEntry) {
        rv = inputSeekable->Seek(nsISeekableStream::NS_SEEK_SET, saveOffset);
        if (NS_FAILED(rv))
            return rv;

        aReader->mCurrentDocumentMapEntry = saveDocMapEntry;
    }

    // Copy URI spec string and initial segment offset in FastLoad file from
    // nsDocumentMapReadEntry in reader to nsDocumentMapWriteEntry in updater.
    // If we didn't enumerate all entries, we ran out of memory.
    n = PL_DHashTableEnumerate(&aReader->mFooter.mDocumentMap,
                               CopyReadDocumentMapEntryToUpdater,
                               this);
    if (n != aReader->mFooter.mDocumentMap.entryCount)
        return NS_ERROR_OUT_OF_MEMORY;

    // Copy source filename dependencies from reader to updater.
    nsISupportsArray* readDeps = aReader->mFooter.mDependencies;
    rv = readDeps->Count(&n);
    if (NS_FAILED(rv))
        return rv;

    for (i = 0; i < n; i++) {
        nsCOMPtr<nsIFile> file;
        rv = readDeps->GetElementAt(i, getter_AddRefs(file));
        if (NS_FAILED(rv))
            return rv;

        rv = AddDependency(file);
        if (NS_FAILED(rv))
            return rv;
    }

    // Seek to the reader's footer offset so we overwrite the footer.  First,
    // update the header to have a zero mFooterOffset, which will invalidate
    // the FastLoad file on next startup read attempt, should we crash before
    // completing this update.
    rv = mSeekableOutput->Seek(nsISeekableStream::NS_SEEK_SET,
                               offsetof(nsFastLoadHeader, mFooterOffset));
    if (NS_FAILED(rv))
        return rv;

    rv = Write32(0);
    if (NS_FAILED(rv))
        return rv;

    rv = mSeekableOutput->Seek(nsISeekableStream::NS_SEEK_SET,
                               aReader->mHeader.mFooterOffset);
    if (NS_FAILED(rv))
        return rv;

    // Avoid creating yet another object by implementing nsIFastLoadFileIO on
    // this updater, and save aReader's input stream so it can be returned by
    // GetInputStream called from nsFastLoadFileWriter::Close.  This requires
    // that we override Close to break the resulting zero-length cycle.
    mFileIO = this;
    mInputStream = aReader->mInputStream;
    mSeekableInput = aReader->mSeekableInput;
    return NS_OK;
}

NS_IMETHODIMP
nsFastLoadFileUpdater::Close()
{
    // Call base-class Close implementation, which uses mFileIO.
    nsresult rv = nsFastLoadFileWriter::Close();

    // Break degenerate cycle from this->mFileIO to this.
    mFileIO = nsnull;
    return rv;
}

NS_COM nsresult
NS_NewFastLoadFileUpdater(nsIObjectOutputStream* *aResult,
                          nsIOutputStream* aOutputStream,
                          nsIObjectInputStream* aReaderAsStream)
{
    // Make sure that aReaderAsStream is an nsFastLoadFileReader.
    nsCOMPtr<nsIFastLoadFileReader> reader(do_QueryInterface(aReaderAsStream));
    if (!reader)
        return NS_ERROR_UNEXPECTED;

    nsFastLoadFileUpdater* updater = new nsFastLoadFileUpdater(aOutputStream);
    if (!updater)
        return NS_ERROR_OUT_OF_MEMORY;

    // Stabilize updater's refcnt.
    nsCOMPtr<nsIObjectOutputStream> stream(updater);

    nsresult rv = updater->Open(NS_STATIC_CAST(nsFastLoadFileReader*,
                                               aReaderAsStream));
    if (NS_FAILED(rv))
        return rv;

    *aResult = stream;
    NS_ADDREF(*aResult);
    return NS_OK;
}
