/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Printf.h"

#include <cfloat>
#include <cmath>
#include <stdarg.h>

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wc++11-narrowing"
#elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wnarrowing"
#endif
namespace tiformat {
#include "glibc_printf_tests/tiformat.c"
}
namespace tllformat {
#include "glibc_printf_tests/tllformat.c"
}
#if defined(__clang__)
#  pragma clang diagnostic pop
#elif defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif
namespace tfformat {
#include "glibc_printf_tests/tfformat.c"
}

// A simple implementation of PrintfTarget, just for testing
// PrintfTarget::print.
class TestPrintfTarget : public mozilla::PrintfTarget {
 public:
  static const char* test_string;

  TestPrintfTarget() : mOut(0) { memset(mBuffer, '\0', sizeof(mBuffer)); }

  ~TestPrintfTarget() {
    MOZ_RELEASE_ASSERT(mOut == strlen(test_string));
    MOZ_RELEASE_ASSERT(strncmp(mBuffer, test_string, strlen(test_string)) == 0);
  }

  bool append(const char* sp, size_t len) override {
    if (mOut + len < sizeof(mBuffer)) {
      memcpy(&mBuffer[mOut], sp, len);
    }
    mOut += len;
    return true;
  }

 private:
  char mBuffer[100];
  size_t mOut;
};

const char* TestPrintfTarget::test_string = "test string";

static void TestPrintfTargetPrint() {
  TestPrintfTarget checker;
  checker.print("test string");
}

static bool MOZ_FORMAT_PRINTF(5, 6)
    check_print(const char* file, int line,
                bool (*cmp)(const char* a, const char* b), const char* expect,
                const char* fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  mozilla::SmprintfPointer output = mozilla::Vsmprintf(fmt, ap);
  va_end(ap);

  bool ret = output && cmp(output.get(), expect);
  if (!ret && strcmp(expect, "ignore") != 0) {
    fprintf(stderr, "(actual) \"%s\" != (expected) \"%s\" (%s:%d)\n",
            output.get(), expect, file, line);
  }
  return ret;
}

bool str_match(const char* a, const char* b) { return !strcmp(a, b); }

bool approx_match(const char* a, const char* b) {
  return tfformat::matches(const_cast<char*>(a), b);
}

#define print_one(...) check_print(__FILE__, __LINE__, str_match, __VA_ARGS__)

static const char* zero() { return nullptr; }

