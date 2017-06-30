// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fxcrt/fx_basic.h"
#include "testing/fx_string_testhelpers.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(fxcrt, WideStringOperatorSubscript) {
  // CFX_WideString includes the NUL terminator for non-empty strings.
  CFX_WideString abc(L"abc");
  EXPECT_EQ(L'a', abc[0]);
  EXPECT_EQ(L'b', abc[1]);
  EXPECT_EQ(L'c', abc[2]);
  EXPECT_EQ(L'\0', abc[3]);
}

TEST(fxcrt, WideStringOperatorLT) {
  CFX_WideString empty;
  CFX_WideString a(L"a");
  CFX_WideString abc(L"\x0110qq");  // Comes before despite endianness.
  CFX_WideString def(L"\x1001qq");  // Comes after despite endianness.

  EXPECT_FALSE(empty < empty);
  EXPECT_FALSE(a < a);
  EXPECT_FALSE(abc < abc);
  EXPECT_FALSE(def < def);

  EXPECT_TRUE(empty < a);
  EXPECT_FALSE(a < empty);

  EXPECT_TRUE(empty < abc);
  EXPECT_FALSE(abc < empty);

  EXPECT_TRUE(empty < def);
  EXPECT_FALSE(def < empty);

  EXPECT_TRUE(a < abc);
  EXPECT_FALSE(abc < a);

  EXPECT_TRUE(a < def);
  EXPECT_FALSE(def < a);

  EXPECT_TRUE(abc < def);
  EXPECT_FALSE(def < abc);
}

TEST(fxcrt, WideStringOperatorEQ) {
  CFX_WideString null_string;
  EXPECT_TRUE(null_string == null_string);

  CFX_WideString empty_string(L"");
  EXPECT_TRUE(empty_string == empty_string);
  EXPECT_TRUE(empty_string == null_string);
  EXPECT_TRUE(null_string == empty_string);

  CFX_WideString deleted_string(L"hello");
  deleted_string.Delete(0, 5);
  EXPECT_TRUE(deleted_string == deleted_string);
  EXPECT_TRUE(deleted_string == null_string);
  EXPECT_TRUE(deleted_string == empty_string);
  EXPECT_TRUE(null_string == deleted_string);
  EXPECT_TRUE(null_string == empty_string);

  CFX_WideString wide_string(L"hello");
  EXPECT_TRUE(wide_string == wide_string);
  EXPECT_FALSE(wide_string == null_string);
  EXPECT_FALSE(wide_string == empty_string);
  EXPECT_FALSE(wide_string == deleted_string);
  EXPECT_FALSE(null_string == wide_string);
  EXPECT_FALSE(empty_string == wide_string);
  EXPECT_FALSE(deleted_string == wide_string);

  CFX_WideString wide_string_same1(L"hello");
  EXPECT_TRUE(wide_string == wide_string_same1);
  EXPECT_TRUE(wide_string_same1 == wide_string);

  CFX_WideString wide_string_same2(wide_string);
  EXPECT_TRUE(wide_string == wide_string_same2);
  EXPECT_TRUE(wide_string_same2 == wide_string);

  CFX_WideString wide_string1(L"he");
  CFX_WideString wide_string2(L"hellp");
  CFX_WideString wide_string3(L"hellod");
  EXPECT_FALSE(wide_string == wide_string1);
  EXPECT_FALSE(wide_string == wide_string2);
  EXPECT_FALSE(wide_string == wide_string3);
  EXPECT_FALSE(wide_string1 == wide_string);
  EXPECT_FALSE(wide_string2 == wide_string);
  EXPECT_FALSE(wide_string3 == wide_string);

  CFX_WideStringC null_string_c;
  CFX_WideStringC empty_string_c(L"");
  EXPECT_TRUE(null_string == null_string_c);
  EXPECT_TRUE(null_string == empty_string_c);
  EXPECT_TRUE(empty_string == null_string_c);
  EXPECT_TRUE(empty_string == empty_string_c);
  EXPECT_TRUE(deleted_string == null_string_c);
  EXPECT_TRUE(deleted_string == empty_string_c);
  EXPECT_TRUE(null_string_c == null_string);
  EXPECT_TRUE(empty_string_c == null_string);
  EXPECT_TRUE(null_string_c == empty_string);
  EXPECT_TRUE(empty_string_c == empty_string);
  EXPECT_TRUE(null_string_c == deleted_string);
  EXPECT_TRUE(empty_string_c == deleted_string);

  CFX_WideStringC wide_string_c_same1(L"hello");
  EXPECT_TRUE(wide_string == wide_string_c_same1);
  EXPECT_TRUE(wide_string_c_same1 == wide_string);

  CFX_WideStringC wide_string_c1(L"he");
  CFX_WideStringC wide_string_c2(L"hellp");
  CFX_WideStringC wide_string_c3(L"hellod");
  EXPECT_FALSE(wide_string == wide_string_c1);
  EXPECT_FALSE(wide_string == wide_string_c2);
  EXPECT_FALSE(wide_string == wide_string_c3);
  EXPECT_FALSE(wide_string_c1 == wide_string);
  EXPECT_FALSE(wide_string_c2 == wide_string);
  EXPECT_FALSE(wide_string_c3 == wide_string);

  const wchar_t* c_null_string = nullptr;
  const wchar_t* c_empty_string = L"";
  EXPECT_TRUE(null_string == c_null_string);
  EXPECT_TRUE(null_string == c_empty_string);
  EXPECT_TRUE(empty_string == c_null_string);
  EXPECT_TRUE(empty_string == c_empty_string);
  EXPECT_TRUE(deleted_string == c_null_string);
  EXPECT_TRUE(deleted_string == c_empty_string);
  EXPECT_TRUE(c_null_string == null_string);
  EXPECT_TRUE(c_empty_string == null_string);
  EXPECT_TRUE(c_null_string == empty_string);
  EXPECT_TRUE(c_empty_string == empty_string);
  EXPECT_TRUE(c_null_string == deleted_string);
  EXPECT_TRUE(c_empty_string == deleted_string);

  const wchar_t* c_string_same1 = L"hello";
  EXPECT_TRUE(wide_string == c_string_same1);
  EXPECT_TRUE(c_string_same1 == wide_string);

  const wchar_t* c_string1 = L"he";
  const wchar_t* c_string2 = L"hellp";
  const wchar_t* c_string3 = L"hellod";
  EXPECT_FALSE(wide_string == c_string1);
  EXPECT_FALSE(wide_string == c_string2);
  EXPECT_FALSE(wide_string == c_string3);
  EXPECT_FALSE(c_string1 == wide_string);
  EXPECT_FALSE(c_string2 == wide_string);
  EXPECT_FALSE(c_string3 == wide_string);
}

