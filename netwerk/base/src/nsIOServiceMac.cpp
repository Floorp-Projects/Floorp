/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* Mac-specific local file url parsing */
#include "nsIOService.h"
#include "nsEscape.h"
#include "nsPrintfCString.h"

static void SwapSlashColon(char *s)
{
    while (*s) {
        if (*s == '/')
            *s++ = ':';
        else if (*s == ':')
            *s++ = '/';
        else
            *s++;
    }
} 

static void PascalStringCopy(Str255 dst, const char *src)
{
    int srcLength = strlen(src);
    NS_ASSERTION(srcLength <= 255, "Oops, Str255 can't hold >255 chars");
    if (srcLength > 255)
        srcLength = 255;
    dst[0] = srcLength;
    memcpy(&dst[1], src, srcLength);
}


NS_IMETHODIMP
nsIOService::GetURLSpecFromFile(nsIFile *aFile, char **aURL)
{
    NS_ENSURE_ARG_POINTER(aURL);
    *aURL = nsnull;
     
    nsresult rv;
    nsXPIDLCString ePath;

    rv = aFile->GetPath(getter_Copies(ePath));
    if (NS_FAILED(rv)) return rv;
 
    SwapSlashColon((char *) ePath.get());
           
    nsCAutoString escPath;
    NS_NAMED_LITERAL_CSTRING(prefix, "file:///");

    // Escape the path with the directory mask
    if (!NS_EscapeURLPart(ePath.get(), ePath.Length(), esc_Directory+esc_Forced, escPath))
        escPath.Assign(ePath);

    // colons [originally slashes, before SwapSlashColon() usage above]
    // need encoding; use %2F which is a forward slash
    escPath.ReplaceSubstring(":", "%2F");

    escPath.Insert(prefix, 0);

    // XXX this should be unnecessary
    if (escPath[escPath.Length() - 1] != '/') {
        PRBool dir;
        rv = aFile->IsDirectory(&dir);
        if (NS_FAILED(rv))
            NS_WARNING(nsPrintfCString("Cannot tell if %s is a directory or file", escPath.get()).get());
        else if (dir) {
            // make sure we have a trailing slash
            escPath += "/";
        }
    }

    *aURL = ToNewCString(escPath);
    return *aURL ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsIOService::InitFileFromURLSpec(nsIFile *aFile, const char *aURL)
{
    NS_ENSURE_ARG(aURL);
    nsresult rv;
       
    nsXPIDLCString host, directory, fileBaseName, fileExtension;
    
    nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(aFile, &rv);
    if (NS_FAILED(rv)) {
        NS_ERROR("Only nsILocalFile supported right now");
        return rv;
    }
     
    rv = ParseFileURL(aURL, getter_Copies(host),
                      getter_Copies(directory),
                      getter_Copies(fileBaseName),
                      getter_Copies(fileExtension));
    if (NS_FAILED(rv)) return rv;
                   
    nsCAutoString path;
 
    if (host) {
        // We can end up with a host when given: file:// instead of file:///
        // Check to see if the host is a volume name - If so prepend it
        Str255 volName;
        FSSpec volSpec;
         
        PascalStringCopy(volName, host);
        volName[++volName[0]] = ':';
        if (::FSMakeFSSpec(0, 0, volName, &volSpec) == noErr)
            path += host;
    }
    if (directory) {
        if (!NS_EscapeURLPart(directory.get(), directory.Length(), esc_Directory, path))
            path += directory;

        // "%2F"s need to become slashes, while all other slashes need to
        // become colons. If we start out by changing "%2F"s to colons, we
        // can reply on SwapSlashColon() to do what we need
        path.ReplaceSubstring("%2F", ":");

        SwapSlashColon((char *) path.get());
    }
    if (fileBaseName) {
        if (!NS_EscapeURLPart(fileBaseName.get(), fileBaseName.Length(), esc_FileBaseName, path))
            path += fileBaseName;
    }
    if (fileExtension) {
        path += '.';
        if (!NS_EscapeURLPart(fileExtension.get(), fileExtension.Length(), esc_FileExtension, path))
            path += fileExtension;
    }
    
    NS_UnescapeURL((char *) path.get());
 
    // wack off leading :'s
    if (path.CharAt(0) == ':')
        path.Cut(0, 1);
 
    return localFile->InitWithPath(path.get());
}
