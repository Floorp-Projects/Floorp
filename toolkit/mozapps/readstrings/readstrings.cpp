/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
 *  Alex Pakhotin <alexp@mozilla.com>
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

#include <limits.h>
#include <string.h>
#include <stdio.h>
#include "readstrings.h"
#include "errors.h"

// Defined bool stuff here to reduce external dependencies
typedef int        PRBool;
#define PR_TRUE    1
#define PR_FALSE   0

#ifdef XP_WIN
# define NS_tfopen _wfopen
# define OPEN_MODE L"rb"
#else
# define NS_tfopen fopen
# define OPEN_MODE "r"
#endif

// stack based FILE wrapper to ensure that fclose is called.
class AutoFILE {
public:
  AutoFILE(FILE *fp) : fp_(fp) {}
  ~AutoFILE() { if (fp_) fclose(fp_); }
  operator FILE *() { return fp_; }
private:
  FILE *fp_;
};

static const char kNL[] = "\r\n";
static const char kEquals[] = "=";
static const char kWhitespace[] = " \t";
static const char kRBracket[] = "]";

static const char*
NS_strspnp(const char *delims, const char *str)
{
  const char *d;
  do {
    for (d = delims; *d != '\0'; ++d) {
      if (*str == *d) {
        ++str;
        break;
      }
    }
  } while (*d);

  return str;
}

static char*
NS_strtok(const char *delims, char **str)
{
  if (!*str)
    return NULL;

  char *ret = (char*) NS_strspnp(delims, *str);

  if (!*ret) {
    *str = ret;
    return NULL;
  }

  char *i = ret;
  do {
    for (const char *d = delims; *d != '\0'; ++d) {
      if (*i == *d) {
        *i = '\0';
        *str = ++i;
        return ret;
      }
    }
    ++i;
  } while (*i);

  *str = NULL;
  return ret;
}

/**
 * Find a key in a keyList containing zero-delimited keys ending with "\0\0".
 * Returns a zero-based index of the key in the list, or -1 if the key is not found.
 */
static int
find_key(const char *keyList, char* key)
{
  if (!keyList)
    return -1;

  int index = 0;
  const char *p = keyList;
  while (*p)
  {
    if (strcmp(key, p) == 0)
      return index;

    p += strlen(p) + 1;
    index++;
  }

  // The key was not found if we came here
  return -1;
}

/**
 * A very basic parser for updater.ini taken mostly from nsINIParser.cpp
 * that can be used by standalone apps.
 *
 * @param path       Path to the .ini file to read
 * @param keyList    List of zero-delimited keys ending with two zero characters
 * @param numStrings Number of strings to read into results buffer - must be equal to the number of keys
 * @param results    Two-dimensional array of strings to be filled in the same order as the keys provided
 * @param section    Optional name of the section to read; defaults to "Strings"
 */
int
ReadStrings(const NS_tchar *path,
            const char *keyList,
            unsigned int numStrings,
            char results[][MAX_TEXT_LEN],
            const char *section)
{
  AutoFILE fp = NS_tfopen(path, OPEN_MODE);

  if (!fp)
    return READ_ERROR;

  /* get file size */
  if (fseek(fp, 0, SEEK_END) != 0)
    return READ_ERROR;

  long flen = ftell(fp);
  if (flen == 0)
    return READ_ERROR;

  char *fileContents = new char[flen + 1];
  if (!fileContents)
    return MEM_ERROR;

  /* read the file in one swoop */
  if (fseek(fp, 0, SEEK_SET) != 0)
    return READ_ERROR;

  size_t rd = fread(fileContents, sizeof(char), flen, fp);
  if (rd != flen)
    return READ_ERROR;

  fileContents[flen] = '\0';

  char *buffer = fileContents;
  PRBool inStringsSection = PR_FALSE;

  unsigned int read = 0;

  while (char *token = NS_strtok(kNL, &buffer)) {
    if (token[0] == '#' || token[0] == ';') // it's a comment
      continue;

    token = (char*) NS_strspnp(kWhitespace, token);
    if (!*token) // empty line
      continue;

    if (token[0] == '[') { // section header!
      ++token;
      char const * currSection = token;

      char *rb = NS_strtok(kRBracket, &token);
      if (!rb || NS_strtok(kWhitespace, &token)) {
        // there's either an unclosed [Section or a [Section]Moretext!
        // we could frankly decide that this INI file is malformed right
        // here and stop, but we won't... keep going, looking for
        // a well-formed [section] to continue working with
        inStringsSection = PR_FALSE;
      }
      else {
        if (section)
          inStringsSection = strcmp(currSection, section) == 0;
        else
          inStringsSection = strcmp(currSection, "Strings") == 0;
      }

      continue;
    }

    if (!inStringsSection) {
      // If we haven't found a section header (or we found a malformed
      // section header), or this isn't the [Strings] section don't bother
      // parsing this line.
      continue;
    }

    char *key = token;
    char *e = NS_strtok(kEquals, &token);
    if (!e)
      continue;

    int keyIndex = find_key(keyList, key);
    if (keyIndex >= 0 && (unsigned int)keyIndex < numStrings)
    {
      strncpy(results[keyIndex], token, MAX_TEXT_LEN - 1);
      results[keyIndex][MAX_TEXT_LEN - 1] = 0;
      read++;
    }
  }

  return (read == numStrings) ? OK : PARSE_ERROR;
}

// A wrapper function to read strings for the updater.
// Added for compatibility with the original code.
int
ReadStrings(const NS_tchar *path, StringTable *results)
{
  const unsigned int kNumStrings = 2;
  const char *kUpdaterKeys = "Title\0Info\0";
  char updater_strings[kNumStrings][MAX_TEXT_LEN];

  int result = ReadStrings(path, kUpdaterKeys, kNumStrings, updater_strings);

  strncpy(results->title, updater_strings[0], MAX_TEXT_LEN - 1);
  results->title[MAX_TEXT_LEN - 1] = 0;
  strncpy(results->info, updater_strings[1], MAX_TEXT_LEN - 1);
  results->info[MAX_TEXT_LEN - 1] = 0;

  return result;
}
