/*
 *  Copyright 2012 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdlib.h>
#include <string.h>

#include "libyuv/basic_types.h"
#include "libyuv/version.h"
#include "../unit_test/unit_test.h"

namespace libyuv {

// Tests SVN version against include/libyuv/version.h
// SVN version is bumped by documentation changes as well as code.
// Although the versions should match, once checked in, a tolerance is allowed.
TEST_F(libyuvTest, TestVersion) {
  EXPECT_GE(LIBYUV_VERSION, 169);  // 169 is first version to support version.
  printf("LIBYUV_VERSION %d\n", LIBYUV_VERSION);
#ifdef LIBYUV_SVNREVISION
  const char *ver = strchr(LIBYUV_SVNREVISION, ':');
  if (ver) {
    ++ver;
  } else {
    ver = LIBYUV_SVNREVISION;
  }
  int svn_revision = atoi(ver);  // NOLINT
  printf("LIBYUV_SVNREVISION %d\n", svn_revision);
  EXPECT_NEAR(LIBYUV_VERSION, svn_revision, 20);  // Allow version to be close.
  if (LIBYUV_VERSION != svn_revision) {
    printf("WARNING - Versions do not match.\n");
  }
#endif
}

}  // namespace libyuv
