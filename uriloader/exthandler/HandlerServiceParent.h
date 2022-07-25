/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef handler_service_parent_h
#define handler_service_parent_h

#include "mozilla/dom/PHandlerServiceParent.h"
#include "nsIMIMEInfo.h"

class nsIHandlerApp;

class HandlerServiceParent final : public mozilla::dom::PHandlerServiceParent {
 public:
  HandlerServiceParent();
  NS_INLINE_DECL_REFCOUNTING(HandlerServiceParent, final)

 private:
  virtual ~HandlerServiceParent();
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvFillHandlerInfo(
      const HandlerInfo& aHandlerInfoData, const nsACString& aOverrideType,
      HandlerInfo* handlerInfoData) override;

  mozilla::ipc::IPCResult RecvGetMIMEInfoFromOS(const nsACString& aMIMEType,
                                                const nsACString& aExtension,
                                                nsresult* aRv,
                                                HandlerInfo* aHandlerInfoData,
                                                bool* aFound) override;

  mozilla::ipc::IPCResult RecvExists(const HandlerInfo& aHandlerInfo,
                                     bool* exists) override;

  mozilla::ipc::IPCResult RecvGetTypeFromExtension(
      const nsACString& aFileExtension, nsCString* type) override;

  mozilla::ipc::IPCResult RecvExistsForProtocolOS(
      const nsACString& aProtocolScheme, bool* aHandlerExists) override;

  mozilla::ipc::IPCResult RecvExistsForProtocol(
      const nsACString& aProtocolScheme, bool* aHandlerExists) override;

  mozilla::ipc::IPCResult RecvGetApplicationDescription(
      const nsACString& aScheme, nsresult* aRv,
      nsString* aDescription) override;

  /*
   * Limit the length of MIME types, filename extensions, and protocol
   * schemes we'll consider.
   */
  static const size_t MAX_MIMETYPE_LENGTH = 129; /* Per RFC 6838, type and
                                                    subtype should be limited
                                                    to 64 characters. We add
                                                    one more to account for
                                                    a '/' separator. */
  static const size_t MAX_EXT_LENGTH = 64;       /* Arbitratily chosen to be
                                                    longer than any known
                                                    extension */
  static const size_t MAX_SCHEME_LENGTH = 1024;  /* Arbitratily chosen to be
                                                    longer than any known
                                                    protocol scheme */
};

#endif
