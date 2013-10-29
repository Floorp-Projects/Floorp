// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILE_VERSION_INFO_WIN_H_
#define BASE_FILE_VERSION_INFO_WIN_H_

#include <string>

#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/file_version_info.h"
#include "base/memory/scoped_ptr.h"

struct tagVS_FIXEDFILEINFO;
typedef tagVS_FIXEDFILEINFO VS_FIXEDFILEINFO;

class FileVersionInfoWin : public FileVersionInfo {
 public:
  BASE_EXPORT FileVersionInfoWin(void* data, int language, int code_page);
  BASE_EXPORT ~FileVersionInfoWin();

  // Accessors to the different version properties.
  // Returns an empty string if the property is not found.
  virtual string16 company_name() OVERRIDE;
  virtual string16 company_short_name() OVERRIDE;
  virtual string16 product_name() OVERRIDE;
  virtual string16 product_short_name() OVERRIDE;
  virtual string16 internal_name() OVERRIDE;
  virtual string16 product_version() OVERRIDE;
  virtual string16 private_build() OVERRIDE;
  virtual string16 special_build() OVERRIDE;
  virtual string16 comments() OVERRIDE;
  virtual string16 original_filename() OVERRIDE;
  virtual string16 file_description() OVERRIDE;
  virtual string16 file_version() OVERRIDE;
  virtual string16 legal_copyright() OVERRIDE;
  virtual string16 legal_trademarks() OVERRIDE;
  virtual string16 last_change() OVERRIDE;
  virtual bool is_official_build() OVERRIDE;

  // Lets you access other properties not covered above.
  BASE_EXPORT bool GetValue(const wchar_t* name, std::wstring* value);

  // Similar to GetValue but returns a wstring (empty string if the property
  // does not exist).
  BASE_EXPORT std::wstring GetStringValue(const wchar_t* name);

  // Get the fixed file info if it exists. Otherwise NULL
  VS_FIXEDFILEINFO* fixed_file_info() { return fixed_file_info_; }

 private:
  scoped_ptr_malloc<char> data_;
  int language_;
  int code_page_;
  // This is a pointer into the data_ if it exists. Otherwise NULL.
  VS_FIXEDFILEINFO* fixed_file_info_;

  DISALLOW_COPY_AND_ASSIGN(FileVersionInfoWin);
};

#endif  // BASE_FILE_VERSION_INFO_WIN_H_
