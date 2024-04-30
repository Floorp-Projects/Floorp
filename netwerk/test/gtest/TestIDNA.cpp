#include "gtest/gtest.h"
#include "gtest/MozGTestBench.h"  // For MOZ_GTEST_BENCH
#include "gtest/BlackBox.h"

#include "nsNetUtil.h"
#include "nsIURI.h"
#include "nsCOMPtr.h"
#include "mozilla/Encoding.h"

#define TEST_COUNT 10000

class TestIDNA : public ::testing::Test {
 protected:
  void SetUp() override {
    // Intentionally Assign and not AssignLiteral
    // to simulate the heap case.
    mPlainASCII.Assign("example.com");
    mLeadingDigitASCII.Assign("1test.example");
    mUnicodeMixed.Assign("مثال.example");
    mPunycodeMixed.Assign("xn--mgbh0fb.example");
    mUnicodeLTR.Assign("නම.උදාහරණ");
    mPunycodeLTR.Assign("xn--r0co.xn--ozc8dl2c3bxd");
    mUnicodeRTL.Assign("الاسم.مثال");
    mPunycodeRTL.Assign("xn--mgba0b1dh.xn--mgbh0fb");
    // Intentionally not assigning to mEmpty

    // For measuring the case inside nsStandardURL
    mUrlPlainASCII.Assign("https://example.com/");
    mUrlLeadingDigitASCII.Assign("https://1test.example/");
    mUrlUnicodeMixed.Assign("https://مثال.example/");
    mUrlPunycodeMixed.Assign("https://xn--mgbh0fb.example/");
    mUrlUnicodeLTR.Assign("https://නම.උදාහරණ/");
    mUrlPunycodeLTR.Assign("https://xn--r0co.xn--ozc8dl2c3bxd/");
    mUrlUnicodeRTL.Assign("https://الاسم.مثال/");
    mUrlPunycodeRTL.Assign("https://xn--mgba0b1dh.xn--mgbh0fb/");
  }

 public:
  nsCString mPlainASCII;
  nsCString mLeadingDigitASCII;
  nsCString mUnicodeMixed;
  nsCString mPunycodeMixed;
  nsCString mUnicodeLTR;
  nsCString mPunycodeLTR;
  nsCString mUnicodeRTL;
  nsCString mPunycodeRTL;
  nsCString mEmpty;  // Extremely suspicious measurement!
  nsCString mUrlPlainASCII;
  nsCString mUrlLeadingDigitASCII;
  nsCString mUrlUnicodeMixed;
  nsCString mUrlPunycodeMixed;
  nsCString mUrlUnicodeLTR;
  nsCString mUrlPunycodeLTR;
  nsCString mUrlUnicodeRTL;
  nsCString mUrlPunycodeRTL;
};

#define IDNA_ITERATIONS 50000

#define IDNA_BENCH(name, func, src)                             \
  MOZ_GTEST_BENCH_F(TestIDNA, name, [this] {                    \
    for (int i = 0; i < IDNA_ITERATIONS; i++) {                 \
      nsCString dst;                                            \
      func(*mozilla::BlackBox(&src), *mozilla::BlackBox(&dst)); \
    }                                                           \
  });

#define IDNA_URL_BENCH(name, src)                                            \
  MOZ_GTEST_BENCH_F(TestIDNA, name, [this] {                                 \
    for (int i = 0; i < IDNA_ITERATIONS; i++) {                              \
      nsCOMPtr<nsIURI> dst;                                                  \
      nsresult rv = NS_NewURI(getter_AddRefs(dst), *mozilla::BlackBox(&src), \
                              UTF_8_ENCODING);                               \
      ASSERT_EQ(NS_OK, rv);                                                  \
      mozilla::BlackBox(&dst);                                               \
    }                                                                        \
  });

