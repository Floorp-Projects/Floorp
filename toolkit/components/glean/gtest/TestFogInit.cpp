/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "nsString.h"

extern "C" {
nsresult fog_init(const nsACString* dataPath, const nsACString* buildId,
                  const nsACString* appDisplayVersion, const char* channel,
                  const nsACString* osVersion, const nsACString* architecture);
}

TEST(FOG, FogInitDoesntCrash)
{
  nsAutoCString dataPath;
  nsAutoCString buildID;
  nsAutoCString appVersion;
  const char* channel = "";
  nsAutoCString osVersion;
  nsAutoCString architecture;
  ASSERT_EQ(NS_OK, fog_init(&dataPath, &buildID, &appVersion, channel,
                            &osVersion, &architecture));
}
