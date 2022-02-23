/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <math.h>
#include "nsVariant.h"
#include "gtest/gtest.h"

TEST(Variant, DoubleNaN)
{
  nsDiscriminatedUnion du;
  du.SetFromDouble(NAN);

  uint8_t ui8;
  EXPECT_EQ(du.ConvertToInt8(&ui8), NS_ERROR_LOSS_OF_SIGNIFICANT_DATA);

  int16_t i16;
  EXPECT_EQ(du.ConvertToInt16(&i16), NS_ERROR_LOSS_OF_SIGNIFICANT_DATA);

  int32_t i32;
  EXPECT_EQ(du.ConvertToInt32(&i32), NS_ERROR_LOSS_OF_SIGNIFICANT_DATA);

  int64_t i64;
  EXPECT_EQ(du.ConvertToInt64(&i64), NS_ERROR_LOSS_OF_SIGNIFICANT_DATA);

  EXPECT_EQ(du.ConvertToUint8(&ui8), NS_ERROR_LOSS_OF_SIGNIFICANT_DATA);

  uint16_t ui16;
  EXPECT_EQ(du.ConvertToUint16(&ui16), NS_ERROR_LOSS_OF_SIGNIFICANT_DATA);

  uint32_t ui32;
  EXPECT_EQ(du.ConvertToUint32(&ui32), NS_ERROR_LOSS_OF_SIGNIFICANT_DATA);

  uint64_t ui64;
  EXPECT_EQ(du.ConvertToUint64(&ui64), NS_ERROR_LOSS_OF_SIGNIFICANT_DATA);

  float f = 0.0f;
  EXPECT_EQ(du.ConvertToFloat(&f), NS_OK);
  EXPECT_TRUE(isnan(f));

  double d = 0.0;
  EXPECT_EQ(du.ConvertToDouble(&d), NS_OK);
  EXPECT_TRUE(isnan(d));

  bool b = true;
  EXPECT_EQ(du.ConvertToBool(&b), NS_OK);
  EXPECT_FALSE(b);

  char c;
  EXPECT_EQ(du.ConvertToChar(&c), NS_ERROR_LOSS_OF_SIGNIFICANT_DATA);

  char16_t c16;
  EXPECT_EQ(du.ConvertToWChar(&c16), NS_ERROR_LOSS_OF_SIGNIFICANT_DATA);

  nsID id = {};
  EXPECT_EQ(du.ConvertToID(&id), NS_ERROR_CANNOT_CONVERT_DATA);

  nsAutoString string;
  EXPECT_EQ(du.ConvertToAString(string), NS_OK);
  EXPECT_EQ(string, u"NaN"_ns);

  nsAutoCString utf8string;
  EXPECT_EQ(du.ConvertToAUTF8String(utf8string), NS_OK);
  EXPECT_EQ(utf8string, "NaN"_ns);

  nsAutoCString autocstring;
  EXPECT_EQ(du.ConvertToACString(autocstring), NS_OK);
  EXPECT_EQ(autocstring, "NaN"_ns);

  char* chars = nullptr;
  EXPECT_EQ(du.ConvertToString(&chars), NS_OK);
  EXPECT_STREQ(chars, "NaN");
  free(chars);

  char16_t* wchars = nullptr;
  EXPECT_EQ(du.ConvertToWString(&wchars), NS_OK);
  {
    // gtest doesn't seem to support EXPECT_STREQ on char16_t, so convert
    // to Gecko strings to do the comparison.
    nsDependentString wstring(wchars);
    EXPECT_EQ(wstring, u"NaN"_ns);
  }
  free(wchars);

  chars = nullptr;
  uint32_t size = 0;
  EXPECT_EQ(du.ConvertToStringWithSize(&size, &chars), NS_OK);
  EXPECT_STREQ(chars, "NaN");
  EXPECT_EQ(size, 3u);
  free(chars);

  wchars = nullptr;
  size = 0;
  EXPECT_EQ(du.ConvertToWStringWithSize(&size, &wchars), NS_OK);
  {
    // gtest doesn't seem to support EXPECT_STREQ on char16_t, so convert
    // to Gecko strings to do the comparison.
    nsDependentString wstring(wchars);
    EXPECT_EQ(wstring, u"NaN"_ns);
    EXPECT_EQ(size, 3u);
  }
  free(wchars);

  nsISupports* isupports;
  EXPECT_EQ(du.ConvertToISupports(&isupports), NS_ERROR_CANNOT_CONVERT_DATA);

  nsIID* idp;
  void* iface;
  EXPECT_EQ(du.ConvertToInterface(&idp, &iface), NS_ERROR_CANNOT_CONVERT_DATA);

  uint16_t type;
  nsIID iid;
  uint32_t count;
  void* array;
  EXPECT_EQ(du.ConvertToArray(&type, &iid, &count, &array),
            NS_ERROR_CANNOT_CONVERT_DATA);
}

TEST(Variant, Bool)
{
  nsDiscriminatedUnion du;
  bool b = false;

  du.SetFromInt64(12);
  EXPECT_EQ(du.ConvertToBool(&b), NS_OK);
  EXPECT_TRUE(b);

  b = true;
  du.SetFromInt64(0);
  EXPECT_EQ(du.ConvertToBool(&b), NS_OK);
  EXPECT_FALSE(b);

  b = true;
  du.SetFromDouble(1.25);
  EXPECT_EQ(du.ConvertToBool(&b), NS_OK);
  EXPECT_TRUE(b);

  b = true;
  du.SetFromDouble(0.0);
  EXPECT_EQ(du.ConvertToBool(&b), NS_OK);
  EXPECT_FALSE(b);

  b = true;
  du.SetFromDouble(-0.0);
  EXPECT_EQ(du.ConvertToBool(&b), NS_OK);
  EXPECT_FALSE(b);

  // This is also checked in the previous test, but I'm including it
  // here for completeness.
  b = true;
  du.SetFromDouble(NAN);
  EXPECT_EQ(du.ConvertToBool(&b), NS_OK);
  EXPECT_FALSE(b);
}
