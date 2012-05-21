/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Unix-specific local file uri parsing */
#include "nsURLHelper.h"
#include "nsEscape.h"
#include "nsILocalFile.h"
#include "nsNativeCharsetUtils.h"

nsresult 
net_GetURLSpecFromActualFile(nsIFile *aFile, nsACString &result)
{
    nsresult rv;
    nsCAutoString nativePath, ePath;
    nsAutoString path;

    rv = aFile->GetNativePath(nativePath);
    if (NS_FAILED(rv)) return rv;

    // Convert to unicode and back to check correct conversion to native charset
    NS_CopyNativeToUnicode(nativePath, path);
    NS_CopyUnicodeToNative(path, ePath);

    // Use UTF8 version if conversion was successful
    if (nativePath == ePath)
        CopyUTF16toUTF8(path, ePath);
    else
        ePath = nativePath;
    
    nsCAutoString escPath;
    NS_NAMED_LITERAL_CSTRING(prefix, "file://");
        
    // Escape the path with the directory mask
    if (NS_EscapeURL(ePath.get(), -1, esc_Directory+esc_Forced, escPath))
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

nsresult
net_GetFileFromURLSpec(const nsACString &aURL, nsIFile **result)
{
    // NOTE: See also the implementation in nsURLHelperOSX.cpp,
    // which is based on this.

    nsresult rv;

    nsCOMPtr<nsILocalFile> localFile;
    rv = NS_NewNativeLocalFile(EmptyCString(), true, getter_AddRefs(localFile));
    if (NS_FAILED(rv))
      return rv;
    
    nsCAutoString directory, fileBaseName, fileExtension, path;

    rv = net_ParseFileURL(aURL, directory, fileBaseName, fileExtension);
    if (NS_FAILED(rv)) return rv;

    if (!directory.IsEmpty())
        NS_EscapeURL(directory, esc_Directory|esc_AlwaysCopy, path);
    if (!fileBaseName.IsEmpty())
        NS_EscapeURL(fileBaseName, esc_FileBaseName|esc_AlwaysCopy, path);
    if (!fileExtension.IsEmpty()) {
        path += '.';
        NS_EscapeURL(fileExtension, esc_FileExtension|esc_AlwaysCopy, path);
    }
    
    NS_UnescapeURL(path);
    if (path.Length() != strlen(path.get()))
        return NS_ERROR_FILE_INVALID_PATH;

    if (IsUTF8(path)) {
        // speed up the start-up where UTF-8 is the native charset
        // (e.g. on recent Linux distributions)
        if (NS_IsNativeUTF8())
            rv = localFile->InitWithNativePath(path);
        else
            rv = localFile->InitWithPath(NS_ConvertUTF8toUTF16(path));
            // XXX In rare cases, a valid UTF-8 string can be valid as a native 
            // encoding (e.g. 0xC5 0x83 is valid both as UTF-8 and Windows-125x).
            // However, the chance is very low that a meaningful word in a legacy
            // encoding is valid as UTF-8.
    }
    else 
        // if path is not in UTF-8, assume it is encoded in the native charset
        rv = localFile->InitWithNativePath(path);

    if (NS_FAILED(rv)) return rv;

    NS_ADDREF(*result = localFile);
    return NS_OK;
}