TEST(fxcrt, WideStringOperatorNE) {
  CFX_WideString null_string;
  EXPECT_FALSE(null_string != null_string);

  CFX_WideString empty_string(L"");
  EXPECT_FALSE(empty_string != empty_string);
  EXPECT_FALSE(empty_string != null_string);
  EXPECT_FALSE(null_string != empty_string);

  CFX_WideString deleted_string(L"hello");
  deleted_string.Delete(0, 5);
  EXPECT_FALSE(deleted_string != deleted_string);
  EXPECT_FALSE(deleted_string != null_string);
  EXPECT_FALSE(deleted_string != empty_string);
  EXPECT_FALSE(null_string != deleted_string);
  EXPECT_FALSE(null_string != empty_string);

  CFX_WideString wide_string(L"hello");
  EXPECT_FALSE(wide_string != wide_string);
  EXPECT_TRUE(wide_string != null_string);
  EXPECT_TRUE(wide_string != empty_string);
  EXPECT_TRUE(wide_string != deleted_string);
  EXPECT_TRUE(null_string != wide_string);
  EXPECT_TRUE(empty_string != wide_string);
  EXPECT_TRUE(deleted_string != wide_string);

  CFX_WideString wide_string_same1(L"hello");
  EXPECT_FALSE(wide_string != wide_string_same1);
  EXPECT_FALSE(wide_string_same1 != wide_string);

  CFX_WideString wide_string_same2(wide_string);
  EXPECT_FALSE(wide_string != wide_string_same2);
  EXPECT_FALSE(wide_string_same2 != wide_string);

  CFX_WideString wide_string1(L"he");
  CFX_WideString wide_string2(L"hellp");
  CFX_WideString wide_string3(L"hellod");
  EXPECT_TRUE(wide_string != wide_string1);
  EXPECT_TRUE(wide_string != wide_string2);
  EXPECT_TRUE(wide_string != wide_string3);
  EXPECT_TRUE(wide_string1 != wide_string);
  EXPECT_TRUE(wide_string2 != wide_string);
  EXPECT_TRUE(wide_string3 != wide_string);

  CFX_WideStringC null_string_c;
  CFX_WideStringC empty_string_c(L"");
  EXPECT_FALSE(null_string != null_string_c);
  EXPECT_FALSE(null_string != empty_string_c);
  EXPECT_FALSE(empty_string != null_string_c);
  EXPECT_FALSE(empty_string != empty_string_c);
  EXPECT_FALSE(deleted_string != null_string_c);
  EXPECT_FALSE(deleted_string != empty_string_c);
  EXPECT_FALSE(null_string_c != null_string);
  EXPECT_FALSE(empty_string_c != null_string);
  EXPECT_FALSE(null_string_c != empty_string);
  EXPECT_FALSE(empty_string_c != empty_string);

  CFX_WideStringC wide_string_c_same1(L"hello");
  EXPECT_FALSE(wide_string != wide_string_c_same1);
  EXPECT_FALSE(wide_string_c_same1 != wide_string);

  CFX_WideStringC wide_string_c1(L"he");
  CFX_WideStringC wide_string_c2(L"hellp");
  CFX_WideStringC wide_string_c3(L"hellod");
  EXPECT_TRUE(wide_string != wide_string_c1);
  EXPECT_TRUE(wide_string != wide_string_c2);
  EXPECT_TRUE(wide_string != wide_string_c3);
  EXPECT_TRUE(wide_string_c1 != wide_string);
  EXPECT_TRUE(wide_string_c2 != wide_string);
  EXPECT_TRUE(wide_string_c3 != wide_string);

  const wchar_t* c_null_string = nullptr;
  const wchar_t* c_empty_string = L"";
  EXPECT_FALSE(null_string != c_null_string);
  EXPECT_FALSE(null_string != c_empty_string);
  EXPECT_FALSE(empty_string != c_null_string);
  EXPECT_FALSE(empty_string != c_empty_string);
  EXPECT_FALSE(deleted_string != c_null_string);
  EXPECT_FALSE(deleted_string != c_empty_string);
  EXPECT_FALSE(c_null_string != null_string);
  EXPECT_FALSE(c_empty_string != null_string);
  EXPECT_FALSE(c_null_string != empty_string);
  EXPECT_FALSE(c_empty_string != empty_string);
  EXPECT_FALSE(c_null_string != deleted_string);
  EXPECT_FALSE(c_empty_string != deleted_string);

  const wchar_t* c_string_same1 = L"hello";
  EXPECT_FALSE(wide_string != c_string_same1);
  EXPECT_FALSE(c_string_same1 != wide_string);

  const wchar_t* c_string1 = L"he";
  const wchar_t* c_string2 = L"hellp";
  const wchar_t* c_string3 = L"hellod";
  EXPECT_TRUE(wide_string != c_string1);
  EXPECT_TRUE(wide_string != c_string2);
  EXPECT_TRUE(wide_string != c_string3);
  EXPECT_TRUE(c_string1 != wide_string);
  EXPECT_TRUE(c_string2 != wide_string);
  EXPECT_TRUE(c_string3 != wide_string);
}

