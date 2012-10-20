/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include "bzlib.h"
#include "archivereader.h"
#include "errors.h"
#ifdef XP_WIN
#include "nsAlgorithm.h" // Needed by nsVersionComparator.cpp
#include "updatehelper.h"
#endif

#define UPDATER_NO_STRING_GLUE_STL
#include "../../../../xpcom/build/nsVersionComparator.cpp"
#undef UPDATER_NO_STRING_GLUE_STL

#if defined(XP_UNIX)
# include <sys/types.h>
#elif defined(XP_WIN)
# include <io.h>
#endif

static int inbuf_size  = 262144;
static int outbuf_size = 262144;
static char *inbuf  = NULL;
static char *outbuf = NULL;

#ifdef XP_WIN
#include "resource.h"

/**
 * Obtains the data of the specified resource name and type.
 *
 * @param  name The name ID of the resource
 * @param  type The type ID of the resource
 * @param  data Out parameter which sets the pointer to a buffer containing
 *                  the needed data.
 * @param  size Out parameter which sets the size of the returned data buffer 
 * @return TRUE on success
*/
BOOL
LoadFileInResource(int name, int type, const uint8_t *&data, uint32_t& size)
{
  HMODULE handle = GetModuleHandle(NULL);
  if (!handle) {
    return FALSE;
  }

  HRSRC resourceInfoBlockHandle = FindResource(handle, 
                                               MAKEINTRESOURCE(name),
                                               MAKEINTRESOURCE(type));
  if (!resourceInfoBlockHandle) {
    FreeLibrary(handle);
    return FALSE;
  }

  HGLOBAL resourceHandle = LoadResource(handle, resourceInfoBlockHandle);
  if (!resourceHandle) {
    FreeLibrary(handle);
    return FALSE;
  }

  size = SizeofResource(handle, resourceInfoBlockHandle);
  data = static_cast<const uint8_t*>(::LockResource(resourceHandle));
  FreeLibrary(handle);
  return TRUE;
}

/**
 * Performs a verification on the opened MAR file with the passed in
 * certificate name ID and type ID.
 *
 * @param  archive   The MAR file to verify the signature on
 * @param  name      The name ID of the resource
 * @param  type      THe type ID of the resource
 * @return OK on success, CERT_LOAD_ERROR or CERT_VERIFY_ERROR on failure.
*/
int
VerifyLoadedCert(MarFile *archive, int name, int type)
{
  uint32_t size = 0;
  const uint8_t *data = NULL;
  if (!LoadFileInResource(name, type, data, size) || !data || !size) {
    return CERT_LOAD_ERROR;
  }

  if (mar_verify_signaturesW(archive, &data, &size, 1)) {
    return CERT_VERIFY_ERROR;
  }

  return OK;
}
#endif


