/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

// NOTE: No ContentChild/Parent tests because it includes headers that aren't
// present in GTest builds (or something).

#include "mozilla/FOGIPC.h"
#include <functional>
#include "mozilla/ipc/ByteBuf.h"
#include "nsTArray.h"
#include "nsXULAppAPI.h"

using mozilla::ipc::ByteBuf;

TEST(FOG, TestFlushFOGData)
{
  // A "It doesn't explode" test.
  std::function<void(ByteBuf &&)> resolver = [](ByteBuf&& bufs) {};
  mozilla::glean::FlushFOGData(std::move(resolver));
}

TEST(FOG, TestFlushAllChildData)
{
  std::function<void(const nsTArray<ByteBuf>&&)> resolver =
      [](const nsTArray<ByteBuf>&& bufs) {
        ASSERT_TRUE(bufs.Length() == 0)
        << "Not expecting any bufs yet.";
      };
  mozilla::glean::FlushAllChildData(std::move(resolver));
}

TEST(FOG, FOGData)
{
  // Another "It doesn't explode" test.
  ByteBuf buf;
  mozilla::glean::FOGData(std::move(buf));
}

TEST(FOG, SendFOGData)
{
  ASSERT_EQ(XRE_GetProcessType(), GeckoProcessType_Default)
      << "If we can run a test as a different process type, we can write a "
         "test for this function.";
}
