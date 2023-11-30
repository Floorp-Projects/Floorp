/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#ifdef XP_WIN
#  include <windows.h>
#endif
#include "archivereader.h"
#include "updatererrors.h"
#ifdef XP_WIN
#  include "nsAlgorithm.h"  // Needed by nsVersionComparator.cpp
#  include "updatehelper.h"
#endif
#define XZ_USE_CRC64
#include "xz.h"

// These are generated at compile time based on the DER file for the channel
// being used
#ifdef MOZ_VERIFY_MAR_SIGNATURE
#  ifdef TEST_UPDATER
#    include "../xpcshellCert.h"
#  elif DEP_UPDATER
#    include "../dep1Cert.h"
#    include "../dep2Cert.h"
#  else
#    include "primaryCert.h"
#    include "secondaryCert.h"
#  endif
#endif

#define UPDATER_NO_STRING_GLUE_STL
#include "nsVersionComparator.cpp"
#undef UPDATER_NO_STRING_GLUE_STL

#if defined(XP_WIN)
#  include <io.h>
#endif

/**
 * Performs a verification on the opened MAR file with the passed in
 * certificate name ID and type ID.
 *
 * @param  archive   The MAR file to verify the signature on.
 * @param  certData  The certificate data.
 * @return OK on success, CERT_VERIFY_ERROR on failure.
 */
template <uint32_t SIZE>
int VerifyLoadedCert(MarFile* archive, const uint8_t (&certData)[SIZE]) {
  (void)archive;
  (void)certData;

#ifdef MOZ_VERIFY_MAR_SIGNATURE
  const uint32_t size = SIZE;
  const uint8_t* const data = &certData[0];
  if (mar_verify_signatures(archive, &data, &size, 1)) {
    return CERT_VERIFY_ERROR;
  }
#endif

  return OK;
}

/**
 * Performs a verification on the opened MAR file.  Both the primary and backup
 * keys stored are stored in the current process and at least the primary key
 * will be tried.  Success will be returned as long as one of the two
 * signatures verify.
 *
 * @return OK on success
 */
int ArchiveReader::VerifySignature() {
  if (!mArchive) {
    return ARCHIVE_NOT_OPEN;
  }

#ifndef MOZ_VERIFY_MAR_SIGNATURE
  return OK;
#else
#  ifdef TEST_UPDATER
  int rv = VerifyLoadedCert(mArchive, xpcshellCertData);
#  elif DEP_UPDATER
  int rv = VerifyLoadedCert(mArchive, dep1CertData);
  if (rv != OK) {
    rv = VerifyLoadedCert(mArchive, dep2CertData);
  }
#  else
  int rv = VerifyLoadedCert(mArchive, primaryCertData);
  if (rv != OK) {
    rv = VerifyLoadedCert(mArchive, secondaryCertData);
  }
#  endif
  return rv;
#endif
}

/**
 * Verifies that the MAR file matches the current product, channel, and version
 *
 * @param MARChannelID   The MAR channel name to use, only updates from MARs
 *                       with a matching MAR channel name will succeed.
 *                       If an empty string is passed, no check will be done
 *                       for the channel name in the product information block.
 *                       If a comma separated list of values is passed then
 *                       one value must match.
 * @param appVersion     The application version to use, only MARs with an
 *                       application version >= to appVersion will be applied.
 * @return OK on success
 *         COULD_NOT_READ_PRODUCT_INFO_BLOCK if the product info block
 *                                           could not be read.
 *         MARCHANNEL_MISMATCH_ERROR         if update-settings.ini's MAR
 *                                           channel ID doesn't match the MAR
 *                                           file's MAR channel ID.
 *         VERSION_DOWNGRADE_ERROR           if the application version for
 *                                           this updater is newer than the
 *                                           one in the MAR.
 */
int ArchiveReader::VerifyProductInformation(const char* MARChannelID,
                                            const char* appVersion) {
  if (!mArchive) {
    return ARCHIVE_NOT_OPEN;
  }

  ProductInformationBlock productInfoBlock;
  int rv = mar_read_product_info_block(mArchive, &productInfoBlock);
  if (rv != OK) {
    return COULD_NOT_READ_PRODUCT_INFO_BLOCK_ERROR;
  }

  // Only check the MAR channel name if specified, it should be passed in from
  // the update-settings.ini file.
  if (MARChannelID && strlen(MARChannelID)) {
    // Check for at least one match in the comma separated list of values.
    const char* delimiter = " ,\t";
    // Make a copy of the string in case a read only memory buffer
    // was specified.  strtok modifies the input buffer.
    char channelCopy[512] = {0};
    strncpy(channelCopy, MARChannelID, sizeof(channelCopy) - 1);
    char* channel = strtok(channelCopy, delimiter);
    rv = MAR_CHANNEL_MISMATCH_ERROR;
    while (channel) {
      if (!strcmp(channel, productInfoBlock.MARChannelID)) {
        rv = OK;
        break;
      }
      channel = strtok(nullptr, delimiter);
    }
  }

  if (rv == OK) {
    /* Compare both versions to ensure we don't have a downgrade
        -1 if appVersion is older than productInfoBlock.productVersion
        1 if appVersion is newer than productInfoBlock.productVersion
        0 if appVersion is the same as productInfoBlock.productVersion
       This even works with strings like:
        - 12.0a1 being older than 12.0a2
        - 12.0a2 being older than 12.0b1
        - 12.0a1 being older than 12.0
        - 12.0 being older than 12.1a1 */
    int versionCompareResult =
        mozilla::CompareVersions(appVersion, productInfoBlock.productVersion);
    if (1 == versionCompareResult) {
      rv = VERSION_DOWNGRADE_ERROR;
    }
  }

  free((void*)productInfoBlock.MARChannelID);
  free((void*)productInfoBlock.productVersion);
  return rv;
}

