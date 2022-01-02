/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This binary tests the updater's ReadStrings ini parser and should run in a
 * directory with a Unicode character to test bug 473417.
 */
#ifdef XP_WIN
#  include <windows.h>
#  define NS_main wmain
#  define PATH_SEPARATOR_CHAR L'\\'
// On Windows, argv[0] can also have forward slashes instead
#  define ALT_PATH_SEPARATOR_CHAR L'/'
#else
#  include <unistd.h>
#  define NS_main main
#  define PATH_SEPARATOR_CHAR '/'
#endif

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "updater/resource.h"
#include "updater/progressui.h"
#include "common/readstrings.h"
#include "common/updatererrors.h"
#include "common/updatedefines.h"
#include "mozilla/ArrayUtils.h"

#ifndef MAXPATHLEN
#  ifdef PATH_MAX
#    define MAXPATHLEN PATH_MAX
#  elif defined(MAX_PATH)
#    define MAXPATHLEN MAX_PATH
#  elif defined(_MAX_PATH)
#    define MAXPATHLEN _MAX_PATH
#  elif defined(CCHMAXPATH)
#    define MAXPATHLEN CCHMAXPATH
#  else
#    define MAXPATHLEN 1024
#  endif
#endif

#define TEST_NAME "Updater ReadStrings"

using namespace mozilla;

static int gFailCount = 0;

/**
 * Prints the given failure message and arguments using printf, prepending
 * "TEST-UNEXPECTED-FAIL " for the benefit of the test harness and
 * appending "\n" to eliminate having to type it at each call site.
 */
void fail(const char* msg, ...) {
  va_list ap;

  printf("TEST-UNEXPECTED-FAIL | ");

  va_start(ap, msg);
  vprintf(msg, ap);
  va_end(ap);

  putchar('\n');
  ++gFailCount;
}

