/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGZFileWriter_h
#define nsGZFileWriter_h

#include "nsISupportsImpl.h"
#include "zlib.h"
#include "nsDependentString.h"
#include <stdio.h>

/**
 * A simple class for writing to a .gz file.
 *
 * Note that the file that this interface produces has a different format than
 * what you'd get if you compressed your data as a gzip stream and dumped the
 * result to a file.
 *
 * The standard gunzip tool cannot decompress a raw gzip stream, but can handle
 * the files produced by this interface.
 */
class nsGZFileWriter final {
  virtual ~nsGZFileWriter();

 public:
  enum Operation { Append, Create };

  explicit nsGZFileWriter(Operation aMode = Create);

  NS_INLINE_DECL_REFCOUNTING(nsGZFileWriter)

  /**
   * Initialize this object.  We'll write our gzip'ed data to the given file,
   * overwriting its contents if the file exists.
   *
   * Init() will return an error if called twice.  It's an error to call any
   * other method on this interface without first calling Init().
   */
  [[nodiscard]] nsresult Init(nsIFile* aFile);

  /**
   * Alternate version of Init() for use when the file is already opened;
   * e.g., with a FileDescriptor passed over IPC.
   */
  [[nodiscard]] nsresult InitANSIFileDesc(FILE* aFile);

  /**
   * Write the given string to the file.
   */
  [[nodiscard]] nsresult Write(const nsACString& aStr);

  /**
   * Write |length| bytes of |str| to the file.
   */
  [[nodiscard]] nsresult Write(const char* aStr, uint32_t aLen) {
    return Write(nsDependentCSubstring(aStr, aLen));
  }

  /**
   * Close this nsGZFileWriter. This method will be run when the underlying
   * object is destroyed, so it's not strictly necessary to explicitly call it
   * from your code.
   *
   * It's an error to call this method twice, and it's an error to call Write()
   * after Finish() has been called.
   */
  nsresult Finish();

 private:
  Operation mMode;
  bool mInitialized;
  bool mFinished;
  gzFile mGZFile;
};

#endif
