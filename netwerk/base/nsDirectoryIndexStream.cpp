/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*

  The converts a filesystem directory into an "HTTP index" stream per
  Lou Montulli's original spec:

  http://www.mozilla.org/projects/netlib/dirindexformat.html

 */

#include "nsEscape.h"
#include "nsDirectoryIndexStream.h"
#include "mozilla/Logging.h"
#include "prtime.h"
#include "nsIFile.h"
#include "nsURLHelper.h"
#include "nsNativeCharsetUtils.h"

// NOTE: This runs on the _file transport_ thread.
// The problem is that now that we're actually doing something with the data,
// we want to do stuff like i18n sorting. However, none of the collation stuff
// is threadsafe.
// So THIS CODE IS ASCII ONLY!!!!!!!! This is no worse than the current
// behaviour, though. See bug 99382.

using namespace mozilla;
static LazyLogModule gLog("nsDirectoryIndexStream");

nsDirectoryIndexStream::nsDirectoryIndexStream() {
  MOZ_LOG(gLog, LogLevel::Debug, ("nsDirectoryIndexStream[%p]: created", this));
}

static int compare(nsIFile* aElement1, nsIFile* aElement2, void* aData) {
  if (!NS_IsNativeUTF8()) {
    // don't check for errors, because we can't report them anyway
    nsAutoString name1, name2;
    aElement1->GetLeafName(name1);
    aElement2->GetLeafName(name2);

    // Note - we should do the collation to do sorting. Why don't we?
    // Because that is _slow_. Using TestProtocols to list file:///dev/
    // goes from 3 seconds to 22. (This may be why nsXULSortService is
    // so slow as well).
    // Does this have bad effects? Probably, but since nsXULTree appears
    // to use the raw RDF literal value as the sort key (which ammounts to an
    // strcmp), it won't be any worse, I think.
    // This could be made faster, by creating the keys once,
    // but CompareString could still be smarter - see bug 99383 - bbaetz
    // NB - 99393 has been WONTFIXed. So if the I18N code is ever made
    // threadsafe so that this matters, we'd have to pass through a
    // struct { nsIFile*, uint8_t* } with the pre-calculated key.
    return Compare(name1, name2);
  }

  nsAutoCString name1, name2;
  aElement1->GetNativeLeafName(name1);
  aElement2->GetNativeLeafName(name2);

  return Compare(name1, name2);
}

nsresult nsDirectoryIndexStream::Init(nsIFile* aDir) {
  nsresult rv;
  bool isDir;
  rv = aDir->IsDirectory(&isDir);
  if (NS_FAILED(rv)) return rv;
  MOZ_ASSERT(isDir, "not a directory");
  if (!isDir) return NS_ERROR_ILLEGAL_VALUE;

  if (MOZ_LOG_TEST(gLog, LogLevel::Debug)) {
    MOZ_LOG(gLog, LogLevel::Debug,
            ("nsDirectoryIndexStream[%p]: initialized on %s", this,
             aDir->HumanReadablePath().get()));
  }

  // Sigh. We have to allocate on the heap because there are no
  // assignment operators defined.
  nsCOMPtr<nsIDirectoryEnumerator> iter;
  rv = aDir->GetDirectoryEntries(getter_AddRefs(iter));
  if (NS_FAILED(rv)) return rv;

  // Now lets sort, because clients expect it that way
  // XXX - should we do so here, or when the first item is requested?
  // XXX - use insertion sort instead?

  nsCOMPtr<nsIFile> file;
  while (NS_SUCCEEDED(iter->GetNextFile(getter_AddRefs(file))) && file) {
    mArray.AppendObject(file);  // addrefs
  }

  mArray.Sort(compare, nullptr);

  mBuf.AppendLiteral("300: ");
  nsAutoCString url;
  rv = net_GetURLSpecFromFile(aDir, url);
  if (NS_FAILED(rv)) return rv;
  mBuf.Append(url);
  mBuf.Append('\n');

  mBuf.AppendLiteral("200: filename content-length last-modified file-type\n");

  return NS_OK;
}

nsDirectoryIndexStream::~nsDirectoryIndexStream() {
  MOZ_LOG(gLog, LogLevel::Debug,
          ("nsDirectoryIndexStream[%p]: destroyed", this));
}

nsresult nsDirectoryIndexStream::Create(nsIFile* aDir,
                                        nsIInputStream** aResult) {
  RefPtr<nsDirectoryIndexStream> result = new nsDirectoryIndexStream();
  if (!result) return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = result->Init(aDir);
  if (NS_FAILED(rv)) {
    return rv;
  }

  result.forget(aResult);
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsDirectoryIndexStream, nsIInputStream)

// The below routines are proxied to the UI thread!
NS_IMETHODIMP
nsDirectoryIndexStream::Close() {
  mStatus = NS_BASE_STREAM_CLOSED;
  return NS_OK;
}

