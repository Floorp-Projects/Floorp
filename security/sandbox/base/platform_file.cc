// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/platform_file.h"

namespace base {

PlatformFileInfo::PlatformFileInfo()
    : size(0),
      is_directory(false),
      is_symbolic_link(false) {
}

PlatformFileInfo::~PlatformFileInfo() {}

#if !defined(OS_NACL)
PlatformFile CreatePlatformFile(const FilePath& name,
                                int flags,
                                bool* created,
                                PlatformFileError* error) {
  if (name.ReferencesParent()) {
    if (error)
      *error = PLATFORM_FILE_ERROR_ACCESS_DENIED;
    return kInvalidPlatformFileValue;
  }
  return CreatePlatformFileUnsafe(name, flags, created, error);
}
#endif

}  // namespace base