TEST(fxcrt, WideStringConcatInPlace) {
  CFX_WideString fred;
  fred.Concat(L"FRED", 4);
  EXPECT_EQ(L"FRED", fred);

  fred.Concat(L"DY", 2);
  EXPECT_EQ(L"FREDDY", fred);

  fred.Delete(3, 3);
  EXPECT_EQ(L"FRE", fred);

  fred.Concat(L"D", 1);
  EXPECT_EQ(L"FRED", fred);

  CFX_WideString copy = fred;
  fred.Concat(L"DY", 2);
  EXPECT_EQ(L"FREDDY", fred);
  EXPECT_EQ(L"FRED", copy);

  // Test invalid arguments.
  copy = fred;
  fred.Concat(L"freddy", -6);
  CFX_WideString not_aliased(L"xxxxxx");
  EXPECT_EQ(L"FREDDY", fred);
  EXPECT_EQ(L"xxxxxx", not_aliased);
}

TEST(fxcrt, WideStringRemove) {
  CFX_WideString freed(L"FREED");
  freed.Remove(L'E');
  EXPECT_EQ(L"FRD", freed);
  freed.Remove(L'F');
  EXPECT_EQ(L"RD", freed);
  freed.Remove(L'D');
  EXPECT_EQ(L"R", freed);
  freed.Remove(L'X');
  EXPECT_EQ(L"R", freed);
  freed.Remove(L'R');
  EXPECT_EQ(L"", freed);

  CFX_WideString empty;
  empty.Remove(L'X');
  EXPECT_EQ(L"", empty);
}

TEST(fxcrt, WideStringRemoveCopies) {
  CFX_WideString freed(L"FREED");
  const FX_WCHAR* old_buffer = freed.c_str();

  // No change with single reference - no copy.
  freed.Remove(L'Q');
  EXPECT_EQ(L"FREED", freed);
  EXPECT_EQ(old_buffer, freed.c_str());

  // Change with single reference - no copy.
  freed.Remove(L'E');
  EXPECT_EQ(L"FRD", freed);
  EXPECT_EQ(old_buffer, freed.c_str());

  // No change with multiple references - no copy.
  CFX_WideString shared(freed);
  freed.Remove(L'Q');
  EXPECT_EQ(L"FRD", freed);
  EXPECT_EQ(old_buffer, freed.c_str());
  EXPECT_EQ(old_buffer, shared.c_str());

  // Change with multiple references -- must copy.
  freed.Remove(L'D');
  EXPECT_EQ(L"FR", freed);
  EXPECT_NE(old_buffer, freed.c_str());
  EXPECT_EQ(L"FRD", shared);
  EXPECT_EQ(old_buffer, shared.c_str());
}

