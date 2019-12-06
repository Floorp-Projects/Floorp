/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include <sys/eventfd.h>
#include <sched.h>

#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsIFile.h"
#include "nsProfileLock.h"
#include "nsString.h"

TEST(ProfileLock, RetryLock)
{
  char templ[] = "/tmp/profilelocktest.XXXXXX";
  char* tmpdir = mkdtemp(templ);
  ASSERT_NE(tmpdir, nullptr);

  nsProfileLock myLock;
  nsProfileLock myLock2;
  nsresult rv;
  nsCOMPtr<nsIFile> dir(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));
  ASSERT_EQ(NS_SUCCEEDED(rv), true);

  rv = dir->InitWithNativePath(nsCString(tmpdir));
  ASSERT_EQ(NS_SUCCEEDED(rv), true);

  int eventfd_fd = eventfd(0, 0);
  ASSERT_GE(eventfd_fd, 0);

  // fcntl advisory locks are per process, so it's hard
  // to avoid using fork here.
  pid_t childpid = fork();

  if (childpid) {
    // parent

    // blocking read causing us to lose the race
    eventfd_t value;
    EXPECT_EQ(0, eventfd_read(eventfd_fd, &value));

    rv = myLock2.Lock(dir, nullptr);
    EXPECT_EQ(NS_ERROR_FILE_ACCESS_DENIED, rv);

    // kill the child
    EXPECT_EQ(0, kill(childpid, SIGTERM));

    // reap zombie (required to collect the lock)
    int status;
    EXPECT_EQ(childpid, waitpid(childpid, &status, 0));

    rv = myLock2.Lock(dir, nullptr);
    EXPECT_EQ(NS_SUCCEEDED(rv), true);
  } else {
    // child
    rv = myLock.Lock(dir, nullptr);
    ASSERT_EQ(NS_SUCCEEDED(rv), true);

    // unblock parent
    EXPECT_EQ(0, eventfd_write(eventfd_fd, 1));

    // parent will kill us
    for (;;) sleep(1);
  }

  close(eventfd_fd);

  myLock.Cleanup();
  myLock2.Cleanup();
}
