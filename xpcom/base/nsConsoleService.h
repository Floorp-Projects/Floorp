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

#include "mozilla/Attributes.h"
#include "mozilla/Mutex.h"

#include "nsInterfaceHashtable.h"
#include "nsHashKeys.h"

#include "nsIConsoleService.h"

class nsConsoleService MOZ_FINAL : public nsIConsoleService
{
public:
  nsConsoleService();
  nsresult Init();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICONSOLESERVICE

  void SetIsDelivering()
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(!mDeliveringMessage);
    mDeliveringMessage = true;
  }

  void SetDoneDelivering()
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mDeliveringMessage);
    mDeliveringMessage = false;
  }

  // This is a variant of LogMessage which allows the caller to determine
  // if the message should be output to an OS-specific log. This is used on
  // B2G to control whether the message is logged to the android log or not.

  enum OutputMode {
    SuppressLog,
    OutputToLog
  };
  virtual nsresult LogMessageWithMode(nsIConsoleMessage* aMessage,
                                      OutputMode aOutputMode);

  typedef nsInterfaceHashtable<nsISupportsHashKey, nsIConsoleListener> ListenerHash;
  void EnumerateListeners(ListenerHash::EnumReadFunction aFunction, void* aClosure);

private:
  ~nsConsoleService();

  // Circular buffer of saved messages
  nsIConsoleMessage** mMessages;

  // How big?
  uint32_t mBufferSize;

  // Index of slot in mMessages that'll be filled by *next* log message
  uint32_t mCurrent;

  // Is the buffer full? (Has mCurrent wrapped around at least once?)
  bool mFull;

  // Are we currently delivering a console message on the main thread? If
  // so, we suppress incoming messages on the main thread only, to avoid
  // infinite repitition.
  bool mDeliveringMessage;

  // Listeners to notify whenever a new message is logged.
  ListenerHash mListeners;

  // To serialize interesting methods.
  mozilla::Mutex mLock;
};

#endif /* __nsconsoleservice_h__ */
