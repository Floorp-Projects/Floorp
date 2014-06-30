/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGZFileWriter_h
#define nsGZFileWriter_h

#include "nsIGZFileWriter.h"
#include "zlib.h"

/**
 * A simple class for writing .gz files.
 */
class nsGZFileWriter : public nsIGZFileWriter
{
  virtual ~nsGZFileWriter();

public:
  nsGZFileWriter();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIGZFILEWRITER

  /**
   * nsIGZFileWriter exposes two non-virtual overloads of Write().  We
   * duplicate them here so that you can call these overloads on a pointer to
   * the concrete nsGZFileWriter class.
   */
  nsresult Write(const char* aStr)
  {
    return nsIGZFileWriter::Write(aStr);
  }

  nsresult Write(const char* aStr, uint32_t aLen)
  {
    return nsIGZFileWriter::Write(aStr, aLen);
  }

private:
  bool mInitialized;
  bool mFinished;
  gzFile mGZFile;
};

#endif
