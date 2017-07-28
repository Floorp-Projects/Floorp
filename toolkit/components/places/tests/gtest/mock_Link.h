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

class mock_Link : public mozilla::dom::Link
{
public:
  NS_DECL_ISUPPORTS

  explicit mock_Link(void (*aHandlerFunction)(nsLinkState),
                     bool aRunNextTest = true)
  : mozilla::dom::Link()
  , mHandler(aHandlerFunction)
  , mRunNextTest(aRunNextTest)
  {
    // Create a cyclic ownership, so that the link will be released only
    // after its status has been updated.  This will ensure that, when it should
    // run the next test, it will happen at the end of the test function, if
    // the link status has already been set before.  Indeed the link status is
    // updated on a separate connection, thus may happen at any time.
    mDeathGrip = this;
  }

  virtual void SetLinkState(nsLinkState aState) override
  {
    // Notify our callback function.
    mHandler(aState);

    // Break the cycle so the object can be destroyed.
    mDeathGrip = nullptr;
  }

  virtual size_t SizeOfExcludingThis(mozilla::SizeOfState& aState)
    const override
  {
    return 0;   // the value shouldn't matter
  }

  virtual void NodeInfoChanged(nsIDocument* aOldDoc) final override {}

protected:
  ~mock_Link() {
    // Run the next test if we are supposed to.
    if (mRunNextTest) {
      run_next_test();
    }
  }

private:
  void (*mHandler)(nsLinkState);
  bool mRunNextTest;
  RefPtr<Link> mDeathGrip;
};

NS_IMPL_ISUPPORTS(
  mock_Link,
  mozilla::dom::Link
)

#endif // mock_Link_h__
