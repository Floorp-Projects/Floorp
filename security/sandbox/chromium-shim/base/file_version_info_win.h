/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is a dummy version of Chromium source file base/file_version_info_win.h
// Within our copy of Chromium files FileVersionInfoWin is only used in
// base/win/windows_version.cc in GetVersionFromKernel32, which we don't use.

#ifndef BASE_FILE_VERSION_INFO_WIN_H_
#define BASE_FILE_VERSION_INFO_WIN_H_

struct tagVS_FIXEDFILEINFO;
typedef tagVS_FIXEDFILEINFO VS_FIXEDFILEINFO;

namespace base {
class FilePath;
}

class FileVersionInfoWin {
 public:
  static FileVersionInfoWin*
    CreateFileVersionInfo(const base::FilePath& file_path) { return nullptr; }

  VS_FIXEDFILEINFO* fixed_file_info() { return nullptr; }
};

#endif  // BASE_FILE_VERSION_INFO_WIN_H_