/**
 * Performs a verification on the opened MAR file.  Both the primary and backup 
 * keys stored are stored in the current process and at least the primary key 
 * will be tried.  Success will be returned as long as one of the two 
 * signatures verify.
 *
 * @return OK on success
*/
int
ArchiveReader::VerifySignature()
{
  if (!mArchive) {
    return ARCHIVE_NOT_OPEN;
  }

#ifdef XP_WIN
  // If the fallback key exists we're running an XPCShell test and we should
  // use the XPCShell specific cert for the signed MAR.
  int rv;
  if (DoesFallbackKeyExist()) {
    rv = VerifyLoadedCert(mArchive, IDR_XPCSHELL_CERT, TYPE_CERT);
  } else {
    rv = VerifyLoadedCert(mArchive, IDR_PRIMARY_CERT, TYPE_CERT);
    if (rv != OK) {
      rv = VerifyLoadedCert(mArchive, IDR_BACKUP_CERT, TYPE_CERT);
    }
  }
  return rv;
#else
  return OK;
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
int
ArchiveReader::VerifyProductInformation(const char *MARChannelID, 
                                        const char *appVersion)
{
  if (!mArchive) {
    return ARCHIVE_NOT_OPEN;
  }

  ProductInformationBlock productInfoBlock;
  int rv = mar_read_product_info_block(mArchive, 
                                       &productInfoBlock);
  if (rv != OK) {
    return COULD_NOT_READ_PRODUCT_INFO_BLOCK_ERROR;
  }

  // Only check the MAR channel name if specified, it should be passed in from
  // the update-settings.ini file.
  if (MARChannelID && strlen(MARChannelID)) {
    // Check for at least one match in the comma separated list of values.
    const char *delimiter = " ,\t";
    // Make a copy of the string in case a read only memory buffer 
    // was specified.  strtok modifies the input buffer.
    char channelCopy[512] = { 0 };
    strncpy(channelCopy, MARChannelID, sizeof(channelCopy) - 1);
    char *channel = strtok(channelCopy, delimiter);
    rv = MAR_CHANNEL_MISMATCH_ERROR;
    while(channel) {
      if (!strcmp(channel, productInfoBlock.MARChannelID)) {
        rv = OK;
        break;
      }
      channel = strtok(NULL, delimiter);
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

  free((void *)productInfoBlock.MARChannelID);
  free((void *)productInfoBlock.productVersion);
  return rv;
}

int
ArchiveReader::Open(const NS_tchar *path)
{
  if (mArchive)
    Close();

  if (!inbuf) {
    inbuf = (char *)malloc(inbuf_size);
    if (!inbuf) {
      // Try again with a smaller buffer.
      inbuf_size = 1024;
      inbuf = (char *)malloc(inbuf_size);
      if (!inbuf)
        return ARCHIVE_READER_MEM_ERROR;
    }
  }

  if (!outbuf) {
    outbuf = (char *)malloc(outbuf_size);
    if (!outbuf) {
      // Try again with a smaller buffer.
      outbuf_size = 1024;
      outbuf = (char *)malloc(outbuf_size);
      if (!outbuf)
        return ARCHIVE_READER_MEM_ERROR;
    }
  }

#ifdef XP_WIN
  mArchive = mar_wopen(path);
#else
  mArchive = mar_open(path);
#endif
  if (!mArchive)
    return READ_ERROR;

  return OK;
}

void
ArchiveReader::Close()
{
  if (mArchive) {
    mar_close(mArchive);
    mArchive = NULL;
  }

  if (inbuf) {
    free(inbuf);
    inbuf = NULL;
  }

  if (outbuf) {
    free(outbuf);
    outbuf = NULL;
  }
}

int
ArchiveReader::ExtractFile(const char *name, const NS_tchar *dest)
{
  const MarItem *item = mar_find_item(mArchive, name);
  if (!item)
    return READ_ERROR;

#ifdef XP_WIN
  FILE* fp = _wfopen(dest, L"wb+");
#else
  int fd = creat(dest, item->flags);
  if (fd == -1)
    return WRITE_ERROR;

  FILE *fp = fdopen(fd, "wb");
#endif
  if (!fp)
    return WRITE_ERROR;

  int rv = ExtractItemToStream(item, fp);

  fclose(fp);
  return rv;
}

int
ArchiveReader::ExtractFileToStream(const char *name, FILE *fp)
{
  const MarItem *item = mar_find_item(mArchive, name);
  if (!item)
    return READ_ERROR;

  return ExtractItemToStream(item, fp);
}

int
ArchiveReader::ExtractItemToStream(const MarItem *item, FILE *fp)
{
  /* decompress the data chunk by chunk */

  bz_stream strm;
  int offset, inlen, outlen, ret = OK;

  memset(&strm, 0, sizeof(strm));
  if (BZ2_bzDecompressInit(&strm, 0, 0) != BZ_OK)
    return UNEXPECTED_BZIP_ERROR;

  offset = 0;
  for (;;) {
    if (!item->length) {
      ret = UNEXPECTED_MAR_ERROR;
      break;
    }

    if (offset < (int) item->length && strm.avail_in == 0) {
      inlen = mar_read(mArchive, item, offset, inbuf, inbuf_size);
      if (inlen <= 0)
        return READ_ERROR;
      offset += inlen;
      strm.next_in = inbuf;
      strm.avail_in = inlen;
    }

    strm.next_out = outbuf;
    strm.avail_out = outbuf_size;

    ret = BZ2_bzDecompress(&strm);
    if (ret != BZ_OK && ret != BZ_STREAM_END) {
      ret = UNEXPECTED_BZIP_ERROR;
      break;
    }

    outlen = outbuf_size - strm.avail_out;
    if (outlen) {
      if (fwrite(outbuf, outlen, 1, fp) != 1) {
        ret = WRITE_ERROR;
        break;
      }
    }

    if (ret == BZ_STREAM_END) {
      ret = OK;
      break;
    }
  }

  BZ2_bzDecompressEnd(&strm);
  return ret;
}
