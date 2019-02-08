/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#if defined(DEBUG)
#define _DEBUG_STRING " (debug)"
#else
#define _DEBUG_STRING ""
#endif

/*
 * A trick to expand the PR_VMAJOR macro before concatenation.
 */
#define CONCAT(x, y) x ## y
#define CONCAT2(x, y) CONCAT(x, y)
#define VERSION_DESC_NAME CONCAT2(prVersionDescription_libplds, PR_VMAJOR)

PRVersionDescription VERSION_DESC_NAME =
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
    /* comment          */  "http://www.mozilla.org/MPL/",
    /* specialString    */ ""
};

#ifdef XP_UNIX

/*
 * Version information for the 'ident' and 'what commands
 *
 * NOTE: the first component of the concatenated rcsid string
 * must not end in a '$' to prevent rcs keyword substitution.
 */
static char rcsid[] = "$Header: NSPR " PR_VERSION _DEBUG_STRING
        "  " _BUILD_STRING " $";
static char sccsid[] = "@(#)NSPR " PR_VERSION _DEBUG_STRING
        "  " _BUILD_STRING;

#endif /* XP_UNIX */

#ifdef _PR_HAS_PRAGMA_DIAGNOSTIC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif
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
    return &VERSION_DESC_NAME;
}  /* versionEntryPointType */
#ifdef _PR_HAS_PRAGMA_DIAGNOSTIC
#pragma GCC diagnostic pop
#endif

/* plvrsion.c */

