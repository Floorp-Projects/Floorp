/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape Portable Runtime (NSPR).
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#include "prinit.h"
#include "prvrsion.h"

/************************************************************************/
/**************************IDENTITY AND VERSIONING***********************/
/************************************************************************/
#ifndef XP_MAC
#include "_pr_bld.h"
#endif
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
#define VERSION_DESC_NAME CONCAT2(prVersionDescription_libnspr, PR_VMAJOR)

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
    /* comment          */  "License information: http://www.mozilla.org/NPL/",
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

PR_IMPLEMENT(const PRVersionDescription*) libVersionPoint(void)
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

/* prvrsion.c */