TEST(fxcrt, WideStringReplace) {
  CFX_WideString fred(L"FRED");
  fred.Replace(L"FR", L"BL");
  EXPECT_EQ(L"BLED", fred);
  fred.Replace(L"D", L"DDY");
  EXPECT_EQ(L"BLEDDY", fred);
  fred.Replace(L"LEDD", L"");
  EXPECT_EQ(L"BY", fred);
  fred.Replace(L"X", L"CLAMS");
  EXPECT_EQ(L"BY", fred);
  fred.Replace(L"BY", L"HI");
  EXPECT_EQ(L"HI", fred);
  fred.Replace(L"", L"CLAMS");
  EXPECT_EQ(L"HI", fred);
  fred.Replace(L"HI", L"");
  EXPECT_EQ(L"", fred);
}

TEST(fxcrt, WideStringInsert) {
  CFX_WideString fred(L"FRED");
  fred.Insert(-1, 'X');
  EXPECT_EQ(L"XFRED", fred);

  fred.Insert(0, 'S');
  EXPECT_EQ(L"SXFRED", fred);

  fred.Insert(2, 'T');
  EXPECT_EQ(L"SXTFRED", fred);

  fred.Insert(5, 'U');
  EXPECT_EQ(L"SXTFRUED", fred);

  fred.Insert(8, 'V');
  EXPECT_EQ(L"SXTFRUEDV", fred);

  fred.Insert(12, 'P');
  EXPECT_EQ(L"SXTFRUEDVP", fred);

  {
    CFX_WideString empty;
    empty.Insert(-1, 'X');
    EXPECT_EQ(L"X", empty);
  }
  {
    CFX_WideString empty;
    empty.Insert(0, 'X');
    EXPECT_EQ(L"X", empty);
  }
  {
    CFX_WideString empty;
    empty.Insert(5, 'X');
    EXPECT_EQ(L"X", empty);
  }
}

TEST(fxcrt, WideStringDelete) {
  CFX_WideString fred(L"FRED");
  fred.Delete(0, 2);
  EXPECT_EQ(L"ED", fred);
  fred.Delete(1);
  EXPECT_EQ(L"E", fred);
  fred.Delete(-1);
  EXPECT_EQ(L"", fred);
  fred.Delete(1);
  EXPECT_EQ(L"", fred);

  CFX_WideString empty;
  empty.Delete(0);
  EXPECT_EQ(L"", empty);
  empty.Delete(-1);
  EXPECT_EQ(L"", empty);
  empty.Delete(1);
  EXPECT_EQ(L"", empty);
}

TEST(fxcrt, WideStringMid) {
  CFX_WideString fred(L"FRED");
  EXPECT_EQ(L"", fred.Mid(0, 0));
  EXPECT_EQ(L"", fred.Mid(3, 0));
  EXPECT_EQ(L"FRED", fred.Mid(0));
  EXPECT_EQ(L"RED", fred.Mid(1));
  EXPECT_EQ(L"ED", fred.Mid(2));
  EXPECT_EQ(L"D", fred.Mid(3));
  EXPECT_EQ(L"F", fred.Mid(0, 1));
  EXPECT_EQ(L"R", fred.Mid(1, 1));
  EXPECT_EQ(L"E", fred.Mid(2, 1));
  EXPECT_EQ(L"D", fred.Mid(3, 1));
  EXPECT_EQ(L"FR", fred.Mid(0, 2));
  EXPECT_EQ(L"FRED", fred.Mid(0, 4));
  EXPECT_EQ(L"FRED", fred.Mid(0, 10));

  EXPECT_EQ(L"FR", fred.Mid(-1, 2));
  EXPECT_EQ(L"RED", fred.Mid(1, 4));
  EXPECT_EQ(L"", fred.Mid(4, 1));

  CFX_WideString empty;
  EXPECT_EQ(L"", empty.Mid(0, 0));
  EXPECT_EQ(L"", empty.Mid(0));
  EXPECT_EQ(L"", empty.Mid(1));
  EXPECT_EQ(L"", empty.Mid(-1));
}

TEST(fxcrt, WideStringLeft) {
  CFX_WideString fred(L"FRED");
  EXPECT_EQ(L"", fred.Left(0));
  EXPECT_EQ(L"F", fred.Left(1));
  EXPECT_EQ(L"FR", fred.Left(2));
  EXPECT_EQ(L"FRE", fred.Left(3));
  EXPECT_EQ(L"FRED", fred.Left(4));

  EXPECT_EQ(L"FRED", fred.Left(5));
  EXPECT_EQ(L"", fred.Left(-1));

  CFX_WideString empty;
  EXPECT_EQ(L"", empty.Left(0));
  EXPECT_EQ(L"", empty.Left(1));
  EXPECT_EQ(L"", empty.Left(-1));
}

