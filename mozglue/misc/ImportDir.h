/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "mozilla/LauncherResult.h"
#include "mozilla/NativeNt.h"
#include "mozilla/WinHeaderOnlyUtils.h"

namespace mozilla {
namespace detail {

inline LauncherResult<nt::DataDirectoryEntry> GetImageDirectoryViaFileIo(
    const nsAutoHandle& aImageFile, const uint32_t aOurImportDirectoryRva) {
  OVERLAPPED ov = {};
  ov.Offset = aOurImportDirectoryRva;

  DWORD bytesRead;
  nt::DataDirectoryEntry result;
  if (!::ReadFile(aImageFile, &result, sizeof(result), &bytesRead, &ov) ||
      bytesRead != sizeof(result)) {
    return LAUNCHER_ERROR_FROM_LAST();
  }

  return result;
}

}  // namespace detail

/**
 * This function ensures that the import directory of a loaded binary image
 * matches the version that is found in the original file on disk. We do this
 * to prevent tampering by third-party code.
 *
 * Yes, this function may perform file I/O on the critical path during
 * startup. A mitigating factor here is that this function must be called
 * immediately after creating a process using the image specified by
 * |aFullImagePath|; by this point, the system has already paid the price of
 * pulling the image file's contents into the page cache.
 *
 * @param aFullImagePath Wide-character string containing the absolute path
 *                       to the binary whose import directory we are touching.
 * @param aLocalExeImage The binary's PE headers that have been loaded into our
 *                       process for examination.
 * @param aTargetProcess Handle to the child process whose import table we are
 *                       touching.
 * @param aRemoteExeImage HMODULE referencing the child process's executable
 *                        binary that we are touching. This value is used to
 *                        determine the base address of the binary within the
 *                        target process.
 */
inline LauncherVoidResult RestoreImportDirectory(
    const wchar_t* aFullImagePath, const nt::PEHeaders& aLocalExeImage,
    HANDLE aTargetProcess, HMODULE aRemoteExeImage) {
  uint32_t importDirEntryRva;
  PIMAGE_DATA_DIRECTORY importDirEntry =
      aLocalExeImage.GetImageDirectoryEntryPtr(IMAGE_DIRECTORY_ENTRY_IMPORT,
                                               &importDirEntryRva);
  if (!importDirEntry) {
    return LAUNCHER_ERROR_FROM_WIN32(ERROR_BAD_EXE_FORMAT);
  }

  nsAutoHandle file(::CreateFileW(aFullImagePath, GENERIC_READ, FILE_SHARE_READ,
                                  nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                                  nullptr));
  if (file.get() == INVALID_HANDLE_VALUE) {
    return LAUNCHER_ERROR_FROM_LAST();
  }

  // Why do we use file I/O here instead of a memory mapping? The simple reason
  // is that we do not want any kernel-mode drivers to start tampering with file
  // contents under the belief that the file is being mapped for execution.
  // Windows 8 supports creation of file mappings using the SEC_IMAGE_NO_EXECUTE
  // flag, which may help to mitigate this, but we might as well just support
  // a single implementation that works everywhere.
  LauncherResult<nt::DataDirectoryEntry> realImportDirectory =
      detail::GetImageDirectoryViaFileIo(file, importDirEntryRva);
  if (realImportDirectory.isErr()) {
    return LAUNCHER_ERROR_FROM_RESULT(realImportDirectory);
  }

  nt::DataDirectoryEntry toWrite = realImportDirectory.unwrap();

  void* remoteAddress = reinterpret_cast<char*>(
                            nt::PEHeaders::HModuleToBaseAddr(aRemoteExeImage)) +
                        importDirEntryRva;

  {  // Scope for prot
    AutoVirtualProtect prot(remoteAddress, sizeof(IMAGE_DATA_DIRECTORY),
                            PAGE_READWRITE, aTargetProcess);
    if (!prot) {
      return LAUNCHER_ERROR_FROM_MOZ_WINDOWS_ERROR(prot.GetError());
    }

    SIZE_T bytesWritten;
    if (!::WriteProcessMemory(aTargetProcess, remoteAddress, &toWrite,
                              sizeof(IMAGE_DATA_DIRECTORY), &bytesWritten) ||
        bytesWritten != sizeof(IMAGE_DATA_DIRECTORY)) {
      return LAUNCHER_ERROR_FROM_LAST();
    }
  }

  return Ok();
}

}  // namespace mozilla
