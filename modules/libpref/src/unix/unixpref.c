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

#include "prefapi.h"
#include "prlink.h"
#include "jsapi.h"
#include "jsbuffer.h"
#include "xpassert.h"

#include <Xm/Xm.h>

extern PRLibrary* pref_LoadAutoAdminLib(void);
extern PRLibrary* m_AutoAdminLib;

#include "icondata.h"

static struct fe_icon_type* splash_screen = NULL;


/*
 * pref_InitInitialObjects
 * Needed by PREF_Init.
 * Sets the default preferences.
 */
JSBool
pref_InitInitialObjects(void)
{
    JSBool status;

    XP_ASSERT(pref_init_buffer);

    status = PREF_EvaluateJSBuffer(pref_init_buffer, strlen(pref_init_buffer));

#if defined(__sgi) || (defined(__sun) && defined(__svr4__))
    PREF_SetDefaultCharPref("print.print_command", "lp");
#endif

    return status;
}


/*
 * PREF_AlterSplashIcon
 */
void
PREF_AlterSplashIcon(struct fe_icon_data* icon)
{
    assert(icon);

    if ( PREF_IsAutoAdminEnabled() && 
         icon && 
         (splash_screen = (struct fe_icon_type*)
#ifndef NSPR20
          PR_FindSymbol("_POLARIS_SplashPro", m_AutoAdminLib)) != NULL ) {
#else
          PR_FindSymbol(m_AutoAdminLib, "_POLARIS_SplashPro")) != NULL ) {
#endif

        memcpy(icon, splash_screen, sizeof(*icon));
    }
}


/*
 * PREF_GetLabelAndMnemonic
 */
XP_Bool
PREF_GetLabelAndMnemonic(char* name, char** str, void* v_xm_str, void* v_mnemonic)
{
    XmString *xm_str = (XmString*)v_xm_str;
    KeySym *mnemonic = (KeySym*)v_mnemonic;
    char buf[256];
    char* _str;
    char* p1;
    char* p2;

    XP_ASSERT(name);
    XP_ASSERT(str);
    XP_ASSERT(xm_str);

    if ( name == NULL || str == NULL || xm_str == NULL ) return FALSE;

    _str = NULL;
	*str = NULL;
    *xm_str = NULL;
    *mnemonic = '\0';

    strncpy(buf, name, 200);
    strcat(buf, ".label");

    PREF_CopyConfigString(buf, &_str);

    if ( _str == NULL || *_str == '\0' ) return FALSE;

    /* Strip out ampersands */
    if ( strchr(_str, '&') != NULL ) {
        for ( p1 = _str, p2 = _str; *p2; p1++, p2++ ) {
            if ( *p1 == '&' && *(++p1) != '&' ) *mnemonic = *p1;
            *p2 = *p1;
        }
    }

    *str = _str;
    *xm_str = XmStringCreateLtoR(_str, XmFONTLIST_DEFAULT_TAG);

    return ( *xm_str != NULL );
}


/*
 * PREF_GetUrl
 */
XP_Bool
PREF_GetUrl(char* name, char** url)
{
    char buf[256];

    XP_ASSERT(name);

    if ( name == NULL || url == NULL ) return FALSE;

    strncpy(buf, name, 200);
    strcat(buf, ".url");

    *url = NULL;

    PREF_CopyConfigString(buf, url);

    return ( url != NULL && *url != NULL && **url != '\0' );
}