TEST(fxcrt, WideStringRight) {
  CFX_WideString fred(L"FRED");
  EXPECT_EQ(L"", fred.Right(0));
  EXPECT_EQ(L"D", fred.Right(1));
  EXPECT_EQ(L"ED", fred.Right(2));
  EXPECT_EQ(L"RED", fred.Right(3));
  EXPECT_EQ(L"FRED", fred.Right(4));

  EXPECT_EQ(L"FRED", fred.Right(5));
  EXPECT_EQ(L"", fred.Right(-1));

  CFX_WideString empty;
  EXPECT_EQ(L"", empty.Right(0));
  EXPECT_EQ(L"", empty.Right(1));
  EXPECT_EQ(L"", empty.Right(-1));
}

TEST(fxcrt, WideStringUpperLower) {
  CFX_WideString fred(L"F-Re.42D");
  fred.MakeLower();
  EXPECT_EQ(L"f-re.42d", fred);
  fred.MakeUpper();
  EXPECT_EQ(L"F-RE.42D", fred);

  CFX_WideString empty;
  empty.MakeLower();
  EXPECT_EQ(L"", empty);
  empty.MakeUpper();
  EXPECT_EQ(L"", empty);
}

TEST(fxcrt, WideStringTrimRight) {
  CFX_WideString fred(L"  FRED  ");
  fred.TrimRight();
  EXPECT_EQ(L"  FRED", fred);
  fred.TrimRight(L'E');
  EXPECT_EQ(L"  FRED", fred);
  fred.TrimRight(L'D');
  EXPECT_EQ(L"  FRE", fred);
  fred.TrimRight(L"ERP");
  EXPECT_EQ(L"  F", fred);

  CFX_WideString blank(L"   ");
  blank.TrimRight(L"ERP");
  EXPECT_EQ(L"   ", blank);
  blank.TrimRight(L'E');
  EXPECT_EQ(L"   ", blank);
  blank.TrimRight();
  EXPECT_EQ(L"", blank);

  CFX_WideString empty;
  empty.TrimRight(L"ERP");
  EXPECT_EQ(L"", empty);
  empty.TrimRight(L'E');
  EXPECT_EQ(L"", empty);
  empty.TrimRight();
  EXPECT_EQ(L"", empty);
}

TEST(fxcrt, WideStringTrimRightCopies) {
  {
    // With a single reference, no copy takes place.
    CFX_WideString fred(L"  FRED  ");
    const FX_WCHAR* old_buffer = fred.c_str();
    fred.TrimRight();
    EXPECT_EQ(L"  FRED", fred);
    EXPECT_EQ(old_buffer, fred.c_str());
  }
  {
    // With multiple references, we must copy.
    CFX_WideString fred(L"  FRED  ");
    CFX_WideString other_fred = fred;
    const FX_WCHAR* old_buffer = fred.c_str();
    fred.TrimRight();
    EXPECT_EQ(L"  FRED", fred);
    EXPECT_EQ(L"  FRED  ", other_fred);
    EXPECT_NE(old_buffer, fred.c_str());
  }
  {
    // With multiple references, but no modifications, no copy.
    CFX_WideString fred(L"FRED");
    CFX_WideString other_fred = fred;
    const FX_WCHAR* old_buffer = fred.c_str();
    fred.TrimRight();
    EXPECT_EQ(L"FRED", fred);
    EXPECT_EQ(L"FRED", other_fred);
    EXPECT_EQ(old_buffer, fred.c_str());
  }
}

TEST(fxcrt, WideStringTrimLeft) {
  CFX_WideString fred(L"  FRED  ");
  fred.TrimLeft();
  EXPECT_EQ(L"FRED  ", fred);
  fred.TrimLeft(L'E');
  EXPECT_EQ(L"FRED  ", fred);
  fred.TrimLeft(L'F');
  EXPECT_EQ(L"RED  ", fred);
  fred.TrimLeft(L"ERP");
  EXPECT_EQ(L"D  ", fred);

  CFX_WideString blank(L"   ");
  blank.TrimLeft(L"ERP");
  EXPECT_EQ(L"   ", blank);
  blank.TrimLeft(L'E');
  EXPECT_EQ(L"   ", blank);
  blank.TrimLeft();
  EXPECT_EQ(L"", blank);

  CFX_WideString empty;
  empty.TrimLeft(L"ERP");
  EXPECT_EQ(L"", empty);
  empty.TrimLeft(L'E');
  EXPECT_EQ(L"", empty);
  empty.TrimLeft();
  EXPECT_EQ(L"", empty);
}

