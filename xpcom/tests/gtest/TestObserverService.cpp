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
  static int sTotalObservations;

  nsString mExpectedData;

private:
  ~TestObserver() {}
};

NS_IMPL_ISUPPORTS( TestObserver, nsIObserver, nsISupportsWeakReference )

int TestObserver::sTotalObservations;

NS_IMETHODIMP
TestObserver::Observe(nsISupports *aSubject,
    const char *aTopic,
    const char16_t *someData ) {
  mObservations++;
  sTotalObservations++;

  if (!mExpectedData.IsEmpty()) {
    EXPECT_TRUE(mExpectedData.Equals(someData));
  }

  return NS_OK;
}

static nsISupports* ToSupports(TestObserver* aObs)
{
  return static_cast<nsIObserver*>(aObs);
}

static void TestExpectedCount(
    nsIObserverService* svc,
    const char* topic,
    size_t expected)
{
  nsCOMPtr<nsISimpleEnumerator> e;
  nsresult rv = svc->EnumerateObservers(topic, getter_AddRefs(e));
  testResult(rv);
  EXPECT_TRUE(e);

  bool hasMore = false;
  rv = e->HasMoreElements(&hasMore);
  testResult(rv);

  if (expected == 0) {
    EXPECT_FALSE(hasMore);
    return;
  }

  size_t count = 0;
  while (hasMore) {
    count++;

    // Grab the element.
    nsCOMPtr<nsISupports> supports;
    e->GetNext(getter_AddRefs(supports));
    ASSERT_TRUE(supports);

    // Move on.
    rv = e->HasMoreElements(&hasMore);
    testResult(rv);
  }

  EXPECT_EQ(count, expected);
}

TEST(ObserverService, Creation)
{
  nsresult rv;
  nsCOMPtr<nsIObserverService> svc =
    do_CreateInstance("@mozilla.org/observer-service;1", &rv);

  ASSERT_EQ(rv, NS_OK);
  ASSERT_TRUE(svc);
}

TEST(ObserverService, AddObserver)
{
  nsCOMPtr<nsIObserverService> svc =
    do_CreateInstance("@mozilla.org/observer-service;1");

  // Add a strong ref.
  RefPtr<TestObserver> a = new TestObserver(NS_LITERAL_STRING("A"));
  nsresult rv = svc->AddObserver(a, "Foo", false);
  testResult(rv);

  // Add a few weak ref.
  RefPtr<TestObserver> b = new TestObserver(NS_LITERAL_STRING("B"));
  rv = svc->AddObserver(b, "Bar", true);
  testResult(rv);
}

TEST(ObserverService, RemoveObserver)
{
  nsCOMPtr<nsIObserverService> svc =
    do_CreateInstance("@mozilla.org/observer-service;1");

  RefPtr<TestObserver> a = new TestObserver(NS_LITERAL_STRING("A"));
  RefPtr<TestObserver> b = new TestObserver(NS_LITERAL_STRING("B"));
  RefPtr<TestObserver> c = new TestObserver(NS_LITERAL_STRING("C"));

  svc->AddObserver(a, "Foo", false);
  svc->AddObserver(b, "Foo", true);

  // Remove from non-existent topic.
  nsresult rv = svc->RemoveObserver(a, "Bar");
  ASSERT_TRUE(NS_FAILED(rv));

  // Remove a.
  testResult(svc->RemoveObserver(a, "Foo"));

  // Remove b.
  testResult(svc->RemoveObserver(b, "Foo"));

  // Attempt to remove c.
  rv = svc->RemoveObserver(c, "Foo");
  ASSERT_TRUE(NS_FAILED(rv));
}

TEST(ObserverService, EnumerateEmpty)
{
  nsCOMPtr<nsIObserverService> svc =
    do_CreateInstance("@mozilla.org/observer-service;1");

  // Try with no observers.
  TestExpectedCount(svc, "A", 0);

  // Now add an observer and enumerate an unobserved topic.
  RefPtr<TestObserver> a = new TestObserver(NS_LITERAL_STRING("A"));
  testResult(svc->AddObserver(a, "Foo", false));

  TestExpectedCount(svc, "A", 0);
}

TEST(ObserverService, Enumerate)
{
  nsCOMPtr<nsIObserverService> svc =
    do_CreateInstance("@mozilla.org/observer-service;1");

  const size_t kFooCount = 10;
  for (size_t i = 0; i < kFooCount; i++) {
    RefPtr<TestObserver> a = new TestObserver(NS_LITERAL_STRING("A"));
    testResult(svc->AddObserver(a, "Foo", false));
  }

  const size_t kBarCount = kFooCount / 2;
  for (size_t i = 0; i < kBarCount; i++) {
    RefPtr<TestObserver> a = new TestObserver(NS_LITERAL_STRING("A"));
    testResult(svc->AddObserver(a, "Bar", false));
  }

  // Enumerate "Foo".
  TestExpectedCount(svc, "Foo", kFooCount);

  // Enumerate "Bar".
  TestExpectedCount(svc, "Bar", kBarCount);
}

