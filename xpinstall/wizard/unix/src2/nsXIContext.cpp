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

#include "nsXIContext.h"

#define __EOT__ "EndOfTable"

nsXIContext::nsXIContext()
{
    me = NULL;

    ldlg = NULL;
    wdlg = NULL;
    sdlg = NULL;
    cdlg = NULL;
    idlg = NULL;

    opt = new nsXIOptions();

    window = NULL;
    back = NULL;
    next = NULL;
    cancel = NULL;
    nextLabel = NULL;
    backLabel = NULL;
    acceptLabel = NULL;
    declineLabel = NULL;
    installLabel = NULL;
    logo = NULL; 
    canvas = NULL;
    notebook = NULL;

    backID = 0;
    nextID = 0;
    cancelID = 0;
    bMoving = FALSE;
    bDone = FALSE;

    reslist = NULL;
}

nsXIContext::~nsXIContext()
{
    // NOTE: don't try to delete "me" cause I control thee

    ReleaseResources();

    XI_IF_DELETE(ldlg);
    XI_IF_DELETE(wdlg);
    XI_IF_DELETE(sdlg);
    XI_IF_DELETE(cdlg);
    XI_IF_DELETE(idlg);

    XI_IF_DELETE(opt);
}

#define MAX_KEY_SIZE 64
#define FIRST_ERR -601
#define LAST_ERR  -630
int
nsXIContext::LoadResources()
{
    int err = OK;
    nsINIParser *parser = NULL;
    char *resfile = NULL;
    kvpair *currkv = NULL;
    char currkey[MAX_KEY_SIZE];
    int len, i;

    resfile = nsINIParser::ResolveName(RES_FILE);
    if (!resfile)
        return E_INVALID_PTR;

    parser = new nsINIParser(resfile);
    if (!parser)
    {
        XI_IF_FREE(resfile);
        return E_MEM;
    }

    char *strkeys[] = 
    {
        "NEXT",
        "BACK",
        "CANCEL",
        "ACCEPT",
        "DECLINE",
        "INSTALL",
        "PAUSE",
        "RESUME",
        "DEFAULT_TITLE",
        "DEST_DIR",
        "BROWSE",
        "SELECT_DIR",
        "DOESNT_EXIST",
        "YES_LABEL",
        "NO_LABEL",
        "OK_LABEL",
        "DELETE_LABEL",
        "CANCEL_LABEL",
        "ERROR",
        "FATAL_ERROR",
        "DESCRIPTION",
        "PREPARING",
        "EXTRACTING",
        "INSTALLING_XPI",
        "COMPLETING_INSTALL",
        "PROCESSING_FILE",
        "NO_PERMS",
        "DL_SETTINGS",
        "SAVE_MODULES",
        "PROXY_SETTINGS",
        "PS_LABEL0",
        "PS_LABEL1",
        "PS_LABEL2",
        "PS_LABEL3",
        "ERROR_TITLE",
        "DS_AVAIL",
        "DS_REQD",
        "NO_DISK_SPACE",
        "CXN_DROPPED",
        "DOWNLOADING",
        "FROM",
        "TO",
        "STATUS",
        "DL_STATUS_STR",
        "CRC_CHECK",
        "USAGE_MSG",
        "UNKNOWN",

        __EOT__
    };

    /* read in UI strings */
    currkv = (kvpair *) malloc(sizeof(kvpair));
    reslist = currkv;
    for (i = 0; strcmp(strkeys[i], __EOT__) != 0; i++)
    {
        err = parser->GetStringAlloc(RES_SECT, strkeys[i], 
                                    &(currkv->val), &len);
        if (err != OK)
            goto BAIL;

        currkv->key = strdup(strkeys[i]);
        currkv->next = (kvpair *) malloc(sizeof(kvpair));
        currkv = currkv->next;

        if (i > 1024) /* inf loop prevention paranoia */
            break;
    }
    currkv->next = NULL; /* seal off list */

    /* read in err strings */
    for (i = FIRST_ERR; i >= LAST_ERR; i--)
    {
        sprintf(currkey, "%d", i);
        err = parser->GetStringAlloc(RES_SECT, currkey, &(currkv->val), &len);
        if (err != OK)
            goto BAIL;
        
        currkv->key = strdup(currkey);
        if (i == LAST_ERR)
            break;
        currkv->next = (kvpair *) malloc(sizeof(kvpair));
        currkv = currkv->next;
    }
    currkv->next = NULL; /* seal off list */

BAIL:
    if (err != OK)
    {
        fprintf(stderr, "FATAL ERROR: Failed to load resources!\n");
    }
    XI_IF_FREE(resfile);
    XI_IF_DELETE(parser);
    return err;
}

int 
nsXIContext::ReleaseResources()
{
    int err = OK;
    kvpair *currkv = NULL, *delkv = NULL;
    
    /* empty list -- should never really happen */
    if (!reslist) 
        return E_PARAM;
    
    currkv = reslist;
    while (currkv)
    {
        XI_IF_FREE(currkv->key);
        XI_IF_FREE(currkv->val);
        delkv = currkv;
        currkv = currkv->next;
        XI_IF_FREE(delkv);
    }

    return err;
}

char *
nsXIContext::Res(char *aKey)
{
    char *val = NULL;
    kvpair *currkv = NULL;

    /* param check */
    if (!aKey || !reslist)
        return NULL;

    /* search through linked list */
    currkv = reslist;
    while (currkv)
    {
        if (strcmp(aKey, currkv->key) == 0)
        {
            val = currkv->val;
            break;
        }
        currkv = currkv->next;
    } 

    return val;
}

