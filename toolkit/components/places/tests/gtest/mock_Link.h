/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This is a mock Link object which can be used in tests.
 */

#ifndef mock_Link_h__
#define mock_Link_h__

#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/Link.h"
#include "mozilla/StaticPrefs_layout.h"

class mock_Link : public mozilla::dom::Link {
 public:
  NS_DECL_ISUPPORTS

  typedef void (*Handler)(nsLinkState);

  explicit mock_Link(Handler aHandlerFunction, bool aRunNextTest = true)
      : mozilla::dom::Link(), mRunNextTest(aRunNextTest) {
    AwaitNewNotification(aHandlerFunction);
  }

  void VisitedQueryFinished(bool aVisited) final {
    // Notify our callback function.
    mHandler(aVisited ? eLinkState_Visited : eLinkState_Unvisited);

    // Break the cycle so the object can be destroyed.
    mDeathGrip = nullptr;
  }

  size_t SizeOfExcludingThis(mozilla::SizeOfState& aState) const final {
    return 0;  // the value shouldn't matter
  }

  void NodeInfoChanged(mozilla::dom::Document* aOldDoc) final {}

  bool GotNotified() const { return !mDeathGrip; }

  void AwaitNewNotification(Handler aNewHandler) {
    MOZ_ASSERT(
        !mDeathGrip || !mozilla::StaticPrefs::layout_css_notify_of_unvisited(),
        "Still waiting for a notification");
    // Create a cyclic ownership, so that the link will be released only
    // after its status has been updated.  This will ensure that, when it should
    // run the next test, it will happen at the end of the test function, if
    // the link status has already been set before.  Indeed the link status is
    // updated on a separate connection, thus may happen at any time.
    mDeathGrip = this;
    mHandler = aNewHandler;
  }

 protected:
  ~mock_Link() {
    // Run the next test if we are supposed to.
    if (mRunNextTest) {
      run_next_test();
    }
  }

 private:
  Handler mHandler = nullptr;
  bool mRunNextTest;
  RefPtr<Link> mDeathGrip;
};

NS_IMPL_ISUPPORTS(mock_Link, mozilla::dom::Link)

#endif  // mock_Link_h__
