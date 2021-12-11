/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=4 sw=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Unix-specific local file uri parsing */
#include "nsURLHelper.h"
#include "nsEscape.h"
#include "nsIFile.h"
#include "nsNativeCharsetUtils.h"
#include "mozilla/Utf8.h"

using mozilla::IsUtf8;

nsresult net_GetURLSpecFromActualFile(nsIFile* aFile, nsACString& result) {
  nsresult rv;
  nsAutoCString nativePath, ePath;
  nsAutoString path;

  rv = aFile->GetNativePath(nativePath);
  if (NS_FAILED(rv)) return rv;

  // Convert to unicode and back to check correct conversion to native charset
  NS_CopyNativeToUnicode(nativePath, path);
  NS_CopyUnicodeToNative(path, ePath);

  // Use UTF8 version if conversion was successful
  if (nativePath == ePath) {
    CopyUTF16toUTF8(path, ePath);
  } else {
    ePath = nativePath;
  }

  nsAutoCString escPath;
  constexpr auto prefix = "file://"_ns;

  // Escape the path with the directory mask
  if (NS_EscapeURL(ePath.get(), -1, esc_Directory + esc_Forced, escPath)) {
    escPath.Insert(prefix, 0);
  } else {
    escPath.Assign(prefix + ePath);
  }

  // esc_Directory does not escape the semicolons, so if a filename
  // contains semicolons we need to manually escape them.
  // This replacement should be removed in bug #473280
  escPath.ReplaceSubstring(";", "%3b");
  result = escPath;
  return NS_OK;
}

nsresult net_GetFileFromURLSpec(const nsACString& aURL, nsIFile** result) {
  // NOTE: See also the implementation in nsURLHelperOSX.cpp,
  // which is based on this.

  nsresult rv;

  nsCOMPtr<nsIFile> localFile;
  rv = NS_NewNativeLocalFile(""_ns, true, getter_AddRefs(localFile));
  if (NS_FAILED(rv)) return rv;

  nsAutoCString directory, fileBaseName, fileExtension, path;

  rv = net_ParseFileURL(aURL, directory, fileBaseName, fileExtension);
  if (NS_FAILED(rv)) return rv;

  if (!directory.IsEmpty()) {
    rv = NS_EscapeURL(directory, esc_Directory | esc_AlwaysCopy, path,
                      mozilla::fallible);
    if (NS_FAILED(rv)) return rv;
  }
  if (!fileBaseName.IsEmpty()) {
    rv = NS_EscapeURL(fileBaseName, esc_FileBaseName | esc_AlwaysCopy, path,
                      mozilla::fallible);
    if (NS_FAILED(rv)) return rv;
  }
  if (!fileExtension.IsEmpty()) {
    path += '.';
    rv = NS_EscapeURL(fileExtension, esc_FileExtension | esc_AlwaysCopy, path,
                      mozilla::fallible);
    if (NS_FAILED(rv)) return rv;
  }

  NS_UnescapeURL(path);
  if (path.Length() != strlen(path.get())) return NS_ERROR_FILE_INVALID_PATH;

  if (IsUtf8(path)) {
    // speed up the start-up where UTF-8 is the native charset
    // (e.g. on recent Linux distributions)
    if (NS_IsNativeUTF8()) {
      rv = localFile->InitWithNativePath(path);
    } else {
      rv = localFile->InitWithPath(NS_ConvertUTF8toUTF16(path));
    }
    // XXX In rare cases, a valid UTF-8 string can be valid as a native
    // encoding (e.g. 0xC5 0x83 is valid both as UTF-8 and Windows-125x).
    // However, the chance is very low that a meaningful word in a legacy
    // encoding is valid as UTF-8.
  } else {
    // if path is not in UTF-8, assume it is encoded in the native charset
    rv = localFile->InitWithNativePath(path);
  }

  if (NS_FAILED(rv)) return rv;

  localFile.forget(result);
  return NS_OK;
}
