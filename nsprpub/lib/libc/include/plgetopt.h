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

/*
** File:          plgetopt.h
** Description:   utilities to parse argc/argv
*/

#if defined(PLGETOPT_H_)
#else
#define PLGETOPT_H_

#include "prtypes.h"

PR_BEGIN_EXTERN_C

typedef struct PLOptionInternal PLOptionInternal; 

typedef enum
{
        PL_OPT_OK,              /* all's well with the option */
        PL_OPT_EOL,             /* end of options list */
        PL_OPT_BAD              /* invalid option (and value) */
} PLOptStatus;

typedef struct PLOptState
{
    char option;                /* the name of the option */
    const char *value;          /* the value of that option | NULL */

    PLOptionInternal *internal; /* private processing state */

} PLOptState;

PR_EXTERN(PLOptState*) PL_CreateOptState(
        PRIntn argc, char **argv, const char *options);

PR_EXTERN(void) PL_DestroyOptState(PLOptState *opt);

PR_EXTERN(PLOptStatus) PL_GetNextOpt(PLOptState *opt);

PR_END_EXTERN_C

#endif /* defined(PLGETOPT_H_) */

/* plgetopt.h */

