/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Robert Strong <robert.bugzilla@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/**
 * This binary tests the updater's ReadStrings ini parser and should run in a
 * directory with a Unicode character to test bug 473417.
 */
#if defined(XP_WIN) || defined(XP_OS2)
  #include <windows.h>
  #define NS_main wmain
  #define NS_tstrrchr wcsrchr
  #define NS_tsnprintf _snwprintf
  #define NS_T(str) L ## str
  #define PATH_SEPARATOR_CHAR L'\\'
#else
  #include <unistd.h>
  #define NS_main main
  #define NS_tstrrchr strrchr
  #define NS_tsnprintf snprintf
  #define NS_T(str) str
  #define PATH_SEPARATOR_CHAR '/'
#endif

#include <stdio.h>
#include <string.h>

#include "resource.h"
#include "progressui.h"
#include "readstrings.h"
#include "errors.h"

#ifndef MAXPATHLEN
# ifdef PATH_MAX
#  define MAXPATHLEN PATH_MAX
# elif defined(_MAX_PATH)
#  define MAXPATHLEN MAX_PATH
# elif defined(_MAX_PATH)
#  define MAXPATHLEN _MAX_PATH
# elif defined(CCHMAXPATH)
#  define MAXPATHLEN CCHMAXPATH
# else
#  define MAXPATHLEN 1024
# endif
#endif

#define TEST_NAME "Updater ReadStrings"
#define UNEXPECTED_FAIL_PREFIX "*** TEST-UNEXPECTED-FAIL"

int NS_main(int argc, NS_tchar **argv)
{
  printf("Running TestAUSReadStrings tests\n");

  int rv = 0;
  int retval;
  NS_tchar inifile[MAXPATHLEN];
  StringTable testStrings;

  NS_tchar *slash = NS_tstrrchr(argv[0], PATH_SEPARATOR_CHAR);
  if (!slash) {
    printf("%s | %s | unable to find platform specific path separator '%s'\n",
           UNEXPECTED_FAIL_PREFIX, TEST_NAME, PATH_SEPARATOR_CHAR);
    return 20;
  }

  *(++slash) = '\0';

  // Test success when the ini file exists with both Title and InfoText in the
  // Strings section and test values for Title and InfoText.
  NS_tsnprintf(inifile, sizeof(inifile), NS_T("%sTestAUSReadStrings1.ini"), argv[0]);
  retval = ReadStrings(inifile, &testStrings);
  if (retval == OK) {
    if (strcmp(testStrings.title, "Title Test - 测试 測試 Δοκιμή テスト Испытание") != 0) {
      rv = 21;
      printf("%s | %s Title ini value incorrect | Test 1\n",
             UNEXPECTED_FAIL_PREFIX, TEST_NAME);
    }

    if (strcmp(testStrings.info, "InfoText Test - 测试 測試 Δοκιμή テスト Испытание…") != 0) {
      rv = 22;
      printf("%s | %s InfoText ini value incorrect | Test 1\n",
             UNEXPECTED_FAIL_PREFIX, TEST_NAME);
    }
  }
  else {
    printf("%s | %s ReadStrings returned %i | Test 1\n",
           UNEXPECTED_FAIL_PREFIX, TEST_NAME, retval);
    rv = 23;
  }

  // Test success when the ini file exists with both Title and InfoText in the
  // Strings section and test values for Title and InfoText.
  NS_tsnprintf(inifile, sizeof(inifile), NS_T("%sTestAUSReadStrings2.ini"), argv[0]);
  retval = ReadStrings(inifile, &testStrings);
  if (retval == OK) {
    if (strcmp(testStrings.title, "Title Test - Испытание Δοκιμή テスト 測試 测试") != 0) {
      rv = 24;
      printf("%s | %s Title ini value incorrect | Test 2\n",
             UNEXPECTED_FAIL_PREFIX, TEST_NAME);
    }

    if (strcmp(testStrings.info, "Info Test - Испытание Δοκιμή テスト 測試 测试…") != 0) {
      rv = 25;
      printf("%s | %s Info ini value incorrect | Test 2\n",
             UNEXPECTED_FAIL_PREFIX, TEST_NAME);
    }
  }
  else {
    printf("%s | %s ReadStrings returned %i | Test 2\n",
           UNEXPECTED_FAIL_PREFIX, TEST_NAME, retval);
    rv = 26;
  }

  // Test failure when the ini file exists without Title and with Info and
  // InfoText in the Strings section.
  NS_tsnprintf(inifile, sizeof(inifile), NS_T("%sTestAUSReadStrings3.ini"), argv[0]);
  retval = ReadStrings(inifile, &testStrings);
  if (retval != PARSE_ERROR) {
    rv = 27;
    printf("%s | %s %s ReadStrings returned %i | Test 3\n",
           UNEXPECTED_FAIL_PREFIX, TEST_NAME, retval);
  }

  // Test failure when the ini file exists with Title and without InfoText and
  // Info in the Strings section.
  NS_tsnprintf(inifile, sizeof(inifile), NS_T("%sTestAUSReadStrings4.ini"), argv[0]);
  retval = ReadStrings(inifile, &testStrings);
  if (retval != PARSE_ERROR) {
    rv = 28;
    printf("%s | %s ReadStrings returned %i | Test 4\n",
           UNEXPECTED_FAIL_PREFIX, TEST_NAME, retval);
  }

  // Test failure when the ini file doesn't exist
  NS_tsnprintf(inifile, sizeof(inifile), NS_T("%sTestAUSReadStringsBogus.ini"), argv[0]);
  retval = ReadStrings(inifile, &testStrings);
  if (retval != READ_ERROR) {
    rv = 29;
    printf("%s | %s ini file doesn't exist | Test 5\n",
           UNEXPECTED_FAIL_PREFIX, TEST_NAME);
  }


  if (rv == 0) {
    printf("*** TEST-PASS | %s | all tests passed\n", TEST_NAME);
  } else {
    printf("*** TEST-FAIL | %s | some tests failed\n", TEST_NAME);
  }

  return rv;
}
