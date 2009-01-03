/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alec Flett <alecf@netscape.com>
 *   Darin Fisher <darin@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* Windows-specific local file uri parsing */
#include "nsURLHelper.h"
#include "nsEscape.h"
#include "nsILocalFile.h"
#include <windows.h>

nsresult
net_GetURLSpecFromFile(nsIFile *aFile, nsACString &result)
{
    nsresult rv;
    nsAutoString path;
  
    // construct URL spec from file path
    rv = aFile->GetPath(path);
    if (NS_FAILED(rv)) return rv;
  
    // Replace \ with / to convert to an url
    path.ReplaceChar(PRUnichar(0x5Cu), PRUnichar(0x2Fu));

    nsCAutoString escPath;

    // Windows Desktop paths beging with a drive letter, so need an 'extra'
    // slash at the begining
#ifdef WINCE  //  /Windows  => file:///Windows
    NS_NAMED_LITERAL_CSTRING(prefix, "file://");
#else  // C:\Windows =>  file:///C:/Windows
    NS_NAMED_LITERAL_CSTRING(prefix, "file:///");
#endif  
    // Escape the path with the directory mask
    NS_ConvertUTF16toUTF8 ePath(path);
    if (NS_EscapeURL(ePath.get(), -1, esc_Directory+esc_Forced, escPath))
        escPath.Insert(prefix, 0);
    else
        escPath.Assign(prefix + ePath);

    // esc_Directory does not escape the semicolons, so if a filename 
    // contains semicolons we need to manually escape them.
    escPath.ReplaceSubstring(";", "%3b");

    // if this file references a directory, then we need to ensure that the
    // URL ends with a slash.  this is important since it affects the rules
    // for relative URL resolution when this URL is used as a base URL.
    // if the file does not exist, then we make no assumption about its type,
    // and simply leave the URL unmodified.
    if (escPath.Last() != '/') {
        PRBool dir;
        rv = aFile->IsDirectory(&dir);
        if (NS_SUCCEEDED(rv) && dir)
            escPath += '/';
    }
    
    result = escPath;
    return NS_OK;
}

nsresult
net_GetFileFromURLSpec(const nsACString &aURL, nsIFile **result)
{
    nsresult rv;

    nsCOMPtr<nsILocalFile> localFile(
            do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));
    if (NS_FAILED(rv)) {
        NS_ERROR("Only nsILocalFile supported right now");
        return rv;
    }

    localFile->SetFollowLinks(PR_TRUE);

    const nsACString *specPtr;

    nsCAutoString buf;
    if (net_NormalizeFileURL(aURL, buf))
        specPtr = &buf;
    else
        specPtr = &aURL;
    
    nsCAutoString directory, fileBaseName, fileExtension;
    
    rv = net_ParseFileURL(*specPtr, directory, fileBaseName, fileExtension);
    if (NS_FAILED(rv)) return rv;

    nsCAutoString path;

    if (!directory.IsEmpty()) {
        NS_EscapeURL(directory, esc_Directory|esc_AlwaysCopy, path);
        if (path.Length() > 2 && path.CharAt(2) == '|')
            path.SetCharAt(':', 2);
        path.ReplaceChar('/', '\\');
    }    
    if (!fileBaseName.IsEmpty())
        NS_EscapeURL(fileBaseName, esc_FileBaseName|esc_AlwaysCopy, path);
    if (!fileExtension.IsEmpty()) {
        path += '.';
        NS_EscapeURL(fileExtension, esc_FileExtension|esc_AlwaysCopy, path);
    }
    
    NS_UnescapeURL(path);
    if (path.Length() != strlen(path.get()))
        return NS_ERROR_FILE_INVALID_PATH;

#ifndef WINCE
    // remove leading '\'
    if (path.CharAt(0) == '\\')
        path.Cut(0, 1);
#endif

    if (IsUTF8(path))
        rv = localFile->InitWithPath(NS_ConvertUTF8toUTF16(path));
        // XXX In rare cases, a valid UTF-8 string can be valid as a native 
        // encoding (e.g. 0xC5 0x83 is valid both as UTF-8 and Windows-125x).
        // However, the chance is very low that a meaningful word in a legacy
        // encoding is valid as UTF-8.
    else 
        // if path is not in UTF-8, assume it is encoded in the native charset
        rv = localFile->InitWithNativePath(path);

    if (NS_FAILED(rv)) return rv;

    NS_ADDREF(*result = localFile);
    return NS_OK;
}