IDNA_BENCH(BenchToASCIIPlainASCII, NS_DomainToASCII, mPlainASCII);
IDNA_BENCH(BenchToASCIILeadingDigitASCII, NS_DomainToASCII, mLeadingDigitASCII);
IDNA_BENCH(BenchToASCIIUnicodeMixed, NS_DomainToASCII, mUnicodeMixed);
IDNA_BENCH(BenchToASCIIPunycodeMixed, NS_DomainToASCII, mPunycodeMixed);
IDNA_BENCH(BenchToASCIIUnicodeLTR, NS_DomainToASCII, mUnicodeLTR);
IDNA_BENCH(BenchToASCIIPunycodeLTR, NS_DomainToASCII, mPunycodeLTR);
IDNA_BENCH(BenchToASCIIUnicodeRTL, NS_DomainToASCII, mUnicodeRTL);
IDNA_BENCH(BenchToASCIIPunycodeRTL, NS_DomainToASCII, mPunycodeRTL);
IDNA_BENCH(BenchToASCIIEmpty, NS_DomainToASCII, mEmpty);

IDNA_BENCH(BenchToDisplayPlainASCII, NS_DomainToDisplay, mPlainASCII);
IDNA_BENCH(BenchToDisplayLeadingDigitASCII, NS_DomainToDisplay,
           mLeadingDigitASCII);
IDNA_BENCH(BenchToDisplayUnicodeMixed, NS_DomainToDisplay, mUnicodeMixed);
IDNA_BENCH(BenchToDisplayPunycodeMixed, NS_DomainToDisplay, mPunycodeMixed);
IDNA_BENCH(BenchToDisplayUnicodeLTR, NS_DomainToDisplay, mUnicodeLTR);
IDNA_BENCH(BenchToDisplayPunycodeLTR, NS_DomainToDisplay, mPunycodeLTR);
IDNA_BENCH(BenchToDisplayUnicodeRTL, NS_DomainToDisplay, mUnicodeRTL);
IDNA_BENCH(BenchToDisplayPunycodeRTL, NS_DomainToDisplay, mPunycodeRTL);
IDNA_BENCH(BenchToDisplayEmpty, NS_DomainToDisplay, mEmpty);

IDNA_BENCH(BenchToUnicodePlainASCII, NS_DomainToUnicode, mPlainASCII);
IDNA_BENCH(BenchToUnicodeLeadingDigitASCII, NS_DomainToUnicode,
           mLeadingDigitASCII);
IDNA_BENCH(BenchToUnicodeUnicodeMixed, NS_DomainToUnicode, mUnicodeMixed);
IDNA_BENCH(BenchToUnicodePunycodeMixed, NS_DomainToUnicode, mPunycodeMixed);
IDNA_BENCH(BenchToUnicodeUnicodeLTR, NS_DomainToUnicode, mUnicodeLTR);
IDNA_BENCH(BenchToUnicodePunycodeLTR, NS_DomainToUnicode, mPunycodeLTR);
IDNA_BENCH(BenchToUnicodeUnicodeRTL, NS_DomainToUnicode, mUnicodeRTL);
IDNA_BENCH(BenchToUnicodePunycodeRTL, NS_DomainToUnicode, mPunycodeRTL);
IDNA_BENCH(BenchToUnicodeEmpty, NS_DomainToUnicode, mEmpty);

IDNA_URL_BENCH(BenchUrlPlainASCII, mUrlPlainASCII);
IDNA_URL_BENCH(BenchUrlLeadingDigitASCII, mUrlLeadingDigitASCII);
IDNA_URL_BENCH(BenchUrlUnicodeMixed, mUrlUnicodeMixed);
IDNA_URL_BENCH(BenchUrlPunycodeMixed, mUrlPunycodeMixed);
IDNA_URL_BENCH(BenchUrlUnicodeLTR, mUrlUnicodeLTR);
IDNA_URL_BENCH(BenchUrlPunycodeLTR, mUrlPunycodeLTR);
IDNA_URL_BENCH(BenchUrlUnicodeRTL, mUrlUnicodeRTL);
IDNA_URL_BENCH(BenchUrlPunycodeRTL, mUrlPunycodeRTL);
