/* -*- Mode: C++; c-basic-offset: 2; tab-width: 8; indent-tabs-mode: nil; -*- */
/*****************************************************************************
 *
 * This 7z Library is based the 7z Client and 7z Standalone Extracting Plugin
 * code from the LZMA SDK.
 * It is in the public domain (see http://www.7-zip.org/sdk.html).
 *
 * Any copyright in these files held by contributors to the Mozilla Project is
 * also dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 *
 * Contributor(s):
 *   Alex Pakhotin <alexp@mozilla.com>
 *
 *****************************************************************************/

#ifndef __7ZLIB_H
#define __7ZLIB_H

#define SZ_OK 0
#define SZ_ERROR_DATA 1
#define SZ_ERROR_PARAM 5
#define SZ_ERROR_FAIL 11
#define SZ_ERROR_NO_ARCHIVE 17

const WCHAR* GetExtractorError();

typedef void SzExtractProgressCallback(int nPercentComplete);

/**
 * Extract 7z-archive
 */
int SzExtract(const WCHAR *archiveName,
              const WCHAR *fileToExtract, const WCHAR *outputDir,
              SzExtractProgressCallback *progressCallback);

int SzExtractSfx(const WCHAR *archiveName, DWORD sfxStubSize,
                 const WCHAR *fileToExtract, const WCHAR *outputDir,
                 SzExtractProgressCallback *progressCallback);

int SzGetSfxArchiveInfo(const WCHAR *archiveName, const DWORD sfxStubSize,
                        ULONGLONG *pUncompressedSize, DWORD *pNumberOfFiles = NULL, DWORD *pNumberOfDirs = NULL);

#endif // __7ZLIB_H
