/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsOfflineObserver_h__
#define nsOfflineObserver_h__

#include "nsIObserver.h"

namespace mozilla {
namespace net {

/**
 * Parents should extend this class and have a nsRefPtr<OfflineObserver> member.
 * The constructor should initialize the member to new OfflineObserver(this)
 * and the destructor should call RemoveObserver on the member.
 *
 * GetAppId and OfflineDisconnect are called from the default implementation
 * of OfflineNotification. These should be overridden by classes that don't
 * provide an implementation of OfflineNotification.
 */
class DisconnectableParent
{
public:
  // This is called on the main thread, by the OfflineObserver.
  // aSubject is of type nsAppOfflineInfo and contains appId and offline mode.
  virtual nsresult OfflineNotification(nsISupports *aSubject);

  // GetAppId returns the appId for the app associated with the parent
  virtual uint32_t GetAppId() = 0;

  // OfflineDisconnect cancels all existing connections in the parent when
  // the app becomes offline.
  virtual void     OfflineDisconnect() { }
};

/**
 * This class observes the "network:app-offline-status-changed" topic and calls
 * OfflineNotification on the DisconnectableParent with the subject.
 */
class OfflineObserver
  : public nsIObserver
{
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER
public:
  // A nsRefPtr to this object should be kept by the disconnectable parent.

  explicit OfflineObserver(DisconnectableParent * parent);
  // This method needs to be called in the destructor of the parent
  // It removes the observer from the nsObserverService list, and it clears
  // the pointer it holds to the disconnectable parent.
  void RemoveObserver();
private:

  // These methods are called to register and unregister the observer.
  // If they are called on the main thread they register the observer right
  // away, otherwise they dispatch and event to the main thread
  void RegisterOfflineObserver();
  void RemoveOfflineObserver();
  void RegisterOfflineObserverMainThread();
  void RemoveOfflineObserverMainThread();
private:
  virtual ~OfflineObserver() { }
  DisconnectableParent * mParent;
};

} // namespace net
} // namespace mozilla

#endif // nsOfflineObserver_h__