int NS_main(int argc, NS_tchar** argv) {
  printf("Running TestAUSReadStrings tests\n");

  int rv = 0;
  int retval;
  NS_tchar inifile[MAXPATHLEN];
  StringTable testStrings;

  NS_tchar* slash = NS_tstrrchr(argv[0], PATH_SEPARATOR_CHAR);
#ifdef ALT_PATH_SEPARATOR_CHAR
  NS_tchar* altslash = NS_tstrrchr(argv[0], ALT_PATH_SEPARATOR_CHAR);
  slash = (slash > altslash) ? slash : altslash;
#endif  // ALT_PATH_SEPARATOR_CHAR

  if (!slash) {
    fail("%s | unable to find platform specific path separator (check 1)",
         TEST_NAME);
    return 20;
  }

  *(++slash) = '\0';
  // Test success when the ini file exists with both Title and Info in the
  // Strings section and the values for Title and Info.
  NS_tsnprintf(inifile, ArrayLength(inifile), NS_T("%sTestAUSReadStrings1.ini"),
               argv[0]);
  retval = ReadStrings(inifile, &testStrings);
  if (retval == OK) {
    if (strcmp(testStrings.title.get(),
               "Title Test - \xD0\x98\xD1\x81\xD0\xBF\xD1\x8B"
               "\xD1\x82\xD0\xB0\xD0\xBD\xD0\xB8\xD0\xB5 "
               "\xCE\x94\xCE\xBF\xCE\xBA\xCE\xB9\xCE\xBC\xCE\xAE "
               "\xE3\x83\x86\xE3\x82\xB9\xE3\x83\x88 "
               "\xE6\xB8\xAC\xE8\xA9\xA6 "
               "\xE6\xB5\x8B\xE8\xAF\x95") != 0) {
      rv = 21;
      fail("%s | Title ini value incorrect (check 3)", TEST_NAME);
    }

    if (strcmp(testStrings.info.get(),
               "Info Test - \xD0\x98\xD1\x81\xD0\xBF\xD1\x8B"
               "\xD1\x82\xD0\xB0\xD0\xBD\xD0\xB8\xD0\xB5 "
               "\xCE\x94\xCE\xBF\xCE\xBA\xCE\xB9\xCE\xBC\xCE\xAE "
               "\xE3\x83\x86\xE3\x82\xB9\xE3\x83\x88 "
               "\xE6\xB8\xAC\xE8\xA9\xA6 "
               "\xE6\xB5\x8B\xE8\xAF\x95\xE2\x80\xA6") != 0) {
      rv = 22;
      fail("%s | Info ini value incorrect (check 4)", TEST_NAME);
    }
  } else {
    fail("%s | ReadStrings returned %i (check 2)", TEST_NAME, retval);
    rv = 23;
  }

  // Test failure when the ini file exists without Title and with Info in the
  // Strings section.
  NS_tsnprintf(inifile, ArrayLength(inifile), NS_T("%sTestAUSReadStrings2.ini"),
               argv[0]);
  retval = ReadStrings(inifile, &testStrings);
  if (retval != PARSE_ERROR) {
    rv = 24;
    fail("%s | ReadStrings returned %i (check 5)", TEST_NAME, retval);
  }

  // Test failure when the ini file exists with Title and without Info in the
  // Strings section.
  NS_tsnprintf(inifile, ArrayLength(inifile), NS_T("%sTestAUSReadStrings3.ini"),
               argv[0]);
  retval = ReadStrings(inifile, &testStrings);
  if (retval != PARSE_ERROR) {
    rv = 25;
    fail("%s | ReadStrings returned %i (check 6)", TEST_NAME, retval);
  }

  // Test failure when the ini file doesn't exist
  NS_tsnprintf(inifile, ArrayLength(inifile),
               NS_T("%sTestAUSReadStringsBogus.ini"), argv[0]);
  retval = ReadStrings(inifile, &testStrings);
  if (retval != READ_ERROR) {
    rv = 26;
    fail("%s | ini file doesn't exist (check 7)", TEST_NAME);
  }

  // Test reading a non-default section name
  NS_tsnprintf(inifile, ArrayLength(inifile), NS_T("%sTestAUSReadStrings3.ini"),
               argv[0]);
  retval =
      ReadStrings(inifile, "Title\0", 1, &testStrings.title, "BogusSection2");
  if (retval == OK) {
    if (strcmp(testStrings.title.get(), "Bogus Title") != 0) {
      rv = 27;
      fail("%s | Title ini value incorrect (check 9)", TEST_NAME);
    }
  } else {
    fail("%s | ReadStrings returned %i (check 8)", TEST_NAME, retval);
    rv = 28;
  }

  // Test reading an exceedingly long string
  NS_tsnprintf(inifile, ArrayLength(inifile), NS_T("%sTestAUSReadStrings4.ini"),
               argv[0]);
  retval = ReadStrings(inifile, "LongValue\0", 1, &testStrings.title);
  const char* expectedValue =
      "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Nullam id "
      "ipsum condimentum, faucibus ante porta, vehicula metus. Nunc nec luctus "
      "lorem. Nunc mattis viverra nisl, eu ornare dui feugiat id. Aenean "
      "commodo ligula porttitor elit aliquam, ut luctus nunc aliquam. In eu "
      "eros at nunc pulvinar porta. Praesent porta felis vitae massa "
      "sollicitudin, a vestibulum dolor rutrum. Aenean finibus, felis ac "
      "dictum hendrerit, ligula arcu semper enim, rhoncus consequat arcu orci "
      "nec est. Sed auctor hendrerit rhoncus. Maecenas dignissim lorem et "
      "tellus maximus, sit amet pretium urna imperdiet. Duis ut libero "
      "volutpat, rhoncus mi non, placerat lacus. Nunc id tortor in quam "
      "lacinia luctus. Nam eu maximus ipsum, eu bibendum enim. Ut iaculis "
      "maximus ipsum in condimentum. Aliquam tellus nulla, congue quis pretium "
      "a, posuere quis ligula. Donec vel quam ipsum. Pellentesque congue urna "
      "eget porttitor pulvinar. Proin non risus lacus. Vestibulum molestie et "
      "ligula sit amet pellentesque. Phasellus luctus auctor lorem, vel "
      "dapibus ante iaculis sed. Cras ligula ex, vehicula a dui vel, posuere "
      "fermentum elit. Vestibulum et nisi at libero maximus interdum a non ex. "
      "Ut ut leo in metus convallis porta a et libero. Pellentesque fringilla "
      "dolor sit amet eleifend fermentum. Quisque blandit dolor facilisis "
      "purus vulputate sodales eget ac arcu. Nulla pulvinar feugiat accumsan. "
      "Phasellus auctor nisl eget diam auctor, sit amet imperdiet mauris "
      "condimentum. In a risus ut felis lobortis facilisis.";
  if (retval == OK) {
    if (strcmp(testStrings.title.get(), expectedValue) != 0) {
      rv = 29;
      fail("%s | LongValue ini value incorrect (check 10)", TEST_NAME);
    }
  } else {
    fail("%s | ReadStrings returned %i (check 11)", TEST_NAME, retval);
    rv = 30;
  }

  if (rv == 0) {
    printf("TEST-PASS | %s | all checks passed\n", TEST_NAME);
  } else {
    fail("%s | %i out of 9 checks failed", TEST_NAME, gFailCount);
  }

  return rv;
}
