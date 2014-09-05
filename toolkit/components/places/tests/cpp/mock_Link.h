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
#include "mozilla/dom/URLSearchParams.h"

class mock_Link : public mozilla::dom::Link
{
public:
  NS_DECL_ISUPPORTS

  explicit mock_Link(void (*aHandlerFunction)(nsLinkState),
                     bool aRunNextTest = true)
  : mozilla::dom::Link(nullptr)
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

  virtual void SetLinkState(nsLinkState aState)
  {
    // Notify our callback function.
    mHandler(aState);

    // Break the cycle so the object can be destroyed.
    mDeathGrip = 0;
  }

  virtual size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
  {
    return 0;   // the value shouldn't matter
  }

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
  nsRefPtr<Link> mDeathGrip;
};

NS_IMPL_ISUPPORTS(
  mock_Link,
  mozilla::dom::Link
)

////////////////////////////////////////////////////////////////////////////////
//// Needed Link Methods

namespace mozilla {
namespace dom {

Link::Link(Element* aElement)
: mElement(aElement)
, mLinkState(eLinkState_NotLink)
, mRegistered(false)
{
}

Link::~Link()
{
}

bool
Link::ElementHasHref() const
{
  NS_NOTREACHED("Unexpected call to Link::ElementHasHref");
  return false; // suppress compiler warning
}

void
Link::SetLinkState(nsLinkState aState)
{
  NS_NOTREACHED("Unexpected call to Link::SetLinkState");
}

void
Link::ResetLinkState(bool aNotify, bool aHasHref)
{
  NS_NOTREACHED("Unexpected call to Link::ResetLinkState");
}

nsIURI*
Link::GetURI() const
{
  NS_NOTREACHED("Unexpected call to Link::GetURI");
  return nullptr; // suppress compiler warning
}

size_t
Link::SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  NS_NOTREACHED("Unexpected call to Link::SizeOfExcludingThis");
  return 0;
}

void
Link::URLSearchParamsUpdated(URLSearchParams* aSearchParams)
{
  NS_NOTREACHED("Unexpected call to Link::URLSearchParamsUpdated");
}

void
Link::UpdateURLSearchParams()
{
  NS_NOTREACHED("Unexpected call to Link::UpdateURLSearchParams");
}

NS_IMPL_CYCLE_COLLECTION_CLASS(URLSearchParams)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(URLSearchParams)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(URLSearchParams)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(URLSearchParams)

NS_IMPL_CYCLE_COLLECTING_ADDREF(URLSearchParams)
NS_IMPL_CYCLE_COLLECTING_RELEASE(URLSearchParams)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(URLSearchParams)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END


URLSearchParams::URLSearchParams()
{
}

URLSearchParams::~URLSearchParams()
{
}

JSObject*
URLSearchParams::WrapObject(JSContext* aCx)
{
  return nullptr;
}

void
URLSearchParams::ParseInput(const nsACString& aInput,
                            URLSearchParamsObserver* aObserver)
{
  NS_NOTREACHED("Unexpected call to URLSearchParams::ParseInput");
}

void
URLSearchParams::AddObserver(URLSearchParamsObserver* aObserver)
{
  NS_NOTREACHED("Unexpected call to URLSearchParams::SetObserver");
}

void
URLSearchParams::RemoveObserver(URLSearchParamsObserver* aObserver)
{
  NS_NOTREACHED("Unexpected call to URLSearchParams::SetObserver");
}

void
URLSearchParams::Serialize(nsAString& aValue) const
{
  NS_NOTREACHED("Unexpected call to URLSearchParams::Serialize");
}

void
URLSearchParams::Get(const nsAString& aName, nsString& aRetval)
{
  NS_NOTREACHED("Unexpected call to URLSearchParams::Get");
}

void
URLSearchParams::GetAll(const nsAString& aName, nsTArray<nsString >& aRetval)
{
  NS_NOTREACHED("Unexpected call to URLSearchParams::GetAll");
}

void
URLSearchParams::Set(const nsAString& aName, const nsAString& aValue)
{
  NS_NOTREACHED("Unexpected call to URLSearchParams::Set");
}

void
URLSearchParams::Append(const nsAString& aName, const nsAString& aValue)
{
  NS_NOTREACHED("Unexpected call to URLSearchParams::Append");
}

void
URLSearchParams::AppendInternal(const nsAString& aName, const nsAString& aValue)
{
  NS_NOTREACHED("Unexpected call to URLSearchParams::AppendInternal");
}

bool
URLSearchParams::Has(const nsAString& aName)
{
  NS_NOTREACHED("Unexpected call to URLSearchParams::Has");
  return false;
}

void
URLSearchParams::Delete(const nsAString& aName)
{
  NS_NOTREACHED("Unexpected call to URLSearchParams::Delete");
}

void
URLSearchParams::DeleteAll()
{
  NS_NOTREACHED("Unexpected call to URLSearchParams::DeleteAll");
}

void
URLSearchParams::NotifyObservers(URLSearchParamsObserver* aExceptObserver)
{
  NS_NOTREACHED("Unexpected call to URLSearchParams::NotifyObservers");
}

} // namespace dom
} // namespace mozilla

#endif // mock_Link_h__
