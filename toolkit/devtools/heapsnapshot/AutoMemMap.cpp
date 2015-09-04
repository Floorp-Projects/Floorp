/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/devtools/AutoMemMap.h"
#include "nsDebug.h"

namespace mozilla {
namespace devtools {

AutoMemMap::~AutoMemMap()
{
  if (addr) {
    NS_WARN_IF(PR_MemUnmap(addr, size()) != PR_SUCCESS);
    addr = nullptr;
  }

  if (fileMap) {
    NS_WARN_IF(PR_CloseFileMap(fileMap) != PR_SUCCESS);
    fileMap = nullptr;
  }

  if (fd) {
    NS_WARN_IF(PR_Close(fd) != PR_SUCCESS);
    fd = nullptr;
  }
}

nsresult
AutoMemMap::init(const char* filePath, PRIntn flags, PRIntn mode, PRFileMapProtect prot)
{
  MOZ_ASSERT(!fd);
  MOZ_ASSERT(!fileMap);
  MOZ_ASSERT(!addr);

  if (PR_GetFileInfo64(filePath, &fileInfo) != PR_SUCCESS)
    return NS_ERROR_FILE_NOT_FOUND;

  // Check if the file is too big to memmap.
  if (fileInfo.size > int64_t(UINT32_MAX))
    return NS_ERROR_INVALID_ARG;
  auto length = uint32_t(fileInfo.size);

  fd = PR_Open(filePath, flags, flags);
  if (!fd)
    return NS_ERROR_UNEXPECTED;

  fileMap = PR_CreateFileMap(fd, fileInfo.size, prot);
  if (!fileMap)
    return NS_ERROR_UNEXPECTED;

  addr = PR_MemMap(fileMap, 0, length);
  if (!addr)
    return NS_ERROR_UNEXPECTED;

  return NS_OK;
}

} // namespace devtools
} // namespace mozilla
