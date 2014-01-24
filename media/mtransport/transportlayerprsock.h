/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#ifndef transportlayerprsock_h__
#define transportlayerprsock_h__

#include "nspr.h"
#include "prio.h"

#include "nsASocketHandler.h"
#include "nsCOMPtr.h"
#include "nsISocketTransportService.h"
#include "nsXPCOM.h"

#include "m_cpp_utils.h"
#include "transportflow.h"
#include "transportlayer.h"

namespace mozilla {

class TransportLayerPrsock : public TransportLayer {
 public:
  TransportLayerPrsock() : fd_(nullptr), handler_() {}

  virtual ~TransportLayerPrsock() {
    Detach();
  }


  // Internal initializer
  virtual nsresult InitInternal();

  void Import(PRFileDesc *fd, nsresult *result);

  void Detach() {
    handler_->Detach();
  }

  // Implement TransportLayer
  virtual TransportResult SendPacket(const unsigned char *data, size_t len);

  TRANSPORT_LAYER_ID("prsock")

 private:
  DISALLOW_COPY_ASSIGN(TransportLayerPrsock);

  // Inner class
  class SocketHandler : public nsASocketHandler {
   public:
      SocketHandler(TransportLayerPrsock *prsock, PRFileDesc *fd) :
        prsock_(prsock), fd_(fd) {
        mPollFlags = PR_POLL_READ;
      }
      virtual ~SocketHandler() {}

      void Detach() {
        mCondition = NS_BASE_STREAM_CLOSED;
        prsock_ = nullptr;
      }

      // Implement nsASocket
      virtual void OnSocketReady(PRFileDesc *fd, int16_t outflags) {
        if (prsock_) {
          prsock_->OnSocketReady(fd, outflags);
        }
      }

      virtual void OnSocketDetached(PRFileDesc *fd) {
        if (prsock_) {
          prsock_->OnSocketDetached(fd);
        }
        PR_Close(fd_);
      }

      virtual void IsLocal(bool *aIsLocal) {
        // TODO(jesup): better check? Does it matter? (likely no)
        *aIsLocal = false;
      }

      virtual uint64_t ByteCountSent() { return 0; }
      virtual uint64_t ByteCountReceived() { return 0; }

      // nsISupports methods
      NS_DECL_THREADSAFE_ISUPPORTS

      private:
      TransportLayerPrsock *prsock_;
      PRFileDesc *fd_;
   private:
    DISALLOW_COPY_ASSIGN(SocketHandler);
  };

  // Allow SocketHandler to talk to our APIs
  friend class SocketHandler;

  // Functions to be called by SocketHandler
  void OnSocketReady(PRFileDesc *fd, int16_t outflags);
  void OnSocketDetached(PRFileDesc *fd) {
    TL_SET_STATE(TS_CLOSED);
  }
  void IsLocal(bool *aIsLocal) {
    // TODO(jesup): better check? Does it matter? (likely no)
    *aIsLocal = false;
  }

  PRFileDesc *fd_;
  nsCOMPtr<SocketHandler> handler_;
  nsCOMPtr<nsISocketTransportService> stservice_;

};



}  // close namespace
#endif
