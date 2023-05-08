#include "gtest/gtest.h"
#include "nsCOMPtr.h"
#include "nsNetCID.h"
#include "nsIURIMutator.h"
#include "nsIURL.h"
#include "nsThreadPool.h"
#include "nsNetUtil.h"

TEST(TestURIMutator, Mutator)
{
  nsAutoCString out;

  // This test instantiates a new nsStandardURL::Mutator (via contractID)
  // and uses it to create a new URI.
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_MutateURI(NS_STANDARDURLMUTATOR_CONTRACTID)
                    .SetSpec("http://example.com"_ns)
                    .Finalize(uri);
  ASSERT_EQ(rv, NS_OK);
  ASSERT_EQ(uri->GetSpec(out), NS_OK);
  ASSERT_TRUE(out == "http://example.com/"_ns);

  // This test verifies that we can use NS_MutateURI to change a URI
  rv = NS_MutateURI(uri)
           .SetScheme("ftp"_ns)
           .SetHost("mozilla.org"_ns)
           .SetPathQueryRef("/path?query#ref"_ns)
           .Finalize(uri);
  ASSERT_EQ(rv, NS_OK);
  ASSERT_EQ(uri->GetSpec(out), NS_OK);
  ASSERT_TRUE(out == "ftp://mozilla.org/path?query#ref"_ns);

  // This test verifies that we can pass nsIURL to Finalize, and
  nsCOMPtr<nsIURL> url;
  rv = NS_MutateURI(uri).SetScheme("https"_ns).Finalize(url);
  ASSERT_EQ(rv, NS_OK);
  ASSERT_EQ(url->GetSpec(out), NS_OK);
  ASSERT_TRUE(out == "https://mozilla.org/path?query#ref"_ns);

  // This test verifies that we can pass nsIURL** to Finalize.
  // We need to use the explicit template because it's actually passing
  // getter_AddRefs
  nsCOMPtr<nsIURL> url2;
  rv = NS_MutateURI(url)
           .SetRef("newref"_ns)
           .Finalize<nsIURL>(getter_AddRefs(url2));
  ASSERT_EQ(rv, NS_OK);
  ASSERT_EQ(url2->GetSpec(out), NS_OK);
  ASSERT_TRUE(out == "https://mozilla.org/path?query#newref"_ns);

  // This test verifies that we can pass nsIURI** to Finalize.
  // No need to be explicit.
  auto functionSetRef = [](nsIURI* aURI, nsIURI** aResult) -> nsresult {
    return NS_MutateURI(aURI).SetRef("originalRef"_ns).Finalize(aResult);
  };

  nsCOMPtr<nsIURI> newURI;
  rv = functionSetRef(url2, getter_AddRefs(newURI));
  ASSERT_EQ(rv, NS_OK);
  ASSERT_EQ(newURI->GetSpec(out), NS_OK);
  ASSERT_TRUE(out == "https://mozilla.org/path?query#originalRef"_ns);

  // This test verifies that we can pass nsIURI** to Finalize.
  nsCOMPtr<nsIURI> uri2;
  rv =
      NS_MutateURI(url2).SetQuery("newquery"_ns).Finalize(getter_AddRefs(uri2));
  ASSERT_EQ(rv, NS_OK);
  ASSERT_EQ(uri2->GetSpec(out), NS_OK);
  ASSERT_TRUE(out == "https://mozilla.org/path?newquery#newref"_ns);

  // This test verifies that we can pass nsIURI** to Finalize.
  // No need to be explicit.
  auto functionSetQuery = [](nsIURI* aURI, nsIURL** aResult) -> nsresult {
    return NS_MutateURI(aURI).SetQuery("originalQuery"_ns).Finalize(aResult);
  };

  nsCOMPtr<nsIURL> newURL;
  rv = functionSetQuery(uri2, getter_AddRefs(newURL));
  ASSERT_EQ(rv, NS_OK);
  ASSERT_EQ(newURL->GetSpec(out), NS_OK);
  ASSERT_TRUE(out == "https://mozilla.org/path?originalQuery#newref"_ns);

  // Check that calling Finalize twice will fail.
  NS_MutateURI mutator(newURL);
  rv = mutator.SetQuery(""_ns).Finalize(uri2);
  ASSERT_EQ(rv, NS_OK);
  ASSERT_EQ(uri2->GetSpec(out), NS_OK);
  ASSERT_TRUE(out == "https://mozilla.org/path#newref"_ns);
  nsCOMPtr<nsIURI> uri3;
  rv = mutator.Finalize(uri3);
  ASSERT_EQ(rv, NS_ERROR_NOT_AVAILABLE);
  ASSERT_TRUE(uri3 == nullptr);

  // Make sure changing scheme updates the default port
  rv = NS_NewURI(getter_AddRefs(uri),
                 "https://example.org:80/path?query#ref"_ns);
  ASSERT_EQ(rv, NS_OK);
  rv = NS_MutateURI(uri).SetScheme("http"_ns).Finalize(uri);
  ASSERT_EQ(rv, NS_OK);
  rv = uri->GetSpec(out);
  ASSERT_EQ(rv, NS_OK);
  ASSERT_EQ(out, "http://example.org/path?query#ref"_ns);
  int32_t port;
  rv = uri->GetPort(&port);
  ASSERT_EQ(rv, NS_OK);
  ASSERT_EQ(port, -1);
  rv = uri->GetFilePath(out);
  ASSERT_EQ(rv, NS_OK);
  ASSERT_EQ(out, "/path"_ns);
  rv = uri->GetQuery(out);
  ASSERT_EQ(rv, NS_OK);
  ASSERT_EQ(out, "query"_ns);
  rv = uri->GetRef(out);
  ASSERT_EQ(rv, NS_OK);
  ASSERT_EQ(out, "ref"_ns);

  // Make sure changing scheme does not change non-default port
  rv = NS_NewURI(getter_AddRefs(uri), "https://example.org:123"_ns);
  ASSERT_EQ(rv, NS_OK);
  rv = NS_MutateURI(uri).SetScheme("http"_ns).Finalize(uri);
  ASSERT_EQ(rv, NS_OK);
  rv = uri->GetSpec(out);
  ASSERT_EQ(rv, NS_OK);
  ASSERT_EQ(out, "http://example.org:123/"_ns);
  rv = uri->GetPort(&port);
  ASSERT_EQ(rv, NS_OK);
  ASSERT_EQ(port, 123);
}

