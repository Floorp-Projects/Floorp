/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "StreamFunctions.h"
#include "nsZipHeader.h"
#include "nsMemory.h"
#include "prtime.h"

#define ZIP_FILE_HEADER_SIGNATURE 0x04034b50
#define ZIP_FILE_HEADER_SIZE 30
#define ZIP_CDS_HEADER_SIGNATURE 0x02014b50
#define ZIP_CDS_HEADER_SIZE 46

#define FLAGS_IS_UTF8 0x800

#define ZIP_EXTENDED_TIMESTAMP_FIELD 0x5455
#define ZIP_EXTENDED_TIMESTAMP_MODTIME 0x01

using namespace mozilla;

/**
 * nsZipHeader represents an entry from a zip file.
 */
NS_IMPL_ISUPPORTS(nsZipHeader, nsIZipEntry)

NS_IMETHODIMP nsZipHeader::GetCompression(uint16_t* aCompression) {
  NS_ASSERTION(mInited, "Not initalised");

  *aCompression = mMethod;
  return NS_OK;
}

NS_IMETHODIMP nsZipHeader::GetSize(uint32_t* aSize) {
  NS_ASSERTION(mInited, "Not initalised");

  *aSize = mCSize;
  return NS_OK;
}

NS_IMETHODIMP nsZipHeader::GetRealSize(uint32_t* aRealSize) {
  NS_ASSERTION(mInited, "Not initalised");

  *aRealSize = mUSize;
  return NS_OK;
}

NS_IMETHODIMP nsZipHeader::GetCRC32(uint32_t* aCRC32) {
  NS_ASSERTION(mInited, "Not initalised");

  *aCRC32 = mCRC;
  return NS_OK;
}

NS_IMETHODIMP nsZipHeader::GetIsDirectory(bool* aIsDirectory) {
  NS_ASSERTION(mInited, "Not initalised");

  if (mName.Last() == '/')
    *aIsDirectory = true;
  else
    *aIsDirectory = false;
  return NS_OK;
}

NS_IMETHODIMP nsZipHeader::GetLastModifiedTime(PRTime* aLastModifiedTime) {
  NS_ASSERTION(mInited, "Not initalised");

  // Try to read timestamp from extra field
  uint16_t blocksize;
  const uint8_t* tsField =
      GetExtraField(ZIP_EXTENDED_TIMESTAMP_FIELD, false, &blocksize);
  if (tsField && blocksize >= 5) {
    uint32_t pos = 4;
    uint8_t flags;
    flags = READ8(tsField, &pos);
    if (flags & ZIP_EXTENDED_TIMESTAMP_MODTIME) {
      *aLastModifiedTime = (PRTime)(READ32(tsField, &pos)) * PR_USEC_PER_SEC;
      return NS_OK;
    }
  }

  // Use DOS date/time fields
  // Note that on DST shift we can't handle correctly the hour that is valid
  // in both DST zones
  PRExplodedTime time;

  time.tm_usec = 0;

  time.tm_hour = (mTime >> 11) & 0x1F;
  time.tm_min = (mTime >> 5) & 0x3F;
  time.tm_sec = (mTime & 0x1F) * 2;

  time.tm_year = (mDate >> 9) + 1980;
  time.tm_month = ((mDate >> 5) & 0x0F) - 1;
  time.tm_mday = mDate & 0x1F;

  time.tm_params.tp_gmt_offset = 0;
  time.tm_params.tp_dst_offset = 0;

  PR_NormalizeTime(&time, PR_GMTParameters);
  time.tm_params.tp_gmt_offset = PR_LocalTimeParameters(&time).tp_gmt_offset;
  PR_NormalizeTime(&time, PR_GMTParameters);
  time.tm_params.tp_dst_offset = PR_LocalTimeParameters(&time).tp_dst_offset;

  *aLastModifiedTime = PR_ImplodeTime(&time);

  return NS_OK;
}

NS_IMETHODIMP nsZipHeader::GetIsSynthetic(bool* aIsSynthetic) {
  NS_ASSERTION(mInited, "Not initalised");

  *aIsSynthetic = false;
  return NS_OK;
}

NS_IMETHODIMP nsZipHeader::GetPermissions(uint32_t* aPermissions) {
  NS_ASSERTION(mInited, "Not initalised");

  // Always give user read access at least, this matches nsIZipReader's
  // behaviour
  *aPermissions = ((mEAttr >> 16) & 0xfff) | 0x100;
  return NS_OK;
}

