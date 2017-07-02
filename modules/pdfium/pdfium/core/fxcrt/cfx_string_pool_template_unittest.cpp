// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fxcrt/cfx_string_pool_template.h"
#include "core/fxcrt/fx_string.h"
#include "testing/fx_string_testhelpers.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(fxcrt, ByteStringPool) {
  CFX_ByteStringPool pool;

  CFX_ByteString null1;
  CFX_ByteString null2;
  CFX_ByteString goats1("goats");
  CFX_ByteString goats2("goats");

  // Underlying storage, if non-null, is not shared.
  EXPECT_EQ(nullptr, null1.m_pData.Get());
  EXPECT_EQ(nullptr, null2.m_pData.Get());
  EXPECT_NE(goats1.m_pData, goats2.m_pData);

  CFX_ByteString interned_null1 = pool.Intern(null1);
  CFX_ByteString interned_null2 = pool.Intern(null2);
  CFX_ByteString interned_goats1 = pool.Intern(goats1);
  CFX_ByteString interned_goats2 = pool.Intern(goats2);

  // Strings are logically equal after being interned.
  EXPECT_EQ(null1, interned_null1);
  EXPECT_EQ(null2, interned_null2);
  EXPECT_EQ(goats1, interned_goats1);
  EXPECT_EQ(goats2, interned_goats2);

  // Interned underlying storage, if non-null, belongs to first seen.
  EXPECT_EQ(nullptr, interned_null1.m_pData.Get());
  EXPECT_EQ(nullptr, interned_null2.m_pData.Get());
  EXPECT_EQ(goats1.m_pData, interned_goats1.m_pData);
  EXPECT_EQ(goats1.m_pData, interned_goats2.m_pData);

  pool.Clear();
  CFX_ByteString reinterned_null2 = pool.Intern(null2);
  CFX_ByteString reinterned_null1 = pool.Intern(null2);
  CFX_ByteString reinterned_goats2 = pool.Intern(goats2);
  CFX_ByteString reinterned_goats1 = pool.Intern(goats2);

  // After clearing pool, storage was re-interned using second strings.
  EXPECT_EQ(nullptr, interned_null1.m_pData.Get());
  EXPECT_EQ(nullptr, interned_null2.m_pData.Get());
  EXPECT_EQ(goats2.m_pData, reinterned_goats1.m_pData);
  EXPECT_EQ(goats2.m_pData, reinterned_goats2.m_pData);
}

TEST(fxcrt, WideStringPool) {
  CFX_WideStringPool pool;

  CFX_WideString null1;
  CFX_WideString null2;
  CFX_WideString goats1(L"goats");
  CFX_WideString goats2(L"goats");

  // Underlying storage, if non-null, is not shared.
  EXPECT_EQ(nullptr, null1.m_pData.Get());
  EXPECT_EQ(nullptr, null2.m_pData.Get());
  EXPECT_NE(goats1.m_pData, goats2.m_pData);

  CFX_WideString interned_null1 = pool.Intern(null1);
  CFX_WideString interned_null2 = pool.Intern(null2);
  CFX_WideString interned_goats1 = pool.Intern(goats1);
  CFX_WideString interned_goats2 = pool.Intern(goats2);

  // Strings are logically equal after being interned.
  EXPECT_EQ(null1, interned_null1);
  EXPECT_EQ(null2, interned_null2);
  EXPECT_EQ(goats1, interned_goats1);
  EXPECT_EQ(goats2, interned_goats2);

  // Interned underlying storage, if non-null, belongs to first seen.
  EXPECT_EQ(nullptr, interned_null1.m_pData.Get());
  EXPECT_EQ(nullptr, interned_null2.m_pData.Get());
  EXPECT_EQ(goats1.m_pData, interned_goats1.m_pData);
  EXPECT_EQ(goats1.m_pData, interned_goats2.m_pData);

  pool.Clear();
  CFX_WideString reinterned_null2 = pool.Intern(null2);
  CFX_WideString reinterned_null1 = pool.Intern(null2);
  CFX_WideString reinterned_goats2 = pool.Intern(goats2);
  CFX_WideString reinterned_goats1 = pool.Intern(goats2);

  // After clearing pool, storage was re-interned using second strings.
  EXPECT_EQ(nullptr, interned_null1.m_pData.Get());
  EXPECT_EQ(nullptr, interned_null2.m_pData.Get());
  EXPECT_EQ(goats2.m_pData, reinterned_goats1.m_pData);
  EXPECT_EQ(goats2.m_pData, reinterned_goats2.m_pData);
}
