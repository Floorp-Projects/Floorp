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
#include "prtypes.h"

#if defined(XP_WIN) || defined(XP_OS2)
#define BINARY_MODE "b"
#else
#define BINARY_MODE
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

// very basic parser for updater.ini taken mostly from nsINIParser.cpp
int
ReadStrings(const char *path, StringTable *results)
{
  AutoFILE fp = fopen(path, "r" BINARY_MODE);

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

  int rd = fread(fileContents, sizeof(char), flen, fp);
  if (rd != flen)
    return READ_ERROR;

  fileContents[flen] = '\0';

  char *buffer = fileContents;
  PRBool inStringsSection = PR_FALSE;
  const unsigned None = 0, Title = 1, Info = 2, All = 3;
  unsigned read = None;

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
      else
        inStringsSection = strcmp(currSection, "Strings") == 0;

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

    if (strcmp(key, "Title") == 0) {
      strncpy(results->title, token, MAX_TEXT_LEN - 1);
      results->title[MAX_TEXT_LEN - 1] = 0;
      read |= Title;
    }
    else if (strcmp(key, "Info") == 0 || strcmp(key, "InfoText") == 0) {
      strncpy(results->info, token, MAX_TEXT_LEN - 1);
      results->info[MAX_TEXT_LEN - 1] = 0;
      read |= Info;
    }
  }

  return (read == All) ? OK : PARSE_ERROR;
}