void nsZipHeader::Init(const nsACString& aPath, PRTime aDate, uint32_t aAttr,
                       uint32_t aOffset) {
  NS_ASSERTION(!mInited, "Already initalised");

  PRExplodedTime time;
  PR_ExplodeTime(aDate, PR_LocalTimeParameters, &time);

  mTime = time.tm_sec / 2 + (time.tm_min << 5) + (time.tm_hour << 11);
  mDate =
      time.tm_mday + ((time.tm_month + 1) << 5) + ((time.tm_year - 1980) << 9);

  // Store modification timestamp as extra field
  // First fill CDS extra field
  mFieldLength = 9;
  mExtraField = MakeUnique<uint8_t[]>(mFieldLength);
  if (!mExtraField) {
    mFieldLength = 0;
  } else {
    uint32_t pos = 0;
    WRITE16(mExtraField.get(), &pos, ZIP_EXTENDED_TIMESTAMP_FIELD);
    WRITE16(mExtraField.get(), &pos, 5);
    WRITE8(mExtraField.get(), &pos, ZIP_EXTENDED_TIMESTAMP_MODTIME);
    WRITE32(mExtraField.get(), &pos, aDate / PR_USEC_PER_SEC);

    // Fill local extra field
    mLocalExtraField = MakeUnique<uint8_t[]>(mFieldLength);
    if (mLocalExtraField) {
      mLocalFieldLength = mFieldLength;
      memcpy(mLocalExtraField.get(), mExtraField.get(), mLocalFieldLength);
    }
  }

  mEAttr = aAttr;
  mOffset = aOffset;
  mName = aPath;
  mComment = NS_LITERAL_CSTRING("");
  // Claim a UTF-8 path in case it needs it.
  mFlags |= FLAGS_IS_UTF8;
  mInited = true;
}

uint32_t nsZipHeader::GetFileHeaderLength() {
  return ZIP_FILE_HEADER_SIZE + mName.Length() + mLocalFieldLength;
}

