/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "nsCOMPtr.h"
#include "nsIFile.h"
#include "nsProfileLock.h"
#include "nsString.h"
#include "nsDirectoryServiceDefs.h"

TEST(ProfileLock, BasicLock)
{
  char tmpExt[] = "profilebasiclocktest";

  nsProfileLock myLock;
  nsresult rv;

  nsCOMPtr<nsIFile> tmpDir;
  rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(tmpDir));
  ASSERT_EQ(NS_SUCCEEDED(rv), true);

  rv = tmpDir->AppendNative(nsCString(tmpExt));
  ASSERT_EQ(NS_SUCCEEDED(rv), true);

  rv = tmpDir->CreateUnique(nsIFile::DIRECTORY_TYPE, 0700);
  ASSERT_EQ(NS_SUCCEEDED(rv), true);

  rv = myLock.Lock(tmpDir, nullptr);
  EXPECT_EQ(NS_SUCCEEDED(rv), true);

  myLock.Cleanup();
}
