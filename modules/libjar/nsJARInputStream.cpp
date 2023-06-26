/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* nsJARInputStream.cpp
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsJARInputStream.h"
#include "zipstruct.h"  // defines ZIP compression codes
#include "nsZipArchive.h"
#include "mozilla/MmapFaultHandler.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/UniquePtrExtensions.h"

#include "nsEscape.h"
#include "nsDebug.h"
#include <algorithm>
#include <limits>
#if defined(XP_WIN)
#  include <windows.h>
#endif

/*---------------------------------------------
 *  nsISupports implementation
 *--------------------------------------------*/

NS_IMPL_ISUPPORTS(nsJARInputStream, nsIInputStream)

/*----------------------------------------------------------
 * nsJARInputStream implementation
 * Takes ownership of |fd|, even on failure
 *--------------------------------------------------------*/

nsresult nsJARInputStream::InitFile(nsZipHandle* aFd, const uint8_t* aData,
                                    nsZipItem* aItem) {
  nsresult rv = NS_OK;
  MOZ_DIAGNOSTIC_ASSERT(aFd, "Argument may not be null");
  if (!aFd) {
    return NS_ERROR_INVALID_ARG;
  }
  MOZ_ASSERT(aItem, "Argument may not be null");

  // Mark it as closed, in case something fails in initialisation
  mMode = MODE_CLOSED;
  //-- prepare for the compression type
  switch (aItem->Compression()) {
    case STORED:
      mMode = MODE_COPY;
      break;

    case DEFLATED:
      rv = gZlibInit(&mZs);
      NS_ENSURE_SUCCESS(rv, rv);

      mMode = MODE_INFLATE;
      mInCrc = aItem->CRC32();
      mOutCrc = crc32(0L, Z_NULL, 0);
      break;

    default:
      mFd = aFd;
      return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Must keep handle to filepointer and mmap structure as long as we need
  // access to the mmapped data
  mFd = aFd;
  mZs.next_in = (Bytef*)aData;
  if (!mZs.next_in) {
    return NS_ERROR_FILE_CORRUPTED;
  }
  mZs.avail_in = aItem->Size();
  mOutSize = aItem->RealSize();
  mZs.total_out = 0;
  return NS_OK;
}

nsresult nsJARInputStream::InitDirectory(nsJAR* aJar,
                                         const nsACString& aJarDirSpec,
                                         const char* aDir) {
  MOZ_ASSERT(aJar, "Argument may not be null");
  MOZ_ASSERT(aDir, "Argument may not be null");

  // Mark it as closed, in case something fails in initialisation
  mMode = MODE_CLOSED;

  // Keep the zipReader for getting the actual zipItems
  mJar = aJar;
  mJar->mLock.AssertCurrentThreadIn();
  mozilla::UniquePtr<nsZipFind> find;
  nsresult rv;
  // We can get aDir's contents as strings via FindEntries
  // with the following pattern (see nsIZipReader.findEntries docs)
  // assuming dirName is properly escaped:
  //
  //   dirName + "?*~" + dirName + "?*/?*"
  nsDependentCString dirName(aDir);
  mNameLen = dirName.Length();

  // iterate through dirName and copy it to escDirName, escaping chars
  // which are special at the "top" level of the regexp so FindEntries
  // works correctly
  nsAutoCString escDirName;
  const char* curr = dirName.BeginReading();
  const char* end = dirName.EndReading();
  while (curr != end) {
    switch (*curr) {
      case '*':
      case '?':
      case '$':
      case '[':
      case ']':
      case '^':
      case '~':
      case '(':
      case ')':
      case '\\':
        escDirName.Append('\\');
        [[fallthrough]];
      default:
        escDirName.Append(*curr);
    }
    ++curr;
  }
  nsAutoCString pattern = escDirName + "?*~"_ns + escDirName + "?*/?*"_ns;
  rv = mJar->mZip->FindInit(pattern.get(), getter_Transfers(find));
  if (NS_FAILED(rv)) return rv;

  const char* name;
  uint16_t nameLen;
  while ((rv = find->FindNext(&name, &nameLen)) == NS_OK) {
    // Must copy, to make it zero-terminated
    mArray.AppendElement(nsCString(name, nameLen));
  }

  if (rv != NS_ERROR_FILE_NOT_FOUND && NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;  // no error translation
  }

  // Sort it
  mArray.Sort();

  mBuffer.AssignLiteral("300: ");
  mBuffer.Append(aJarDirSpec);
  mBuffer.AppendLiteral(
      "\n200: filename content-length last-modified file-type\n");

  // Open for reading
  mMode = MODE_DIRECTORY;
  mZs.total_out = 0;
  mArrPos = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsJARInputStream::Available(uint64_t* _retval) {
  // A lot of callers don't check the error code.
  // They just use the _retval value.
  *_retval = 0;

  uint64_t maxAvailableSize = 0;

  switch (mMode) {
    case MODE_NOTINITED:
      break;

    case MODE_CLOSED:
      return NS_BASE_STREAM_CLOSED;

    case MODE_DIRECTORY:
      *_retval = mBuffer.Length();
      break;

    case MODE_INFLATE:
    case MODE_COPY:
      maxAvailableSize = mozilla::StaticPrefs::network_jar_max_available_size();
      if (!maxAvailableSize) {
        maxAvailableSize = std::numeric_limits<uint64_t>::max();
      }
      *_retval = std::min<uint64_t>(mOutSize - mZs.total_out, maxAvailableSize);
      break;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsJARInputStream::StreamStatus() {
  return mMode == MODE_CLOSED ? NS_BASE_STREAM_CLOSED : NS_OK;
}

NS_IMETHODIMP
nsJARInputStream::Read(char* aBuffer, uint32_t aCount, uint32_t* aBytesRead) {
  NS_ENSURE_ARG_POINTER(aBuffer);
  NS_ENSURE_ARG_POINTER(aBytesRead);

  *aBytesRead = 0;

  nsresult rv = NS_OK;
  MMAP_FAULT_HANDLER_BEGIN_HANDLE(mFd)
  switch (mMode) {
    case MODE_NOTINITED:
      return NS_OK;

    case MODE_CLOSED:
      return NS_BASE_STREAM_CLOSED;

    case MODE_DIRECTORY:
      return ReadDirectory(aBuffer, aCount, aBytesRead);

    case MODE_INFLATE:
      if (mZs.total_out < mOutSize) {
        rv = ContinueInflate(aBuffer, aCount, aBytesRead);
      }
      // be aggressive about releasing the file!
      // note that sometimes, we will release  mFd before we've finished
      // deflating - this is because zlib buffers the input
      if (mZs.avail_in == 0) {
        mFd = nullptr;
      }
      break;

    case MODE_COPY:
      if (mFd) {
        MOZ_DIAGNOSTIC_ASSERT(mOutSize >= mZs.total_out,
                              "Did we read more than expected?");
        uint32_t count = std::min(aCount, mOutSize - uint32_t(mZs.total_out));
        if (count) {
          std::copy(mZs.next_in + mZs.total_out,
                    mZs.next_in + mZs.total_out + count, aBuffer);
          mZs.total_out += count;
        }
        *aBytesRead = count;
      }
      // be aggressive about releasing the file!
      // note that sometimes, we will release mFd before we've finished copying.
      if (mZs.total_out >= mOutSize) {
        mFd = nullptr;
      }
      break;
  }
  MMAP_FAULT_HANDLER_CATCH(NS_ERROR_FAILURE)
  return rv;
}

NS_IMETHODIMP
nsJARInputStream::ReadSegments(nsWriteSegmentFun writer, void* closure,
                               uint32_t count, uint32_t* _retval) {
  // don't have a buffer to read from, so this better not be called!
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJARInputStream::IsNonBlocking(bool* aNonBlocking) {
  *aNonBlocking = false;
  return NS_OK;
}

NS_IMETHODIMP
nsJARInputStream::Close() {
  if (mMode == MODE_INFLATE) {
    inflateEnd(&mZs);
  }
  mMode = MODE_CLOSED;
  mFd = nullptr;
  return NS_OK;
}

nsresult nsJARInputStream::ContinueInflate(char* aBuffer, uint32_t aCount,
                                           uint32_t* aBytesRead) {
  bool finished = false;

  // No need to check the args, ::Read did that, but assert them at least
  NS_ASSERTION(aBuffer, "aBuffer parameter must not be null");
  NS_ASSERTION(aBytesRead, "aBytesRead parameter must not be null");

  // Keep old total_out count
  const uint32_t oldTotalOut = mZs.total_out;

  // make sure we aren't reading too much
  mZs.avail_out = std::min(aCount, (mOutSize - oldTotalOut));
  mZs.next_out = (unsigned char*)aBuffer;

  if (mMode == MODE_INFLATE) {
    // now inflate
    int zerr = inflate(&mZs, Z_SYNC_FLUSH);
    if ((zerr != Z_OK) && (zerr != Z_STREAM_END)) {
      return NS_ERROR_FILE_CORRUPTED;
    }
    // If inflating did not read anything more, then the stream is finished.
    finished = (zerr == Z_STREAM_END) ||
               (mZs.avail_out && mZs.total_out == oldTotalOut);
  }

  *aBytesRead = (mZs.total_out - oldTotalOut);

  // Calculate the CRC on the output
  mOutCrc = crc32(mOutCrc, (unsigned char*)aBuffer, *aBytesRead);

  // be aggressive about ending the inflation
  // for some reason we don't always get Z_STREAM_END
  if (finished || mZs.total_out >= mOutSize) {
    if (mMode == MODE_INFLATE) {
      int zerr = inflateEnd(&mZs);
      if (zerr != Z_OK) {
        return NS_ERROR_FILE_CORRUPTED;
      }

      // Stream is finished but has a different size from what
      // we expected.
      if (mozilla::StaticPrefs::network_jar_require_size_match() &&
          mZs.total_out != mOutSize) {
        return NS_ERROR_FILE_CORRUPTED;
      }
    }

    // stop returning valid data as soon as we know we have a bad CRC
    if (mOutCrc != mInCrc) {
      return NS_ERROR_FILE_CORRUPTED;
    }
  }

  return NS_OK;
}

nsresult nsJARInputStream::ReadDirectory(char* aBuffer, uint32_t aCount,
                                         uint32_t* aBytesRead) {
  // No need to check the args, ::Read did that, but assert them at least
  NS_ASSERTION(aBuffer, "aBuffer parameter must not be null");
  NS_ASSERTION(aBytesRead, "aBytesRead parameter must not be null");

  // If the buffer contains data, copy what's there up to the desired amount
  uint32_t numRead = CopyDataToBuffer(aBuffer, aCount);

  if (aCount > 0) {
    mozilla::RecursiveMutexAutoLock lock(mJar->mLock);
    // empty the buffer and start writing directory entry lines to it
    mBuffer.Truncate();
    mCurPos = 0;
    const uint32_t arrayLen = mArray.Length();

    for (; aCount > mBuffer.Length(); mArrPos++) {
      // have we consumed all the directory contents?
      if (arrayLen <= mArrPos) break;

      const char* entryName = mArray[mArrPos].get();
      uint32_t entryNameLen = mArray[mArrPos].Length();
      nsZipItem* ze = mJar->mZip->GetItem(entryName);
      NS_ENSURE_TRUE(ze, NS_ERROR_FILE_NOT_FOUND);

      // Last Modified Time
      PRExplodedTime tm;
      PR_ExplodeTime(ze->LastModTime(), PR_GMTParameters, &tm);
      char itemLastModTime[65];
      PR_FormatTimeUSEnglish(itemLastModTime, sizeof(itemLastModTime),
                             " %a,%%20%d%%20%b%%20%Y%%20%H:%M:%S%%20GMT ", &tm);

      // write a 201: line to the buffer for this item
      // 200: filename content-length last-modified file-type
      mBuffer.AppendLiteral("201: ");

      // Names must be escaped and relative, so use the pre-calculated length
      // of the directory name as the offset into the string
      // NS_EscapeURL adds the escaped URL to the give string buffer
      NS_EscapeURL(entryName + mNameLen, entryNameLen - mNameLen,
                   esc_Minimal | esc_AlwaysCopy, mBuffer);

      mBuffer.Append(' ');
      mBuffer.AppendInt(ze->RealSize(), 10);
      mBuffer.Append(itemLastModTime);  // starts/ends with ' '
      if (ze->IsDirectory())
        mBuffer.AppendLiteral("DIRECTORY\n");
      else
        mBuffer.AppendLiteral("FILE\n");
    }

    // Copy up to the desired amount of data to buffer
    numRead += CopyDataToBuffer(aBuffer, aCount);
  }

  *aBytesRead = numRead;
  return NS_OK;
}

uint32_t nsJARInputStream::CopyDataToBuffer(char*& aBuffer, uint32_t& aCount) {
  const uint32_t writeLength =
      std::min<uint32_t>(aCount, mBuffer.Length() - mCurPos);

  if (writeLength > 0) {
    std::copy(mBuffer.get() + mCurPos, mBuffer.get() + mCurPos + writeLength,
              aBuffer);
    mCurPos += writeLength;
    aCount -= writeLength;
    aBuffer += writeLength;
  }

  // return number of bytes copied to the buffer so the
  // Read method can return the number of bytes copied
  return writeLength;
}
