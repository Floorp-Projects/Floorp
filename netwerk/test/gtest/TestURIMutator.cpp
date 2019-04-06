#include "gtest/gtest.h"
#include "nsCOMPtr.h"
#include "nsNetCID.h"
#include "nsIURL.h"
#include "nsIURIMutator.h"
#include "nsThreadPool.h"
#include "nsNetUtil.h"

TEST(TestURIMutator, Mutator)
{
  nsAutoCString out;

  // This test instantiates a new nsStandardURL::Mutator (via contractID)
  // and uses it to create a new URI.
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_MutateURI(NS_STANDARDURLMUTATOR_CONTRACTID)
                    .SetSpec(NS_LITERAL_CSTRING("http://example.com"))
                    .Finalize(uri);
  ASSERT_EQ(rv, NS_OK);
  ASSERT_EQ(uri->GetSpec(out), NS_OK);
  ASSERT_TRUE(out == NS_LITERAL_CSTRING("http://example.com/"));

  // This test verifies that we can use NS_MutateURI to change a URI
  rv = NS_MutateURI(uri)
           .SetScheme(NS_LITERAL_CSTRING("ftp"))
           .SetHost(NS_LITERAL_CSTRING("mozilla.org"))
           .SetPathQueryRef(NS_LITERAL_CSTRING("/path?query#ref"))
           .Finalize(uri);
  ASSERT_EQ(rv, NS_OK);
  ASSERT_EQ(uri->GetSpec(out), NS_OK);
  ASSERT_TRUE(out == NS_LITERAL_CSTRING("ftp://mozilla.org/path?query#ref"));

  // This test verifies that we can pass nsIURL to Finalize, and
  nsCOMPtr<nsIURL> url;
  rv = NS_MutateURI(uri).SetScheme(NS_LITERAL_CSTRING("https")).Finalize(url);
  ASSERT_EQ(rv, NS_OK);
  ASSERT_EQ(url->GetSpec(out), NS_OK);
  ASSERT_TRUE(out == NS_LITERAL_CSTRING("https://mozilla.org/path?query#ref"));

  // This test verifies that we can pass nsIURL** to Finalize.
  // We need to use the explicit template because it's actually passing
  // getter_AddRefs
  nsCOMPtr<nsIURL> url2;
  rv = NS_MutateURI(url)
           .SetRef(NS_LITERAL_CSTRING("newref"))
           .Finalize<nsIURL>(getter_AddRefs(url2));
  ASSERT_EQ(rv, NS_OK);
  ASSERT_EQ(url2->GetSpec(out), NS_OK);
  ASSERT_TRUE(out ==
              NS_LITERAL_CSTRING("https://mozilla.org/path?query#newref"));

  // This test verifies that we can pass nsIURI** to Finalize.
  // No need to be explicit.
  auto functionSetRef = [](nsIURI* aURI, nsIURI** aResult) -> nsresult {
    return NS_MutateURI(aURI)
        .SetRef(NS_LITERAL_CSTRING("originalRef"))
        .Finalize(aResult);
  };

  nsCOMPtr<nsIURI> newURI;
  rv = functionSetRef(url2, getter_AddRefs(newURI));
  ASSERT_EQ(rv, NS_OK);
  ASSERT_EQ(newURI->GetSpec(out), NS_OK);
  ASSERT_TRUE(out ==
              NS_LITERAL_CSTRING("https://mozilla.org/path?query#originalRef"));

  // This test verifies that we can pass nsIURI** to Finalize.
  nsCOMPtr<nsIURI> uri2;
  rv = NS_MutateURI(url2)
           .SetQuery(NS_LITERAL_CSTRING("newquery"))
           .Finalize(getter_AddRefs(uri2));
  ASSERT_EQ(rv, NS_OK);
  ASSERT_EQ(uri2->GetSpec(out), NS_OK);
  ASSERT_TRUE(out ==
              NS_LITERAL_CSTRING("https://mozilla.org/path?newquery#newref"));

  // This test verifies that we can pass nsIURI** to Finalize.
  // No need to be explicit.
  auto functionSetQuery = [](nsIURI* aURI, nsIURL** aResult) -> nsresult {
    return NS_MutateURI(aURI)
        .SetQuery(NS_LITERAL_CSTRING("originalQuery"))
        .Finalize(aResult);
  };

  nsCOMPtr<nsIURL> newURL;
  rv = functionSetQuery(uri2, getter_AddRefs(newURL));
  ASSERT_EQ(rv, NS_OK);
  ASSERT_EQ(newURL->GetSpec(out), NS_OK);
  ASSERT_TRUE(out == NS_LITERAL_CSTRING(
                         "https://mozilla.org/path?originalQuery#newref"));

  // Check that calling Finalize twice will fail.
  NS_MutateURI mutator(newURL);
  rv = mutator.SetQuery(EmptyCString()).Finalize(uri2);
  ASSERT_EQ(rv, NS_OK);
  ASSERT_EQ(uri2->GetSpec(out), NS_OK);
  ASSERT_TRUE(out == NS_LITERAL_CSTRING("https://mozilla.org/path#newref"));
  nsCOMPtr<nsIURI> uri3;
  rv = mutator.Finalize(uri3);
  ASSERT_EQ(rv, NS_ERROR_NOT_AVAILABLE);
  ASSERT_TRUE(uri3 == nullptr);
}

extern MOZ_THREAD_LOCAL(uint32_t) gTlsURLRecursionCount;

TEST(TestURIMutator, NS_NewURIOnAnyThread)
{
  nsCOMPtr<nsIThreadPool> pool = new nsThreadPool();
  pool->SetThreadLimit(60);

  pool = new nsThreadPool();
  for (int i = 0; i < 1000; ++i) {
    nsCOMPtr<nsIRunnable> task =
        NS_NewRunnableFunction("gtest-NS_NewURIOnAnyThread", []() {
          nsCOMPtr<nsIURI> uri;
          nsresult rv = NS_NewURIOnAnyThread(
              getter_AddRefs(uri), NS_LITERAL_CSTRING("http://example.com"));
          ASSERT_EQ(rv, NS_OK);
          nsAutoCString out;
          ASSERT_EQ(uri->GetSpec(out), NS_OK);
          ASSERT_TRUE(out == NS_LITERAL_CSTRING("http://example.com/"));
        });
    EXPECT_TRUE(task);

    pool->Dispatch(task, NS_DISPATCH_NORMAL);
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURIOnAnyThread(getter_AddRefs(uri),
                                     NS_LITERAL_CSTRING("http://example.com"));
  ASSERT_EQ(rv, NS_OK);
  nsAutoCString out;
  ASSERT_EQ(uri->GetSpec(out), NS_OK);
  ASSERT_TRUE(out == NS_LITERAL_CSTRING("http://example.com/"));

  pool->Shutdown();

  ASSERT_EQ(gTlsURLRecursionCount.get(), 0u);
}