NS_IMETHODIMP
nsDirectoryIndexStream::Available(uint64_t* aLength) {
  if (NS_FAILED(mStatus)) return mStatus;

  // If there's data in our buffer, use that
  if (mOffset < (int32_t)mBuf.Length()) {
    *aLength = mBuf.Length() - mOffset;
    return NS_OK;
  }

  // Returning one byte is not ideal, but good enough
  *aLength = (mPos < mArray.Count()) ? 1 : 0;
  return NS_OK;
}

NS_IMETHODIMP
nsDirectoryIndexStream::Read(char* aBuf, uint32_t aCount,
                             uint32_t* aReadCount) {
  if (mStatus == NS_BASE_STREAM_CLOSED) {
    *aReadCount = 0;
    return NS_OK;
  }
  if (NS_FAILED(mStatus)) return mStatus;

  uint32_t nread = 0;

  // If anything is enqueued (or left-over) in mBuf, then feed it to
  // the reader first.
  while (mOffset < (int32_t)mBuf.Length() && aCount != 0) {
    *(aBuf++) = char(mBuf.CharAt(mOffset++));
    --aCount;
    ++nread;
  }

  // Room left?
  if (aCount > 0) {
    mOffset = 0;
    mBuf.Truncate();

    // Okay, now we'll suck stuff off of our iterator into the mBuf...
    while (uint32_t(mBuf.Length()) < aCount) {
      bool more = mPos < mArray.Count();
      if (!more) break;

      // don't addref, for speed - an addref happened when it
      // was placed in the array, so it's not going to go stale
      nsIFile* current = mArray.ObjectAt(mPos);
      ++mPos;

      if (MOZ_LOG_TEST(gLog, LogLevel::Debug)) {
        MOZ_LOG(gLog, LogLevel::Debug,
                ("nsDirectoryIndexStream[%p]: iterated %s", this,
                 current->HumanReadablePath().get()));
      }

      // rjc: don't return hidden files/directories!
      // bbaetz: why not?
      nsresult rv;
#ifndef XP_UNIX
      bool hidden = false;
      current->IsHidden(&hidden);
      if (hidden) {
        MOZ_LOG(gLog, LogLevel::Debug,
                ("nsDirectoryIndexStream[%p]: skipping hidden file/directory",
                 this));
        continue;
      }
#endif

      int64_t fileSize = 0;
      current->GetFileSize(&fileSize);

      PRTime fileInfoModifyTime = 0;
      current->GetLastModifiedTime(&fileInfoModifyTime);
      fileInfoModifyTime *= PR_USEC_PER_MSEC;

      mBuf.AppendLiteral("201: ");

      // The "filename" field
      if (!NS_IsNativeUTF8()) {
        nsAutoString leafname;
        rv = current->GetLeafName(leafname);
        if (NS_FAILED(rv)) return rv;

        nsAutoCString escaped;
        if (!leafname.IsEmpty() &&
            NS_Escape(NS_ConvertUTF16toUTF8(leafname), escaped, url_Path)) {
          mBuf.Append(escaped);
          mBuf.Append(' ');
        }
      } else {
        nsAutoCString leafname;
        rv = current->GetNativeLeafName(leafname);
        if (NS_FAILED(rv)) return rv;

        nsAutoCString escaped;
        if (!leafname.IsEmpty() && NS_Escape(leafname, escaped, url_Path)) {
          mBuf.Append(escaped);
          mBuf.Append(' ');
        }
      }

      // The "content-length" field
      mBuf.AppendInt(fileSize, 10);
      mBuf.Append(' ');

      // The "last-modified" field
      PRExplodedTime tm;
      PR_ExplodeTime(fileInfoModifyTime, PR_GMTParameters, &tm);
      {
        char buf[64];
        PR_FormatTimeUSEnglish(
            buf, sizeof(buf), "%a,%%20%d%%20%b%%20%Y%%20%H:%M:%S%%20GMT ", &tm);
        mBuf.Append(buf);
      }

      // The "file-type" field
      bool isFile = true;
      current->IsFile(&isFile);
      if (isFile) {
        mBuf.AppendLiteral("FILE ");
      } else {
        bool isDir;
        rv = current->IsDirectory(&isDir);
        if (NS_FAILED(rv)) return rv;
        if (isDir) {
          mBuf.AppendLiteral("DIRECTORY ");
        } else {
          bool isLink;
          rv = current->IsSymlink(&isLink);
          if (NS_FAILED(rv)) return rv;
          if (isLink) {
            mBuf.AppendLiteral("SYMBOLIC-LINK ");
          }
        }
      }

      mBuf.Append('\n');
    }

    // ...and once we've either run out of directory entries, or
    // filled up the buffer, then we'll push it to the reader.
    while (mOffset < (int32_t)mBuf.Length() && aCount != 0) {
      *(aBuf++) = char(mBuf.CharAt(mOffset++));
      --aCount;
      ++nread;
    }
  }

  *aReadCount = nread;
  return NS_OK;
}

NS_IMETHODIMP
nsDirectoryIndexStream::ReadSegments(nsWriteSegmentFun writer, void* closure,
                                     uint32_t count, uint32_t* _retval) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDirectoryIndexStream::IsNonBlocking(bool* aNonBlocking) {
  *aNonBlocking = false;
  return NS_OK;
}
