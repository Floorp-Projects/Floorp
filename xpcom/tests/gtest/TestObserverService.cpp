/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsISupports.h"
#include "nsIComponentManager.h"
#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "nsISimpleEnumerator.h"
#include "nsComponentManagerUtils.h"

#include "nsCOMPtr.h"
#include "nsWeakReference.h"

#include "mozilla/RefPtr.h"

#include "gtest/gtest.h"

static void testResult( nsresult rv ) {
  EXPECT_TRUE(NS_SUCCEEDED(rv)) << "0x" << std::hex << (int)rv;
}

class TestObserver final : public nsIObserver,
  public nsSupportsWeakReference
{
public:
  explicit TestObserver( const nsAString &name )
    : mName( name )
    , mObservations( 0 ) {
    }
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  nsString mName;
  int mObservations;

private:
  ~TestObserver() {}
};

NS_IMPL_ISUPPORTS( TestObserver, nsIObserver, nsISupportsWeakReference )

NS_IMETHODIMP
TestObserver::Observe(nsISupports *aSubject,
    const char *aTopic,
    const char16_t *someData ) {
  mObservations++;
  return NS_OK;
}

static nsISupports* ToSupports(TestObserver* aObs)
{
  return static_cast<nsIObserver*>(aObs);
}

TEST(ObserverService, Tests)
{
  nsCString topicA; topicA.Assign( "topic-A" );
  nsCString topicB; topicB.Assign( "topic-B" );
  nsresult rv;

  nsCOMPtr<nsIObserverService> anObserverService =
    do_CreateInstance("@mozilla.org/observer-service;1", &rv);

  ASSERT_EQ(rv, NS_OK);
  ASSERT_TRUE(anObserverService);

  RefPtr<TestObserver> aObserver = new TestObserver(NS_LITERAL_STRING("Observer-A"));
  RefPtr<TestObserver> bObserver = new TestObserver(NS_LITERAL_STRING("Observer-B"));

  rv = anObserverService->AddObserver(aObserver, topicA.get(), false);
  testResult(rv);

  rv = anObserverService->AddObserver(bObserver, topicA.get(), false);
  testResult(rv);

  rv = anObserverService->AddObserver(bObserver, topicB.get(), false);
  testResult(rv);

  rv = anObserverService->NotifyObservers(ToSupports(aObserver),
      topicA.get(),
      u"Testing Notify(observer-A, topic-A)" );
  testResult(rv);
  ASSERT_EQ(aObserver->mObservations, 1);
  ASSERT_EQ(bObserver->mObservations, 1);

  rv = anObserverService->NotifyObservers(ToSupports(bObserver),
      topicB.get(),
      u"Testing Notify(observer-B, topic-B)" );
  testResult(rv);
  ASSERT_EQ(aObserver->mObservations, 1);
  ASSERT_EQ(bObserver->mObservations, 2);

  nsCOMPtr<nsISimpleEnumerator> e;
  rv = anObserverService->EnumerateObservers(topicA.get(), getter_AddRefs(e));
  testResult(rv);
  ASSERT_TRUE(e);

  bool loop = true;
  int count = 0;
  while( NS_SUCCEEDED(e->HasMoreElements(&loop)) && loop)
  {
    count++;

    nsCOMPtr<nsISupports> supports;
    e->GetNext(getter_AddRefs(supports));
    ASSERT_TRUE(supports);

    nsCOMPtr<nsIObserver> observer = do_QueryInterface(supports);
    ASSERT_TRUE(observer);

    rv = observer->Observe( observer,
        topicA.get(),
        u"during enumeration" );
    testResult(rv);
  }

  ASSERT_EQ(count, 2);
  ASSERT_EQ(aObserver->mObservations, 2);
  ASSERT_EQ(bObserver->mObservations, 3);

  rv = anObserverService->RemoveObserver(aObserver, topicA.get());
  testResult(rv);

  rv = anObserverService->RemoveObserver(bObserver, topicB.get());
  testResult(rv);
  rv = anObserverService->RemoveObserver(bObserver, topicA.get());
  testResult(rv);
}
