/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef READSTRINGS_H__
#define READSTRINGS_H__

#define MAX_TEXT_LEN 600

#ifdef XP_WIN
# include <windows.h>
  typedef WCHAR NS_tchar;
#else
  typedef char NS_tchar;
#endif

struct StringTable 
{
  char title[MAX_TEXT_LEN];
  char info[MAX_TEXT_LEN];
};

/**
 * This function reads in localized strings from updater.ini
 */
int ReadStrings(const NS_tchar *path, StringTable *results);

/**
 * This function reads in localized strings corresponding to the keys from a given .ini
 */
int ReadStrings(const NS_tchar *path,
                const char *keyList,
                unsigned int numStrings,
                char results[][MAX_TEXT_LEN],
                const char *section = NULL);

#endif  // READSTRINGS_H__
