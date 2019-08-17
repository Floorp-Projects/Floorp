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
#include "nsTArray.h"
#include "gtest/gtest.h"
#include "gtest/MozGTestBench.h"  // For MOZ_GTEST_BENCH
#include "gtest/BlackBox.h"
#include "nsBidiUtils.h"

#define CONVERSION_ITERATIONS 50000

#define CONVERSION_BENCH(name, func, src, dstType)    \
  MOZ_GTEST_BENCH_F(Strings, name, [this] {           \
    for (int i = 0; i < CONVERSION_ITERATIONS; i++) { \
      dstType dst;                                    \
      func(*BlackBox(&src), *BlackBox(&dst));         \
    }                                                 \
  });

// Disable the C++ 2a warning. See bug #1509926
#if defined(__clang__) && (__clang_major__ >= 6)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wc++2a-compat"
#endif

namespace TestStrings {

using mozilla::BlackBox;
using mozilla::fallible;

#define TestExample1                                                           \
  "Sed ut perspiciatis unde omnis iste natus error sit voluptatem "            \
  "accusantium doloremque laudantium,\n totam rem aperiam, eaque ipsa quae "   \
  "ab illo inventore veritatis et quasi\r architecto beatae vitae dicta sunt " \
  "explicabo. Nemo enim ipsam voluptatem quia voluptas sit aspernatur\n aut "  \
  "odit aut fugit, sed quia consequuntur magni dolores eos qui ratione "       \
  "voluptatem sequi nesciunt. Neque porro quisquam est, qui\r\n\r dolorem "    \
  "ipsum quia dolor sit amet, consectetur, adipisci velit, sed quia non "      \
  "numquam eius modi tempora incidunt ut labore et dolore magnam aliquam "     \
  "quaerat voluptatem. Ut enim ad minima veniam, quis nostrum exercitationem " \
  "ullam corporis suscipit laboriosam, nisi ut aliquid ex ea commodi "         \
  "consequatur? Quis autem vel eum iure reprehenderit qui in ea voluptate "    \
  "velit esse quam nihil molestiae consequatur, vel illum qui dolorem eum "    \
  "fugiat quo voluptas nulla pariatur?"

#define TestExample2                                                           \
  "At vero eos et accusamus et iusto odio dignissimos ducimus\n\n qui "        \
  "blanditiis praesentium voluptatum deleniti atque corrupti quos dolores et " \
  "quas molestias excepturi sint occaecati cupiditate non provident, "         \
  "similique sunt in culpa qui officia deserunt\r\r  \n mollitia animi, id "   \
  "est laborum et dolorum fuga. Et harum quidem rerum facilis est et "         \
  "expedita distinctio. Nam libero tempore, cum soluta nobis est eligendi "    \
  "optio cumque nihil impedit quo minus id quod maxime placeat facere "        \
  "possimus, omnis voluptas assumenda       est, omnis dolor repellendus. "    \
  "Temporibus autem quibusdam et aut officiis debitis aut rerum "              \
  "necessitatibus saepe eveniet ut et voluptates repudiandae sint et "         \
  "molestiae non recusandae. Itaque earum rerum hic tenetur a sapiente "       \
  "delectus, ut aut reiciendis voluptatibus maiores alias consequatur aut "    \
  "perferendis doloribus asperiores repellat."

#define TestExample3                                                           \
  " Lorem ipsum dolor sit amet, consectetur adipiscing elit. Duis ac tellus "  \
  "eget velit viverra viverra id sit amet neque. Sed id consectetur mi, "      \
  "vestibulum aliquet arcu. Curabitur sagittis accumsan convallis. Sed eu "    \
  "condimentum ipsum, a laoreet tortor. Orci varius natoque penatibus et "     \
  "magnis dis    \r\r\n\n parturient montes, nascetur ridiculus mus. Sed non " \
  "tellus nec ante sodales placerat a nec risus. Cras vel bibendum sapien, "   \
  "nec ullamcorper felis. Pellentesque congue eget nisi sit amet vehicula. "   \
  "Morbi pulvinar turpis justo, in commodo dolor vulputate id. Curabitur in "  \
  "dui urna. Vestibulum placerat dui in sem congue, ut faucibus nibh rutrum. " \
  "Duis mattis turpis facilisis ullamcorper tincidunt. Vestibulum pharetra "   \
  "tortor at enim sagittis, dapibus consectetur ex blandit. Curabitur ac "     \
  "fringilla quam. In ornare lectus ut ipsum mattis venenatis. Etiam in "      \
  "mollis lectus, sed luctus risus.\nCras dapibus\f\t  \n finibus justo sit "  \
  "amet dictum. Aliquam non elit diam. Fusce magna nulla, bibendum in massa "  \
  "a, commodo finibus lectus. Sed rutrum a augue id imperdiet. Aliquam "       \
  "sagittis sodales felis, a tristique ligula. Aliquam erat volutpat. "        \
  "Pellentesque habitant morbi tristique senectus et netus et malesuada "      \
  "fames ac turpis egestas. Duis volutpat interdum lorem et congue. "          \
  "Phasellus porttitor posuere justo eget euismod. Nam a condimentum turpis, " \
  "sit amet gravida lacus. Vestibulum dolor diam, lobortis ac metus et, "      \
  "convallis dapibus tellus. Ut nec metus in velit malesuada tincidunt et "    \
  "eget justo. Curabitur ut libero bibendum, porttitor diam vitae, aliquet "   \
  "justo. "

#define TestExample4                                                           \
  " Donec feugiat volutpat massa. Cras ornare lacinia porta. Fusce in "        \
  "feugiat nunc. Praesent non felis varius diam feugiat ultrices ultricies a " \
  "risus. Donec maximus nisi nisl, non consectetur nulla eleifend in. Nulla "  \
  "in massa interdum, eleifend orci a, vestibulum est. Mauris aliquet, massa " \
  "et convallis mollis, felis augue vestibulum augue, in lobortis metus eros " \
  "a quam. Nam              ac diam ornare, vestibulum elit sit amet, "        \
  "consectetur ante. Praesent massa mauris, pulvinar sit amet sapien vel, "    \
  "tempus gravida neque. Praesent id quam sit amet est maximus molestie eget " \
  "at turpis. Nunc sit amet orci id arcu dapibus fermentum non eu "            \
  "erat.\f\tSuspendisse commodo nunc sem, eu congue eros condimentum vel. "    \
  "Nullam sit amet posuere arcu. Nulla facilisi. Mauris dapibus iaculis "      \
  "massa sed gravida. Nullam vitae urna at tortor feugiat auctor ut sit amet " \
  "dolor. Proin rutrum at nunc et faucibus. Quisque suscipit id nibh a "       \
  "aliquet. Pellentesque habitant morbi tristique senectus et netus et "       \
  "malesuada fames ac turpis egestas. Aliquam a dapibus erat, id imperdiet "   \
  "mauris. Nulla blandit libero non magna dapibus tristique. Integer "         \
  "hendrerit imperdiet lorem, quis facilisis lacus semper ut. Vestibulum "     \
  "ante ipsum primis in faucibus orci luctus et ultrices posuere cubilia "     \
  "Curae Nullam dignissim elit in congue ultricies. Quisque erat odio, "       \
  "maximus mollis laoreet id, iaculis at turpis. "

#define TestExample5                                                          \
  "Donec id risus urna. Nunc consequat lacinia urna id bibendum. Nulla "      \
  "faucibus faucibus enim. Cras ex risus, ultrices id semper vitae, luctus "  \
  "ut nulla. Sed vehicula tellus sed purus imperdiet efficitur. Suspendisse " \
  "feugiat\n\n\n     imperdiet odio, sed porta lorem feugiat nec. Curabitur " \
  "laoreet massa venenatis\r\n risus ornare\r\n, vitae feugiat tortor "       \
  "accumsan. Lorem ipsum dolor sit amet, consectetur adipiscing elit. "       \
  "Maecenas id scelerisque mauris, eget facilisis erat. Ut nec pulvinar "     \
  "risus, sed iaculis ante. Mauris tincidunt, risus et pretium elementum, "   \
  "leo nisi consectetur ligula, tincidunt suscipit erat velit eget libero. "  \
  "Sed ac est tempus, consequat dolor mattis, mattis mi. "

// Originally ReadVPXFile in TestVPXDecoding.cpp
static void ReadFile(const char* aPath, nsACString& aBuffer) {
  FILE* f = fopen(aPath, "rb");
  ASSERT_NE(f, (FILE*)nullptr);

  int r = fseek(f, 0, SEEK_END);
  ASSERT_EQ(r, 0);

  long size = ftell(f);
  ASSERT_NE(size, -1);
  aBuffer.SetLength(size);

  r = fseek(f, 0, SEEK_SET);
  ASSERT_EQ(r, 0);

  size_t got = fread(aBuffer.BeginWriting(), 1, size, f);
  ASSERT_EQ(got, size_t(size));

  r = fclose(f);
  ASSERT_EQ(r, 0);
}

class Strings : public ::testing::Test {
 protected:
  void SetUp() override {
    // Intentionally AssignASCII and not AssignLiteral
    // to simulate the usual heap case.
    mExample1Utf8.AssignASCII(TestExample1);
    mExample2Utf8.AssignASCII(TestExample2);
    mExample3Utf8.AssignASCII(TestExample3);
    mExample4Utf8.AssignASCII(TestExample4);
    mExample5Utf8.AssignASCII(TestExample5);

    // Use span to make the resulting string as ordinary as possible
    mAsciiOneUtf8.Append(MakeSpan(mExample3Utf8).To(1));
    mAsciiThreeUtf8.Append(MakeSpan(mExample3Utf8).To(3));
    mAsciiFifteenUtf8.Append(MakeSpan(mExample3Utf8).To(15));
    mAsciiHundredUtf8.Append(MakeSpan(mExample3Utf8).To(100));
    mAsciiThousandUtf8.Append(MakeSpan(mExample3Utf8).To(1000));

    ReadFile("ar.txt", mArUtf8);
    ReadFile("de.txt", mDeUtf8);
    ReadFile("de-edit.txt", mDeEditUtf8);
    ReadFile("ru.txt", mRuUtf8);
    ReadFile("th.txt", mThUtf8);
    ReadFile("ko.txt", mKoUtf8);
    ReadFile("ja.txt", mJaUtf8);
    ReadFile("tr.txt", mTrUtf8);
    ReadFile("vi.txt", mViUtf8);

    CopyASCIItoUTF16(mExample1Utf8, mExample1Utf16);
    CopyASCIItoUTF16(mExample2Utf8, mExample2Utf16);
    CopyASCIItoUTF16(mExample3Utf8, mExample3Utf16);
    CopyASCIItoUTF16(mExample4Utf8, mExample4Utf16);
    CopyASCIItoUTF16(mExample5Utf8, mExample5Utf16);

    CopyASCIItoUTF16(mAsciiOneUtf8, mAsciiOneUtf16);
    CopyASCIItoUTF16(mAsciiFifteenUtf8, mAsciiFifteenUtf16);
    CopyASCIItoUTF16(mAsciiHundredUtf8, mAsciiHundredUtf16);
    CopyASCIItoUTF16(mAsciiThousandUtf8, mAsciiThousandUtf16);

    CopyUTF8toUTF16(mArUtf8, mArUtf16);
    CopyUTF8toUTF16(mDeUtf8, mDeUtf16);
    CopyUTF8toUTF16(mDeEditUtf8, mDeEditUtf16);
    CopyUTF8toUTF16(mRuUtf8, mRuUtf16);
    CopyUTF8toUTF16(mThUtf8, mThUtf16);
    CopyUTF8toUTF16(mJaUtf8, mJaUtf16);
    CopyUTF8toUTF16(mKoUtf8, mKoUtf16);
    CopyUTF8toUTF16(mTrUtf8, mTrUtf16);
    CopyUTF8toUTF16(mViUtf8, mViUtf16);

    LossyCopyUTF16toASCII(mDeEditUtf16, mDeEditLatin1);

    // Use span to make the resulting string as ordinary as possible
    mArOneUtf16.Append(MakeSpan(mArUtf16).To(1));
    mDeOneUtf16.Append(MakeSpan(mDeUtf16).To(1));
    mDeEditOneUtf16.Append(MakeSpan(mDeEditUtf16).To(1));
    mRuOneUtf16.Append(MakeSpan(mRuUtf16).To(1));
    mThOneUtf16.Append(MakeSpan(mThUtf16).To(1));
    mJaOneUtf16.Append(MakeSpan(mJaUtf16).To(1));
    mKoOneUtf16.Append(MakeSpan(mKoUtf16).To(1));
    mTrOneUtf16.Append(MakeSpan(mTrUtf16).To(1));
    mViOneUtf16.Append(MakeSpan(mViUtf16).To(1));

    mDeEditOneLatin1.Append(MakeSpan(mDeEditLatin1).To(1));

    mArThreeUtf16.Append(MakeSpan(mArUtf16).To(3));
    mDeThreeUtf16.Append(MakeSpan(mDeUtf16).To(3));
    mDeEditThreeUtf16.Append(MakeSpan(mDeEditUtf16).To(3));
    mRuThreeUtf16.Append(MakeSpan(mRuUtf16).To(3));
    mThThreeUtf16.Append(MakeSpan(mThUtf16).To(3));
    mJaThreeUtf16.Append(MakeSpan(mJaUtf16).To(3));
    mKoThreeUtf16.Append(MakeSpan(mKoUtf16).To(3));
    mTrThreeUtf16.Append(MakeSpan(mTrUtf16).To(3));
    mViThreeUtf16.Append(MakeSpan(mViUtf16).To(3));

    mDeEditThreeLatin1.Append(MakeSpan(mDeEditLatin1).To(3));

    mArFifteenUtf16.Append(MakeSpan(mArUtf16).To(15));
    mDeFifteenUtf16.Append(MakeSpan(mDeUtf16).To(15));
    mDeEditFifteenUtf16.Append(MakeSpan(mDeEditUtf16).To(15));
    mRuFifteenUtf16.Append(MakeSpan(mRuUtf16).To(15));
    mThFifteenUtf16.Append(MakeSpan(mThUtf16).To(15));
    mJaFifteenUtf16.Append(MakeSpan(mJaUtf16).To(15));
    mKoFifteenUtf16.Append(MakeSpan(mKoUtf16).To(15));
    mTrFifteenUtf16.Append(MakeSpan(mTrUtf16).To(15));
    mViFifteenUtf16.Append(MakeSpan(mViUtf16).To(15));

    mDeEditFifteenLatin1.Append(MakeSpan(mDeEditLatin1).To(15));

    mArHundredUtf16.Append(MakeSpan(mArUtf16).To(100));
    mDeHundredUtf16.Append(MakeSpan(mDeUtf16).To(100));
    mDeEditHundredUtf16.Append(MakeSpan(mDeEditUtf16).To(100));
    mRuHundredUtf16.Append(MakeSpan(mRuUtf16).To(100));
    mThHundredUtf16.Append(MakeSpan(mThUtf16).To(100));
    mJaHundredUtf16.Append(MakeSpan(mJaUtf16).To(100));
    mKoHundredUtf16.Append(MakeSpan(mKoUtf16).To(100));
    mTrHundredUtf16.Append(MakeSpan(mTrUtf16).To(100));
    mViHundredUtf16.Append(MakeSpan(mViUtf16).To(100));

    mDeEditHundredLatin1.Append(MakeSpan(mDeEditLatin1).To(100));

    mArThousandUtf16.Append(MakeSpan(mArUtf16).To(1000));
    mDeThousandUtf16.Append(MakeSpan(mDeUtf16).To(1000));
    mDeEditThousandUtf16.Append(MakeSpan(mDeEditUtf16).To(1000));
    mRuThousandUtf16.Append(MakeSpan(mRuUtf16).To(1000));
    mThThousandUtf16.Append(MakeSpan(mThUtf16).To(1000));
    mJaThousandUtf16.Append(MakeSpan(mJaUtf16).To(1000));
    mKoThousandUtf16.Append(MakeSpan(mKoUtf16).To(1000));
    mTrThousandUtf16.Append(MakeSpan(mTrUtf16).To(1000));
    mViThousandUtf16.Append(MakeSpan(mViUtf16).To(1000));

    mDeEditThousandLatin1.Append(MakeSpan(mDeEditLatin1).To(1000));

    CopyUTF16toUTF8(mArOneUtf16, mArOneUtf8);
    CopyUTF16toUTF8(mDeOneUtf16, mDeOneUtf8);
    CopyUTF16toUTF8(mDeEditOneUtf16, mDeEditOneUtf8);
    CopyUTF16toUTF8(mRuOneUtf16, mRuOneUtf8);
    CopyUTF16toUTF8(mThOneUtf16, mThOneUtf8);
    CopyUTF16toUTF8(mJaOneUtf16, mJaOneUtf8);
    CopyUTF16toUTF8(mKoOneUtf16, mKoOneUtf8);
    CopyUTF16toUTF8(mTrOneUtf16, mTrOneUtf8);
    CopyUTF16toUTF8(mViOneUtf16, mViOneUtf8);

    CopyUTF16toUTF8(mArThreeUtf16, mArThreeUtf8);
    CopyUTF16toUTF8(mDeThreeUtf16, mDeThreeUtf8);
    CopyUTF16toUTF8(mDeEditThreeUtf16, mDeEditThreeUtf8);
    CopyUTF16toUTF8(mRuThreeUtf16, mRuThreeUtf8);
    CopyUTF16toUTF8(mThThreeUtf16, mThThreeUtf8);
    CopyUTF16toUTF8(mJaThreeUtf16, mJaThreeUtf8);
    CopyUTF16toUTF8(mKoThreeUtf16, mKoThreeUtf8);
    CopyUTF16toUTF8(mTrThreeUtf16, mTrThreeUtf8);
    CopyUTF16toUTF8(mViThreeUtf16, mViThreeUtf8);

    CopyUTF16toUTF8(mArFifteenUtf16, mArFifteenUtf8);
    CopyUTF16toUTF8(mDeFifteenUtf16, mDeFifteenUtf8);
    CopyUTF16toUTF8(mDeEditFifteenUtf16, mDeEditFifteenUtf8);
    CopyUTF16toUTF8(mRuFifteenUtf16, mRuFifteenUtf8);
    CopyUTF16toUTF8(mThFifteenUtf16, mThFifteenUtf8);
    CopyUTF16toUTF8(mJaFifteenUtf16, mJaFifteenUtf8);
    CopyUTF16toUTF8(mKoFifteenUtf16, mKoFifteenUtf8);
    CopyUTF16toUTF8(mTrFifteenUtf16, mTrFifteenUtf8);
    CopyUTF16toUTF8(mViFifteenUtf16, mViFifteenUtf8);

    CopyUTF16toUTF8(mArHundredUtf16, mArHundredUtf8);
    CopyUTF16toUTF8(mDeHundredUtf16, mDeHundredUtf8);
    CopyUTF16toUTF8(mDeEditHundredUtf16, mDeEditHundredUtf8);
    CopyUTF16toUTF8(mRuHundredUtf16, mRuHundredUtf8);
    CopyUTF16toUTF8(mThHundredUtf16, mThHundredUtf8);
    CopyUTF16toUTF8(mJaHundredUtf16, mJaHundredUtf8);
    CopyUTF16toUTF8(mKoHundredUtf16, mKoHundredUtf8);
    CopyUTF16toUTF8(mTrHundredUtf16, mTrHundredUtf8);
    CopyUTF16toUTF8(mViHundredUtf16, mViHundredUtf8);

    CopyUTF16toUTF8(mArThousandUtf16, mArThousandUtf8);
    CopyUTF16toUTF8(mDeThousandUtf16, mDeThousandUtf8);
    CopyUTF16toUTF8(mDeEditThousandUtf16, mDeEditThousandUtf8);
    CopyUTF16toUTF8(mRuThousandUtf16, mRuThousandUtf8);
    CopyUTF16toUTF8(mThThousandUtf16, mThThousandUtf8);
    CopyUTF16toUTF8(mJaThousandUtf16, mJaThousandUtf8);
    CopyUTF16toUTF8(mKoThousandUtf16, mKoThousandUtf8);
    CopyUTF16toUTF8(mTrThousandUtf16, mTrThousandUtf8);
    CopyUTF16toUTF8(mViThousandUtf16, mViThousandUtf8);
  }

