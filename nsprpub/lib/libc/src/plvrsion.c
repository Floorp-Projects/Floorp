/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

#include "prinit.h"
#include "prvrsion.h"

/************************************************************************/
/**************************IDENTITY AND VERSIONING***********************/
/************************************************************************/
#include "_pl_bld.h"
#if !defined(_BUILD_TIME)
#ifdef HAVE_LONG_LONG
#define _BUILD_TIME 0
#else
#define _BUILD_TIME {0, 0}
#endif
#endif
#if !defined(_BUILD_STRING)
#define _BUILD_STRING ""
#endif
#if !defined(_PRODUCTION)
#define _PRODUCTION ""
#endif

static PRVersionDescription prVersionDescription_libplc21 =
{
    /* version          */  2,                  /* this is the only one supported */
    /* buildTime        */  _BUILD_TIME,        /* usecs since midnight 1/1/1970 GMT */
    /* buildTimeString  */  _BUILD_STRING,       /*    ditto, but human readable */
    /* vMajor           */  PR_VMAJOR,          /* NSPR's version number */
    /* vMinor           */  PR_VMINOR,          /*  and minor version */
    /* vPatch           */  PR_VPATCH,          /*  and patch */
    /* beta             */  PR_BETA,            /* beta build boolean */
#if defined(DEBUG)
    /* debug            */  PR_TRUE,            /* a debug build */
#else
    /* debug            */  PR_FALSE,           /* an optomized build */
#endif
    /* special          */  PR_FALSE,           /* they're all special, but ... */
    /* filename         */  _PRODUCTION,        /* the produced library name */
    /* description      */ "Portable runtime",  /* what we are */
    /* security         */ "N/A",               /* not applicable here */
    /* copywrite        */  "Copyright (c) 1998 Netscape Communications Corporation. All Rights Reserved",
    /* comment          */  "http://www.mozilla.org/NPL/",
    /* specialString    */ ""
};

#ifdef XP_UNIX

/*
 * Version information for the 'ident' and 'what commands
 */
static char rcsid[] = "$Version: NSPR " PR_VERSION "  " _BUILD_STRING " $";
static char sccsid[] = "@(#)NSPR " PR_VERSION "  " _BUILD_STRING;

#endif /* XP_UNIX */

PR_IMPLEMENT(const PRVersionDescription*) libVersionPoint()
{
#ifdef XP_UNIX
    /*
     * Add dummy references to rcsid and sccsid to prevent them
     * from being optimized away as unused variables.
     */
    const char *dummy;
    
    dummy = rcsid;
    dummy = sccsid;
#endif
    return &prVersionDescription_libplc21;
}  /* versionEntryPointType */

/* plvrsion.c */

