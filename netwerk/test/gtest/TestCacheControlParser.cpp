#include "gtest/gtest.h"

#include "CacheControlParser.h"

using namespace mozilla;
using namespace mozilla::net;

TEST(TestCacheControlParser, NegativeMaxAge)
{
  CacheControlParser cc(
      "no-store,no-cache,max-age=-3600,max-stale=7,private"_ns);
  ASSERT_TRUE(cc.NoStore());
  ASSERT_TRUE(cc.NoCache());
  uint32_t max_age{2};
  ASSERT_FALSE(cc.MaxAge(&max_age));
  ASSERT_EQ(max_age, 0U);
  uint32_t max_stale;
  ASSERT_TRUE(cc.MaxStale(&max_stale));
  ASSERT_EQ(max_stale, 7U);
  ASSERT_TRUE(cc.Private());
}

TEST(TestCacheControlParser, EmptyMaxAge)
{
  CacheControlParser cc("no-store,no-cache,max-age,max-stale=77,private"_ns);
  ASSERT_TRUE(cc.NoStore());
  ASSERT_TRUE(cc.NoCache());
  uint32_t max_age{5};
  ASSERT_FALSE(cc.MaxAge(&max_age));
  ASSERT_EQ(max_age, 0U);
  uint32_t max_stale;
  ASSERT_TRUE(cc.MaxStale(&max_stale));
  ASSERT_EQ(max_stale, 77U);
  ASSERT_TRUE(cc.Private());
}

TEST(TestCacheControlParser, NegaTiveMaxStale)
{
  CacheControlParser cc(
      "no-store,no-cache,max-age=3600,max-stale=-222,private"_ns);
  ASSERT_TRUE(cc.NoStore());
  ASSERT_TRUE(cc.NoCache());
  uint32_t max_age;
  ASSERT_TRUE(cc.MaxAge(&max_age));
  ASSERT_EQ(max_age, 3600U);
  uint32_t max_stale;
  ASSERT_TRUE(cc.MaxStale(&max_stale));
  ASSERT_EQ(max_stale, PR_UINT32_MAX);
  ASSERT_TRUE(cc.Private());
}

TEST(TestCacheControlParser, EmptyMaxStale)
{
  CacheControlParser cc("no-store,no-cache,max-age=3600,max-stale,private"_ns);
  ASSERT_TRUE(cc.NoStore());
  ASSERT_TRUE(cc.NoCache());
  uint32_t max_age;
  ASSERT_TRUE(cc.MaxAge(&max_age));
  ASSERT_EQ(max_age, 3600U);
  uint32_t max_stale;
  ASSERT_TRUE(cc.MaxStale(&max_stale));
  ASSERT_EQ(max_stale, PR_UINT32_MAX);
  ASSERT_TRUE(cc.Private());
}
