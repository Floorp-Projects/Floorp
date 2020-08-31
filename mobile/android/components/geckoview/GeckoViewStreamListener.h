/* -*- Mode: c++; c-basic-offset: 2; tab-width: 2; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GeckoViewStreamListener_h__
#define GeckoViewStreamListener_h__

#include "nsIStreamListener.h"
#include "nsIInterfaceRequestor.h"
#include "nsIChannelEventSink.h"

#include "mozilla/widget/EventDispatcher.h"
#include "mozilla/java/GeckoInputStreamNatives.h"
#include "mozilla/java/WebResponseWrappers.h"

#include "JavaBuiltins.h"

namespace mozilla {

class GeckoViewStreamListener : public nsIStreamListener,
                                public nsIInterfaceRequestor,
                                public nsIChannelEventSink {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSICHANNELEVENTSINK

  explicit GeckoViewStreamListener() {}

  static std::tuple<jni::ByteArray::LocalRef, java::sdk::Boolean::LocalRef>
  CertificateFromChannel(nsIChannel* aChannel);

 protected:
  virtual ~GeckoViewStreamListener() {}

  java::GeckoInputStream::GlobalRef mStream;
  java::GeckoInputStream::Support::GlobalRef mSupport;

  void InitializeStreamSupport(nsIRequest* aRequest);

  static nsresult WriteSegment(nsIInputStream* aInputStream, void* aClosure,
                               const char* aFromSegment, uint32_t aToOffset,
                               uint32_t aCount, uint32_t* aWriteCount);

  virtual nsresult HandleWebResponse(nsIRequest* aRequest);

  virtual void SendWebResponse(java::WebResponse::Param aResponse) = 0;

  virtual void CompleteWithError(nsresult aStatus, nsIChannel* aChannel) = 0;
};

}  // namespace mozilla

#endif  // GeckoViewStreamListener_h__