TEST(fxcrt, WideStringTrimLeftCopies) {
  {
    // With a single reference, no copy takes place.
    CFX_WideString fred(L"  FRED  ");
    const FX_WCHAR* old_buffer = fred.c_str();
    fred.TrimLeft();
    EXPECT_EQ(L"FRED  ", fred);
    EXPECT_EQ(old_buffer, fred.c_str());
  }
  {
    // With multiple references, we must copy.
    CFX_WideString fred(L"  FRED  ");
    CFX_WideString other_fred = fred;
    const FX_WCHAR* old_buffer = fred.c_str();
    fred.TrimLeft();
    EXPECT_EQ(L"FRED  ", fred);
    EXPECT_EQ(L"  FRED  ", other_fred);
    EXPECT_NE(old_buffer, fred.c_str());
  }
  {
    // With multiple references, but no modifications, no copy.
    CFX_WideString fred(L"FRED");
    CFX_WideString other_fred = fred;
    const FX_WCHAR* old_buffer = fred.c_str();
    fred.TrimLeft();
    EXPECT_EQ(L"FRED", fred);
    EXPECT_EQ(L"FRED", other_fred);
    EXPECT_EQ(old_buffer, fred.c_str());
  }
}

TEST(fxcrt, WideStringReserve) {
  {
    CFX_WideString str;
    str.Reserve(6);
    const FX_WCHAR* old_buffer = str.c_str();
    str += L"ABCDEF";
    EXPECT_EQ(old_buffer, str.c_str());
    str += L"Blah Blah Blah Blah Blah Blah";
    EXPECT_NE(old_buffer, str.c_str());
  }
  {
    CFX_WideString str(L"A");
    str.Reserve(6);
    const FX_WCHAR* old_buffer = str.c_str();
    str += L"BCDEF";
    EXPECT_EQ(old_buffer, str.c_str());
    str += L"Blah Blah Blah Blah Blah Blah";
    EXPECT_NE(old_buffer, str.c_str());
  }
}

TEST(fxcrt, WideStringGetBuffer) {
  {
    CFX_WideString str;
    FX_WCHAR* buffer = str.GetBuffer(12);
    wcscpy(buffer, L"clams");
    str.ReleaseBuffer();
    EXPECT_EQ(L"clams", str);
  }
  {
    CFX_WideString str(L"cl");
    FX_WCHAR* buffer = str.GetBuffer(12);
    wcscpy(buffer + 2, L"ams");
    str.ReleaseBuffer();
    EXPECT_EQ(L"clams", str);
  }
}

TEST(fxcrt, WideStringReleaseBuffer) {
  {
    CFX_WideString str;
    str.Reserve(12);
    str += L"clams";
    const FX_WCHAR* old_buffer = str.c_str();
    str.ReleaseBuffer(4);
    EXPECT_EQ(old_buffer, str.c_str());
    EXPECT_EQ(L"clam", str);
  }
  {
    CFX_WideString str(L"c");
    str.Reserve(12);
    str += L"lams";
    const FX_WCHAR* old_buffer = str.c_str();
    str.ReleaseBuffer(4);
    EXPECT_EQ(old_buffer, str.c_str());
    EXPECT_EQ(L"clam", str);
  }
  {
    CFX_WideString str;
    str.Reserve(200);
    str += L"clams";
    const FX_WCHAR* old_buffer = str.c_str();
    str.ReleaseBuffer(4);
    EXPECT_NE(old_buffer, str.c_str());
    EXPECT_EQ(L"clam", str);
  }
  {
    CFX_WideString str(L"c");
    str.Reserve(200);
    str += L"lams";
    const FX_WCHAR* old_buffer = str.c_str();
    str.ReleaseBuffer(4);
    EXPECT_NE(old_buffer, str.c_str());
    EXPECT_EQ(L"clam", str);
  }
}

TEST(fxcrt, WideStringUTF16LE_Encode) {
  struct UTF16LEEncodeCase {
    CFX_WideString ws;
    CFX_ByteString bs;
  } utf16le_encode_cases[] = {
      {L"", CFX_ByteString("\0\0", 2)},
      {L"abc", CFX_ByteString("a\0b\0c\0\0\0", 8)},
      {L"abcdef", CFX_ByteString("a\0b\0c\0d\0e\0f\0\0\0", 14)},
      {L"abc\0def", CFX_ByteString("a\0b\0c\0\0\0", 8)},
      {L"\xaabb\xccdd", CFX_ByteString("\xbb\xaa\xdd\xcc\0\0", 6)},
      {L"\x3132\x6162", CFX_ByteString("\x32\x31\x62\x61\0\0", 6)},
  };

  for (size_t i = 0; i < FX_ArraySize(utf16le_encode_cases); ++i) {
    EXPECT_EQ(utf16le_encode_cases[i].bs,
              utf16le_encode_cases[i].ws.UTF16LE_Encode())
        << " for case number " << i;
  }
}

TEST(fxcrt, WideStringCOperatorSubscript) {
  // CFX_WideStringC includes the NUL terminator for non-empty strings.
  CFX_WideStringC abc(L"abc");
  EXPECT_EQ(L'a', abc.CharAt(0));
  EXPECT_EQ(L'b', abc.CharAt(1));
  EXPECT_EQ(L'c', abc.CharAt(2));
  EXPECT_EQ(L'\0', abc.CharAt(3));
}

