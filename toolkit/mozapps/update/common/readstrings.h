/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef READSTRINGS_H__
#define READSTRINGS_H__

#include "mozilla/Maybe.h"
#include "mozilla/UniquePtr.h"

#include <vector>

#ifdef XP_WIN
#  include <windows.h>
typedef WCHAR NS_tchar;
#else
typedef char NS_tchar;
#endif

struct StringTable {
  mozilla::UniquePtr<char[]> title;
  mozilla::UniquePtr<char[]> info;
};

/**
 * This function reads in localized strings from updater.ini
 */
int ReadStrings(const NS_tchar* path, StringTable* results);

/**
 * This function reads in localized strings corresponding to the keys from a
 * given .ini
 */
int ReadStrings(const NS_tchar* path, const char* keyList,
                unsigned int numStrings, mozilla::UniquePtr<char[]>* results,
                const char* section = nullptr);

/**
 * This class is meant to be a slightly cleaner interface into the ReadStrings
 * function.
 */
class IniReader {
 public:
  // IniReader must be initialized with the path of the INI file and a
  // section to read from. If the section is null or not specified, the
  // default section name ("Strings") will be used.
  explicit IniReader(const NS_tchar* iniPath, const char* section = nullptr);

  // Records a key that ought to be read from the INI file. When
  // IniReader::Read() is invoked it will, if successful, store the value
  // corresponding to the given key in the UniquePtr given.
  // If IniReader::Read() has already been invoked, these functions do nothing.
  // The given key must not be the empty string.
  void AddKey(const char* key, mozilla::UniquePtr<char[]>* outputPtr);
#ifdef XP_WIN
  void AddKey(const char* key, mozilla::UniquePtr<wchar_t[]>* outputPtr);
#endif
  bool HasRead() { return mMaybeStatusCode.isSome(); }
  // Performs the actual reading and assigns values to the requested locations.
  // Returns the same possible values that `ReadStrings` returns.
  // If this is called more than once, no action will be taken on subsequent
  // calls, and the stored status code will be returned instead.
  int Read();

 private:
  bool MaybeAddKey(const char* key, size_t& insertionIndex);

  mozilla::UniquePtr<NS_tchar[]> mPath;
  mozilla::UniquePtr<char[]> mSection;
  std::vector<mozilla::UniquePtr<char[]>> mKeys;

  template <class T>
  struct ValueOutput {
    size_t keyIndex;
    T* outputPtr;
  };

  // Stores associations between keys and the buffers where their values will
  // be stored.
  std::vector<ValueOutput<mozilla::UniquePtr<char[]>>> mNarrowOutputs;
#ifdef XP_WIN
  std::vector<ValueOutput<mozilla::UniquePtr<wchar_t[]>>> mWideOutputs;
#endif
  // If we have attempted to read the INI, this will store the resulting
  // status code.
  mozilla::Maybe<int> mMaybeStatusCode;
};

#endif  // READSTRINGS_H__
