/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/ArrayUtils.h"
#include "nsPrintfCString.h"
#include <stdio.h>

#include "ParseFTPList.h"

PRTime gTestTime = 0;

// Pretend this is the current time for the purpose of running the
// test. The day and year matter because they are used to figure out a
// default value when no year is specified. Tests will fail if this is
// changed too much.
const char* kDefaultTestTime = "01-Aug-2002 00:00:00 GMT";

static PRTime TestTime() { return gTestTime; }

static void ParseFTPLine(char* inputLine, FILE* resultFile, list_state* state) {
  struct list_result result;
  int rc = ParseFTPList(inputLine, state, &result, PR_GMTParameters, TestTime);

  if (rc == '?' || rc == '"') {
    // Ignore junk and comments.
    return;
  }

  if (!resultFile) {
    // No result file was passed in, so there's nothing to check.
    return;
  }

  char resultLine[512];
  ASSERT_NE(fgets(resultLine, sizeof(resultLine), resultFile), nullptr);

  nsPrintfCString parsed(
      "%02u-%02u-%04u  %02u:%02u:%02u %20s %.*s%s%.*s\n",
      (result.fe_time.tm_mday ? (result.fe_time.tm_month + 1) : 0),
      result.fe_time.tm_mday,
      (result.fe_time.tm_mday ? result.fe_time.tm_year : 0),
      result.fe_time.tm_hour, result.fe_time.tm_min, result.fe_time.tm_sec,
      (rc == 'd' ? "<DIR>         "
                 : (rc == 'l' ? "<JUNCTION>    " : result.fe_size)),
      (int)result.fe_fnlen, result.fe_fname,
      ((rc == 'l' && result.fe_lnlen) ? " -> " : ""),
      (int)((rc == 'l' && result.fe_lnlen) ? result.fe_lnlen : 0),
      ((rc == 'l' && result.fe_lnlen) ? result.fe_lname : ""));

  ASSERT_STREQ(parsed.get(), resultLine);
}

FILE* OpenResultFile(const char* resultFileName) {
  if (!resultFileName) {
    return nullptr;
  }

  FILE* resultFile = fopen(resultFileName, "r");
  EXPECT_NE(resultFile, nullptr);

  // Ignore anything in the expected result file before and including the first
  // blank line.
  char resultLine[512];
  while (fgets(resultLine, sizeof(resultLine), resultFile)) {
    size_t lineLen = strlen(resultLine);
    EXPECT_LT(lineLen, sizeof(resultLine) - 1);
    if (lineLen > 0 && resultLine[lineLen - 1] == '\n') {
      lineLen--;
    }
    if (lineLen == 0) {
      break;
    }
  }

  // There must be a blank line somewhere in the result file.
  EXPECT_EQ(strcmp(resultLine, "\n"), 0);

  return resultFile;
}

void ParseFTPFile(const char* inputFileName, const char* resultFileName) {
  printf("Checking %s\n", inputFileName);
  FILE* inFile = fopen(inputFileName, "r");
  ASSERT_NE(inFile, nullptr);

  FILE* resultFile = OpenResultFile(resultFileName);

  char inputLine[512];
  struct list_state state;
  memset(&state, 0, sizeof(state));
  while (fgets(inputLine, sizeof(inputLine), inFile)) {
    size_t lineLen = strlen(inputLine);
    EXPECT_LT(lineLen, sizeof(inputLine) - 1);
    if (lineLen > 0 && inputLine[lineLen - 1] == '\n') {
      lineLen--;
    }

    ParseFTPLine(inputLine, resultFile, &state);
  }

  // Make sure there are no extra lines in the result file.
  if (resultFile) {
    char resultLine[512];
    EXPECT_EQ(fgets(resultLine, sizeof(resultLine), resultFile), nullptr)
        << "There should not be more lines in the expected results file than "
           "in the parser output.";
    fclose(resultFile);
  }

  fclose(inFile);
}

static const char* testFiles[] = {
    "3-guess",   "C-VMold",      "C-zVM",      "D-WinNT",   "E-EPLF",
    "O-guess",   "R-dls",        "U-HellSoft", "U-hethmon", "U-murksw",
    "U-ncFTPd",  "U-NetPresenz", "U-NetWare",  "U-nogid",   "U-no_ug",
    "U-Novonyx", "U-proftpd",    "U-Surge",    "U-WarFTPd", "U-WebStar",
    "U-WinNT",   "U-wu",         "V-MultiNet", "V-VMS-mix",
};

TEST(ParseFTPTest, Check)
{
  PRStatus result = PR_ParseTimeString(kDefaultTestTime, true, &gTestTime);
  ASSERT_EQ(PR_SUCCESS, result);

  char inputFileName[200];
  char resultFileName[200];
  for (size_t test = 0; test < mozilla::ArrayLength(testFiles); ++test) {
    snprintf(inputFileName, mozilla::ArrayLength(inputFileName), "%s.in",
             testFiles[test]);
    snprintf(resultFileName, mozilla::ArrayLength(inputFileName), "%s.out",
             testFiles[test]);
    ParseFTPFile(inputFileName, resultFileName);
  }
}
