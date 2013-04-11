/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGZFileWriter.h"
#include "nsIFile.h"
#include "nsString.h"
#include "zlib.h"

#ifdef XP_WIN
#include <io.h>
#define _dup dup
#else
#include <unistd.h>
#endif

NS_IMPL_ISUPPORTS1(nsGZFileWriter, nsIGZFileWriter)

nsGZFileWriter::nsGZFileWriter()
  : mInitialized(false)
  , mFinished(false)
{}

nsGZFileWriter::~nsGZFileWriter()
{
  if (mInitialized && !mFinished) {
    Finish();
  }
}

NS_IMETHODIMP
nsGZFileWriter::Init(nsIFile* aFile)
{
  NS_ENSURE_FALSE(mInitialized, NS_ERROR_FAILURE);
  NS_ENSURE_FALSE(mFinished, NS_ERROR_FAILURE);

  // Get a FILE out of our nsIFile.  Convert that into a file descriptor which
  // gzip can own.  Then close our FILE, leaving only gzip's fd open.

  FILE* file;
  nsresult rv = aFile->OpenANSIFileDesc("wb", &file);
  NS_ENSURE_SUCCESS(rv, rv);

  mGZFile = gzdopen(dup(fileno(file)), "wb");
  fclose(file);

  // gzdopen returns NULL on error.
  NS_ENSURE_TRUE(mGZFile, NS_ERROR_FAILURE);
  mInitialized = true;

  return NS_OK;
}

NS_IMETHODIMP
nsGZFileWriter::Write(const nsACString& aStr)
{
  NS_ENSURE_TRUE(mInitialized, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_FALSE(mFinished, NS_ERROR_FAILURE);

  // gzwrite uses a return value of 0 to indicate failure.  Otherwise, it
  // returns the number of uncompressed bytes written.  To ensure we can
  // distinguish between success and failure, don't call gzwrite when we have 0
  // bytes to write.
  if (aStr.IsEmpty()) {
    return NS_OK;
  }

  // gzwrite never does a short write -- that is, the return value should
  // always be either 0 or aStr.Length(), and we shouldn't have to call it
  // multiple times in order to get it to read the whole buffer.
  int rv = gzwrite(mGZFile, aStr.BeginReading(), aStr.Length());
  NS_ENSURE_TRUE(rv == static_cast<int>(aStr.Length()), NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
nsGZFileWriter::Finish()
{
  NS_ENSURE_TRUE(mInitialized, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_FALSE(mFinished, NS_ERROR_FAILURE);

  mFinished = true;
  gzclose(mGZFile);

  // Ignore errors from gzclose; it's not like there's anything we can do about
  // it, at this point!
  return NS_OK;
}
