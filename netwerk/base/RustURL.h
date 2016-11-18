/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RustURL_h__
#define RustURL_h__

#include "nsISerializable.h"
#include "nsIFileURL.h"
#include "nsIStandardURL.h"
#include "nsIClassInfo.h"
#include "nsISizeOf.h"
#include "nsIIPCSerializableURI.h"
#include "nsISensitiveInfoHiddenURI.h"

#include "rust-url-capi/src/rust-url-capi.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {
namespace net {

class RustURL
  : public nsIFileURL
  , public nsIStandardURL
  , public nsISerializable
  , public nsIClassInfo
  , public nsISizeOf
  , public nsIIPCSerializableURI
  , public nsISensitiveInfoHiddenURI
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIURI
  NS_DECL_NSIURIWITHQUERY
  NS_DECL_NSIURL
  NS_DECL_NSIFILEURL
  NS_DECL_NSISTANDARDURL
  NS_DECL_NSISERIALIZABLE
  NS_DECL_NSICLASSINFO
  NS_DECL_NSIMUTABLE
  NS_DECL_NSIIPCSERIALIZABLEURI
  NS_DECL_NSISENSITIVEINFOHIDDENURI

  RustURL();
  // nsISizeOf
  virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override;
  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override;
private:
  virtual ~RustURL();

  struct FreeRustURL { void operator()(rusturl* aPtr) { rusturl_free(aPtr); } };
  mutable mozilla::UniquePtr<rusturl, FreeRustURL> mURL;

  bool mMutable;
};

} // namespace net
} // namespace mozilla

#endif // RustURL_h__
