/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
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

#include "plstr.h"
#include "prmem.h"

PR_IMPLEMENT(char *)
PL_strdup(const char *s)
{
    char *rv;
    PRUint32 l;

    l = PL_strlen(s);

    rv = (char *)malloc(l+1);
    if( (char *)0 == rv ) return rv;

    if( (const char *)0 == s )
        *rv = '\0';
    else
        (void)PL_strcpy(rv, s);

    return rv;
}

PR_IMPLEMENT(void)
PL_strfree(char *s)
{
	free(s);
}

PR_IMPLEMENT(char *)
PL_strndup(const char *s, PRUint32 max)
{
    char *rv;
    PRUint32 l;

    l = PL_strnlen(s, max);

    rv = (char *)malloc(l+1);
    if( (char *)0 == rv ) return rv;

    if( (const char *)0 == s )
        *rv = '\0';
    else
        (void)PL_strncpyz(rv, s, l+1);

    return rv;
}