 public:
  nsCString mAsciiOneUtf8;
  nsCString mAsciiThreeUtf8;
  nsCString mAsciiFifteenUtf8;
  nsCString mAsciiHundredUtf8;
  nsCString mAsciiThousandUtf8;
  nsCString mExample1Utf8;
  nsCString mExample2Utf8;
  nsCString mExample3Utf8;
  nsCString mExample4Utf8;
  nsCString mExample5Utf8;
  nsCString mArUtf8;
  nsCString mDeUtf8;
  nsCString mDeEditUtf8;
  nsCString mRuUtf8;
  nsCString mThUtf8;
  nsCString mJaUtf8;
  nsCString mKoUtf8;
  nsCString mTrUtf8;
  nsCString mViUtf8;

  nsString mAsciiOneUtf16;
  nsString mAsciiThreeUtf16;
  nsString mAsciiFifteenUtf16;
  nsString mAsciiHundredUtf16;
  nsString mAsciiThousandUtf16;
  nsString mExample1Utf16;
  nsString mExample2Utf16;
  nsString mExample3Utf16;
  nsString mExample4Utf16;
  nsString mExample5Utf16;
  nsString mArUtf16;
  nsString mDeUtf16;
  nsString mDeEditUtf16;
  nsString mRuUtf16;
  nsString mThUtf16;
  nsString mJaUtf16;
  nsString mKoUtf16;
  nsString mTrUtf16;
  nsString mViUtf16;

  nsCString mDeEditLatin1;

  nsString mArOneUtf16;
  nsString mDeOneUtf16;
  nsString mDeEditOneUtf16;
  nsString mRuOneUtf16;
  nsString mThOneUtf16;
  nsString mJaOneUtf16;
  nsString mKoOneUtf16;
  nsString mTrOneUtf16;
  nsString mViOneUtf16;

  nsCString mDeEditOneLatin1;

  nsCString mArOneUtf8;
  nsCString mDeOneUtf8;
  nsCString mDeEditOneUtf8;
  nsCString mRuOneUtf8;
  nsCString mThOneUtf8;
  nsCString mJaOneUtf8;
  nsCString mKoOneUtf8;
  nsCString mTrOneUtf8;
  nsCString mViOneUtf8;

  nsString mArThreeUtf16;
  nsString mDeThreeUtf16;
  nsString mDeEditThreeUtf16;
  nsString mRuThreeUtf16;
  nsString mThThreeUtf16;
  nsString mJaThreeUtf16;
  nsString mKoThreeUtf16;
  nsString mTrThreeUtf16;
  nsString mViThreeUtf16;

  nsCString mDeEditThreeLatin1;

  nsCString mArThreeUtf8;
  nsCString mDeThreeUtf8;
  nsCString mDeEditThreeUtf8;
  nsCString mRuThreeUtf8;
  nsCString mThThreeUtf8;
  nsCString mJaThreeUtf8;
  nsCString mKoThreeUtf8;
  nsCString mTrThreeUtf8;
  nsCString mViThreeUtf8;

  nsString mArFifteenUtf16;
  nsString mDeFifteenUtf16;
  nsString mDeEditFifteenUtf16;
  nsString mRuFifteenUtf16;
  nsString mThFifteenUtf16;
  nsString mJaFifteenUtf16;
  nsString mKoFifteenUtf16;
  nsString mTrFifteenUtf16;
  nsString mViFifteenUtf16;

  nsCString mDeEditFifteenLatin1;

  nsCString mArFifteenUtf8;
  nsCString mDeFifteenUtf8;
  nsCString mDeEditFifteenUtf8;
  nsCString mRuFifteenUtf8;
  nsCString mThFifteenUtf8;
  nsCString mJaFifteenUtf8;
  nsCString mKoFifteenUtf8;
  nsCString mTrFifteenUtf8;
  nsCString mViFifteenUtf8;

  nsString mArHundredUtf16;
  nsString mDeHundredUtf16;
  nsString mDeEditHundredUtf16;
  nsString mRuHundredUtf16;
  nsString mThHundredUtf16;
  nsString mJaHundredUtf16;
  nsString mKoHundredUtf16;
  nsString mTrHundredUtf16;
  nsString mViHundredUtf16;

  nsCString mDeEditHundredLatin1;

  nsCString mArHundredUtf8;
  nsCString mDeHundredUtf8;
  nsCString mDeEditHundredUtf8;
  nsCString mRuHundredUtf8;
  nsCString mThHundredUtf8;
  nsCString mJaHundredUtf8;
  nsCString mKoHundredUtf8;
  nsCString mTrHundredUtf8;
  nsCString mViHundredUtf8;

  nsString mArThousandUtf16;
  nsString mDeThousandUtf16;
  nsString mDeEditThousandUtf16;
  nsString mRuThousandUtf16;
  nsString mThThousandUtf16;
  nsString mJaThousandUtf16;
  nsString mKoThousandUtf16;
  nsString mTrThousandUtf16;
  nsString mViThousandUtf16;

  nsCString mDeEditThousandLatin1;

  nsCString mArThousandUtf8;
  nsCString mDeThousandUtf8;
  nsCString mDeEditThousandUtf8;
  nsCString mRuThousandUtf8;
  nsCString mThThousandUtf8;
  nsCString mJaThousandUtf8;
  nsCString mKoThousandUtf8;
  nsCString mTrThousandUtf8;
  nsCString mViThousandUtf8;
};

static void test_assign_helper(const nsACString& in, nsACString& _retval) {
  _retval = in;
}

// Simple helper struct to test if conditionally enabled string functions are
// working.
template <typename T>
struct EnableTest {
  template <typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
  bool IsChar16() {
    return true;
  }

