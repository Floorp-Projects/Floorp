/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/**********************************************************************
 unixpref.c
 **********************************************************************/

#include <stdio.h>
#include <assert.h>
#include "prefapi.h"
#include "prlink.h"
#include "prlog.h"
#include "jsapi.h"
#include "jsbuffer.h"
#include "xpassert.h"
#include "xp_mcom.h"

/*
 * pref_InitInitialObjects
 * Needed by PREF_Init.
 * Sets the default preferences.
 */
extern char *fe_GetConfigDirFilename(char *filename);

XP_Bool
FE_GetLabelAndMnemonic(char* name, char** str, void* v_xm_str, void* v_mnemonic);
char *fe_GetConfigDirFilename(char *filename);

JSBool
pref_InitInitialObjects(void)
{
    JSBool status;
    int i;

    PR_ASSERT(pref_init_buffer);

    /* loop over all the strings in the init buffer */
    for (i=0; i < (sizeof pref_init_buffer / sizeof *pref_init_buffer) && pref_init_buffer[i] != 0; ++i)
    {
      status = PREF_EvaluateJSBuffer(pref_init_buffer[i], strlen(pref_init_buffer[i]));
    }

    /* these strings never get freed, but that's probably the way it should be */
    PREF_SetDefaultCharPref("browser.cache.directory", 
                            fe_GetConfigDirFilename("cache"));
    PREF_SetDefaultCharPref("browser.sarcache.directory",
                            fe_GetConfigDirFilename("sarcache"));
    PREF_SetDefaultCharPref("browser.bookmark_file", 
                            fe_GetConfigDirFilename("bookmarks.html"));
    PREF_SetDefaultCharPref("browser.history_file", 
                            fe_GetConfigDirFilename("history.db"));
    PREF_SetDefaultCharPref("browser.user_history_file", 
                            fe_GetConfigDirFilename("history.list"));

#if defined(__sgi) || (defined(__sun) && defined(__svr4__))
    PREF_SetDefaultCharPref("print.print_command", "lp");
#endif

    return status;
}


/*
 * PREF_GetLabelAndMnemonic
 */
PRBool
PREF_GetLabelAndMnemonic(char* name, char** str, void* v_xm_str, void* v_mnemonic)
{
    /* Code moved to where it should have been. */
    return FE_GetLabelAndMnemonic(name, str, v_xm_str, v_mnemonic);
}

/*
 * PREF_GetUrl
 */
PRBool
PREF_GetUrl(char* name, char** url)
{
    char buf[256];

    PR_ASSERT(name);

    if ( name == NULL || url == NULL ) return PR_FALSE;

    strncpy(buf, name, 200);
    strcat(buf, ".url");

    *url = NULL;

    PREF_CopyConfigString(buf, url);

    return ( url != NULL && *url != NULL && **url != '\0' );
}

XP_Bool
FE_GetLabelAndMnemonic(char* name, char** str, void* v_xm_str, void* v_mnemonic)
{
  return 0;
}

char *fe_GetConfigDirFilename(char *filename)
{
  return "/tmp";
}