TEST(fxcrt, WideStringCOperatorLT) {
  CFX_WideStringC empty;
  CFX_WideStringC a(L"a");
  CFX_WideStringC abc(L"\x0110qq");  // Comes before despite endianness.
  CFX_WideStringC def(L"\x1001qq");  // Comes after despite endianness.

  EXPECT_FALSE(empty < empty);
  EXPECT_FALSE(a < a);
  EXPECT_FALSE(abc < abc);
  EXPECT_FALSE(def < def);

  EXPECT_TRUE(empty < a);
  EXPECT_FALSE(a < empty);

  EXPECT_TRUE(empty < abc);
  EXPECT_FALSE(abc < empty);

  EXPECT_TRUE(empty < def);
  EXPECT_FALSE(def < empty);

  EXPECT_TRUE(a < abc);
  EXPECT_FALSE(abc < a);

  EXPECT_TRUE(a < def);
  EXPECT_FALSE(def < a);

  EXPECT_TRUE(abc < def);
  EXPECT_FALSE(def < abc);
}

TEST(fxcrt, WideStringCOperatorEQ) {
  CFX_WideStringC wide_string_c(L"hello");
  EXPECT_TRUE(wide_string_c == wide_string_c);

  CFX_WideStringC wide_string_c_same1(L"hello");
  EXPECT_TRUE(wide_string_c == wide_string_c_same1);
  EXPECT_TRUE(wide_string_c_same1 == wide_string_c);

  CFX_WideStringC wide_string_c_same2(wide_string_c);
  EXPECT_TRUE(wide_string_c == wide_string_c_same2);
  EXPECT_TRUE(wide_string_c_same2 == wide_string_c);

  CFX_WideStringC wide_string_c1(L"he");
  CFX_WideStringC wide_string_c2(L"hellp");
  CFX_WideStringC wide_string_c3(L"hellod");
  EXPECT_FALSE(wide_string_c == wide_string_c1);
  EXPECT_FALSE(wide_string_c == wide_string_c2);
  EXPECT_FALSE(wide_string_c == wide_string_c3);
  EXPECT_FALSE(wide_string_c1 == wide_string_c);
  EXPECT_FALSE(wide_string_c2 == wide_string_c);
  EXPECT_FALSE(wide_string_c3 == wide_string_c);

  CFX_WideString wide_string_same1(L"hello");
  EXPECT_TRUE(wide_string_c == wide_string_same1);
  EXPECT_TRUE(wide_string_same1 == wide_string_c);

  CFX_WideString wide_string1(L"he");
  CFX_WideString wide_string2(L"hellp");
  CFX_WideString wide_string3(L"hellod");
  EXPECT_FALSE(wide_string_c == wide_string1);
  EXPECT_FALSE(wide_string_c == wide_string2);
  EXPECT_FALSE(wide_string_c == wide_string3);
  EXPECT_FALSE(wide_string1 == wide_string_c);
  EXPECT_FALSE(wide_string2 == wide_string_c);
  EXPECT_FALSE(wide_string3 == wide_string_c);

  const wchar_t* c_string_same1 = L"hello";
  EXPECT_TRUE(wide_string_c == c_string_same1);
  EXPECT_TRUE(c_string_same1 == wide_string_c);

  const wchar_t* c_string1 = L"he";
  const wchar_t* c_string2 = L"hellp";
  const wchar_t* c_string3 = L"hellod";
  EXPECT_FALSE(wide_string_c == c_string1);
  EXPECT_FALSE(wide_string_c == c_string2);
  EXPECT_FALSE(wide_string_c == c_string3);

  EXPECT_FALSE(c_string1 == wide_string_c);
  EXPECT_FALSE(c_string2 == wide_string_c);
  EXPECT_FALSE(c_string3 == wide_string_c);
}

