// Copyright 2015 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#include "core/fxcrt/fx_string.h"
#include "fpdfsdk/javascript/PublicMethods.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/js_embedder_test.h"

namespace {

double RoundDownDate(double date) {
  return date - fmod(date, 86400000);
}

}  // namespace

class PublicMethodsEmbedderTest : public JSEmbedderTest {};

TEST_F(PublicMethodsEmbedderTest, MakeRegularDate) {
  v8::Isolate::Scope isolate_scope(isolate());
  v8::HandleScope handle_scope(isolate());
  v8::Context::Scope context_scope(GetV8Context());
  bool bWrongFormat;
  double date;

  // 1968
  bWrongFormat = false;
  date = CJS_PublicMethods::MakeRegularDate(L"06/25/1968", L"mm/dd/yyyy",
                                            &bWrongFormat);
  date = RoundDownDate(date);
  EXPECT_DOUBLE_EQ(-47865600000, date);
  EXPECT_FALSE(bWrongFormat);

  // 1968
  bWrongFormat = false;
  date = CJS_PublicMethods::MakeRegularDate(L"25061968", L"ddmmyyyy",
                                            &bWrongFormat);
  date = RoundDownDate(date);
  EXPECT_DOUBLE_EQ(-47865600000, date);
  EXPECT_FALSE(bWrongFormat);

  // 1968
  bWrongFormat = false;
  date = CJS_PublicMethods::MakeRegularDate(L"19680625", L"yyyymmdd",
                                            &bWrongFormat);
  date = RoundDownDate(date);
  EXPECT_DOUBLE_EQ(-47865600000, date);
  EXPECT_FALSE(bWrongFormat);

  // 1985
  bWrongFormat = false;
  date = CJS_PublicMethods::MakeRegularDate(L"31121985", L"ddmmyyyy",
                                            &bWrongFormat);
  date = RoundDownDate(date);
  EXPECT_DOUBLE_EQ(504835200000.0, date);
  EXPECT_FALSE(bWrongFormat);

  // 2085, the other '85.
  bWrongFormat = false;
  date =
      CJS_PublicMethods::MakeRegularDate(L"311285", L"ddmmyy", &bWrongFormat);
  date = RoundDownDate(date);
  EXPECT_DOUBLE_EQ(3660595200000.0, date);
  EXPECT_FALSE(bWrongFormat);

  // 1995
  bWrongFormat = false;
  date = CJS_PublicMethods::MakeRegularDate(L"01021995", L"ddmmyyyy",
                                            &bWrongFormat);
  date = RoundDownDate(date);
  EXPECT_DOUBLE_EQ(791596800000.0, date);
  EXPECT_FALSE(bWrongFormat);

  // 2095, the other '95.
  bWrongFormat = false;
  date =
      CJS_PublicMethods::MakeRegularDate(L"010295", L"ddmmyy", &bWrongFormat);
  date = RoundDownDate(date);
  EXPECT_DOUBLE_EQ(3947356800000.0, date);
  EXPECT_FALSE(bWrongFormat);

  // 2005
  bWrongFormat = false;
  date = CJS_PublicMethods::MakeRegularDate(L"01022005", L"ddmmyyyy",
                                            &bWrongFormat);
  date = RoundDownDate(date);
  EXPECT_DOUBLE_EQ(1107216000000.0, date);
  EXPECT_FALSE(bWrongFormat);

  // 2005
  bWrongFormat = false;
  date =
      CJS_PublicMethods::MakeRegularDate(L"010205", L"ddmmyy", &bWrongFormat);
  date = RoundDownDate(date);
  EXPECT_DOUBLE_EQ(1107216000000.0, date);
  EXPECT_FALSE(bWrongFormat);
}

TEST_F(PublicMethodsEmbedderTest, MakeFormatDate) {
  v8::Isolate::Scope isolate_scope(isolate());
  v8::HandleScope handle_scope(isolate());
  v8::Context::Scope context_scope(GetV8Context());
  CFX_WideString formatted_date;

  // 1968-06-25
  formatted_date = CJS_PublicMethods::MakeFormatDate(-47952000000, L"ddmmyy");
  EXPECT_STREQ(L"250668", formatted_date.c_str());
  formatted_date = CJS_PublicMethods::MakeFormatDate(-47952000000, L"yy/mm/dd");
  EXPECT_STREQ(L"68/06/25", formatted_date.c_str());

  // 1969-12-31
  formatted_date = CJS_PublicMethods::MakeFormatDate(-0.0001, L"ddmmyy");
  EXPECT_STREQ(L"311269", formatted_date.c_str());
  formatted_date = CJS_PublicMethods::MakeFormatDate(-0.0001, L"yy!mmdd");
  EXPECT_STREQ(L"69!1231", formatted_date.c_str());

  // 1970-01-01
  formatted_date = CJS_PublicMethods::MakeFormatDate(0, L"ddmmyy");
  EXPECT_STREQ(L"010170", formatted_date.c_str());
  formatted_date = CJS_PublicMethods::MakeFormatDate(0, L"mm-yyyy-dd");
  EXPECT_STREQ(L"01-1970-01", formatted_date.c_str());

  // 1985-12-31
  formatted_date = CJS_PublicMethods::MakeFormatDate(504835200000.0, L"ddmmyy");
  EXPECT_STREQ(L"311285", formatted_date.c_str());
  formatted_date = CJS_PublicMethods::MakeFormatDate(504835200000.0, L"yymmdd");
  EXPECT_STREQ(L"851231", formatted_date.c_str());

  // 1995-02-01
  formatted_date = CJS_PublicMethods::MakeFormatDate(791596800000.0, L"ddmmyy");
  EXPECT_STREQ(L"010295", formatted_date.c_str());
  formatted_date =
      CJS_PublicMethods::MakeFormatDate(791596800000.0, L"yyyymmdd");
  EXPECT_STREQ(L"19950201", formatted_date.c_str());

  // 2005-02-01
  formatted_date =
      CJS_PublicMethods::MakeFormatDate(1107216000000.0, L"ddmmyy");
  EXPECT_STREQ(L"010205", formatted_date.c_str());
  formatted_date =
      CJS_PublicMethods::MakeFormatDate(1107216000000.0, L"yyyyddmm");
  EXPECT_STREQ(L"20050102", formatted_date.c_str());

  // 2085-12-31
  formatted_date =
      CJS_PublicMethods::MakeFormatDate(3660595200000.0, L"ddmmyy");
  EXPECT_STREQ(L"311285", formatted_date.c_str());
  formatted_date =
      CJS_PublicMethods::MakeFormatDate(3660595200000.0, L"yyyydd");
  EXPECT_STREQ(L"208531", formatted_date.c_str());

  // 2095-02-01
  formatted_date =
      CJS_PublicMethods::MakeFormatDate(3947356800000.0, L"ddmmyy");
  EXPECT_STREQ(L"010295", formatted_date.c_str());
  formatted_date =
      CJS_PublicMethods::MakeFormatDate(3947356800000.0, L"mmddyyyy");
  EXPECT_STREQ(L"02012095", formatted_date.c_str());
}
