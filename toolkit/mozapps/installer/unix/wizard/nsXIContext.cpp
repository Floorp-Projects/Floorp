/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Samir Gehani <sgehani@netscape.com>
 */

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
    notebook = NULL;
    header = NULL;

    backID = 0;
    nextID = 0;
    cancelID = 0;
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

char *
nsXIContext::itoa(int n)
{
	char *s;
	int i, j, sign, tmp;
	
	/* check sign and convert to positive to stringify numbers */
	if ( (sign = n) < 0)
		n = -n;
	i = 0;
	s = (char*) malloc(sizeof(char));
	
	/* grow string as needed to add numbers from powers of 10 
     * down till none left 
     */
	do
	{
		s = (char*) realloc(s, (i+1)*sizeof(char));
		s[i++] = n % 10 + '0';  /* '0' or 30 is where ASCII numbers start */
		s[i] = '\0';
	}
	while( (n /= 10) > 0);	
	
	/* tack on minus sign if we found earlier that this was negative */
	if (sign < 0)
	{
		s = (char*) realloc(s, (i+1)*sizeof(char));
		s[i++] = '-';
	}
	s[i] = '\0';
	
	/* pop numbers (and sign) off of string to push back into right direction */
	for (i = 0, j = strlen(s) - 1; i < j; i++, j--)
	{
		tmp = s[i];
		s[i] = s[j];
		s[j] = tmp;
	}
	
	return s;
}

#define MAX_KEY_SIZE 64
#define FIRST_ERR -601
#define LAST_ERR  -625
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
        "WILL_INSTALL",
        "TO_LOCATION",
        "PREPARING",
        "EXTRACTING",
        "INSTALLING_XPI",
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
        "CRC_FAILED",
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

