/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/VolatileBuffer.h"
#include <string.h>

#if defined(ANDROID)
#  include <fcntl.h>
#  include <linux/ashmem.h>
#  include <sys/ioctl.h>
#  include <sys/stat.h>
#  include <sys/types.h>
#elif defined(XP_DARWIN)
#  include <mach/mach.h>
#endif

using namespace mozilla;

TEST(VolatileBufferTest, HeapVolatileBuffersWork)
{
  RefPtr<VolatileBuffer> heapbuf = new VolatileBuffer();

  ASSERT_TRUE(heapbuf)
  << "Failed to create VolatileBuffer";
  ASSERT_TRUE(heapbuf->Init(512))
  << "Failed to initialize VolatileBuffer";

  VolatileBufferPtr<char> ptr(heapbuf);

  EXPECT_FALSE(ptr.WasBufferPurged())
      << "Buffer should not be purged immediately after initialization";
  EXPECT_TRUE(ptr) << "Couldn't get pointer from VolatileBufferPtr";
}

TEST(VolatileBufferTest, RealVolatileBuffersWork)
{
  RefPtr<VolatileBuffer> buf = new VolatileBuffer();

  ASSERT_TRUE(buf)
  << "Failed to create VolatileBuffer";
  ASSERT_TRUE(buf->Init(16384))
  << "Failed to initialize VolatileBuffer";

  const char teststr[] = "foobar";

  {
    VolatileBufferPtr<char> ptr(buf);

    EXPECT_FALSE(ptr.WasBufferPurged())
        << "Buffer should not be purged immediately after initialization";
    EXPECT_TRUE(ptr) << "Couldn't get pointer from VolatileBufferPtr";

    {
      VolatileBufferPtr<char> ptr2(buf);

      EXPECT_FALSE(ptr.WasBufferPurged())
          << "Failed to lock buffer again while currently locked";
      ASSERT_TRUE(ptr2)
      << "Didn't get a pointer on the second lock";

      strcpy(ptr2, teststr);
    }
  }

  {
    VolatileBufferPtr<char> ptr(buf);

    EXPECT_FALSE(ptr.WasBufferPurged())
        << "Buffer was immediately purged after unlock";
    EXPECT_STREQ(ptr, teststr) << "Buffer failed to retain data after unlock";
  }

  // Test purging if we know how to
#if defined(XP_DARWIN)
  int state;
  vm_purgable_control(mach_task_self(), (vm_address_t)NULL,
                      VM_PURGABLE_PURGE_ALL, &state);
#else
  return;
#endif

  EXPECT_GT(buf->NonHeapSizeOfExcludingThis(), 0ul)
      << "Buffer should not be allocated on heap";

  {
    VolatileBufferPtr<char> ptr(buf);

    EXPECT_TRUE(ptr.WasBufferPurged())
        << "Buffer should not be unpurged after forced purge";
    EXPECT_STRNE(ptr, teststr) << "Purge did not actually purge data";
  }

  {
    VolatileBufferPtr<char> ptr(buf);

    EXPECT_FALSE(ptr.WasBufferPurged()) << "Buffer still purged after lock";
  }
}