TEST(ObserverService, EnumerateWeakRefs)
{
  nsCOMPtr<nsIObserverService> svc =
    do_CreateInstance("@mozilla.org/observer-service;1");

  const size_t kFooCount = 10;
  for (size_t i = 0; i < kFooCount; i++) {
    RefPtr<TestObserver> a = new TestObserver(NS_LITERAL_STRING("A"));
    testResult(svc->AddObserver(a, "Foo", true));
  }

  // All refs are out of scope, expect enumeration to be empty.
  TestExpectedCount(svc, "Foo", 0);

  // Now test a mixture.
  for (size_t i = 0; i < kFooCount; i++) {
    RefPtr<TestObserver> a = new TestObserver(NS_LITERAL_STRING("A"));
    RefPtr<TestObserver> b = new TestObserver(NS_LITERAL_STRING("B"));

    // Register a as weak for "Foo".
    testResult(svc->AddObserver(a, "Foo", true));

    // Register b as strong for "Foo".
    testResult(svc->AddObserver(b, "Foo", false));
  }

  // Expect the b instances to stick around.
  TestExpectedCount(svc, "Foo", kFooCount);

  // Now add a couple weak refs, but don't go out of scope.
  RefPtr<TestObserver> a = new TestObserver(NS_LITERAL_STRING("A"));
  testResult(svc->AddObserver(a, "Foo", true));
  RefPtr<TestObserver> b = new TestObserver(NS_LITERAL_STRING("B"));
  testResult(svc->AddObserver(b, "Foo", true));

  // Expect all the observers from before and the two new ones.
  TestExpectedCount(svc, "Foo", kFooCount + 2);
}

TEST(ObserverService, TestNotify)
{
  nsCString topicA; topicA.Assign( "topic-A" );
  nsCString topicB; topicB.Assign( "topic-B" );

  nsCOMPtr<nsIObserverService> svc =
    do_CreateInstance("@mozilla.org/observer-service;1");

  RefPtr<TestObserver> aObserver = new TestObserver(NS_LITERAL_STRING("Observer-A"));
  RefPtr<TestObserver> bObserver = new TestObserver(NS_LITERAL_STRING("Observer-B"));

  // Add two observers for topicA.
  testResult(svc->AddObserver(aObserver, topicA.get(), false));
  testResult(svc->AddObserver(bObserver, topicA.get(), false));

  // Add one observer for topicB.
  testResult(svc->AddObserver(bObserver, topicB.get(), false));

  // Notify topicA.
  NS_NAMED_LITERAL_STRING(dataA, "Testing Notify(observer-A, topic-A)");
  aObserver->mExpectedData = dataA;
  bObserver->mExpectedData = dataA;
  nsresult rv =
      svc->NotifyObservers(ToSupports(aObserver), topicA.get(), dataA.get());
  testResult(rv);
  ASSERT_EQ(aObserver->mObservations, 1);
  ASSERT_EQ(bObserver->mObservations, 1);

  // Notify topicB.
  NS_NAMED_LITERAL_STRING(dataB, "Testing Notify(observer-B, topic-B)");
  bObserver->mExpectedData = dataB;
  rv = svc->NotifyObservers(ToSupports(bObserver), topicB.get(), dataB.get());
  testResult(rv);
  ASSERT_EQ(aObserver->mObservations, 1);
  ASSERT_EQ(bObserver->mObservations, 2);

  // Remove one of the topicA observers, make sure it's not notified.
  testResult(svc->RemoveObserver(aObserver, topicA.get()));

  // Notify topicA, only bObserver is expected to be notified.
  bObserver->mExpectedData = dataA;
  rv = svc->NotifyObservers(ToSupports(aObserver), topicA.get(), dataA.get());
  testResult(rv);
  ASSERT_EQ(aObserver->mObservations, 1);
  ASSERT_EQ(bObserver->mObservations, 3);

  // Remove the other topicA observer, make sure none are notified.
  testResult(svc->RemoveObserver(bObserver, topicA.get()));
  rv = svc->NotifyObservers(ToSupports(aObserver), topicA.get(), dataA.get());
  testResult(rv);
  ASSERT_EQ(aObserver->mObservations, 1);
  ASSERT_EQ(bObserver->mObservations, 3);
}
