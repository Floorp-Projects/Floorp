// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a partial implementation of Chromium's source file
// base/file/file_path.cc.

#include "base/files/file_path.h"

#include "mozilla/Assertions.h"

namespace base {

using StringType = FilePath::StringType;
using StringPieceType = FilePath::StringPieceType;

namespace {

const FilePath::CharType kStringTerminator = FILE_PATH_LITERAL('\0');

}  // namespace

FilePath::FilePath() = default;

FilePath::FilePath(const FilePath& that) = default;
FilePath::FilePath(FilePath&& that) noexcept = default;

FilePath::FilePath(StringPieceType path) : path_(path) {
  StringType::size_type nul_pos = path_.find(kStringTerminator);
  if (nul_pos != StringType::npos)
    path_.erase(nul_pos, StringType::npos);
}

FilePath::~FilePath() = default;

FilePath& FilePath::operator=(const FilePath& that) = default;

FilePath& FilePath::operator=(FilePath&& that) = default;

} // namespace base