extern MOZ_THREAD_LOCAL(uint32_t) gTlsURLRecursionCount;

TEST(TestURIMutator, OnAnyThread)
{
  nsCOMPtr<nsIThreadPool> pool = new nsThreadPool();
  pool->SetThreadLimit(60);

  pool = new nsThreadPool();
  for (int i = 0; i < 1000; ++i) {
    nsCOMPtr<nsIRunnable> task =
        NS_NewRunnableFunction("gtest-OnAnyThread", []() {
          nsCOMPtr<nsIURI> uri;
          nsresult rv = NS_NewURI(getter_AddRefs(uri), "http://example.com"_ns);
          ASSERT_EQ(rv, NS_OK);
          nsAutoCString out;
          ASSERT_EQ(uri->GetSpec(out), NS_OK);
          ASSERT_TRUE(out == "http://example.com/"_ns);
        });
    EXPECT_TRUE(task);

    pool->Dispatch(task, NS_DISPATCH_NORMAL);
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), "http://example.com"_ns);
  ASSERT_EQ(rv, NS_OK);
  nsAutoCString out;
  ASSERT_EQ(uri->GetSpec(out), NS_OK);
  ASSERT_TRUE(out == "http://example.com/"_ns);

  pool->Shutdown();

  ASSERT_EQ(gTlsURLRecursionCount.get(), 0u);
}
