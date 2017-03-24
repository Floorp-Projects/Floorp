// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a dummy version of Chromium source file base/file/file_path.cc.
// To provide the functions required in base/win/windows_version.cc
// GetVersionFromKernel32, which we don't use.

#include "base/files/file_path.h"

namespace base {

FilePath::FilePath(FilePath::StringPieceType path) {
}

FilePath::~FilePath() {
}

} // namespace base