static void TestPrintfFormats() {
  MOZ_RELEASE_ASSERT(print_one("0", "%d", 0));
  MOZ_RELEASE_ASSERT(print_one("23", "%d", 23));
  MOZ_RELEASE_ASSERT(print_one("+23", "%+d", 23));
  MOZ_RELEASE_ASSERT(print_one("-23", "%+d", -23));
  MOZ_RELEASE_ASSERT(print_one("0023", "%04d", 23));
  MOZ_RELEASE_ASSERT(print_one("777777", "%04d", 777777));
  MOZ_RELEASE_ASSERT(print_one("  23", "% 4d", 23));
  MOZ_RELEASE_ASSERT(print_one("23  ", "%-4d", 23));
  MOZ_RELEASE_ASSERT(print_one("  23", "%*d", 4, 23));
  MOZ_RELEASE_ASSERT(print_one("-23    ", "%*d", -7, -23));
  MOZ_RELEASE_ASSERT(print_one("  077", "%5.3d", 77));
  MOZ_RELEASE_ASSERT(print_one("  077", "%5.*d", 3, 77));
  MOZ_RELEASE_ASSERT(print_one("  077", "%*.*d", 5, 3, 77));
  MOZ_RELEASE_ASSERT(print_one("077  ", "%*.*d", -5, 3, 77));
  MOZ_RELEASE_ASSERT(print_one("77   ", "%*.*d", -5, -3, 77));
  MOZ_RELEASE_ASSERT(print_one("-1", "%d", -1));
  MOZ_RELEASE_ASSERT(print_one("23", "%u", 23u));
  MOZ_RELEASE_ASSERT(print_one("0x17", "0x%x", 23u));
  MOZ_RELEASE_ASSERT(print_one("0xFF", "0x%X", 255u));
  MOZ_RELEASE_ASSERT(print_one("027", "0%o", 23u));
  MOZ_RELEASE_ASSERT(print_one("-1", "%hd", (short)-1));
  // A funny special case.
  MOZ_RELEASE_ASSERT(print_one("", "%.*d", 0, 0));
  // This could be expanded if need be, it's just convenient to do
  // it this way.
  if (sizeof(short) == 2) {
    MOZ_RELEASE_ASSERT(print_one("8000", "%hx", (unsigned short)0x8000));
  }
  MOZ_RELEASE_ASSERT(print_one("2305", "%ld", 2305l));
  MOZ_RELEASE_ASSERT(print_one("-2305", "%ld", -2305l));
  MOZ_RELEASE_ASSERT(print_one("0xf0f0", "0x%lx", 0xf0f0ul));
  MOZ_RELEASE_ASSERT(print_one("0", "%lld", 0ll));
  MOZ_RELEASE_ASSERT(print_one("2305", "%lld", 2305ll));
  MOZ_RELEASE_ASSERT(print_one("-2305", "%lld", -2305ll));
  // A funny special case.
  MOZ_RELEASE_ASSERT(print_one("", "%.*lld", 0, 0ll));
  MOZ_RELEASE_ASSERT(print_one("0xF0F0", "0x%llX", 0xf0f0ull));
  MOZ_RELEASE_ASSERT(print_one("27270", "%zu", (size_t)27270));
  MOZ_RELEASE_ASSERT(print_one("27270", "%tu", (ptrdiff_t)27270));
  MOZ_RELEASE_ASSERT(print_one("27270", "%ju", (intmax_t)27270));
  MOZ_RELEASE_ASSERT(print_one("hello", "he%so", "ll"));
  MOZ_RELEASE_ASSERT(print_one("hello   ", "%-8s", "hello"));
  MOZ_RELEASE_ASSERT(print_one("   hello", "%8s", "hello"));
  MOZ_RELEASE_ASSERT(print_one("hello   ", "%*s", -8, "hello"));
  MOZ_RELEASE_ASSERT(print_one("hello", "%.*s", 5, "hello there"));
  MOZ_RELEASE_ASSERT(print_one("", "%.*s", 0, "hello there"));
  MOZ_RELEASE_ASSERT(print_one("%%", "%%%%"));
  MOZ_RELEASE_ASSERT(print_one("0", "%p", (char*)0));
  MOZ_RELEASE_ASSERT(print_one("h", "%c", 'h'));
  MOZ_RELEASE_ASSERT(print_one("1.500000", "%f", 1.5f));
  MOZ_RELEASE_ASSERT(print_one("1.5", "%g", 1.5));
  MOZ_RELEASE_ASSERT(print_one("1.50000", "%.5f", 1.5));

  MOZ_RELEASE_ASSERT(print_one("z      ", "%-7s", "z"));
  MOZ_RELEASE_ASSERT(print_one("z      ", "%*s", -7, "z"));
  MOZ_RELEASE_ASSERT(print_one("hello", "%*s", -3, "hello"));

  MOZ_RELEASE_ASSERT(print_one("  q", "%3c", 'q'));
  MOZ_RELEASE_ASSERT(print_one("q  ", "%-3c", 'q'));
  MOZ_RELEASE_ASSERT(print_one("  q", "%*c", 3, 'q'));
  MOZ_RELEASE_ASSERT(print_one("q  ", "%*c", -3, 'q'));

  // Regression test for bug#1350097.  The bug was an assertion
  // failure caused by printing a very long floating point value.
  print_one("ignore", "%lf", DBL_MAX);

  // Regression test for bug#1517433.  The bug was an assertion
  // failure caused by printing a floating point value with a large
  // precision and/or width.
  print_one("ignore", "%500.500lf", DBL_MAX);

  MOZ_RELEASE_ASSERT(print_one("2727", "%" PRIu32, (uint32_t)2727));
  MOZ_RELEASE_ASSERT(print_one("aa7", "%" PRIx32, (uint32_t)2727));
  MOZ_RELEASE_ASSERT(print_one("2727", "%" PRIu64, (uint64_t)2727));
  MOZ_RELEASE_ASSERT(print_one("aa7", "%" PRIx64, (uint64_t)2727));

  int n1, n2;
  MOZ_RELEASE_ASSERT(print_one(" hi ", "%n hi %n", &n1, &n2));
  MOZ_RELEASE_ASSERT(n1 == 0);
  MOZ_RELEASE_ASSERT(n2 == 4);

  MOZ_RELEASE_ASSERT(print_one("23 % 24", "%2$ld %% %1$d", 24, 23l));
  MOZ_RELEASE_ASSERT(
      print_one("7 8 9 10", "%4$lld %3$ld %2$d %1$hd", (short)10, 9, 8l, 7ll));

  MOZ_RELEASE_ASSERT(print_one("0 ", "%2$p %1$n", &n1, zero()));
  MOZ_RELEASE_ASSERT(n1 == 2);

  MOZ_RELEASE_ASSERT(print_one("23 % 024", "%2$-3ld%%%1$4.3d", 24, 23l));
  MOZ_RELEASE_ASSERT(print_one("23 1.5", "%2$d %1$g", 1.5, 23));
  MOZ_RELEASE_ASSERT(
      print_one("ff number FF", "%3$llx %1$s %2$lX", "number", 255ul, 255ull));
  MOZ_RELEASE_ASSERT(
      print_one("7799 9977", "%2$zu %1$zu", (size_t)9977, (size_t)7799));
}

