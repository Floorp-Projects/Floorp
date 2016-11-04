/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTextFormatter.h"
#include "nsString.h"
#include "gtest/gtest.h"

TEST(TextFormatter, Tests)
{
  nsAutoString fmt(NS_LITERAL_STRING("%3$s %4$S %1$d %2$d %2$d %3$s"));
  char utf8[] = "Hello";
  char16_t ucs2[]={'W', 'o', 'r', 'l', 'd', 0x4e00, 0xAc00, 0xFF45, 0x0103, 0x00};
  int d=3;

  char16_t buf[256];
  nsTextFormatter::snprintf(buf, 256, fmt.get(), d, 333, utf8, ucs2);
  nsAutoString out(buf);
  ASSERT_STREQ("Hello World", NS_LossyConvertUTF16toASCII(out).get());

  const char16_t *uout = out.get();
  const char16_t expected[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20,
                                0x57, 0x6F, 0x72, 0x6C, 0x64, 0x4E00,
                                0xAC00, 0xFF45, 0x0103, 0x20, 0x33,
                                0x20, 0x33, 0x33, 0x33, 0x20, 0x33,
                                0x33, 0x33, 0x20, 0x48, 0x65, 0x6C,
                                0x6C, 0x6F};

  for (uint32_t i=0; i<out.Length(); i++) {
    ASSERT_EQ(uout[i], expected[i]);
  }
}