nsresult nsZipHeader::WriteFileHeader(nsIOutputStream* aStream) {
  NS_ASSERTION(mInited, "Not initalised");

  uint8_t buf[ZIP_FILE_HEADER_SIZE];
  uint32_t pos = 0;
  WRITE32(buf, &pos, ZIP_FILE_HEADER_SIGNATURE);
  WRITE16(buf, &pos, mVersionNeeded);
  WRITE16(buf, &pos, mFlags);
  WRITE16(buf, &pos, mMethod);
  WRITE16(buf, &pos, mTime);
  WRITE16(buf, &pos, mDate);
  WRITE32(buf, &pos, mCRC);
  WRITE32(buf, &pos, mCSize);
  WRITE32(buf, &pos, mUSize);
  WRITE16(buf, &pos, mName.Length());
  WRITE16(buf, &pos, mLocalFieldLength);

  nsresult rv = ZW_WriteData(aStream, (const char*)buf, pos);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ZW_WriteData(aStream, mName.get(), mName.Length());
  NS_ENSURE_SUCCESS(rv, rv);

  if (mLocalFieldLength) {
    rv = ZW_WriteData(aStream, (const char*)mLocalExtraField.get(),
                      mLocalFieldLength);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

uint32_t nsZipHeader::GetCDSHeaderLength() {
  return ZIP_CDS_HEADER_SIZE + mName.Length() + mComment.Length() +
         mFieldLength;
}

nsresult nsZipHeader::WriteCDSHeader(nsIOutputStream* aStream) {
  NS_ASSERTION(mInited, "Not initalised");

  uint8_t buf[ZIP_CDS_HEADER_SIZE];
  uint32_t pos = 0;
  WRITE32(buf, &pos, ZIP_CDS_HEADER_SIGNATURE);
  WRITE16(buf, &pos, mVersionMade);
  WRITE16(buf, &pos, mVersionNeeded);
  WRITE16(buf, &pos, mFlags);
  WRITE16(buf, &pos, mMethod);
  WRITE16(buf, &pos, mTime);
  WRITE16(buf, &pos, mDate);
  WRITE32(buf, &pos, mCRC);
  WRITE32(buf, &pos, mCSize);
  WRITE32(buf, &pos, mUSize);
  WRITE16(buf, &pos, mName.Length());
  WRITE16(buf, &pos, mFieldLength);
  WRITE16(buf, &pos, mComment.Length());
  WRITE16(buf, &pos, mDisk);
  WRITE16(buf, &pos, mIAttr);
  WRITE32(buf, &pos, mEAttr);
  WRITE32(buf, &pos, mOffset);

  nsresult rv = ZW_WriteData(aStream, (const char*)buf, pos);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ZW_WriteData(aStream, mName.get(), mName.Length());
  NS_ENSURE_SUCCESS(rv, rv);
  if (mExtraField) {
    rv = ZW_WriteData(aStream, (const char*)mExtraField.get(), mFieldLength);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return ZW_WriteData(aStream, mComment.get(), mComment.Length());
}

nsresult nsZipHeader::ReadCDSHeader(nsIInputStream* stream) {
  NS_ASSERTION(!mInited, "Already initalised");

  uint8_t buf[ZIP_CDS_HEADER_SIZE];

  nsresult rv = ZW_ReadData(stream, (char*)buf, ZIP_CDS_HEADER_SIZE);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t pos = 0;
  uint32_t signature = READ32(buf, &pos);
  if (signature != ZIP_CDS_HEADER_SIGNATURE) return NS_ERROR_FILE_CORRUPTED;

  mVersionMade = READ16(buf, &pos);
  mVersionNeeded = READ16(buf, &pos);
  mFlags = READ16(buf, &pos);
  mMethod = READ16(buf, &pos);
  mTime = READ16(buf, &pos);
  mDate = READ16(buf, &pos);
  mCRC = READ32(buf, &pos);
  mCSize = READ32(buf, &pos);
  mUSize = READ32(buf, &pos);
  uint16_t namelength = READ16(buf, &pos);
  mFieldLength = READ16(buf, &pos);
  uint16_t commentlength = READ16(buf, &pos);
  mDisk = READ16(buf, &pos);
  mIAttr = READ16(buf, &pos);
  mEAttr = READ32(buf, &pos);
  mOffset = READ32(buf, &pos);

  if (namelength > 0) {
    auto field = MakeUnique<char[]>(namelength);
    NS_ENSURE_TRUE(field, NS_ERROR_OUT_OF_MEMORY);
    rv = ZW_ReadData(stream, field.get(), namelength);
    NS_ENSURE_SUCCESS(rv, rv);
    mName.Assign(field.get(), namelength);
  } else
    mName = NS_LITERAL_CSTRING("");

  if (mFieldLength > 0) {
    mExtraField = MakeUnique<uint8_t[]>(mFieldLength);
    NS_ENSURE_TRUE(mExtraField, NS_ERROR_OUT_OF_MEMORY);
    rv = ZW_ReadData(stream, (char*)mExtraField.get(), mFieldLength);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (commentlength > 0) {
    auto field = MakeUnique<char[]>(commentlength);
    NS_ENSURE_TRUE(field, NS_ERROR_OUT_OF_MEMORY);
    rv = ZW_ReadData(stream, field.get(), commentlength);
    NS_ENSURE_SUCCESS(rv, rv);
    mComment.Assign(field.get(), commentlength);
  } else
    mComment = NS_LITERAL_CSTRING("");

  mInited = true;
  return NS_OK;
}

const uint8_t* nsZipHeader::GetExtraField(uint16_t aTag, bool aLocal,
                                          uint16_t* aBlockSize) {
  const uint8_t* buf = aLocal ? mLocalExtraField.get() : mExtraField.get();
  uint32_t buflen = aLocal ? mLocalFieldLength : mFieldLength;
  uint32_t pos = 0;
  uint16_t tag, blocksize;

  while (buf && (pos + 4) <= buflen) {
    tag = READ16(buf, &pos);
    blocksize = READ16(buf, &pos);

    if (aTag == tag && (pos + blocksize) <= buflen) {
      *aBlockSize = blocksize;
      return buf + pos - 4;
    }

    pos += blocksize;
  }

  return nullptr;
}

/*
 * Pad extra field to align data starting position to specified size.
 */
nsresult nsZipHeader::PadExtraField(uint32_t aOffset, uint16_t aAlignSize) {
  uint32_t pad_size;
  uint32_t pa_offset;
  uint32_t pa_end;

  // Check for range and power of 2.
  if (aAlignSize < 2 || aAlignSize > 32768 ||
      (aAlignSize & (aAlignSize - 1)) != 0) {
    return NS_ERROR_INVALID_ARG;
  }

  // Point to current starting data position.
  aOffset += ZIP_FILE_HEADER_SIZE + mName.Length() + mLocalFieldLength;

  // Calculate aligned offset.
  pa_offset = aOffset & ~(aAlignSize - 1);
  pa_end = pa_offset + aAlignSize;
  pad_size = pa_end - aOffset;
  if (pad_size == 0) {
    return NS_OK;
  }

  // Leave enough room(at least 4 bytes) for valid values in extra field.
  while (pad_size < 4) {
    pad_size += aAlignSize;
  }
  // Extra field length is 2 bytes.
  if (mLocalFieldLength + pad_size > 65535) {
    return NS_ERROR_FAILURE;
  }

  UniquePtr<uint8_t[]> field = std::move(mLocalExtraField);
  uint32_t pos = mLocalFieldLength;

  mLocalExtraField = MakeUnique<uint8_t[]>(mLocalFieldLength + pad_size);
  memcpy(mLocalExtraField.get(), field.get(), mLocalFieldLength);
  // Use 0xFFFF as tag ID to avoid conflict with other IDs.
  // For more information, please read "Extensible data fields" section in:
  // http://www.pkware.com/documents/casestudies/APPNOTE.TXT
  WRITE16(mLocalExtraField.get(), &pos, 0xFFFF);
  WRITE16(mLocalExtraField.get(), &pos, pad_size - 4);
  memset(mLocalExtraField.get() + pos, 0, pad_size - 4);
  mLocalFieldLength += pad_size;

  return NS_OK;
}