template <typename T, size_t N>
static void TestGlibcPrintf(T (&test_cases)[N], const char* file,
                            bool (*cmp)(const char* a, const char* b)) {
  bool ok = true;
  char fmt2[40];
  for (auto& line : test_cases) {
    // mozilla::PrintfTarget doesn't support the `#` flag character or the
    // `a` conversion specifier.
    if (!line.line || strchr(line.format_string, '#') ||
        strchr(line.format_string, 'a')) {
      continue;
    }

    // Derive the format string in the test case to add "2$" in the specifier
    // (transforming e.g. "%f" into "%2$f"), and append "%1$.0d".
    // The former will make the format string take the `line.value` as the
    // second argument, and the latter will make the first argument formatted
    // with no precision. We'll pass 0 as the first argument, such that the
    // formatted value for it is "", which means the expected result string
    // is still the same.
    MOZ_RELEASE_ASSERT(sizeof(fmt2) > strlen(line.format_string) + 8);
    const char* percent = strchr(line.format_string, '%');
    MOZ_RELEASE_ASSERT(percent);
    size_t percent_off = percent - line.format_string;
    memcpy(fmt2, line.format_string, percent_off + 1);
    memcpy(fmt2 + percent_off + 1, "2$", 2);
    strcpy(fmt2 + percent_off + 3, percent + 1);
    strcat(fmt2, "%1$.0d");

    int l = line.line;
    const char* res = line.result;
    const char* fmt = line.format_string;
    if (strchr(line.format_string, 'I')) {
      ok = check_print(file, l, cmp, res, fmt, (size_t)line.value) && ok;
      ok = check_print(file, l, cmp, res, fmt2, 0, (size_t)line.value) && ok;
    } else {
      ok = check_print(file, l, cmp, res, fmt, line.value) && ok;
      ok = check_print(file, l, cmp, res, fmt2, 0, line.value) && ok;
    }
  }
  MOZ_RELEASE_ASSERT(ok);
}

#if defined(XP_WIN)
int wmain()
#else
int main()
#endif  // defined(XP_WIN)
{
  TestPrintfFormats();
  TestPrintfTargetPrint();
  TestGlibcPrintf(tiformat::sprint_ints, "tiformat.c", str_match);
  TestGlibcPrintf(tllformat::sprint_ints, "tllformat.c", str_match);
  TestGlibcPrintf(tfformat::sprint_doubles, "tfformat.c", approx_match);

  // %f is actually a not very useful formatting specifier, and if you give
  // large numbers, it will print... large amounts of characters. Ensure
  // that it does (which requires a patch to double-conversion).
  mozilla::SmprintfPointer dbl_max = mozilla::Smprintf("%f", -DBL_MAX);
  MOZ_RELEASE_ASSERT(dbl_max);
  // Its length should be 309 digits before the dot, 6 after, plus the dot
  // and the negative sign.
  MOZ_RELEASE_ASSERT(strlen(dbl_max.get()) == 317);

  return 0;
}
