/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#ifndef nsPluginsDirUtils_h___
#define nsPluginsDirUtils_h___

#include "nsPluginsDir.h"
#include "nsVoidArray.h"
#include "prmem.h"

///////////////////////////////////////////////////////////////////////////////
// Ouput format from NPP_GetMIMEDescription: "...mime type[;version]:[extension]:[desecription];..."
// The ambiguity of mime description could cause the browser fail to parse the MIME types 
// correctly.
// E.g. "mime type::desecription;" // correct w/o ext 
//      "mime type:desecription;"  // wrong w/o ext 
//
static nsresult
ParsePluginMimeDescription(const char *mdesc, nsPluginInfo &info)
{
    nsresult rv = NS_ERROR_FAILURE;
    if (!mdesc || !*mdesc)
       return rv;

    char *mdescDup = PL_strdup(mdesc); // make a dup of intput string we'll change it content
    char anEmptyString[] = "";
    nsAutoVoidArray tmpMimeTypeArr;
    char delimiters[] = {':',':',';'};
    int mimeTypeVariantCount = 0;
    char *p = mdescDup; // make a dup of intput string we'll change it content
    while(p) {
        char *ptrMimeArray[] = {anEmptyString, anEmptyString, anEmptyString};

        // It's easy to point out ptrMimeArray[0] to the string sounds like
        // "Mime type is not specified, plugin will not function properly."
        // and show this on "about:plugins" page, but we have to mark this particular
        // mime type of given plugin as disable on "about:plugins" page,
        // which feature is not implemented yet.
        // So we'll ignore, without any warnings, an empty description strings, 
        // in other words, if after parsing ptrMimeArray[0] == anEmptyString is true.
        // It is possible do not to registry a plugin at all if it returns
        // an empty string on GetMIMEDescription() call,
        // e.g. plugger returns "" if pluggerrc file is not found.

        char *s = p;
        int i;
        for (i = 0; i < (int) sizeof(delimiters) && (p = PL_strchr(s, delimiters[i])); i++) {
            ptrMimeArray[i] = s; // save start ptr
            *p++ = 0; // overwrite delimiter
            s = p; // move forward
        }
        if (i == 2)
           ptrMimeArray[i] = s;
        // fill out the temp array 
        // the order is important, it should be the same in for loop below 
        if (ptrMimeArray[0] != anEmptyString) {
            tmpMimeTypeArr.AppendElement((void*) ptrMimeArray[0]);
            tmpMimeTypeArr.AppendElement((void*) ptrMimeArray[1]);
            tmpMimeTypeArr.AppendElement((void*) ptrMimeArray[2]);
            mimeTypeVariantCount++;
        }
    }

    // fill out info structure
    if (mimeTypeVariantCount) {
        info.fVariantCount         = mimeTypeVariantCount;
        // we can do these 3 mallocs at ones, later on code cleanup
        info.fMimeTypeArray        = (char **)PR_Malloc(mimeTypeVariantCount * sizeof(char *));
        info.fMimeDescriptionArray = (char **)PR_Malloc(mimeTypeVariantCount * sizeof(char *));
        info.fExtensionArray       = (char **)PR_Malloc(mimeTypeVariantCount * sizeof(char *));

        int j,i;
        for (j = i = 0; i < mimeTypeVariantCount; i++) {
            // the order is important, do not change it
           // we can get rid of PL_strdup here, later on code cleanup
            info.fMimeTypeArray[i]        =  PL_strdup((char*) tmpMimeTypeArr.ElementAt(j++));
            info.fExtensionArray[i]       =  PL_strdup((char*) tmpMimeTypeArr.ElementAt(j++));
            info.fMimeDescriptionArray[i] =  PL_strdup((char*) tmpMimeTypeArr.ElementAt(j++));
        }
        rv = NS_OK;
    }
    if (mdescDup)
        PR_Free(mdescDup);
    return rv;
}

#endif /* nsPluginsDirUtils_h___ */
