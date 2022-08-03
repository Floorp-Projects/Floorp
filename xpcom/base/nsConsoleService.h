/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * nsConsoleService class declaration.
 */

#ifndef __nsconsoleservice_h__
#define __nsconsoleservice_h__

#include <cstdint>
#include <utility>

#include "mozilla/Assertions.h"
#include "mozilla/LinkedList.h"
#include "mozilla/Mutex.h"

#include "MainThreadUtils.h"
#include "nsCOMPtr.h"
#include "nsInterfaceHashtable.h"
#include "nsHashKeys.h"

#include "nsIConsoleListener.h"
#include "nsIConsoleMessage.h"
#include "nsIConsoleService.h"
#include "nsIObserver.h"
#include "nsISupports.h"

template <class T>
class nsCOMArray;

class nsConsoleService final : public nsIConsoleService, public nsIObserver {
 public:
  nsConsoleService();
  nsresult Init();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICONSOLESERVICE
  NS_DECL_NSIOBSERVER

  void SetIsDelivering() {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(!mDeliveringMessage);
    mDeliveringMessage = true;
  }

  void SetDoneDelivering() {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mDeliveringMessage);
    mDeliveringMessage = false;
  }

  typedef nsInterfaceHashtable<nsISupportsHashKey, nsIConsoleListener>
      ListenerHash;
  void CollectCurrentListeners(nsCOMArray<nsIConsoleListener>& aListeners);

 private:
  class MessageElement : public mozilla::LinkedListElement<MessageElement> {
   public:
    explicit MessageElement(nsIConsoleMessage* aMessage) : mMessage(aMessage) {}

    nsIConsoleMessage* Get() { return mMessage.get(); }

    // Swap directly into an nsCOMPtr to avoid spurious refcount
    // traffic off the main thread in debug builds from
    // NSCAP_ASSERT_NO_QUERY_NEEDED().
    void swapMessage(nsCOMPtr<nsIConsoleMessage>& aRetVal) {
      mMessage.swap(aRetVal);
    }

    ~MessageElement();

   private:
    nsCOMPtr<nsIConsoleMessage> mMessage;

    MessageElement(const MessageElement&) = delete;
    MessageElement& operator=(const MessageElement&) = delete;
    MessageElement(MessageElement&&) = delete;
    MessageElement& operator=(MessageElement&&) = delete;
  };

  ~nsConsoleService();

  nsresult MaybeForwardScriptError(nsIConsoleMessage* aMessage, bool* sent);

  void ClearMessagesForWindowID(const uint64_t innerID);
  void ClearMessages() MOZ_REQUIRES(mLock);

  mozilla::LinkedList<MessageElement> mMessages MOZ_GUARDED_BY(mLock);

  // The current size of mMessages.
  uint32_t mCurrentSize MOZ_GUARDED_BY(mLock);

  // The maximum size of mMessages.
  const uint32_t mMaximumSize;

  // Are we currently delivering a console message on the main thread? If
  // so, we suppress incoming messages on the main thread only, to avoid
  // infinite repitition.
  // Only touched on MainThread
  bool mDeliveringMessage;

  // Listeners to notify whenever a new message is logged.
  ListenerHash mListeners MOZ_GUARDED_BY(mLock);

  // To serialize interesting methods.
  mozilla::Mutex mLock;
};

#endif /* __nsconsoleservice_h__ */
