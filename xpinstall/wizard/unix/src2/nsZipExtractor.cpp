/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Samir Gehani <sgehani@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsZipExtractor.h"

#define STANDALONE 1
#include "zipstub.h"
#include "zipfile.h"

nsZipExtractor::nsZipExtractor(char *aSrc, char *aDest) :
    mSrc(NULL),
    mDest(NULL)
{
    if (aSrc)
        mSrc = strdup(aSrc);
    if (aDest)
        mDest = strdup(aDest);
}

nsZipExtractor::~nsZipExtractor()
{
    XI_IF_FREE(mSrc);
    XI_IF_FREE(mDest);
}

int
nsZipExtractor::Extract(nsComponent *aXPIEngine, int aTotal)
{
    DUMP("Extract");
   
    char apath[MAXPATHLEN]; /* archive path */
    char bindir[512];
    char zpath[MAXPATHLEN]; /* path of file in zip archive */
    char epath[MAXPATHLEN]; /* path of file after being extracted */
    char *leaf = NULL, *lslash = NULL;
    struct stat dummy;
    int i, bFoundAll = FALSE, err = OK;
    PRInt32 zerr = ZIP_OK;
    void *hZip = NULL, *hFind = NULL;

    if (!aXPIEngine || !(aXPIEngine->GetArchive()))
        return E_PARAM;

    sprintf(apath, "%s/%s", mSrc, aXPIEngine->GetArchive());
    if (-1 == stat(apath, &dummy))
        return E_NO_DOWNLOAD;

    /* initialize archive etc. 
     */
    zerr = ZIP_OpenArchive(apath, &hZip);
    if (zerr != ZIP_OK) return E_EXTRACTION;
    hFind = ZIP_FindInit(hZip, (const char *) NULL);
    if (!hFind)
    {
        err = E_EXTRACTION;
        goto au_revoir;
    }

    /* extract files 
     */
    i = 0;
    while (!bFoundAll)
    {
        memset(zpath, 0, MAXPATHLEN);
        zerr = ZIP_FindNext(hFind, zpath, MAXPATHLEN);
        if (zerr == ZIP_ERR_FNF)
        {
            bFoundAll = true;
            break;
        }
        if (zerr != ZIP_OK)
        {
            err = E_EXTRACTION;
            goto au_revoir;
        }

        /* directory encountered: ignore entry
         */
        lslash = strrchr(zpath, '/');
        if (lslash == (zpath + strlen(zpath) - 1))
            continue;

        if (!lslash)
            leaf = zpath;
        else
            leaf = lslash + 1;
        if (!leaf)
            continue;

        /* update UI
         */
        if (gCtx->opt->mMode != nsXIOptions::MODE_SILENT)
            nsInstallDlg::MajorProgressCB(leaf, i, 
                aTotal, nsInstallDlg::ACT_EXTRACT);

        sprintf(epath, "%s/%s", mDest, zpath);
        err = DirCreateRecursive(epath);
        if (err != OK) goto au_revoir;

        zerr = ZIP_ExtractFile(hZip, zpath, epath);
        if (zerr != ZIP_OK)
        {
            err = E_EXTRACTION;
            goto au_revoir;
        }

        i++;
    }
    
    sprintf(bindir, "%s/%s", mDest, TMP_EXTRACT_SUBDIR);
    if (-1 == stat(bindir, &dummy))
        err = E_EXTRACTION;

au_revoir:
    /* close archive etc.
     */
    if (hFind)
        ZIP_FindFree(hFind);
    if (hZip)
        ZIP_CloseArchive(&hZip);
    return err;
}

int
nsZipExtractor::DirCreateRecursive(char *aPath)
{
    int err = OK;
    char *slash = NULL, *pathpos = NULL;
    char currdir[MAXPATHLEN];
    struct stat dummy;
    
    if (!aPath || !mDest)
        return E_PARAM;

    slash = aPath + strlen(mDest);
    if (*slash != '/')
        return E_INVALID_PTR;

    while (slash)
    {
        memset(currdir, 0, MAXPATHLEN);
        strncpy(currdir, aPath, slash - aPath);
        
        if (-1 == stat(currdir, &dummy))
            mkdir(currdir, 0755);
        
        pathpos = slash;
        slash = strchr(pathpos+1, '/');
    }   

    return err;
}
