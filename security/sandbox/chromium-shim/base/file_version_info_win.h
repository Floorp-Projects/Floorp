/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is a partial implementation of Chromium's source file
// base/file_version_info_win.h.

#ifndef BASE_FILE_VERSION_INFO_WIN_H_
#define BASE_FILE_VERSION_INFO_WIN_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/version.h"

#include "mozilla/Assertions.h"

struct tagVS_FIXEDFILEINFO;
typedef tagVS_FIXEDFILEINFO VS_FIXEDFILEINFO;

namespace base {
class FilePath;
}

class FileVersionInfoWin {
 public:
  static std::unique_ptr<FileVersionInfoWin> CreateFileVersionInfoWin(
      const base::FilePath& file_path);

  // Get file version number in dotted version format.
  base::Version GetFileVersion() const;

 private:
  // |data| is a VS_VERSION_INFO resource. |language| and |code_page| are
  // extracted from the \VarFileInfo\Translation value of |data|.
  FileVersionInfoWin(std::vector<uint8_t>&& data,
                     WORD language,
                     WORD code_page);

  const std::vector<uint8_t> owned_data_;
  const void* const data_;
  const WORD language_;
  const WORD code_page_;

  // This is a reference for a portion of |data_|.
  const VS_FIXEDFILEINFO& fixed_file_info_;

  DISALLOW_COPY_AND_ASSIGN(FileVersionInfoWin);
};

#endif  // BASE_FILE_VERSION_INFO_WIN_H_
