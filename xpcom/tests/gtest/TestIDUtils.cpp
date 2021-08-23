/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsID.h"
#include "nsIDUtils.h"

#include "gtest/gtest.h"

static const char* const bare_ids[] = {
    "5c347b10-d55c-11d1-89b7-006008911b81",
    "fc347b10-d55c-f1d1-f9b7-006008911b81",
};

TEST(nsIDUtils, NSID_TrimBracketsUTF16)
{
  nsID id{};
  for (const auto* idstr : bare_ids) {
    ASSERT_TRUE(id.Parse(idstr));

    NSID_TrimBracketsUTF16 trimmed(id);
    ASSERT_TRUE(trimmed.EqualsASCII(idstr));
  }
}

TEST(nsIDUtils, NSID_TrimBracketsASCII)
{
  nsID id{};
  for (const auto* idstr : bare_ids) {
    ASSERT_TRUE(id.Parse(idstr));

    NSID_TrimBracketsASCII trimmed(id);
    ASSERT_TRUE(trimmed.EqualsASCII(idstr));
  }
}