TEST(fxcrt, WideStringCOperatorNE) {
  CFX_WideStringC wide_string_c(L"hello");
  EXPECT_FALSE(wide_string_c != wide_string_c);

  CFX_WideStringC wide_string_c_same1(L"hello");
  EXPECT_FALSE(wide_string_c != wide_string_c_same1);
  EXPECT_FALSE(wide_string_c_same1 != wide_string_c);

  CFX_WideStringC wide_string_c_same2(wide_string_c);
  EXPECT_FALSE(wide_string_c != wide_string_c_same2);
  EXPECT_FALSE(wide_string_c_same2 != wide_string_c);

  CFX_WideStringC wide_string_c1(L"he");
  CFX_WideStringC wide_string_c2(L"hellp");
  CFX_WideStringC wide_string_c3(L"hellod");
  EXPECT_TRUE(wide_string_c != wide_string_c1);
  EXPECT_TRUE(wide_string_c != wide_string_c2);
  EXPECT_TRUE(wide_string_c != wide_string_c3);
  EXPECT_TRUE(wide_string_c1 != wide_string_c);
  EXPECT_TRUE(wide_string_c2 != wide_string_c);
  EXPECT_TRUE(wide_string_c3 != wide_string_c);

  CFX_WideString wide_string_same1(L"hello");
  EXPECT_FALSE(wide_string_c != wide_string_same1);
  EXPECT_FALSE(wide_string_same1 != wide_string_c);

  CFX_WideString wide_string1(L"he");
  CFX_WideString wide_string2(L"hellp");
  CFX_WideString wide_string3(L"hellod");
  EXPECT_TRUE(wide_string_c != wide_string1);
  EXPECT_TRUE(wide_string_c != wide_string2);
  EXPECT_TRUE(wide_string_c != wide_string3);
  EXPECT_TRUE(wide_string1 != wide_string_c);
  EXPECT_TRUE(wide_string2 != wide_string_c);
  EXPECT_TRUE(wide_string3 != wide_string_c);

  const wchar_t* c_string_same1 = L"hello";
  EXPECT_FALSE(wide_string_c != c_string_same1);
  EXPECT_FALSE(c_string_same1 != wide_string_c);

  const wchar_t* c_string1 = L"he";
  const wchar_t* c_string2 = L"hellp";
  const wchar_t* c_string3 = L"hellod";
  EXPECT_TRUE(wide_string_c != c_string1);
  EXPECT_TRUE(wide_string_c != c_string2);
  EXPECT_TRUE(wide_string_c != c_string3);

  EXPECT_TRUE(c_string1 != wide_string_c);
  EXPECT_TRUE(c_string2 != wide_string_c);
  EXPECT_TRUE(c_string3 != wide_string_c);
}

TEST(fxcrt, WideStringCFind) {
  CFX_WideStringC null_string;
  EXPECT_EQ(-1, null_string.Find(L'a'));
  EXPECT_EQ(-1, null_string.Find(0));

  CFX_WideStringC empty_string(L"");
  EXPECT_EQ(-1, empty_string.Find(L'a'));
  EXPECT_EQ(-1, empty_string.Find(0));

  CFX_WideStringC single_string(L"a");
  EXPECT_EQ(0, single_string.Find(L'a'));
  EXPECT_EQ(-1, single_string.Find(L'b'));
  EXPECT_EQ(-1, single_string.Find(0));

  CFX_WideStringC longer_string(L"abccc");
  EXPECT_EQ(0, longer_string.Find(L'a'));
  EXPECT_EQ(2, longer_string.Find(L'c'));
  EXPECT_EQ(-1, longer_string.Find(0));

  CFX_WideStringC hibyte_string(
      L"ab\xff08"
      L"def");
  EXPECT_EQ(2, hibyte_string.Find(L'\xff08'));
}

TEST(fxcrt, WideStringFormatWidth) {
  {
    CFX_WideString str;
    str.Format(L"%5d", 1);
    EXPECT_EQ(L"    1", str);
  }

  {
    CFX_WideString str;
    str.Format(L"%d", 1);
    EXPECT_EQ(L"1", str);
  }

  {
    CFX_WideString str;
    str.Format(L"%*d", 5, 1);
    EXPECT_EQ(L"    1", str);
  }

  {
    CFX_WideString str;
    str.Format(L"%-1d", 1);
    EXPECT_EQ(L"1", str);
  }

  {
    CFX_WideString str;
    str.Format(L"%0d", 1);
    EXPECT_EQ(L"1", str);
  }

  {
    CFX_WideString str;
    str.Format(L"%1048576d", 1);
    EXPECT_EQ(L"Bad width", str);
  }
}

TEST(fxcrt, WideStringFormatPrecision) {
  {
    CFX_WideString str;
    str.Format(L"%.2f", 1.12345);
    EXPECT_EQ(L"1.12", str);
  }

  {
    CFX_WideString str;
    str.Format(L"%.*f", 3, 1.12345);
    EXPECT_EQ(L"1.123", str);
  }

  {
    CFX_WideString str;
    str.Format(L"%f", 1.12345);
    EXPECT_EQ(L"1.123450", str);
  }

  {
    CFX_WideString str;
    str.Format(L"%-1f", 1.12345);
    EXPECT_EQ(L"1.123450", str);
  }

  {
    CFX_WideString str;
    str.Format(L"%0f", 1.12345);
    EXPECT_EQ(L"1.123450", str);
  }

  {
    CFX_WideString str;
    str.Format(L"%.1048576f", 1.2);
    EXPECT_EQ(L"Bad precision", str);
  }
}

TEST(fxcrt, EmptyWideString) {
  CFX_WideString empty_str;
  EXPECT_TRUE(empty_str.IsEmpty());
  EXPECT_EQ(0, empty_str.GetLength());
  const FX_WCHAR* cstr = empty_str.c_str();
  EXPECT_EQ(0, FXSYS_wcslen(cstr));
}