  template <typename Q = T, typename EnableIfChar = mozilla::CharOnlyT<Q>>
  bool IsChar16(int dummy = 42) {
    return false;
  }

  template <typename Q = T, typename EnableIfChar16 = mozilla::Char16OnlyT<Q>>
  bool IsChar() {
    return false;
  }

  template <typename Q = T, typename EnableIfChar = mozilla::CharOnlyT<Q>>
  bool IsChar(int dummy = 42) {
    return true;
  }
};

TEST_F(Strings, IsChar) {
  EnableTest<char> charTest;
  EXPECT_TRUE(charTest.IsChar());
  EXPECT_FALSE(charTest.IsChar16());

  EnableTest<char16_t> char16Test;
  EXPECT_TRUE(char16Test.IsChar16());
  EXPECT_FALSE(char16Test.IsChar());

#ifdef COMPILATION_FAILURE_TEST
  nsAutoCString a_ctest;
  nsAutoString a_test;

  a_ctest.AssignLiteral("hello");
  // This should cause a compilation failure.
  a_ctest.AssignLiteral(u"hello");
  a_test.AssignLiteral(u"hello");
  a_test.AssignLiteral("hello");
#endif
}

TEST_F(Strings, DependentStrings) {
  // A few tests that make sure copying nsTDependentStrings behaves properly.
  using DataFlags = mozilla::detail::StringDataFlags;

  {
    // Test copy ctor.
    nsDependentCString tmp("foo");
    auto data = tmp.Data();
    nsDependentCString foo(tmp);
    // Neither string should be using a shared buffer.
    EXPECT_FALSE(tmp.GetDataFlags() & DataFlags::REFCOUNTED);
    EXPECT_FALSE(foo.GetDataFlags() & DataFlags::REFCOUNTED);
    // Both strings should be pointing to the original buffer.
    EXPECT_EQ(data, tmp.Data());
    EXPECT_EQ(data, foo.Data());
  }
  {
    // Test move ctor.
    nsDependentCString tmp("foo");
    auto data = tmp.Data();
    nsDependentCString foo(std::move(tmp));
    // Neither string should be using a shared buffer.
    EXPECT_FALSE(tmp.GetDataFlags() & DataFlags::REFCOUNTED);
    EXPECT_FALSE(foo.GetDataFlags() & DataFlags::REFCOUNTED);
    // First string should be reset, the second should be pointing to the
    // original buffer.
    EXPECT_NE(data, tmp.Data());
    EXPECT_EQ(data, foo.Data());
    EXPECT_TRUE(tmp.IsEmpty());
  }
  {
    // Test copying to a nsCString.
    nsDependentCString tmp("foo");
    auto data = tmp.Data();
    nsCString foo(tmp);
    // Original string should not be shared, copy should be shared.
    EXPECT_FALSE(tmp.GetDataFlags() & DataFlags::REFCOUNTED);
    EXPECT_TRUE(foo.GetDataFlags() & DataFlags::REFCOUNTED);
    // First string should remain the same, the second should be pointing to
    // a new buffer.
    EXPECT_EQ(data, tmp.Data());
    EXPECT_NE(data, foo.Data());
  }
}

TEST_F(Strings, assign) {
  nsCString result;
  test_assign_helper(NS_LITERAL_CSTRING("a") + NS_LITERAL_CSTRING("b"), result);
  EXPECT_STREQ(result.get(), "ab");
}

TEST_F(Strings, assign_c) {
  nsCString c;
  c.Assign('c');
  EXPECT_STREQ(c.get(), "c");
}

TEST_F(Strings, test1) {
  NS_NAMED_LITERAL_STRING(empty, "");
  const nsAString& aStr = empty;

  nsAutoString buf(aStr);

  int32_t n = buf.FindChar(',');
  EXPECT_EQ(n, kNotFound);

  n = buf.Length();

  buf.Cut(0, n + 1);
  n = buf.FindChar(',');

  EXPECT_EQ(n, kNotFound);
}

TEST_F(Strings, test2) {
  nsCString data("hello world");
  const nsACString& aStr = data;

  nsCString temp(aStr);
  temp.Cut(0, 6);

  EXPECT_STREQ(temp.get(), "world");
}

TEST_F(Strings, find) {
  nsCString src("<!DOCTYPE blah blah blah>");

  int32_t i = src.Find("DOCTYPE", true, 2, 1);
  EXPECT_EQ(i, 2);
}

TEST_F(Strings, rfind) {
  const char text[] = "<!DOCTYPE blah blah blah>";
  const char term[] = "bLaH";
  nsCString src(text);
  int32_t i;

  i = src.RFind(term, true, 3, -1);
  EXPECT_EQ(i, kNotFound);

  i = src.RFind(term, true, -1, -1);
  EXPECT_EQ(i, 20);

  i = src.RFind(term, true, 13, -1);
  EXPECT_EQ(i, 10);

  i = src.RFind(term, true, 22, 3);
  EXPECT_EQ(i, 20);
}

TEST_F(Strings, rfind_2) {
  const char text[] = "<!DOCTYPE blah blah blah>";
  nsCString src(text);
  int32_t i = src.RFind("TYPE", false, 5, -1);
  EXPECT_EQ(i, 5);
}

TEST_F(Strings, rfind_3) {
  const char text[] = "urn:mozilla:locale:en-US:necko";
  nsAutoCString value(text);
  int32_t i = value.RFind(":");
  EXPECT_EQ(i, 24);
}

TEST_F(Strings, rfind_4) {
  nsCString value("a.msf");
  int32_t i = value.RFind(".msf");
  EXPECT_EQ(i, 1);
}

TEST_F(Strings, findinreadable) {
  const char text[] =
      "jar:jar:file:///c:/software/mozilla/mozilla_2006_02_21.jar!/browser/"
      "chrome/classic.jar!/";
  nsAutoCString value(text);

  nsACString::const_iterator begin, end;
  value.BeginReading(begin);
  value.EndReading(end);
  nsACString::const_iterator delim_begin(begin), delim_end(end);

  // Search for last !/ at the end of the string
  EXPECT_TRUE(FindInReadable(NS_LITERAL_CSTRING("!/"), delim_begin, delim_end));
  char* r = ToNewCString(Substring(delim_begin, delim_end));
  // Should match the first "!/" but not the last
  EXPECT_NE(delim_end, end);
  EXPECT_STREQ(r, "!/");
  free(r);

  delim_begin = begin;
  delim_end = end;

  // Search for first jar:
  EXPECT_TRUE(
      FindInReadable(NS_LITERAL_CSTRING("jar:"), delim_begin, delim_end));

  r = ToNewCString(Substring(delim_begin, delim_end));
  // Should not match the first jar:, but the second one
  EXPECT_EQ(delim_begin, begin);
  EXPECT_STREQ(r, "jar:");
  free(r);

  // Search for jar: in a Substring
  delim_begin = begin;
  delim_begin++;
  delim_end = end;
  EXPECT_TRUE(
      FindInReadable(NS_LITERAL_CSTRING("jar:"), delim_begin, delim_end));

  r = ToNewCString(Substring(delim_begin, delim_end));
  // Should not match the first jar:, but the second one
  EXPECT_NE(delim_begin, begin);
  EXPECT_STREQ(r, "jar:");
  free(r);

  // Should not find a match
  EXPECT_FALSE(
      FindInReadable(NS_LITERAL_CSTRING("gecko"), delim_begin, delim_end));

  // When no match is found, range should be empty
  EXPECT_EQ(delim_begin, delim_end);

  // Should not find a match (search not beyond Substring)
  delim_begin = begin;
  for (int i = 0; i < 6; i++) delim_begin++;
  delim_end = end;
  EXPECT_FALSE(
      FindInReadable(NS_LITERAL_CSTRING("jar:"), delim_begin, delim_end));

  // When no match is found, range should be empty
  EXPECT_EQ(delim_begin, delim_end);

  // Should not find a match (search not beyond Substring)
  delim_begin = begin;
  delim_end = end;
  for (int i = 0; i < 7; i++) delim_end--;
  EXPECT_FALSE(
      FindInReadable(NS_LITERAL_CSTRING("classic"), delim_begin, delim_end));

  // When no match is found, range should be empty
  EXPECT_EQ(delim_begin, delim_end);
}

TEST_F(Strings, rfindinreadable) {
  const char text[] =
      "jar:jar:file:///c:/software/mozilla/mozilla_2006_02_21.jar!/browser/"
      "chrome/classic.jar!/";
  nsAutoCString value(text);

  nsACString::const_iterator begin, end;
  value.BeginReading(begin);
  value.EndReading(end);
  nsACString::const_iterator delim_begin(begin), delim_end(end);

  // Search for last !/ at the end of the string
  EXPECT_TRUE(
      RFindInReadable(NS_LITERAL_CSTRING("!/"), delim_begin, delim_end));
  char* r = ToNewCString(Substring(delim_begin, delim_end));
  // Should match the last "!/"
  EXPECT_EQ(delim_end, end);
  EXPECT_STREQ(r, "!/");
  free(r);

  delim_begin = begin;
  delim_end = end;

  // Search for last jar: but not the first one...
  EXPECT_TRUE(
      RFindInReadable(NS_LITERAL_CSTRING("jar:"), delim_begin, delim_end));

  r = ToNewCString(Substring(delim_begin, delim_end));
  // Should not match the first jar:, but the second one
  EXPECT_NE(delim_begin, begin);
  EXPECT_STREQ(r, "jar:");
  free(r);

  // Search for jar: in a Substring
  delim_begin = begin;
  delim_end = begin;
  for (int i = 0; i < 6; i++) delim_end++;
  EXPECT_TRUE(
      RFindInReadable(NS_LITERAL_CSTRING("jar:"), delim_begin, delim_end));

  r = ToNewCString(Substring(delim_begin, delim_end));
  // Should not match the first jar:, but the second one
  EXPECT_EQ(delim_begin, begin);
  EXPECT_STREQ(r, "jar:");
  free(r);

  // Should not find a match
  delim_begin = begin;
  delim_end = end;
  EXPECT_FALSE(
      RFindInReadable(NS_LITERAL_CSTRING("gecko"), delim_begin, delim_end));

  // When no match is found, range should be empty
  EXPECT_EQ(delim_begin, delim_end);

  // Should not find a match (search not before Substring)
  delim_begin = begin;
  for (int i = 0; i < 6; i++) delim_begin++;
  delim_end = end;
  EXPECT_FALSE(
      RFindInReadable(NS_LITERAL_CSTRING("jar:"), delim_begin, delim_end));

  // When no match is found, range should be empty
  EXPECT_EQ(delim_begin, delim_end);

  // Should not find a match (search not beyond Substring)
  delim_begin = begin;
  delim_end = end;
  for (int i = 0; i < 7; i++) delim_end--;
  EXPECT_FALSE(
      RFindInReadable(NS_LITERAL_CSTRING("classic"), delim_begin, delim_end));

  // When no match is found, range should be empty
  EXPECT_EQ(delim_begin, delim_end);
}

TEST_F(Strings, distance) {
  const char text[] = "abc-xyz";
  nsCString s(text);
  nsCString::const_iterator begin, end;
  s.BeginReading(begin);
  s.EndReading(end);
  size_t d = Distance(begin, end);
  EXPECT_EQ(d, sizeof(text) - 1);
}

TEST_F(Strings, length) {
  const char text[] = "abc-xyz";
  nsCString s(text);
  size_t d = s.Length();
  EXPECT_EQ(d, sizeof(text) - 1);
}

TEST_F(Strings, trim) {
  const char text[] = " a\t    $   ";
  const char set[] = " \t$";

  nsCString s(text);
  s.Trim(set);
  EXPECT_STREQ(s.get(), "a");

  s.AssignLiteral("\t  \t\t  \t");
  s.Trim(set);
  EXPECT_STREQ(s.get(), "");

  s.AssignLiteral(" ");
  s.Trim(set);
  EXPECT_STREQ(s.get(), "");

  s.AssignLiteral(" ");
  s.Trim(set, false, true);
  EXPECT_STREQ(s.get(), "");

  s.AssignLiteral(" ");
  s.Trim(set, true, false);
  EXPECT_STREQ(s.get(), "");
}

TEST_F(Strings, replace_substr) {
  const char text[] = "abc-ppp-qqq-ppp-xyz";
  nsCString s(text);
  s.ReplaceSubstring("ppp", "www");
  EXPECT_STREQ(s.get(), "abc-www-qqq-www-xyz");

  s.AssignLiteral("foobar");
  s.ReplaceSubstring("foo", "bar");
  s.ReplaceSubstring("bar", "");
  EXPECT_STREQ(s.get(), "");

  s.AssignLiteral("foofoofoo");
  s.ReplaceSubstring("foo", "foo");
  EXPECT_STREQ(s.get(), "foofoofoo");

  s.AssignLiteral("foofoofoo");
  s.ReplaceSubstring("of", "fo");
  EXPECT_STREQ(s.get(), "fofoofooo");
}

TEST_F(Strings, replace_substr_2) {
  const char* newName = "user";
  nsString acctName;
  acctName.AssignLiteral("forums.foo.com");
  nsAutoString newAcctName, oldVal, newVal;
  CopyASCIItoUTF16(mozilla::MakeStringSpan(newName), newVal);
  newAcctName.Assign(acctName);

  // here, oldVal is empty.  we are testing that this function
  // does not hang.  see bug 235355.
  newAcctName.ReplaceSubstring(oldVal, newVal);

  // we expect that newAcctName will be unchanged.
  EXPECT_TRUE(newAcctName.Equals(acctName));
}

TEST_F(Strings, replace_substr_3) {
  nsCString s;
  s.AssignLiteral("abcabcabc");
  s.ReplaceSubstring("ca", "X");
  EXPECT_STREQ(s.get(), "abXbXbc");

  s.AssignLiteral("abcabcabc");
  s.ReplaceSubstring("ca", "XYZ");
  EXPECT_STREQ(s.get(), "abXYZbXYZbc");

  s.AssignLiteral("abcabcabc");
  s.ReplaceSubstring("ca", "XY");
  EXPECT_STREQ(s.get(), "abXYbXYbc");

  s.AssignLiteral("abcabcabc");
  s.ReplaceSubstring("ca", "XYZ!");
  EXPECT_STREQ(s.get(), "abXYZ!bXYZ!bc");

  s.AssignLiteral("abcdabcdabcd");
  s.ReplaceSubstring("bcd", "X");
  EXPECT_STREQ(s.get(), "aXaXaX");

  s.AssignLiteral("abcdabcdabcd");
  s.ReplaceSubstring("bcd", "XYZ!");
  EXPECT_STREQ(s.get(), "aXYZ!aXYZ!aXYZ!");

  s.AssignLiteral("abcdabcdabcd");
  s.ReplaceSubstring("bcd", "XY");
  EXPECT_STREQ(s.get(), "aXYaXYaXY");

  s.AssignLiteral("abcdabcdabcd");
  s.ReplaceSubstring("bcd", "XYZABC");
  EXPECT_STREQ(s.get(), "aXYZABCaXYZABCaXYZABC");

  s.AssignLiteral("abcdabcdabcd");
  s.ReplaceSubstring("bcd", "XYZ");
  EXPECT_STREQ(s.get(), "aXYZaXYZaXYZ");

  s.AssignLiteral("abcdabcdabcd");
  s.ReplaceSubstring("bcd", "XYZ!");
  EXPECT_STREQ(s.get(), "aXYZ!aXYZ!aXYZ!");

  s.AssignLiteral("abcdabcdabcd");
  s.ReplaceSubstring("ab", "X");
  EXPECT_STREQ(s.get(), "XcdXcdXcd");

  s.AssignLiteral("abcdabcdabcd");
  s.ReplaceSubstring("ab", "XYZABC");
  EXPECT_STREQ(s.get(), "XYZABCcdXYZABCcdXYZABCcd");

  s.AssignLiteral("abcdabcdabcd");
  s.ReplaceSubstring("ab", "XY");
  EXPECT_STREQ(s.get(), "XYcdXYcdXYcd");

  s.AssignLiteral("abcdabcdabcd");
  s.ReplaceSubstring("ab", "XYZ!");
  EXPECT_STREQ(s.get(), "XYZ!cdXYZ!cdXYZ!cd");

  s.AssignLiteral("abcdabcdabcd");
  s.ReplaceSubstring("notfound", "X");
  EXPECT_STREQ(s.get(), "abcdabcdabcd");

  s.AssignLiteral("abcdabcdabcd");
  s.ReplaceSubstring("notfound", "longlongstring");
  EXPECT_STREQ(s.get(), "abcdabcdabcd");
}

TEST_F(Strings, strip_ws) {
  const char* texts[] = {"", " a    $   ", "Some\fother\t thing\r\n",
                         "And   \f\t\r\n even\nmore\r \f"};
  const char* results[] = {"", "a$", "Someotherthing", "Andevenmore"};
  for (size_t i = 0; i < sizeof(texts) / sizeof(texts[0]); i++) {
    nsCString s(texts[i]);
    s.StripWhitespace();
    EXPECT_STREQ(s.get(), results[i]);
  }
}

TEST_F(Strings, equals_ic) {
  nsCString s;
  EXPECT_FALSE(s.LowerCaseEqualsLiteral("view-source"));
}

TEST_F(Strings, concat) {
  nsCString bar("bar");
  const nsACString& barRef = bar;

  const nsPromiseFlatCString& result = PromiseFlatCString(
      NS_LITERAL_CSTRING("foo") + NS_LITERAL_CSTRING(",") + barRef);
  EXPECT_STREQ(result.get(), "foo,bar");
}

TEST_F(Strings, concat_2) {
  nsCString fieldTextStr("xyz");
  nsCString text("text");
  const nsACString& aText = text;

  nsAutoCString result(fieldTextStr + aText);

  EXPECT_STREQ(result.get(), "xyztext");
}

TEST_F(Strings, concat_3) {
  nsCString result;
  nsCString ab("ab"), c("c");

  result = ab + result + c;
  EXPECT_STREQ(result.get(), "abc");
}

TEST_F(Strings, empty_assign) {
  nsCString a;
  a.AssignLiteral("");

  a.AppendLiteral("");

  nsCString b;
  b.SetCapacity(0);
}

TEST_F(Strings, set_length) {
  const char kText[] = "Default Plugin";
  nsCString buf;
  buf.SetCapacity(sizeof(kText) - 1);
  buf.Assign(kText);
  buf.SetLength(sizeof(kText) - 1);
  EXPECT_STREQ(buf.get(), kText);
}

TEST_F(Strings, substring) {
  nsCString super("hello world"), sub("hello");

  // this tests that |super| starts with |sub|,

  EXPECT_TRUE(sub.Equals(StringHead(super, sub.Length())));

  // and verifies that |sub| does not start with |super|.

  EXPECT_FALSE(super.Equals(StringHead(sub, super.Length())));
}

#define test_append_expect(str, int, suffix, expect) \
  str.Truncate();                                    \
  str.AppendInt(suffix = int);                       \
  EXPECT_TRUE(str.EqualsLiteral(expect));

#define test_appends_expect(int, suffix, expect) \
  test_append_expect(str, int, suffix, expect)   \
      test_append_expect(cstr, int, suffix, expect)

#define test_appendbase(str, prefix, int, suffix, base) \
  str.Truncate();                                       \
  str.AppendInt(suffix = prefix##int##suffix, base);    \
  EXPECT_TRUE(str.EqualsLiteral(#int));

#define test_appendbases(prefix, int, suffix, base) \
  test_appendbase(str, prefix, int, suffix, base)   \
      test_appendbase(cstr, prefix, int, suffix, base)

TEST_F(Strings, appendint) {
  nsString str;
  nsCString cstr;
  int32_t L;
  uint32_t UL;
  int64_t LL;
  uint64_t ULL;
  test_appends_expect(INT32_MAX, L, "2147483647");
  test_appends_expect(INT32_MIN, L, "-2147483648");
  test_appends_expect(UINT32_MAX, UL, "4294967295");
  test_appends_expect(INT64_MAX, LL, "9223372036854775807");
  test_appends_expect(INT64_MIN, LL, "-9223372036854775808");
  test_appends_expect(UINT64_MAX, ULL, "18446744073709551615");
  test_appendbases(0, 17777777777, L, 8);
  test_appendbases(0, 20000000000, L, 8);
  test_appendbases(0, 37777777777, UL, 8);
  test_appendbases(0, 777777777777777777777, LL, 8);
  test_appendbases(0, 1000000000000000000000, LL, 8);
  test_appendbases(0, 1777777777777777777777, ULL, 8);
  test_appendbases(0x, 7fffffff, L, 16);
  test_appendbases(0x, 80000000, L, 16);
  test_appendbases(0x, ffffffff, UL, 16);
  test_appendbases(0x, 7fffffffffffffff, LL, 16);
  test_appendbases(0x, 8000000000000000, LL, 16);
  test_appendbases(0x, ffffffffffffffff, ULL, 16);
}

TEST_F(Strings, appendint64) {
  nsCString str;

  int64_t max = INT64_MAX;
  static const char max_expected[] = "9223372036854775807";
  int64_t min = INT64_MIN;
  static const char min_expected[] = "-9223372036854775808";
  static const char min_expected_oct[] = "1000000000000000000000";
  int64_t maxint_plus1 = 1LL << 32;
  static const char maxint_plus1_expected[] = "4294967296";
  static const char maxint_plus1_expected_x[] = "100000000";

  str.AppendInt(max);

  EXPECT_TRUE(str.Equals(max_expected));

  str.Truncate();
  str.AppendInt(min);
  EXPECT_TRUE(str.Equals(min_expected));
  str.Truncate();
  str.AppendInt(min, 8);
  EXPECT_TRUE(str.Equals(min_expected_oct));

  str.Truncate();
  str.AppendInt(maxint_plus1);
  EXPECT_TRUE(str.Equals(maxint_plus1_expected));
  str.Truncate();
  str.AppendInt(maxint_plus1, 16);
  EXPECT_TRUE(str.Equals(maxint_plus1_expected_x));
}

TEST_F(Strings, appendfloat) {
  nsCString str;
  double bigdouble = 11223344556.66;
  static const char double_expected[] = "11223344556.66";
  static const char float_expected[] = "0.01";

  // AppendFloat is used to append doubles, therefore the precision must be
  // large enough (see bug 327719)
  str.AppendFloat(bigdouble);
  EXPECT_TRUE(str.Equals(double_expected));

  str.Truncate();
  // AppendFloat is used to append floats (bug 327719 #27)
  str.AppendFloat(0.1f * 0.1f);
  EXPECT_TRUE(str.Equals(float_expected));
}

TEST_F(Strings, findcharinset) {
  nsCString buf("hello, how are you?");

  int32_t index = buf.FindCharInSet(",?", 5);
  EXPECT_EQ(index, 5);

  index = buf.FindCharInSet("helo", 0);
  EXPECT_EQ(index, 0);

  index = buf.FindCharInSet("z?", 6);
  EXPECT_EQ(index, (int32_t)buf.Length() - 1);
}

TEST_F(Strings, rfindcharinset) {
  nsCString buf("hello, how are you?");

  int32_t index = buf.RFindCharInSet(",?", 5);
  EXPECT_EQ(index, 5);

  index = buf.RFindCharInSet("helo", 0);
  EXPECT_EQ(index, 0);

  index = buf.RFindCharInSet("z?", 6);
  EXPECT_EQ(index, kNotFound);

  index = buf.RFindCharInSet("l", 5);
  EXPECT_EQ(index, 3);

  buf.AssignLiteral("abcdefghijkabc");

  index = buf.RFindCharInSet("ab");
  EXPECT_EQ(index, 12);

  index = buf.RFindCharInSet("ab", 11);
  EXPECT_EQ(index, 11);

  index = buf.RFindCharInSet("ab", 10);
  EXPECT_EQ(index, 1);

  index = buf.RFindCharInSet("ab", 0);
  EXPECT_EQ(index, 0);

  index = buf.RFindCharInSet("cd", 1);
  EXPECT_EQ(index, kNotFound);

  index = buf.RFindCharInSet("h");
  EXPECT_EQ(index, 7);
}

TEST_F(Strings, stringbuffer) {
  const char kData[] = "hello world";

  RefPtr<nsStringBuffer> buf;

  buf = nsStringBuffer::Alloc(sizeof(kData));
  EXPECT_TRUE(!!buf);

  buf = nsStringBuffer::Alloc(sizeof(kData));
  EXPECT_TRUE(!!buf);
  char* data = (char*)buf->Data();
  memcpy(data, kData, sizeof(kData));

  nsCString str;
  buf->ToString(sizeof(kData) - 1, str);

  nsStringBuffer* buf2;
  buf2 = nsStringBuffer::FromString(str);

  EXPECT_EQ(buf, buf2);
}

TEST_F(Strings, voided) {
  const char kData[] = "hello world";

  nsCString str;
  EXPECT_TRUE(!str.IsVoid());
  EXPECT_TRUE(str.IsEmpty());
  EXPECT_STREQ(str.get(), "");

  str.SetIsVoid(true);
  EXPECT_TRUE(str.IsVoid());
  EXPECT_TRUE(str.IsEmpty());
  EXPECT_STREQ(str.get(), "");

  str.Assign(kData);
  EXPECT_TRUE(!str.IsVoid());
  EXPECT_TRUE(!str.IsEmpty());
  EXPECT_STREQ(str.get(), kData);

  str.SetIsVoid(true);
  EXPECT_TRUE(str.IsVoid());
  EXPECT_TRUE(str.IsEmpty());
  EXPECT_STREQ(str.get(), "");

  str.SetIsVoid(false);
  EXPECT_TRUE(!str.IsVoid());
  EXPECT_TRUE(str.IsEmpty());
  EXPECT_STREQ(str.get(), "");

  str.Assign(kData);
  EXPECT_TRUE(!str.IsVoid());
  EXPECT_TRUE(!str.IsEmpty());
  EXPECT_STREQ(str.get(), kData);

  str.Adopt(nullptr);
  EXPECT_TRUE(str.IsVoid());
  EXPECT_TRUE(str.IsEmpty());
  EXPECT_STREQ(str.get(), "");
}

TEST_F(Strings, voided_autostr) {
  const char kData[] = "hello world";

  nsAutoCString str;
  EXPECT_FALSE(str.IsVoid());
  EXPECT_TRUE(str.IsEmpty());

  str.Assign(kData);
  EXPECT_STREQ(str.get(), kData);

  str.SetIsVoid(true);
  EXPECT_TRUE(str.IsVoid());
  EXPECT_TRUE(str.IsEmpty());

  str.Assign(kData);
  EXPECT_FALSE(str.IsVoid());
  EXPECT_FALSE(str.IsEmpty());
  EXPECT_STREQ(str.get(), kData);
}

TEST_F(Strings, voided_assignment) {
  nsCString a, b;
  b.SetIsVoid(true);
  a = b;
  EXPECT_TRUE(a.IsVoid());
  EXPECT_EQ(a.get(), b.get());
}

TEST_F(Strings, empty_assignment) {
  nsCString a, b;
  a = b;
  EXPECT_EQ(a.get(), b.get());
}

struct ToIntegerTest {
  const char* str;
  uint32_t radix;
  int32_t result;
  nsresult rv;
};

static const ToIntegerTest kToIntegerTests[] = {
    {"123", 10, 123, NS_OK},
    {"7b", 16, 123, NS_OK},
    {"90194313659", 10, 0, NS_ERROR_ILLEGAL_VALUE},
    {nullptr, 0, 0, NS_OK}};

TEST_F(Strings, string_tointeger) {
  nsresult rv;
  for (const ToIntegerTest* t = kToIntegerTests; t->str; ++t) {
    int32_t result = nsAutoCString(t->str).ToInteger(&rv, t->radix);
    EXPECT_EQ(rv, t->rv);
    EXPECT_EQ(result, t->result);
    result = nsAutoCString(t->str).ToInteger(&rv, t->radix);
    EXPECT_EQ(rv, t->rv);
    EXPECT_EQ(result, t->result);
  }
}

static void test_parse_string_helper(const char* str, char separator, int len,
                                     const char* s1, const char* s2) {
  nsCString data(str);
  nsTArray<nsCString> results;
  EXPECT_TRUE(ParseString(data, separator, results));
  EXPECT_EQ(int(results.Length()), len);
  const char* strings[] = {s1, s2};
  for (int i = 0; i < len; ++i) {
    EXPECT_TRUE(results[i].Equals(strings[i]));
  }
}

static void test_parse_string_helper0(const char* str, char separator) {
  test_parse_string_helper(str, separator, 0, nullptr, nullptr);
}

static void test_parse_string_helper1(const char* str, char separator,
                                      const char* s1) {
  test_parse_string_helper(str, separator, 1, s1, nullptr);
}

static void test_parse_string_helper2(const char* str, char separator,
                                      const char* s1, const char* s2) {
  test_parse_string_helper(str, separator, 2, s1, s2);
}

TEST(String, parse_string)
{
  test_parse_string_helper1("foo, bar", '_', "foo, bar");
  test_parse_string_helper2("foo, bar", ',', "foo", " bar");
  test_parse_string_helper2("foo, bar ", ' ', "foo,", "bar");
  test_parse_string_helper2("foo,bar", 'o', "f", ",bar");
  test_parse_string_helper0("", '_');
  test_parse_string_helper0("  ", ' ');
  test_parse_string_helper1(" foo", ' ', "foo");
  test_parse_string_helper1("  foo", ' ', "foo");
}

static void test_strip_chars_helper(const char16_t* str, const char16_t* strip,
                                    const nsAString& result) {
  nsAutoString data(str);
  data.StripChars(strip);
  EXPECT_TRUE(data.Equals(result));
}

TEST(String, strip_chars)
{
  test_strip_chars_helper(u"foo \r \nbar", u" \n\r",
                          NS_LITERAL_STRING("foobar"));
  test_strip_chars_helper(u"\r\nfoo\r\n", u" \n\r", NS_LITERAL_STRING("foo"));
  test_strip_chars_helper(u"foo", u" \n\r", NS_LITERAL_STRING("foo"));
  test_strip_chars_helper(u"foo", u"fo", NS_LITERAL_STRING(""));
  test_strip_chars_helper(u"foo", u"foo", NS_LITERAL_STRING(""));
  test_strip_chars_helper(u" foo", u" ", NS_LITERAL_STRING("foo"));
}

TEST_F(Strings, append_with_capacity) {
  nsAutoString s;
  const char16_t* origPtr = s.BeginReading();
  s.SetCapacity(8000);
  const char16_t* ptr = s.BeginReading();
  EXPECT_NE(origPtr, ptr);
  for (int i = 0; i < 100; i++) {
    s.Append(u'a');
    EXPECT_EQ(s.BeginReading(), ptr);
    EXPECT_EQ(s.Length(), uint32_t(i + 1));
  }
}

TEST_F(Strings, append_string_with_capacity) {
  nsAutoString aa;
  aa.Append(u'a');
  aa.Append(u'a');
  nsAutoString s;
  const char16_t* origPtr = s.BeginReading();
  s.SetCapacity(8000);
  const char16_t* ptr = s.BeginReading();
  EXPECT_NE(origPtr, ptr);
  nsAutoString empty;
  s.Append(empty);
  EXPECT_EQ(s.BeginReading(), ptr);
  for (int i = 0; i < 100; i++) {
    s.Append(aa);
    EXPECT_EQ(s.BeginReading(), ptr);
    EXPECT_EQ(s.Length(), uint32_t(2 * (i + 1)));
  }
}

TEST_F(Strings, append_literal_with_capacity) {
  nsAutoString s;
  const char16_t* origPtr = s.BeginReading();
  s.SetCapacity(8000);
  const char16_t* ptr = s.BeginReading();
  EXPECT_NE(origPtr, ptr);
  s.AppendLiteral(u"");
  EXPECT_EQ(s.BeginReading(), ptr);
  for (int i = 0; i < 100; i++) {
    s.AppendLiteral(u"aa");
    EXPECT_EQ(s.BeginReading(), ptr);
    EXPECT_EQ(s.Length(), uint32_t(2 * (i + 1)));
  }
}

// The following test is intentionally not memory
// checking-clean.
#if !(defined(MOZ_HAVE_MEM_CHECKS) || defined(MOZ_MSAN))
TEST_F(Strings, legacy_set_length_semantics) {
  const char* foobar = "foobar";
  nsCString s;
  s.SetCapacity(2048);
  memcpy(s.BeginWriting(), foobar, strlen(foobar));
  s.SetLength(strlen(foobar));
  EXPECT_FALSE(s.EqualsASCII(foobar));
}
#endif

TEST_F(Strings, bulk_write) {
  nsresult rv;
  nsCString s;
  const char* ptrTwoThousand;
  {
    auto handle = s.BulkWrite(500, 0, true, rv);
    EXPECT_EQ(rv, NS_OK);
    auto span = handle.AsSpan();
    for (auto&& c : span) {
      c = 'a';
    }
    mozilla::Unused << handle.RestartBulkWrite(2000, 500, false);
    span = handle.AsSpan().From(500);
    for (auto&& c : span) {
      c = 'b';
    }
    ptrTwoThousand = handle.Elements();
    handle.Finish(1000, true);
  }
  EXPECT_EQ(s.Length(), 1000U);
  EXPECT_NE(s.BeginReading(), ptrTwoThousand);
  EXPECT_EQ(s.BeginReading()[1000], '\0');
  for (uint32_t i = 0; i < 500; i++) {
    EXPECT_EQ(s[i], 'a');
  }
  for (uint32_t i = 500; i < 1000; i++) {
    EXPECT_EQ(s[i], 'b');
  }
}

TEST_F(Strings, bulk_write_fail) {
  nsresult rv;
  nsCString s;
  {
    auto handle = s.BulkWrite(500, 0, true, rv);
    EXPECT_EQ(rv, NS_OK);
  }
  EXPECT_EQ(s.Length(), 3U);
  EXPECT_TRUE(s.Equals(u8"\uFFFD"));
}

TEST_F(Strings, huge_capacity) {
  nsString a, b, c, d, e, f, g, h, i, j, k, l, m, n;
  nsCString n1;

  // Ignore the result if the address space is less than 64-bit because
  // some of the allocations above will exhaust the address space.
  if (sizeof(void*) >= 8) {
    EXPECT_TRUE(a.SetCapacity(1, fallible));
    EXPECT_FALSE(a.SetCapacity(nsString::size_type(-1) / 2, fallible));
    a.Truncate();  // free the allocated memory

    EXPECT_TRUE(b.SetCapacity(1, fallible));
    EXPECT_FALSE(b.SetCapacity(nsString::size_type(-1) / 2 - 1, fallible));
    b.Truncate();

    EXPECT_TRUE(c.SetCapacity(1, fallible));
    EXPECT_FALSE(c.SetCapacity(nsString::size_type(-1) / 2, fallible));
    c.Truncate();

    EXPECT_FALSE(d.SetCapacity(nsString::size_type(-1) / 2 - 1, fallible));
    EXPECT_FALSE(d.SetCapacity(nsString::size_type(-1) / 2, fallible));
    d.Truncate();

    EXPECT_FALSE(e.SetCapacity(nsString::size_type(-1) / 4, fallible));
    EXPECT_FALSE(e.SetCapacity(nsString::size_type(-1) / 4 + 1, fallible));
    e.Truncate();

    EXPECT_FALSE(f.SetCapacity(nsString::size_type(-1) / 2, fallible));
    f.Truncate();

    EXPECT_FALSE(g.SetCapacity(nsString::size_type(-1) / 4 + 1000, fallible));
    EXPECT_FALSE(g.SetCapacity(nsString::size_type(-1) / 4 + 1001, fallible));
    g.Truncate();

    EXPECT_FALSE(h.SetCapacity(nsString::size_type(-1) / 4 + 1, fallible));
    EXPECT_FALSE(h.SetCapacity(nsString::size_type(-1) / 2, fallible));
    h.Truncate();

    EXPECT_TRUE(i.SetCapacity(1, fallible));
    EXPECT_TRUE(i.SetCapacity(nsString::size_type(-1) / 4 - 1000, fallible));
    EXPECT_FALSE(i.SetCapacity(nsString::size_type(-1) / 4 + 1, fallible));
    i.Truncate();

    EXPECT_TRUE(j.SetCapacity(nsString::size_type(-1) / 4 - 1000, fallible));
    EXPECT_FALSE(j.SetCapacity(nsString::size_type(-1) / 4 + 1, fallible));
    j.Truncate();

// Disabled due to intermittent failures.
// https://bugzilla.mozilla.org/show_bug.cgi?id=1493458
#if 0
    EXPECT_TRUE(k.SetCapacity(nsString::size_type(-1)/8 - 1000, fallible));
    EXPECT_TRUE(k.SetCapacity(nsString::size_type(-1)/4 - 1001, fallible));
    EXPECT_TRUE(k.SetCapacity(nsString::size_type(-1)/4 - 998, fallible));
    EXPECT_FALSE(k.SetCapacity(nsString::size_type(-1)/4 + 1, fallible));
    k.Truncate();
#endif

    EXPECT_TRUE(l.SetCapacity(nsString::size_type(-1) / 8, fallible));
    EXPECT_TRUE(l.SetCapacity(nsString::size_type(-1) / 8 + 1, fallible));
    EXPECT_TRUE(l.SetCapacity(nsString::size_type(-1) / 8 + 2, fallible));
    l.Truncate();

    EXPECT_TRUE(m.SetCapacity(nsString::size_type(-1) / 8 + 1000, fallible));
    EXPECT_TRUE(m.SetCapacity(nsString::size_type(-1) / 8 + 1001, fallible));
    m.Truncate();

    EXPECT_TRUE(n.SetCapacity(nsString::size_type(-1) / 8 + 1, fallible));
    EXPECT_FALSE(n.SetCapacity(nsString::size_type(-1) / 4, fallible));
    n.Truncate();

    n.Truncate();
    EXPECT_TRUE(n.SetCapacity(
        (nsString::size_type(-1) / 2 - sizeof(nsStringBuffer)) / 2 - 2,
        fallible));
    n.Truncate();
    EXPECT_FALSE(n.SetCapacity(
        (nsString::size_type(-1) / 2 - sizeof(nsStringBuffer)) / 2 - 1,
        fallible));
    n.Truncate();
    n1.Truncate();
    EXPECT_TRUE(n1.SetCapacity(
        (nsCString::size_type(-1) / 2 - sizeof(nsStringBuffer)) / 1 - 2,
        fallible));
    n1.Truncate();
    EXPECT_FALSE(n1.SetCapacity(
        (nsCString::size_type(-1) / 2 - sizeof(nsStringBuffer)) / 1 - 1,
        fallible));
    n1.Truncate();
  }
}

static void test_tofloat_helper(const nsString& aStr, float aExpected,
                                bool aSuccess) {
  nsresult result;
  EXPECT_EQ(aStr.ToFloat(&result), aExpected);
  if (aSuccess) {
    EXPECT_EQ(result, NS_OK);
  } else {
    EXPECT_NE(result, NS_OK);
  }
}

TEST_F(Strings, tofloat) {
  test_tofloat_helper(NS_LITERAL_STRING("42"), 42.f, true);
  test_tofloat_helper(NS_LITERAL_STRING("42.0"), 42.f, true);
  test_tofloat_helper(NS_LITERAL_STRING("-42"), -42.f, true);
  test_tofloat_helper(NS_LITERAL_STRING("+42"), 42, true);
  test_tofloat_helper(NS_LITERAL_STRING("13.37"), 13.37f, true);
  test_tofloat_helper(NS_LITERAL_STRING("1.23456789"), 1.23456789f, true);
  test_tofloat_helper(NS_LITERAL_STRING("1.98765432123456"), 1.98765432123456f,
                      true);
  test_tofloat_helper(NS_LITERAL_STRING("0"), 0.f, true);
  test_tofloat_helper(NS_LITERAL_STRING("1.e5"), 100000, true);
  test_tofloat_helper(NS_LITERAL_STRING(""), 0.f, false);
  test_tofloat_helper(NS_LITERAL_STRING("42foo"), 42.f, false);
  test_tofloat_helper(NS_LITERAL_STRING("foo"), 0.f, false);
  test_tofloat_helper(NS_LITERAL_STRING("1.5e-"), 1.5f, false);
}

static void test_tofloat_allow_trailing_chars_helper(const nsString& aStr,
                                                     float aExpected,
                                                     bool aSuccess) {
  nsresult result;
  EXPECT_EQ(aStr.ToFloatAllowTrailingChars(&result), aExpected);
  if (aSuccess) {
    EXPECT_EQ(result, NS_OK);
  } else {
    EXPECT_NE(result, NS_OK);
  }
}

TEST_F(Strings, ToFloatAllowTrailingChars) {
  test_tofloat_allow_trailing_chars_helper(NS_LITERAL_STRING(""), 0.f, false);
  test_tofloat_allow_trailing_chars_helper(NS_LITERAL_STRING("foo"), 0.f,
                                           false);
  test_tofloat_allow_trailing_chars_helper(NS_LITERAL_STRING("42foo"), 42.f,
                                           true);
  test_tofloat_allow_trailing_chars_helper(NS_LITERAL_STRING("42-5"), 42.f,
                                           true);
  test_tofloat_allow_trailing_chars_helper(NS_LITERAL_STRING("13.37.8"), 13.37f,
                                           true);
  test_tofloat_allow_trailing_chars_helper(NS_LITERAL_STRING("1.5e-"), 1.5f,
                                           true);
}

static void test_todouble_helper(const nsString& aStr, double aExpected,
                                 bool aSuccess) {
  nsresult result;
  EXPECT_EQ(aStr.ToDouble(&result), aExpected);
  if (aSuccess) {
    EXPECT_EQ(result, NS_OK);
  } else {
    EXPECT_NE(result, NS_OK);
  }
}

TEST_F(Strings, todouble) {
  test_todouble_helper(NS_LITERAL_STRING("42"), 42, true);
  test_todouble_helper(NS_LITERAL_STRING("42.0"), 42, true);
  test_todouble_helper(NS_LITERAL_STRING("-42"), -42, true);
  test_todouble_helper(NS_LITERAL_STRING("+42"), 42, true);
  test_todouble_helper(NS_LITERAL_STRING("13.37"), 13.37, true);
  test_todouble_helper(NS_LITERAL_STRING("1.23456789"), 1.23456789, true);
  test_todouble_helper(NS_LITERAL_STRING("1.98765432123456"), 1.98765432123456,
                       true);
  test_todouble_helper(NS_LITERAL_STRING("123456789.98765432123456"),
                       123456789.98765432123456, true);
  test_todouble_helper(NS_LITERAL_STRING("0"), 0, true);
  test_todouble_helper(NS_LITERAL_STRING("1.e5"), 100000, true);
  test_todouble_helper(NS_LITERAL_STRING(""), 0, false);
  test_todouble_helper(NS_LITERAL_STRING("42foo"), 42, false);
  test_todouble_helper(NS_LITERAL_STRING("foo"), 0, false);
  test_todouble_helper(NS_LITERAL_STRING("1.5e-"), 1.5, false);
}

static void test_todouble_allow_trailing_chars_helper(const nsString& aStr,
                                                      double aExpected,
                                                      bool aSuccess) {
  nsresult result;
  EXPECT_EQ(aStr.ToDoubleAllowTrailingChars(&result), aExpected);
  if (aSuccess) {
    EXPECT_EQ(result, NS_OK);
  } else {
    EXPECT_NE(result, NS_OK);
  }
}

TEST_F(Strings, ToDoubleAllowTrailingChars) {
  test_todouble_allow_trailing_chars_helper(NS_LITERAL_STRING(""), 0, false);
  test_todouble_allow_trailing_chars_helper(NS_LITERAL_STRING("foo"), 0, false);
  test_todouble_allow_trailing_chars_helper(NS_LITERAL_STRING("42foo"), 42,
                                            true);
  test_todouble_allow_trailing_chars_helper(NS_LITERAL_STRING("42-5"), 42,
                                            true);
  test_todouble_allow_trailing_chars_helper(NS_LITERAL_STRING("13.37.8"), 13.37,
                                            true);
  test_todouble_allow_trailing_chars_helper(NS_LITERAL_STRING("1.5e-"), 1.5,
                                            true);
}

TEST_F(Strings, Split) {
  nsCString one("one"), two("one;two"), three("one--three"), empty(""),
      delimStart("-two"), delimEnd("one-");

  nsString wide(u"hello world");

  size_t counter = 0;
  for (const nsACString& token : one.Split(',')) {
    EXPECT_TRUE(token.EqualsLiteral("one"));
    counter++;
  }
  EXPECT_EQ(counter, (size_t)1);

  counter = 0;
  for (const nsACString& token : two.Split(';')) {
    if (counter == 0) {
      EXPECT_TRUE(token.EqualsLiteral("one"));
    } else if (counter == 1) {
      EXPECT_TRUE(token.EqualsLiteral("two"));
    }
    counter++;
  }
  EXPECT_EQ(counter, (size_t)2);

  counter = 0;
  for (const nsACString& token : three.Split('-')) {
    if (counter == 0) {
      EXPECT_TRUE(token.EqualsLiteral("one"));
    } else if (counter == 1) {
      EXPECT_TRUE(token.EqualsLiteral(""));
    } else if (counter == 2) {
      EXPECT_TRUE(token.EqualsLiteral("three"));
    }
    counter++;
  }
  EXPECT_EQ(counter, (size_t)3);

  counter = 0;
  for (const nsACString& token : empty.Split(',')) {
    mozilla::Unused << token;
    counter++;
  }
  EXPECT_EQ(counter, (size_t)0);

  counter = 0;
  for (const nsACString& token : delimStart.Split('-')) {
    if (counter == 0) {
      EXPECT_TRUE(token.EqualsLiteral(""));
    } else if (counter == 1) {
      EXPECT_TRUE(token.EqualsLiteral("two"));
    }
    counter++;
  }
  EXPECT_EQ(counter, (size_t)2);

  counter = 0;
  for (const nsACString& token : delimEnd.Split('-')) {
    if (counter == 0) {
      EXPECT_TRUE(token.EqualsLiteral("one"));
    } else if (counter == 1) {
      EXPECT_TRUE(token.EqualsLiteral(""));
    }
    counter++;
  }
  EXPECT_EQ(counter, (size_t)2);

  counter = 0;
  for (const nsAString& token : wide.Split(' ')) {
    if (counter == 0) {
      EXPECT_TRUE(token.Equals(NS_LITERAL_STRING("hello")));
    } else if (counter == 1) {
      EXPECT_TRUE(token.Equals(NS_LITERAL_STRING("world")));
    }
    counter++;
  }
  EXPECT_EQ(counter, (size_t)2);
}

constexpr bool TestSomeChars(char c) {
  return c == 'a' || c == 'c' || c == 'e' || c == '7' || c == 'G' || c == 'Z' ||
         c == '\b' || c == '?';
}
TEST_F(Strings, ASCIIMask) {
  const ASCIIMaskArray& maskCRLF = mozilla::ASCIIMask::MaskCRLF();
  EXPECT_TRUE(maskCRLF['\n'] && mozilla::ASCIIMask::IsMasked(maskCRLF, '\n'));
  EXPECT_TRUE(maskCRLF['\r'] && mozilla::ASCIIMask::IsMasked(maskCRLF, '\r'));
  EXPECT_FALSE(maskCRLF['g'] || mozilla::ASCIIMask::IsMasked(maskCRLF, 'g'));
  EXPECT_FALSE(maskCRLF[' '] || mozilla::ASCIIMask::IsMasked(maskCRLF, ' '));
  EXPECT_FALSE(maskCRLF['\0'] || mozilla::ASCIIMask::IsMasked(maskCRLF, '\0'));
  EXPECT_FALSE(mozilla::ASCIIMask::IsMasked(maskCRLF, 14324));

  const ASCIIMaskArray& mask0to9 = mozilla::ASCIIMask::Mask0to9();
  EXPECT_TRUE(mask0to9['9'] && mozilla::ASCIIMask::IsMasked(mask0to9, '9'));
  EXPECT_TRUE(mask0to9['0'] && mozilla::ASCIIMask::IsMasked(mask0to9, '0'));
  EXPECT_TRUE(mask0to9['4'] && mozilla::ASCIIMask::IsMasked(mask0to9, '4'));
  EXPECT_FALSE(mask0to9['g'] || mozilla::ASCIIMask::IsMasked(mask0to9, 'g'));
  EXPECT_FALSE(mask0to9[' '] || mozilla::ASCIIMask::IsMasked(mask0to9, ' '));
  EXPECT_FALSE(mask0to9['\n'] || mozilla::ASCIIMask::IsMasked(mask0to9, '\n'));
  EXPECT_FALSE(mask0to9['\0'] || mozilla::ASCIIMask::IsMasked(mask0to9, '\0'));
  EXPECT_FALSE(mozilla::ASCIIMask::IsMasked(maskCRLF, 14324));

  const ASCIIMaskArray& maskWS = mozilla::ASCIIMask::MaskWhitespace();
  EXPECT_TRUE(maskWS[' '] && mozilla::ASCIIMask::IsMasked(maskWS, ' '));
  EXPECT_TRUE(maskWS['\t'] && mozilla::ASCIIMask::IsMasked(maskWS, '\t'));
  EXPECT_FALSE(maskWS['8'] || mozilla::ASCIIMask::IsMasked(maskWS, '8'));
  EXPECT_FALSE(maskWS['\0'] || mozilla::ASCIIMask::IsMasked(maskWS, '\0'));
  EXPECT_FALSE(mozilla::ASCIIMask::IsMasked(maskCRLF, 14324));

  constexpr ASCIIMaskArray maskSome = mozilla::CreateASCIIMask(TestSomeChars);
  EXPECT_TRUE(maskSome['a'] && mozilla::ASCIIMask::IsMasked(maskSome, 'a'));
  EXPECT_TRUE(maskSome['c'] && mozilla::ASCIIMask::IsMasked(maskSome, 'c'));
  EXPECT_TRUE(maskSome['e'] && mozilla::ASCIIMask::IsMasked(maskSome, 'e'));
  EXPECT_TRUE(maskSome['7'] && mozilla::ASCIIMask::IsMasked(maskSome, '7'));
  EXPECT_TRUE(maskSome['G'] && mozilla::ASCIIMask::IsMasked(maskSome, 'G'));
  EXPECT_TRUE(maskSome['Z'] && mozilla::ASCIIMask::IsMasked(maskSome, 'Z'));
  EXPECT_TRUE(maskSome['\b'] && mozilla::ASCIIMask::IsMasked(maskSome, '\b'));
  EXPECT_TRUE(maskSome['?'] && mozilla::ASCIIMask::IsMasked(maskSome, '?'));
  EXPECT_FALSE(maskSome['8'] || mozilla::ASCIIMask::IsMasked(maskSome, '8'));
  EXPECT_FALSE(maskSome['\0'] || mozilla::ASCIIMask::IsMasked(maskSome, '\0'));
  EXPECT_FALSE(mozilla::ASCIIMask::IsMasked(maskCRLF, 14324));
}

template <typename T>
void CompressWhitespaceHelper() {
  T s;
  s.AssignLiteral("abcabcabc");
  s.CompressWhitespace(true, true);
  EXPECT_TRUE(s.EqualsLiteral("abcabcabc"));

  s.AssignLiteral("   \n\rabcabcabc\r\n");
  s.CompressWhitespace(true, true);
  EXPECT_TRUE(s.EqualsLiteral("abcabcabc"));

  s.AssignLiteral("   \n\rabc   abc   abc\r\n");
  s.CompressWhitespace(true, true);
  EXPECT_TRUE(s.EqualsLiteral("abc abc abc"));

  s.AssignLiteral("   \n\rabc\r   abc\n   abc\r\n");
  s.CompressWhitespace(true, true);
  EXPECT_TRUE(s.EqualsLiteral("abc abc abc"));

  s.AssignLiteral("   \n\rabc\r  \nabc\n   \rabc\r\n");
  s.CompressWhitespace(true, true);
  EXPECT_TRUE(s.EqualsLiteral("abc abc abc"));

  s.AssignLiteral("   \n\rabc\r   abc\n   abc\r\n");
  s.CompressWhitespace(false, true);
  EXPECT_TRUE(s.EqualsLiteral(" abc abc abc"));

  s.AssignLiteral("   \n\rabc\r   abc\n   abc\r\n");
  s.CompressWhitespace(true, false);
  EXPECT_TRUE(s.EqualsLiteral("abc abc abc "));

  s.AssignLiteral("   \n\rabc\r   abc\n   abc\r\n");
  s.CompressWhitespace(false, false);
  EXPECT_TRUE(s.EqualsLiteral(" abc abc abc "));

  s.AssignLiteral("  \r\n  ");
  s.CompressWhitespace(true, true);
  EXPECT_TRUE(s.EqualsLiteral(""));

  s.AssignLiteral("  \r\n  \t");
  s.CompressWhitespace(true, true);
  EXPECT_TRUE(s.EqualsLiteral(""));

  s.AssignLiteral("\n  \r\n  \t");
  s.CompressWhitespace(false, false);
  EXPECT_TRUE(s.EqualsLiteral(" "));

  s.AssignLiteral("\n  \r\n  \t");
  s.CompressWhitespace(false, true);
  EXPECT_TRUE(s.EqualsLiteral(""));

  s.AssignLiteral("\n  \r\n  \t");
  s.CompressWhitespace(true, false);
  EXPECT_TRUE(s.EqualsLiteral(""));

  s.AssignLiteral("");
  s.CompressWhitespace(false, false);
  EXPECT_TRUE(s.EqualsLiteral(""));

  s.AssignLiteral("");
  s.CompressWhitespace(false, true);
  EXPECT_TRUE(s.EqualsLiteral(""));

  s.AssignLiteral("");
  s.CompressWhitespace(true, false);
  EXPECT_TRUE(s.EqualsLiteral(""));

  s.AssignLiteral("");
  s.CompressWhitespace(true, true);
  EXPECT_TRUE(s.EqualsLiteral(""));
}

TEST_F(Strings, CompressWhitespace) { CompressWhitespaceHelper<nsCString>(); }

TEST_F(Strings, CompressWhitespaceW) {
  CompressWhitespaceHelper<nsString>();

  nsString str, result;
  str.AssignLiteral(u"\u263A    is\r\n   ;-)");
  result.AssignLiteral(u"\u263A is ;-)");
  str.CompressWhitespace(true, true);
  EXPECT_TRUE(str == result);
}

template <typename T>
void StripCRLFHelper() {
  T s;
  s.AssignLiteral("abcabcabc");
  s.StripCRLF();
  EXPECT_TRUE(s.EqualsLiteral("abcabcabc"));

  s.AssignLiteral("   \n\rabcabcabc\r\n");
  s.StripCRLF();
  EXPECT_TRUE(s.EqualsLiteral("   abcabcabc"));

  s.AssignLiteral("   \n\rabc   abc   abc\r\n");
  s.StripCRLF();
  EXPECT_TRUE(s.EqualsLiteral("   abc   abc   abc"));

  s.AssignLiteral("   \n\rabc\r   abc\n   abc\r\n");
  s.StripCRLF();
  EXPECT_TRUE(s.EqualsLiteral("   abc   abc   abc"));

  s.AssignLiteral("   \n\rabc\r  \nabc\n   \rabc\r\n");
  s.StripCRLF();
  EXPECT_TRUE(s.EqualsLiteral("   abc  abc   abc"));

  s.AssignLiteral("   \n\rabc\r   abc\n   abc\r\n");
  s.StripCRLF();
  EXPECT_TRUE(s.EqualsLiteral("   abc   abc   abc"));

  s.AssignLiteral("  \r\n  ");
  s.StripCRLF();
  EXPECT_TRUE(s.EqualsLiteral("    "));

  s.AssignLiteral("  \r\n  \t");
  s.StripCRLF();
  EXPECT_TRUE(s.EqualsLiteral("    \t"));

  s.AssignLiteral("\n  \r\n  \t");
  s.StripCRLF();
  EXPECT_TRUE(s.EqualsLiteral("    \t"));

  s.AssignLiteral("");
  s.StripCRLF();
  EXPECT_TRUE(s.EqualsLiteral(""));
}

TEST_F(Strings, StripCRLF) { StripCRLFHelper<nsCString>(); }

TEST_F(Strings, StripCRLFW) {
  StripCRLFHelper<nsString>();

  nsString str, result;
  str.AssignLiteral(u"\u263A    is\r\n   ;-)");
  result.AssignLiteral(u"\u263A    is   ;-)");
  str.StripCRLF();
  EXPECT_TRUE(str == result);
}

TEST_F(Strings, utf8_to_latin1_sharing) {
  nsCString s;
  s.Append('a');
  s.Append('b');
  s.Append('c');
  nsCString t;
  LossyAppendUTF8toLatin1(s, t);
  EXPECT_TRUE(t.EqualsLiteral("abc"));
  EXPECT_EQ(s.BeginReading(), t.BeginReading());
  LossyCopyUTF8toLatin1(s, t);
  EXPECT_TRUE(t.EqualsLiteral("abc"));
  EXPECT_EQ(s.BeginReading(), t.BeginReading());
}

TEST_F(Strings, latin1_to_utf8_sharing) {
  nsCString s;
  s.Append('a');
  s.Append('b');
  s.Append('c');
  nsCString t;
  AppendLatin1toUTF8(s, t);
  EXPECT_TRUE(t.EqualsLiteral("abc"));
  EXPECT_EQ(s.BeginReading(), t.BeginReading());
  CopyLatin1toUTF8(s, t);
  EXPECT_TRUE(t.EqualsLiteral("abc"));
  EXPECT_EQ(s.BeginReading(), t.BeginReading());
}

TEST_F(Strings, utf8_to_latin1) {
  nsCString s;
  s.AssignLiteral("\xC3\xA4");
  nsCString t;
  LossyCopyUTF8toLatin1(s, t);
  // EqualsLiteral requires ASCII
  EXPECT_TRUE(t.Equals("\xE4"));
}

TEST_F(Strings, latin1_to_utf8) {
  nsCString s;
  s.AssignLiteral("\xE4");
  nsCString t;
  CopyLatin1toUTF8(s, t);
  // EqualsLiteral requires ASCII
  EXPECT_TRUE(t.Equals("\xC3\xA4"));
}

// Note the five calls in the loop, so divide by 100k
MOZ_GTEST_BENCH_F(Strings, PerfStripWhitespace, [this] {
  nsCString test1(mExample1Utf8);
  nsCString test2(mExample2Utf8);
  nsCString test3(mExample3Utf8);
  nsCString test4(mExample4Utf8);
  nsCString test5(mExample5Utf8);
  for (int i = 0; i < 20000; i++) {
    test1.StripWhitespace();
    test2.StripWhitespace();
    test3.StripWhitespace();
    test4.StripWhitespace();
    test5.StripWhitespace();
  }
});

MOZ_GTEST_BENCH_F(Strings, PerfStripCharsWhitespace, [this] {
  // This is the unoptimized (original) version of
  // StripWhitespace using StripChars.
  nsCString test1(mExample1Utf8);
  nsCString test2(mExample2Utf8);
  nsCString test3(mExample3Utf8);
  nsCString test4(mExample4Utf8);
  nsCString test5(mExample5Utf8);
  for (int i = 0; i < 20000; i++) {
    test1.StripChars("\f\t\r\n ");
    test2.StripChars("\f\t\r\n ");
    test3.StripChars("\f\t\r\n ");
    test4.StripChars("\f\t\r\n ");
    test5.StripChars("\f\t\r\n ");
  }
});

MOZ_GTEST_BENCH_F(Strings, PerfCompressWhitespace, [this] {
  nsCString test1(mExample1Utf8);
  nsCString test2(mExample2Utf8);
  nsCString test3(mExample3Utf8);
  nsCString test4(mExample4Utf8);
  nsCString test5(mExample5Utf8);
  for (int i = 0; i < 20000; i++) {
    test1.CompressWhitespace();
    test2.CompressWhitespace();
    test3.CompressWhitespace();
    test4.CompressWhitespace();
    test5.CompressWhitespace();
  }
});

MOZ_GTEST_BENCH_F(Strings, PerfStripCRLF, [this] {
  nsCString test1(mExample1Utf8);
  nsCString test2(mExample2Utf8);
  nsCString test3(mExample3Utf8);
  nsCString test4(mExample4Utf8);
  nsCString test5(mExample5Utf8);
  for (int i = 0; i < 20000; i++) {
    test1.StripCRLF();
    test2.StripCRLF();
    test3.StripCRLF();
    test4.StripCRLF();
    test5.StripCRLF();
  }
});

MOZ_GTEST_BENCH_F(Strings, PerfStripCharsCRLF, [this] {
  // This is the unoptimized (original) version of
  // stripping \r\n using StripChars.
  nsCString test1(mExample1Utf8);
  nsCString test2(mExample2Utf8);
  nsCString test3(mExample3Utf8);
  nsCString test4(mExample4Utf8);
  nsCString test5(mExample5Utf8);
  for (int i = 0; i < 20000; i++) {
    test1.StripChars("\r\n");
    test2.StripChars("\r\n");
    test3.StripChars("\r\n");
    test4.StripChars("\r\n");
    test5.StripChars("\r\n");
  }
});

MOZ_GTEST_BENCH_F(Strings, PerfIsUTF8One, [this] {
  for (int i = 0; i < 200000; i++) {
    bool b = IsUTF8(*BlackBox(&mAsciiOneUtf8));
    BlackBox(&b);
  }
});

MOZ_GTEST_BENCH_F(Strings, PerfIsUTF8Fifteen, [this] {
  for (int i = 0; i < 200000; i++) {
    bool b = IsUTF8(*BlackBox(&mAsciiFifteenUtf8));
    BlackBox(&b);
  }
});

MOZ_GTEST_BENCH_F(Strings, PerfIsUTF8Hundred, [this] {
  for (int i = 0; i < 200000; i++) {
    bool b = IsUTF8(*BlackBox(&mAsciiHundredUtf8));
    BlackBox(&b);
  }
});

MOZ_GTEST_BENCH_F(Strings, PerfIsUTF8Example3, [this] {
  for (int i = 0; i < 100000; i++) {
    bool b = IsUTF8(*BlackBox(&mExample3Utf8));
    BlackBox(&b);
  }
});

MOZ_GTEST_BENCH_F(Strings, PerfIsASCII8One, [this] {
  for (int i = 0; i < 200000; i++) {
    bool b = IsASCII(*BlackBox(&mAsciiOneUtf8));
    BlackBox(&b);
  }
});

MOZ_GTEST_BENCH_F(Strings, PerfIsASCIIFifteen, [this] {
  for (int i = 0; i < 200000; i++) {
    bool b = IsASCII(*BlackBox(&mAsciiFifteenUtf8));
    BlackBox(&b);
  }
});

MOZ_GTEST_BENCH_F(Strings, PerfIsASCIIHundred, [this] {
  for (int i = 0; i < 200000; i++) {
    bool b = IsASCII(*BlackBox(&mAsciiHundredUtf8));
    BlackBox(&b);
  }
});

MOZ_GTEST_BENCH_F(Strings, PerfIsASCIIExample3, [this] {
  for (int i = 0; i < 100000; i++) {
    bool b = IsASCII(*BlackBox(&mExample3Utf8));
    BlackBox(&b);
  }
});

MOZ_GTEST_BENCH_F(Strings, PerfHasRTLCharsExample3, [this] {
  for (int i = 0; i < 5000; i++) {
    bool b = HasRTLChars(*BlackBox(&mExample3Utf16));
    BlackBox(&b);
  }
});

MOZ_GTEST_BENCH_F(Strings, PerfHasRTLCharsDE, [this] {
  for (int i = 0; i < 5000; i++) {
    bool b = HasRTLChars(*BlackBox(&mDeUtf16));
    BlackBox(&b);
  }
});

MOZ_GTEST_BENCH_F(Strings, PerfHasRTLCharsRU, [this] {
  for (int i = 0; i < 5000; i++) {
    bool b = HasRTLChars(*BlackBox(&mRuUtf16));
    BlackBox(&b);
  }
});

MOZ_GTEST_BENCH_F(Strings, PerfHasRTLCharsTH, [this] {
  for (int i = 0; i < 5000; i++) {
    bool b = HasRTLChars(*BlackBox(&mThUtf16));
    BlackBox(&b);
  }
});

MOZ_GTEST_BENCH_F(Strings, PerfHasRTLCharsJA, [this] {
  for (int i = 0; i < 5000; i++) {
    bool b = HasRTLChars(*BlackBox(&mJaUtf16));
    BlackBox(&b);
  }
});

CONVERSION_BENCH(PerfUTF16toLatin1ASCIIOne, LossyCopyUTF16toASCII,
                 mAsciiOneUtf16, nsAutoCString);

CONVERSION_BENCH(PerfUTF16toLatin1ASCIIThree, LossyCopyUTF16toASCII,
                 mAsciiThreeUtf16, nsAutoCString);

CONVERSION_BENCH(PerfUTF16toLatin1ASCIIFifteen, LossyCopyUTF16toASCII,
                 mAsciiFifteenUtf16, nsAutoCString);

CONVERSION_BENCH(PerfUTF16toLatin1ASCIIHundred, LossyCopyUTF16toASCII,
                 mAsciiHundredUtf16, nsAutoCString);

CONVERSION_BENCH(PerfUTF16toLatin1ASCIIThousand, LossyCopyUTF16toASCII,
                 mAsciiThousandUtf16, nsAutoCString);

CONVERSION_BENCH(PerfUTF16toLatin1DEOne, LossyCopyUTF16toASCII, mDeEditOneUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF16toLatin1DEThree, LossyCopyUTF16toASCII,
                 mDeEditThreeUtf16, nsAutoCString);

CONVERSION_BENCH(PerfUTF16toLatin1DEFifteen, LossyCopyUTF16toASCII,
                 mDeEditFifteenUtf16, nsAutoCString);

CONVERSION_BENCH(PerfUTF16toLatin1DEHundred, LossyCopyUTF16toASCII,
                 mDeEditHundredUtf16, nsAutoCString);

CONVERSION_BENCH(PerfUTF16toLatin1DEThousand, LossyCopyUTF16toASCII,
                 mDeEditThousandUtf16, nsAutoCString);

CONVERSION_BENCH(PerfLatin1toUTF16AsciiOne, CopyASCIItoUTF16, mAsciiOneUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfLatin1toUTF16AsciiThree, CopyASCIItoUTF16, mAsciiThreeUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfLatin1toUTF16AsciiFifteen, CopyASCIItoUTF16,
                 mAsciiFifteenUtf8, nsAutoString);

CONVERSION_BENCH(PerfLatin1toUTF16AsciiHundred, CopyASCIItoUTF16,
                 mAsciiHundredUtf8, nsAutoString);

CONVERSION_BENCH(PerfLatin1toUTF16AsciiThousand, CopyASCIItoUTF16,
                 mAsciiThousandUtf8, nsAutoString);

CONVERSION_BENCH(PerfLatin1toUTF16DEOne, CopyASCIItoUTF16, mDeEditOneLatin1,
                 nsAutoString);

CONVERSION_BENCH(PerfLatin1toUTF16DEThree, CopyASCIItoUTF16, mDeEditThreeLatin1,
                 nsAutoString);

CONVERSION_BENCH(PerfLatin1toUTF16DEFifteen, CopyASCIItoUTF16,
                 mDeEditFifteenLatin1, nsAutoString);

CONVERSION_BENCH(PerfLatin1toUTF16DEHundred, CopyASCIItoUTF16,
                 mDeEditHundredLatin1, nsAutoString);

CONVERSION_BENCH(PerfLatin1toUTF16DEThousand, CopyASCIItoUTF16,
                 mDeEditThousandLatin1, nsAutoString);

CONVERSION_BENCH(PerfUTF16toUTF8AsciiOne, CopyUTF16toUTF8, mAsciiOneUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF16toUTF8AsciiThree, CopyUTF16toUTF8, mAsciiThreeUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF16toUTF8AsciiFifteen, CopyUTF16toUTF8,
                 mAsciiFifteenUtf16, nsAutoCString);

CONVERSION_BENCH(PerfUTF16toUTF8AsciiHundred, CopyUTF16toUTF8,
                 mAsciiHundredUtf16, nsAutoCString);

CONVERSION_BENCH(PerfUTF16toUTF8AsciiThousand, CopyUTF16toUTF8,
                 mAsciiThousandUtf16, nsAutoCString);

CONVERSION_BENCH(PerfUTF8toUTF16AsciiOne, CopyUTF8toUTF16, mAsciiOneUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfUTF8toUTF16AsciiThree, CopyUTF8toUTF16, mAsciiThreeUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfUTF8toUTF16AsciiFifteen, CopyUTF8toUTF16,
                 mAsciiFifteenUtf8, nsAutoString);

CONVERSION_BENCH(PerfUTF8toUTF16AsciiHundred, CopyUTF8toUTF16,
                 mAsciiHundredUtf8, nsAutoString);

CONVERSION_BENCH(PerfUTF8toUTF16AsciiThousand, CopyUTF8toUTF16,
                 mAsciiThousandUtf8, nsAutoString);

CONVERSION_BENCH(PerfUTF16toUTF8AROne, CopyUTF16toUTF8, mArOneUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF16toUTF8ARThree, CopyUTF16toUTF8, mArThreeUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF16toUTF8ARFifteen, CopyUTF16toUTF8, mArFifteenUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF16toUTF8ARHundred, CopyUTF16toUTF8, mArHundredUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF16toUTF8ARThousand, CopyUTF16toUTF8, mArThousandUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF8toUTF16AROne, CopyUTF8toUTF16, mArOneUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfUTF8toUTF16ARThree, CopyUTF8toUTF16, mArThreeUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfUTF8toUTF16ARFifteen, CopyUTF8toUTF16, mArFifteenUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfUTF8toUTF16ARHundred, CopyUTF8toUTF16, mArHundredUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfUTF8toUTF16ARThousand, CopyUTF8toUTF16, mArThousandUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfUTF16toUTF8DEOne, CopyUTF16toUTF8, mDeOneUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF16toUTF8DEThree, CopyUTF16toUTF8, mDeThreeUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF16toUTF8DEFifteen, CopyUTF16toUTF8, mDeFifteenUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF16toUTF8DEHundred, CopyUTF16toUTF8, mDeHundredUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF16toUTF8DEThousand, CopyUTF16toUTF8, mDeThousandUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF8toUTF16DEOne, CopyUTF8toUTF16, mDeOneUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfUTF8toUTF16DEThree, CopyUTF8toUTF16, mDeThreeUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfUTF8toUTF16DEFifteen, CopyUTF8toUTF16, mDeFifteenUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfUTF8toUTF16DEHundred, CopyUTF8toUTF16, mDeHundredUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfUTF8toUTF16DEThousand, CopyUTF8toUTF16, mDeThousandUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfUTF16toUTF8RUOne, CopyUTF16toUTF8, mRuOneUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF16toUTF8RUThree, CopyUTF16toUTF8, mRuThreeUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF16toUTF8RUFifteen, CopyUTF16toUTF8, mRuFifteenUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF16toUTF8RUHundred, CopyUTF16toUTF8, mRuHundredUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF16toUTF8RUThousand, CopyUTF16toUTF8, mRuThousandUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF8toUTF16RUOne, CopyUTF8toUTF16, mRuOneUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfUTF8toUTF16RUThree, CopyUTF8toUTF16, mRuThreeUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfUTF8toUTF16RUFifteen, CopyUTF8toUTF16, mRuFifteenUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfUTF8toUTF16RUHundred, CopyUTF8toUTF16, mRuHundredUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfUTF8toUTF16RUThousand, CopyUTF8toUTF16, mRuThousandUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfUTF16toUTF8THOne, CopyUTF16toUTF8, mThOneUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF16toUTF8THThree, CopyUTF16toUTF8, mThThreeUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF16toUTF8THFifteen, CopyUTF16toUTF8, mThFifteenUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF16toUTF8THHundred, CopyUTF16toUTF8, mThHundredUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF16toUTF8THThousand, CopyUTF16toUTF8, mThThousandUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF8toUTF16THOne, CopyUTF8toUTF16, mThOneUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfUTF8toUTF16THThree, CopyUTF8toUTF16, mThThreeUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfUTF8toUTF16THFifteen, CopyUTF8toUTF16, mThFifteenUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfUTF8toUTF16THHundred, CopyUTF8toUTF16, mThHundredUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfUTF8toUTF16THThousand, CopyUTF8toUTF16, mThThousandUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfUTF16toUTF8JAOne, CopyUTF16toUTF8, mJaOneUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF16toUTF8JAThree, CopyUTF16toUTF8, mJaThreeUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF16toUTF8JAFifteen, CopyUTF16toUTF8, mJaFifteenUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF16toUTF8JAHundred, CopyUTF16toUTF8, mJaHundredUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF16toUTF8JAThousand, CopyUTF16toUTF8, mJaThousandUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF8toUTF16JAOne, CopyUTF8toUTF16, mJaOneUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfUTF8toUTF16JAThree, CopyUTF8toUTF16, mJaThreeUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfUTF8toUTF16JAFifteen, CopyUTF8toUTF16, mJaFifteenUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfUTF8toUTF16JAHundred, CopyUTF8toUTF16, mJaHundredUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfUTF8toUTF16JAThousand, CopyUTF8toUTF16, mJaThousandUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfUTF16toUTF8KOOne, CopyUTF16toUTF8, mKoOneUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF16toUTF8KOThree, CopyUTF16toUTF8, mKoThreeUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF16toUTF8KOFifteen, CopyUTF16toUTF8, mKoFifteenUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF16toUTF8KOHundred, CopyUTF16toUTF8, mKoHundredUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF16toUTF8KOThousand, CopyUTF16toUTF8, mKoThousandUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF8toUTF16KOOne, CopyUTF8toUTF16, mKoOneUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfUTF8toUTF16KOThree, CopyUTF8toUTF16, mKoThreeUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfUTF8toUTF16KOFifteen, CopyUTF8toUTF16, mKoFifteenUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfUTF8toUTF16KOHundred, CopyUTF8toUTF16, mKoHundredUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfUTF8toUTF16KOThousand, CopyUTF8toUTF16, mKoThousandUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfUTF16toUTF8TROne, CopyUTF16toUTF8, mTrOneUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF16toUTF8TRThree, CopyUTF16toUTF8, mTrThreeUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF16toUTF8TRFifteen, CopyUTF16toUTF8, mTrFifteenUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF16toUTF8TRHundred, CopyUTF16toUTF8, mTrHundredUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF16toUTF8TRThousand, CopyUTF16toUTF8, mTrThousandUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF8toUTF16TROne, CopyUTF8toUTF16, mTrOneUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfUTF8toUTF16TRThree, CopyUTF8toUTF16, mTrThreeUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfUTF8toUTF16TRFifteen, CopyUTF8toUTF16, mTrFifteenUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfUTF8toUTF16TRHundred, CopyUTF8toUTF16, mTrHundredUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfUTF8toUTF16TRThousand, CopyUTF8toUTF16, mTrThousandUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfUTF16toUTF8VIOne, CopyUTF16toUTF8, mViOneUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF16toUTF8VIThree, CopyUTF16toUTF8, mViThreeUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF16toUTF8VIFifteen, CopyUTF16toUTF8, mViFifteenUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF16toUTF8VIHundred, CopyUTF16toUTF8, mViHundredUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF16toUTF8VIThousand, CopyUTF16toUTF8, mViThousandUtf16,
                 nsAutoCString);

CONVERSION_BENCH(PerfUTF8toUTF16VIOne, CopyUTF8toUTF16, mViOneUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfUTF8toUTF16VIThree, CopyUTF8toUTF16, mViThreeUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfUTF8toUTF16VIFifteen, CopyUTF8toUTF16, mViFifteenUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfUTF8toUTF16VIHundred, CopyUTF8toUTF16, mViHundredUtf8,
                 nsAutoString);

CONVERSION_BENCH(PerfUTF8toUTF16VIThousand, CopyUTF8toUTF16, mViThousandUtf8,
                 nsAutoString);

}  // namespace TestStrings

#if defined(__clang__) && (__clang_major__ >= 6)
#  pragma clang diagnostic pop
#endif
