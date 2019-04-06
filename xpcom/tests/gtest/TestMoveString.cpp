/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>
#include "nsASCIIMask.h"
#include "nsString.h"
#include "nsStringBuffer.h"
#include "nsReadableUtils.h"
#include "nsCRTGlue.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Unused.h"
#include "gtest/gtest.h"

namespace TestMoveString {

#define NEW_VAL "**new value**"
#define OLD_VAL "old value"

typedef mozilla::detail::StringDataFlags Df;

static void SetAsOwned(nsACString& aStr, const char* aValue) {
  size_t len = strlen(aValue);
  char* data = new char[len + 1];
  memcpy(data, aValue, len + 1);
  aStr.Adopt(data, len);
  EXPECT_EQ(aStr.GetDataFlags(), Df::OWNED | Df::TERMINATED);
  EXPECT_STREQ(aStr.BeginReading(), aValue);
}

static void ExpectTruncated(const nsACString& aStr) {
  EXPECT_EQ(aStr.Length(), uint32_t(0));
  EXPECT_STREQ(aStr.BeginReading(), "");
  EXPECT_EQ(aStr.GetDataFlags(), Df::TERMINATED);
}

static void ExpectNew(const nsACString& aStr) {
  EXPECT_EQ(aStr.Length(), strlen(NEW_VAL));
  EXPECT_TRUE(aStr.EqualsASCII(NEW_VAL));
}

TEST(MoveString, SharedIntoOwned)
{
  nsCString out;
  SetAsOwned(out, OLD_VAL);
  EXPECT_EQ(out.GetDataFlags(), Df::OWNED | Df::TERMINATED);

  nsCString in;
  in.Assign(NEW_VAL);
  EXPECT_EQ(in.GetDataFlags(), Df::REFCOUNTED | Df::TERMINATED);
  const char* data = in.get();

  out.Assign(std::move(in));
  ExpectTruncated(in);
  ExpectNew(out);

  EXPECT_EQ(out.GetDataFlags(), Df::REFCOUNTED | Df::TERMINATED);
  EXPECT_EQ(out.get(), data);
}

TEST(MoveString, OwnedIntoOwned)
{
  nsCString out;
  SetAsOwned(out, OLD_VAL);
  EXPECT_EQ(out.GetDataFlags(), Df::OWNED | Df::TERMINATED);

  nsCString in;
  SetAsOwned(in, NEW_VAL);
  EXPECT_EQ(in.GetDataFlags(), Df::OWNED | Df::TERMINATED);
  const char* data = in.get();

  out.Assign(std::move(in));
  ExpectTruncated(in);
  ExpectNew(out);

  EXPECT_EQ(out.GetDataFlags(), Df::OWNED | Df::TERMINATED);
  EXPECT_EQ(out.get(), data);
}

TEST(MoveString, LiteralIntoOwned)
{
  nsCString out;
  SetAsOwned(out, OLD_VAL);
  EXPECT_EQ(out.GetDataFlags(), Df::OWNED | Df::TERMINATED);

  nsCString in;
  in.AssignLiteral(NEW_VAL);
  EXPECT_EQ(in.GetDataFlags(), Df::LITERAL | Df::TERMINATED);
  const char* data = in.get();

  out.Assign(std::move(in));
  ExpectTruncated(in);
  ExpectNew(out);

  EXPECT_EQ(out.GetDataFlags(), Df::LITERAL | Df::TERMINATED);
  EXPECT_EQ(out.get(), data);
}

TEST(MoveString, AutoIntoOwned)
{
  nsCString out;
  SetAsOwned(out, OLD_VAL);
  EXPECT_EQ(out.GetDataFlags(), Df::OWNED | Df::TERMINATED);

  nsAutoCString in;
  in.Assign(NEW_VAL);
  EXPECT_EQ(in.GetDataFlags(), Df::INLINE | Df::TERMINATED);
  const char* data = in.get();

  out.Assign(std::move(in));
  ExpectTruncated(in);
  ExpectNew(out);

  EXPECT_EQ(out.GetDataFlags(), Df::REFCOUNTED | Df::TERMINATED);
  EXPECT_NE(out.get(), data);
}

TEST(MoveString, DepIntoOwned)
{
  nsCString out;
  SetAsOwned(out, OLD_VAL);
  EXPECT_EQ(out.GetDataFlags(), Df::OWNED | Df::TERMINATED);

  nsDependentCSubstring in(NEW_VAL "garbage after", strlen(NEW_VAL));
  EXPECT_EQ(in.GetDataFlags(), Df(0));

  out.Assign(std::move(in));
  ExpectTruncated(in);
  ExpectNew(out);

  EXPECT_EQ(out.GetDataFlags(), Df::REFCOUNTED | Df::TERMINATED);
}

TEST(MoveString, VoidIntoOwned)
{
  nsCString out;
  SetAsOwned(out, OLD_VAL);
  EXPECT_EQ(out.GetDataFlags(), Df::OWNED | Df::TERMINATED);

  nsCString in = VoidCString();
  EXPECT_EQ(in.GetDataFlags(), Df::VOIDED | Df::TERMINATED);

  out.Assign(std::move(in));
  ExpectTruncated(in);

  EXPECT_EQ(out.Length(), 0u);
  EXPECT_STREQ(out.get(), "");
  EXPECT_EQ(out.GetDataFlags(), Df::VOIDED | Df::TERMINATED);
}

TEST(MoveString, SharedIntoAuto)
{
  nsAutoCString out;
  out.Assign(OLD_VAL);
  EXPECT_EQ(out.GetDataFlags(), Df::INLINE | Df::TERMINATED);

  nsCString in;
  in.Assign(NEW_VAL);
  EXPECT_EQ(in.GetDataFlags(), Df::REFCOUNTED | Df::TERMINATED);
  const char* data = in.get();

  out.Assign(std::move(in));
  ExpectTruncated(in);
  ExpectNew(out);

  EXPECT_EQ(out.GetDataFlags(), Df::REFCOUNTED | Df::TERMINATED);
  EXPECT_EQ(out.get(), data);
}

TEST(MoveString, OwnedIntoAuto)
{
  nsAutoCString out;
  out.Assign(OLD_VAL);
  EXPECT_EQ(out.GetDataFlags(), Df::INLINE | Df::TERMINATED);

  nsCString in;
  SetAsOwned(in, NEW_VAL);
  EXPECT_EQ(in.GetDataFlags(), Df::OWNED | Df::TERMINATED);
  const char* data = in.get();

  out.Assign(std::move(in));
  ExpectTruncated(in);
  ExpectNew(out);

  EXPECT_EQ(out.GetDataFlags(), Df::OWNED | Df::TERMINATED);
  EXPECT_EQ(out.get(), data);
}

TEST(MoveString, LiteralIntoAuto)
{
  nsAutoCString out;
  out.Assign(OLD_VAL);
  EXPECT_EQ(out.GetDataFlags(), Df::INLINE | Df::TERMINATED);

  nsCString in;
  in.AssignLiteral(NEW_VAL);
  EXPECT_EQ(in.GetDataFlags(), Df::LITERAL | Df::TERMINATED);
  const char* data = in.get();

  out.Assign(std::move(in));
  ExpectTruncated(in);
  ExpectNew(out);

  EXPECT_EQ(out.GetDataFlags(), Df::LITERAL | Df::TERMINATED);
  EXPECT_EQ(out.get(), data);
}

TEST(MoveString, AutoIntoAuto)
{
  nsAutoCString out;
  out.Assign(OLD_VAL);
  EXPECT_EQ(out.GetDataFlags(), Df::INLINE | Df::TERMINATED);

  nsAutoCString in;
  in.Assign(NEW_VAL);
  EXPECT_EQ(in.GetDataFlags(), Df::INLINE | Df::TERMINATED);
  const char* data = in.get();

  out.Assign(std::move(in));
  ExpectTruncated(in);
  ExpectNew(out);

  EXPECT_EQ(out.GetDataFlags(), Df::INLINE | Df::TERMINATED);
  EXPECT_NE(out.get(), data);
}

TEST(MoveString, DepIntoAuto)
{
  nsAutoCString out;
  out.Assign(OLD_VAL);
  EXPECT_EQ(out.GetDataFlags(), Df::INLINE | Df::TERMINATED);

  nsDependentCSubstring in(NEW_VAL "garbage after", strlen(NEW_VAL));
  EXPECT_EQ(in.GetDataFlags(), Df(0));

  out.Assign(std::move(in));
  ExpectTruncated(in);
  ExpectNew(out);

  EXPECT_EQ(out.GetDataFlags(), Df::INLINE | Df::TERMINATED);
}

TEST(MoveString, VoidIntoAuto)
{
  nsAutoCString out;
  out.Assign(OLD_VAL);
  EXPECT_EQ(out.GetDataFlags(), Df::INLINE | Df::TERMINATED);

  nsCString in = VoidCString();
  EXPECT_EQ(in.GetDataFlags(), Df::VOIDED | Df::TERMINATED);

  out.Assign(std::move(in));
  ExpectTruncated(in);

  EXPECT_EQ(out.Length(), 0u);
  EXPECT_STREQ(out.get(), "");
  EXPECT_EQ(out.GetDataFlags(), Df::VOIDED | Df::TERMINATED);
}

#undef NEW_VAL
#undef OLD_VAL

}  // namespace TestMoveString
