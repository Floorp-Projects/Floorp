/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GeckoViewInputStream_h__
#define GeckoViewInputStream_h__

#include "mozilla/java/GeckoViewInputStreamWrappers.h"
#include "mozilla/java/ContentInputStreamWrappers.h"
#include "nsIInputStream.h"

class GeckoViewInputStream : public nsIInputStream {
  NS_DECL_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM

  explicit GeckoViewInputStream(
      mozilla::java::GeckoViewInputStream::LocalRef aInstance)
      : mInstance(aInstance){};
  bool isClosed() const;

 protected:
  virtual ~GeckoViewInputStream() = default;

 private:
  mozilla::java::GeckoViewInputStream::LocalRef mInstance;
  bool mClosed{false};
};

class GeckoViewContentInputStream final : public GeckoViewInputStream {
 public:
  static nsresult getInstance(const nsAutoCString& aUri,
                              nsIInputStream** aInstance);
  static bool isReadable(const nsAutoCString& aUri);

 private:
  explicit GeckoViewContentInputStream(const nsAutoCString& aUri)
      : GeckoViewInputStream(mozilla::java::ContentInputStream::GetInstance(
            mozilla::jni::StringParam(aUri))) {}
};

#endif  // !GeckoViewInputStream_h__
