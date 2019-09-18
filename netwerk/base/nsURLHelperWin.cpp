/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=4 sw=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Windows-specific local file uri parsing */
#include "nsURLHelper.h"
#include "nsEscape.h"
#include "nsIFile.h"
#include <windows.h>
#include "mozilla/Utf8.h"

nsresult net_GetURLSpecFromActualFile(nsIFile* aFile, nsACString& result) {
  nsresult rv;
  nsAutoString path;

  // construct URL spec from file path
  rv = aFile->GetPath(path);
  if (NS_FAILED(rv)) return rv;

  // Replace \ with / to convert to an url
  path.ReplaceChar(char16_t(0x5Cu), char16_t(0x2Fu));

  nsAutoCString escPath;

  // Windows Desktop paths begin with a drive letter, so need an 'extra'
  // slash at the begining
  // C:\Windows =>  file:///C:/Windows
  NS_NAMED_LITERAL_CSTRING(prefix, "file:///");

  // Escape the path with the directory mask
  NS_ConvertUTF16toUTF8 ePath(path);
  if (NS_EscapeURL(ePath.get(), -1, esc_Directory + esc_Forced, escPath))
    escPath.Insert(prefix, 0);
  else
    escPath.Assign(prefix + ePath);

  // esc_Directory does not escape the semicolons, so if a filename
  // contains semicolons we need to manually escape them.
  // This replacement should be removed in bug #473280
  escPath.ReplaceSubstring(";", "%3b");

  result = escPath;
  return NS_OK;
}

nsresult net_GetFileFromURLSpec(const nsACString& aURL, nsIFile** result) {
  nsresult rv;

  if (aURL.Length() > (uint32_t)net_GetURLMaxLength()) {
    return NS_ERROR_MALFORMED_URI;
  }

  nsCOMPtr<nsIFile> localFile(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));
  if (NS_FAILED(rv)) {
    NS_ERROR("Only nsIFile supported right now");
    return rv;
  }

  localFile->SetFollowLinks(true);

  const nsACString* specPtr;

  nsAutoCString buf;
  if (net_NormalizeFileURL(aURL, buf))
    specPtr = &buf;
  else
    specPtr = &aURL;

  nsAutoCString directory, fileBaseName, fileExtension;

  rv = net_ParseFileURL(*specPtr, directory, fileBaseName, fileExtension);
  if (NS_FAILED(rv)) return rv;

  nsAutoCString path;

  if (!directory.IsEmpty()) {
    NS_EscapeURL(directory, esc_Directory | esc_AlwaysCopy, path);
    if (path.Length() > 2 && path.CharAt(2) == '|') path.SetCharAt(':', 2);
    path.ReplaceChar('/', '\\');
  }
  if (!fileBaseName.IsEmpty())
    NS_EscapeURL(fileBaseName, esc_FileBaseName | esc_AlwaysCopy, path);
  if (!fileExtension.IsEmpty()) {
    path += '.';
    NS_EscapeURL(fileExtension, esc_FileExtension | esc_AlwaysCopy, path);
  }

  NS_UnescapeURL(path);
  if (path.Length() != strlen(path.get())) return NS_ERROR_FILE_INVALID_PATH;

  // remove leading '\'
  if (path.CharAt(0) == '\\') path.Cut(0, 1);

  if (mozilla::IsUtf8(path))
    rv = localFile->InitWithPath(NS_ConvertUTF8toUTF16(path));
  // XXX In rare cases, a valid UTF-8 string can be valid as a native
  // encoding (e.g. 0xC5 0x83 is valid both as UTF-8 and Windows-125x).
  // However, the chance is very low that a meaningful word in a legacy
  // encoding is valid as UTF-8.
  else
    // if path is not in UTF-8, assume it is encoded in the native charset
    rv = localFile->InitWithNativePath(path);

  if (NS_FAILED(rv)) return rv;

  localFile.forget(result);
  return NS_OK;
}
