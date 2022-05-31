
/* -*- Mode: c++; c-basic-offset: 2; tab-width: 2; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GeckoViewOutputStream_h__
#define GeckoViewOutputStream_h__

#include "mozilla/java/GeckoInputStreamNatives.h"
#include "mozilla/java/GeckoInputStreamWrappers.h"

#include "nsIOutputStream.h"
#include "nsIRequest.h"

class GeckoViewOutputStream : public nsIOutputStream {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOUTPUTSTREAM
  explicit GeckoViewOutputStream(
      mozilla::java::GeckoInputStream::GlobalRef aStream)
      : mStream(aStream) {}

 private:
  const mozilla::java::GeckoInputStream::GlobalRef mStream;
  virtual ~GeckoViewOutputStream() = default;
};

#endif  // GeckoViewOutputStream_h__
