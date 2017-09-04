/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_WebRequestService_h
#define mozilla_WebRequestService_h

#include "mozIWebRequestService.h"

#include "mozilla/LinkedList.h"
#include "mozilla/Mutex.h"
#include "nsCOMPtr.h"
#include "nsHashKeys.h"
#include "nsClassHashtable.h"
#include "nsIAtom.h"
#include "nsIDOMWindowUtils.h"
#include "nsWeakPtr.h"

using namespace mozilla;

namespace mozilla {
namespace dom {
  class TabParent;
  class nsIContentParent;
}
}

class WebRequestService : public mozIWebRequestService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZIWEBREQUESTSERVICE

  explicit WebRequestService();

  static already_AddRefed<WebRequestService> GetInstance()
  {
    return do_AddRef(&GetSingleton());
  }

  static WebRequestService& GetSingleton();

  already_AddRefed<nsIChannel>
  GetTraceableChannel(uint64_t aChannelId, nsIAtom* aAddonId,
                      dom::nsIContentParent* aContentParent);

protected:
  virtual ~WebRequestService();

private:
  class ChannelParent : public LinkedListElement<ChannelParent>
  {
  public:
    explicit ChannelParent(uint64_t aChannelId, nsIChannel* aChannel, nsIAtom* aAddonId, nsITabParent* aTabParent);
    ~ChannelParent();

    void Detach();

    const RefPtr<dom::TabParent> mTabParent;
    const nsCOMPtr<nsIAtom> mAddonId;

  private:
    const uint64_t mChannelId;
    bool mDetached = false;
  };

  class Destructor : public nsIJSRAIIHelper
  {
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIJSRAIIHELPER

    explicit Destructor(ChannelParent* aChannelParent)
      : mChannelParent(aChannelParent)
      , mDestructCalled(false)
    {}

  protected:
    virtual ~Destructor();

  private:
    ChannelParent* mChannelParent;
    bool mDestructCalled;
  };

  class ChannelEntry
  {
  public:
    ~ChannelEntry();
    // Note: We can't keep a strong pointer to the channel here, since channels
    // are not cycle collected, and a reference to this object will be stored on
    // the channel in order to keep the entry alive.
    nsWeakPtr mChannel;
    LinkedList<ChannelParent> mTabParents;
  };

  nsClassHashtable<nsUint64HashKey, ChannelEntry> mChannelEntries;
  Mutex mDataLock;
};

#endif // mozilla_WebRequestService_h
