/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file was shamelessly copied from mozilla/xpinstall/wizard/unix/src2

#ifndef nsINIParser_h__
#define nsINIParser_h__

#ifdef MOZILLA_INTERNAL_API
#  define nsINIParser nsINIParser_internal
#endif

#include "nscore.h"
#include "nsClassHashtable.h"
#include "mozilla/UniquePtr.h"

#include <stdio.h>

class nsIFile;

class nsINIParser {
 public:
  nsINIParser() {}
  ~nsINIParser() = default;

  /**
   * Initialize the INIParser with a nsIFile. If this method fails, no
   * other methods should be called. This method reads and parses the file,
   * the class does not hold a file handle open. An instance must only be
   * initialized once.
   */
  nsresult Init(nsIFile* aFile);

  /**
   * Callback for GetSections
   * @return false to stop enumeration, or true to continue.
   */
  typedef bool (*INISectionCallback)(const char* aSection, void* aClosure);

  /**
   * Enumerate the sections within the INI file.
   */
  nsresult GetSections(INISectionCallback aCB, void* aClosure);

  /**
   * Callback for GetStrings
   * @return false to stop enumeration, or true to continue
   */
  typedef bool (*INIStringCallback)(const char* aString, const char* aValue,
                                    void* aClosure);

  /**
   * Enumerate the strings within a section. If the section does
   * not exist, this function will silently return.
   */
  nsresult GetStrings(const char* aSection, INIStringCallback aCB,
                      void* aClosure);

  /**
   * Get the value of the specified key in the specified section
   * of the INI file represented by this instance.
   *
   * @param aSection      section name
   * @param aKey          key name
   * @param aResult       the value found
   * @throws NS_ERROR_FAILURE if the specified section/key could not be
   *                          found.
   */
  nsresult GetString(const char* aSection, const char* aKey,
                     nsACString& aResult);

  /**
   * Alternate signature of GetString that uses a pre-allocated buffer
   * instead of a nsACString (for use in the standalone glue before
   * the glue is initialized).
   *
   * @throws NS_ERROR_LOSS_OF_SIGNIFICANT_DATA if the aResult buffer is not
   *         large enough for the data. aResult will be filled with as
   *         much data as possible.
   *
   * @see GetString [1]
   */
  nsresult GetString(const char* aSection, const char* aKey, char* aResult,
                     uint32_t aResultLen);

  /**
   * Sets the value of the specified key in the specified section. The section
   * is created if it does not already exist.
   *
   * @oaram aSection      section name
   * @param aKey          key name
   * @param aValue        the value to set
   */
  nsresult SetString(const char* aSection, const char* aKey,
                     const char* aValue);

  /**
   * Deletes the value of the specified key in the specified section.
   *
   * @param aSection      section name
   * @param aKey          key name
   *
   * @throws NS_ERROR_FAILURE if the string was not set.
   */
  nsresult DeleteString(const char* aSection, const char* aKey);

  /**
   * Deletes the specified section.
   *
   * @param aSection      section name
   *
   * @throws NS_ERROR_FAILURE if the section did not exist.
   */
  nsresult DeleteSection(const char* aSection);

  /**
   * Renames the specified section.
   *
   * @param aSection      section name
   * @param aNewName      new section name
   *
   * @throws NS_ERROR_FAILURE if the section did not exist.
   * @throws NS_ERROR_ILLEGAL_VALUE if the new section name already exists.
   */
  nsresult RenameSection(const char* aSection, const char* aNewName);

  /**
   * Writes the ini data to disk.
   * @param aFile         the file to write to
   * @throws NS_ERROR_FAILURE on failure.
   */
  nsresult WriteToFile(nsIFile* aFile);

 private:
  struct INIValue {
    INIValue(const char* aKey, const char* aValue)
        : key(strdup(aKey)), value(strdup(aValue)) {}

    ~INIValue() {
      delete key;
      delete value;
    }

    void SetValue(const char* aValue) {
      delete value;
      value = strdup(aValue);
    }

    const char* key;
    const char* value;
    mozilla::UniquePtr<INIValue> next;
  };

  nsClassHashtable<nsCharPtrHashKey, INIValue> mSections;

  nsresult InitFromString(const nsCString& aStr);

  bool IsValidSection(const char* aSection);
  bool IsValidKey(const char* aKey);
  bool IsValidValue(const char* aValue);
};

#endif /* nsINIParser_h__ */
