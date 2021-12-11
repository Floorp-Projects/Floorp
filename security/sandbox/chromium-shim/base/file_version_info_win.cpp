/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is a partial implementation of Chromium's source file
// base/file_version_info_win.cc

#include "base/file_version_info_win.h"

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/threading/scoped_blocking_call.h"

#include "mozilla/Unused.h"

namespace {

struct LanguageAndCodePage {
  WORD language;
  WORD code_page;
};

// Returns the \VarFileInfo\Translation value extracted from the
// VS_VERSION_INFO resource in |data|.
LanguageAndCodePage* GetTranslate(const void* data) {
  static constexpr wchar_t kTranslation[] = L"\\VarFileInfo\\Translation";
  LPVOID translate = nullptr;
  UINT dummy_size;
  if (::VerQueryValue(data, kTranslation, &translate, &dummy_size))
    return static_cast<LanguageAndCodePage*>(translate);
  return nullptr;
}

const VS_FIXEDFILEINFO& GetVsFixedFileInfo(const void* data) {
  static constexpr wchar_t kRoot[] = L"\\";
  LPVOID fixed_file_info = nullptr;
  UINT dummy_size;
  CHECK(::VerQueryValue(data, kRoot, &fixed_file_info, &dummy_size));
  return *static_cast<VS_FIXEDFILEINFO*>(fixed_file_info);
}

}  // namespace

// static
std::unique_ptr<FileVersionInfoWin>
FileVersionInfoWin::CreateFileVersionInfoWin(const base::FilePath& file_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  DWORD dummy;
  const wchar_t* path = file_path.value().c_str();
  const DWORD length = ::GetFileVersionInfoSize(path, &dummy);
  if (length == 0)
    return nullptr;

  std::vector<uint8_t> data(length, 0);

  if (!::GetFileVersionInfo(path, dummy, length, data.data()))
    return nullptr;

  const LanguageAndCodePage* translate = GetTranslate(data.data());
  if (!translate)
    return nullptr;

  return base::WrapUnique(new FileVersionInfoWin(
      std::move(data), translate->language, translate->code_page));
}

base::Version FileVersionInfoWin::GetFileVersion() const {
  return base::Version({HIWORD(fixed_file_info_.dwFileVersionMS),
                        LOWORD(fixed_file_info_.dwFileVersionMS),
                        HIWORD(fixed_file_info_.dwFileVersionLS),
                        LOWORD(fixed_file_info_.dwFileVersionLS)});
}

FileVersionInfoWin::FileVersionInfoWin(std::vector<uint8_t>&& data,
                                       WORD language,
                                       WORD code_page)
    : owned_data_(std::move(data)),
      data_(owned_data_.data()),
      language_(language),
      code_page_(code_page),
      fixed_file_info_(GetVsFixedFileInfo(data_)) {
  DCHECK(!owned_data_.empty());

  mozilla::Unused << language_;
  mozilla::Unused << code_page_;
}
