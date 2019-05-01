/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <limits.h>
#include <string.h>
#include <stdio.h>
#include "readstrings.h"
#include "updatererrors.h"

#ifdef XP_WIN
#  define NS_tfopen _wfopen
#  define OPEN_MODE L"rb"
#else
#  define NS_tfopen fopen
#  define OPEN_MODE "r"
#endif

// stack based FILE wrapper to ensure that fclose is called.
class AutoFILE {
 public:
  explicit AutoFILE(FILE* fp) : fp_(fp) {}
  ~AutoFILE() {
    if (fp_) {
      fclose(fp_);
    }
  }
  operator FILE*() { return fp_; }

 private:
  FILE* fp_;
};

class AutoCharArray {
 public:
  explicit AutoCharArray(size_t len) { ptr_ = new char[len]; }
  ~AutoCharArray() { delete[] ptr_; }
  operator char*() { return ptr_; }

 private:
  char* ptr_;
};

static const char kNL[] = "\r\n";
static const char kEquals[] = "=";
static const char kWhitespace[] = " \t";
static const char kRBracket[] = "]";

static const char* NS_strspnp(const char* delims, const char* str) {
  const char* d;
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

static char* NS_strtok(const char* delims, char** str) {
  if (!*str) {
    return nullptr;
  }

  char* ret = (char*)NS_strspnp(delims, *str);

  if (!*ret) {
    *str = ret;
    return nullptr;
  }

  char* i = ret;
  do {
    for (const char* d = delims; *d != '\0'; ++d) {
      if (*i == *d) {
        *i = '\0';
        *str = ++i;
        return ret;
      }
    }
    ++i;
  } while (*i);

  *str = nullptr;
  return ret;
}

/**
 * Find a key in a keyList containing zero-delimited keys ending with "\0\0".
 * Returns a zero-based index of the key in the list, or -1 if the key is not
 * found.
 */
static int find_key(const char* keyList, char* key) {
  if (!keyList) {
    return -1;
  }

  int index = 0;
  const char* p = keyList;
  while (*p) {
    if (strcmp(key, p) == 0) {
      return index;
    }

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
 * @param numStrings Number of strings to read into results buffer - must be
 *                   equal to the number of keys
 * @param results    Two-dimensional array of strings to be filled in the same
 *                   order as the keys provided
 * @param section    Optional name of the section to read; defaults to "Strings"
 */
int ReadStrings(const NS_tchar* path, const char* keyList,
                unsigned int numStrings, char results[][MAX_TEXT_LEN],
                const char* section) {
  AutoFILE fp(NS_tfopen(path, OPEN_MODE));

  if (!fp) {
    return READ_ERROR;
  }

  /* get file size */
  if (fseek(fp, 0, SEEK_END) != 0) {
    return READ_ERROR;
  }

  long len = ftell(fp);
  if (len <= 0) {
    return READ_ERROR;
  }

  size_t flen = size_t(len);
  AutoCharArray fileContents(flen + 1);
  if (!fileContents) {
    return READ_STRINGS_MEM_ERROR;
  }

  /* read the file in one swoop */
  if (fseek(fp, 0, SEEK_SET) != 0) {
    return READ_ERROR;
  }

  size_t rd = fread(fileContents, sizeof(char), flen, fp);
  if (rd != flen) {
    return READ_ERROR;
  }

  fileContents[flen] = '\0';

  char* buffer = fileContents;
  bool inStringsSection = false;

  unsigned int read = 0;

  while (char* token = NS_strtok(kNL, &buffer)) {
    if (token[0] == '#' || token[0] == ';') {  // it's a comment
      continue;
    }

    token = (char*)NS_strspnp(kWhitespace, token);
    if (!*token) {  // empty line
      continue;
    }

    if (token[0] == '[') {  // section header!
      ++token;
      char const* currSection = token;

      char* rb = NS_strtok(kRBracket, &token);
      if (!rb || NS_strtok(kWhitespace, &token)) {
        // there's either an unclosed [Section or a [Section]Moretext!
        // we could frankly decide that this INI file is malformed right
        // here and stop, but we won't... keep going, looking for
        // a well-formed [section] to continue working with
        inStringsSection = false;
      } else {
        if (section) {
          inStringsSection = strcmp(currSection, section) == 0;
        } else {
          inStringsSection = strcmp(currSection, "Strings") == 0;
        }
      }

      continue;
    }

    if (!inStringsSection) {
      // If we haven't found a section header (or we found a malformed
      // section header), or this isn't the [Strings] section don't bother
      // parsing this line.
      continue;
    }

    char* key = token;
    char* e = NS_strtok(kEquals, &token);
    if (!e) {
      continue;
    }

    int keyIndex = find_key(keyList, key);
    if (keyIndex >= 0 && (unsigned int)keyIndex < numStrings) {
      strncpy(results[keyIndex], token, MAX_TEXT_LEN - 1);
      results[keyIndex][MAX_TEXT_LEN - 1] = '\0';
      read++;
    }
  }

  return (read == numStrings) ? OK : PARSE_ERROR;
}

// A wrapper function to read strings for the updater.
// Added for compatibility with the original code.
int ReadStrings(const NS_tchar* path, StringTable* results) {
  const unsigned int kNumStrings = 2;
  const char* kUpdaterKeys = "Title\0Info\0";
  char updater_strings[kNumStrings][MAX_TEXT_LEN];

  int result = ReadStrings(path, kUpdaterKeys, kNumStrings, updater_strings);

  strncpy(results->title, updater_strings[0], MAX_TEXT_LEN - 1);
  results->title[MAX_TEXT_LEN - 1] = '\0';
  strncpy(results->info, updater_strings[1], MAX_TEXT_LEN - 1);
  results->info[MAX_TEXT_LEN - 1] = '\0';

  return result;
}