int ArchiveReader::Open(const NS_tchar* path) {
  if (mArchive) {
    Close();
  }

  if (!mInBuf) {
    mInBuf = (uint8_t*)malloc(mInBufSize);
    if (!mInBuf) {
      // Try again with a smaller buffer.
      mInBufSize = 1024;
      mInBuf = (uint8_t*)malloc(mInBufSize);
      if (!mInBuf) {
        return ARCHIVE_READER_MEM_ERROR;
      }
    }
  }

  if (!mOutBuf) {
    mOutBuf = (uint8_t*)malloc(mOutBufSize);
    if (!mOutBuf) {
      // Try again with a smaller buffer.
      mOutBufSize = 1024;
      mOutBuf = (uint8_t*)malloc(mOutBufSize);
      if (!mOutBuf) {
        return ARCHIVE_READER_MEM_ERROR;
      }
    }
  }

  MarReadResult result =
#ifdef XP_WIN
      mar_wopen(path, &mArchive);
#else
      mar_open(path, &mArchive);
#endif
  if (result == MAR_MEM_ERROR) {
    return ARCHIVE_READER_MEM_ERROR;
  }
  if (result != MAR_READ_SUCCESS) {
    return READ_ERROR;
  }

  xz_crc32_init();
  xz_crc64_init();

  return OK;
}

void ArchiveReader::Close() {
  if (mArchive) {
    mar_close(mArchive);
    mArchive = nullptr;
  }

  if (mInBuf) {
    free(mInBuf);
    mInBuf = nullptr;
  }

  if (mOutBuf) {
    free(mOutBuf);
    mOutBuf = nullptr;
  }
}

int ArchiveReader::ExtractFile(const char* name, const NS_tchar* dest) {
  const MarItem* item = mar_find_item(mArchive, name);
  if (!item) {
    return READ_ERROR;
  }

#ifdef XP_WIN
  FILE* fp = _wfopen(dest, L"wb+");
#else
  int fd = creat(dest, item->flags);
  if (fd == -1) {
    return WRITE_ERROR;
  }

  FILE* fp = fdopen(fd, "wb");
#endif
  if (!fp) {
    return WRITE_ERROR;
  }

  int rv = ExtractItemToStream(item, fp);

  fclose(fp);
  return rv;
}

int ArchiveReader::ExtractFileToStream(const char* name, FILE* fp) {
  const MarItem* item = mar_find_item(mArchive, name);
  if (!item) {
    return READ_ERROR;
  }

  return ExtractItemToStream(item, fp);
}

int ArchiveReader::ExtractItemToStream(const MarItem* item, FILE* fp) {
  /* decompress the data chunk by chunk */

  int offset, inlen, ret = OK;
  struct xz_buf strm = {0};
  enum xz_ret xz_rv = XZ_OK;

  struct xz_dec* dec = xz_dec_init(XZ_DYNALLOC, 64 * 1024 * 1024);
  if (!dec) {
    return UNEXPECTED_XZ_ERROR;
  }

  strm.in = mInBuf;
  strm.in_pos = 0;
  strm.in_size = 0;
  strm.out = mOutBuf;
  strm.out_pos = 0;
  strm.out_size = mOutBufSize;

  offset = 0;
  for (;;) {
    if (!item->length) {
      ret = UNEXPECTED_MAR_ERROR;
      break;
    }

    if (offset < (int)item->length && strm.in_pos == strm.in_size) {
      inlen = mar_read(mArchive, item, offset, mInBuf, mInBufSize);
      if (inlen <= 0) {
        ret = READ_ERROR;
        break;
      }
      offset += inlen;
      strm.in_size = inlen;
      strm.in_pos = 0;
    }

    xz_rv = xz_dec_run(dec, &strm);

    if (strm.out_pos == mOutBufSize) {
      if (fwrite(mOutBuf, 1, strm.out_pos, fp) != strm.out_pos) {
        ret = WRITE_ERROR_EXTRACT;
        break;
      }

      strm.out_pos = 0;
    }

    if (xz_rv == XZ_OK) {
      // There is still more data to decompress.
      continue;
    }

    // The return value of xz_dec_run is not XZ_OK and if it isn't XZ_STREAM_END
    // an error has occured.
    if (xz_rv != XZ_STREAM_END) {
      ret = UNEXPECTED_XZ_ERROR;
      break;
    }

    // Write out the remainder of the decompressed data. In the case of
    // strm.out_pos == 0 this is needed to create empty files included in the
    // mar file.
    if (fwrite(mOutBuf, 1, strm.out_pos, fp) != strm.out_pos) {
      ret = WRITE_ERROR_EXTRACT;
    }
    break;
  }

  xz_dec_end(dec);
  return ret;
}
