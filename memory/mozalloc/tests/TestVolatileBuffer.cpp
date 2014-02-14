/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestHarness.h"
#include "mozilla/VolatileBuffer.h"
#include "mozilla/NullPtr.h"
#include <string.h>

#if defined(ANDROID)
#include <fcntl.h>
#include <linux/ashmem.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#elif defined(XP_DARWIN)
#include <mach/mach.h>
#endif

using namespace mozilla;

int main(int argc, char **argv)
{
  ScopedXPCOM xpcom("VolatileBufferTests");
  if (xpcom.failed()) {
    return 1;
  }

  RefPtr<VolatileBuffer> heapbuf = new VolatileBuffer();
  if (!heapbuf || !heapbuf->Init(512)) {
    fail("Failed to initialize VolatileBuffer");
    return 1;
  }

  {
    VolatileBufferPtr<char> ptr(heapbuf);
    if (ptr.WasBufferPurged()) {
      fail("Buffer was immediately purged after initialization");
      return 1;
    }

    if (!ptr) {
      fail("Didn't get a pointer");
      return 1;
    }
  }

  RefPtr<VolatileBuffer> buf = new VolatileBuffer();
  if (!buf || !buf->Init(16384)) {
    fail("Failed to initialize VolatileBuffer");
    return 1;
  }

  const char teststr[] = "foobar";

  {
    VolatileBufferPtr<char> ptr(buf);
    if (ptr.WasBufferPurged()) {
      fail("Buffer should not be purged immediately after initialization");
      return 1;
    }

    if (!ptr) {
      fail("Didn't get a pointer");
      return 1;
    }

    {
      VolatileBufferPtr<char> ptr2(buf);
      if (ptr2.WasBufferPurged()) {
        fail("Failed to Lock buffer again while currently locked");
        return 1;
      }

      if (!ptr2) {
        fail("Didn't get a pointer on the second lock");
        return 1;
      }

      strcpy(ptr2, teststr);
    }
  }

  {
    VolatileBufferPtr<char> ptr(buf);
    if (ptr.WasBufferPurged()) {
      fail("Buffer was immediately purged after unlock");
      return 1;
    }

    if (strcmp(ptr, teststr)) {
      fail("Buffer failed to retain data after unlock");
      return 1;
    }
  }

  // Test purging if we know how to
#if defined(MOZ_WIDGET_GONK)
  // This also works on Android, but we need root.
  int fd = open("/" ASHMEM_NAME_DEF, O_RDWR);
  if (fd < 0) {
    fail("Failed to open ashmem device");
    return 1;
  }
  if (ioctl(fd, ASHMEM_PURGE_ALL_CACHES, NULL) < 0) {
    fail("Failed to purge ashmem caches");
    return 1;
  }
#elif defined(XP_DARWIN)
  int state;
  vm_purgable_control(mach_task_self(), (vm_address_t)NULL,
                      VM_PURGABLE_PURGE_ALL, &state);
#else
  return 0;
#endif

  if (!buf->NonHeapSizeOfExcludingThis()) {
    fail("Buffer should not be allocated on heap");
    return 1;
  }

  {
    VolatileBufferPtr<char> ptr(buf);
    if (!ptr.WasBufferPurged()) {
      fail("Buffer should not be unpurged after forced purge");
      return 1;
    }

    if (!strcmp(ptr, teststr)) {
      fail("Purge did not actually purge data");
      return 1;
    }
  }

  {
    VolatileBufferPtr<char> ptr(buf);
    if (ptr.WasBufferPurged()) {
      fail("Buffer still purged after lock");
      return 1;
    }
  }

  return 0;
}
