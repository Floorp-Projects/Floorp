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

/*
** File:        tpd.c
** Description: Exercising the thread private data bailywick.
*/

#include "prinit.h"
#include "prlog.h"
#include "prprf.h"
#include "prthread.h"
#include "prtypes.h"

#if defined(XP_MAC)
#include "pprio.h"
#else
#include "private/pprio.h"
#endif

#include "plgetopt.h"

static PRUintn key[128];
static PRIntn debug = 0;
static PRBool failed = PR_FALSE;
static PRBool should = PR_TRUE;
static PRBool did = PR_TRUE;
static PRFileDesc *fout = NULL;

static void PrintProgress(void)
{
    if (debug > 0)
    {
#if defined(WIN16)
        printf(
            "Destructor should %s have been called and was%s\n",
            ((should) ? "" : " NOT"), ((did) ? "" : " NOT"));
#else    
        PR_fprintf(
            fout, "Destructor should %s have been called and was%s\n",
            ((should) ? "" : " NOT"), ((did) ? "" : " NOT"));
#endif
    }
}  /* PrintProgress */

static void MyAssert(const char *expr, const char *file, PRIntn line)
{
    if (debug > 0)
        (void)PR_fprintf(fout, "'%s' in file: %s: %d\n", expr, file, line);
}  /* MyAssert */

#define MY_ASSERT(_expr) \
    ((_expr)?((void)0):MyAssert(# _expr,__FILE__,__LINE__))


static void PR_CALLBACK Destructor(void *data)
{
    if (should) did = PR_TRUE;
    else failed = PR_TRUE;
    MY_ASSERT(NULL != data);
    if (debug > 0) MyAssert((const char*)data, __FILE__, __LINE__);
}  /* Destructor */

static void PR_CALLBACK Thread(void *null)
{
    void *pd;
    PRStatus rv;
    PRUintn keys;
    char *key_string[] = {
        "Key #0", "Key #1", "Key #2", "Key #3",
        "Bogus #5", "Bogus #6", "Bogus #7", "Bogus #8"};
    
    did = should = PR_FALSE;
    for (keys = 0; keys < 8; ++keys)
    {
        pd = PR_GetThreadPrivate(key[keys]);
        MY_ASSERT(NULL == pd);
    }
    PrintProgress();

    did = should = PR_FALSE;
    for (keys = 0; keys < 4; ++keys)
    {
        rv = PR_SetThreadPrivate(keys, key_string[keys]);
        MY_ASSERT(PR_SUCCESS == rv);
    }
    PrintProgress();

#if !defined(DEBUG)
    did = should = PR_FALSE;
    for (keys = 4; keys < 8; ++keys)
    {
        rv = PR_SetThreadPrivate(keys, key_string[keys]);
        MY_ASSERT(PR_FAILURE == rv);
    }
    PrintProgress();
#endif
    
    did = PR_FALSE; should = PR_TRUE;
    for (keys = 0; keys < 4; ++keys)
    {
        rv = PR_SetThreadPrivate(key[keys], key_string[keys]);
        MY_ASSERT(PR_SUCCESS == rv);
    }
    PrintProgress();

    did = PR_FALSE; should = PR_TRUE;
    for (keys = 0; keys < 4; ++keys)
    {
        rv = PR_SetThreadPrivate(key[keys], NULL);
        MY_ASSERT(PR_SUCCESS == rv);
    }
    PrintProgress();

    did = should = PR_FALSE;
    for (keys = 0; keys < 4; ++keys)
    {
        rv = PR_SetThreadPrivate(key[keys], NULL);
        MY_ASSERT(PR_SUCCESS == rv);
    }
    PrintProgress();

    did = should = PR_FALSE;
    for (keys = 8; keys < 127; ++keys)
    {
        rv = PR_SetThreadPrivate(key[keys], "EXTENSION");
        MY_ASSERT(PR_SUCCESS == rv);
    }
    PrintProgress();

    did = PR_FALSE; should = PR_TRUE;
    for (keys = 8; keys < 127; ++keys)
    {
        rv = PR_SetThreadPrivate(key[keys], NULL);
        MY_ASSERT(PR_SUCCESS == rv);
    }
    PrintProgress();

    did = should = PR_FALSE;
    for (keys = 8; keys < 127; ++keys)
    {
        rv = PR_SetThreadPrivate(key[keys], NULL);
        MY_ASSERT(PR_SUCCESS == rv);
    }
}  /* Thread */

static PRIntn PR_CALLBACK Tpd(PRIntn argc, char **argv)
{
    void *pd;
    PRStatus rv;
    PRUintn keys;
    PRThread *thread;
    char *key_string[] = {
        "Key #0", "Key #1", "Key #2", "Key #3",
        "Bogus #5", "Bogus #6", "Bogus #7", "Bogus #8"};
    
    fout = PR_STDOUT;

    did = should = PR_FALSE;
    for (keys = 0; keys < 4; ++keys)
    {
        rv = PR_NewThreadPrivateIndex(&key[keys], Destructor);
        MY_ASSERT(PR_SUCCESS == rv);
    }
    PrintProgress();

    did = should = PR_FALSE;
    for (keys = 0; keys < 8; ++keys)
    {
        pd = PR_GetThreadPrivate(key[keys]);
        MY_ASSERT(NULL == pd);
    }
    PrintProgress();

    did = should = PR_FALSE;
    for (keys = 0; keys < 4; ++keys)
    {
        rv = PR_SetThreadPrivate(keys, key_string[keys]);
        MY_ASSERT(PR_SUCCESS == rv);
    }
    PrintProgress();

#if !defined(DEBUG)
    did = should = PR_FALSE;
    for (keys = 4; keys < 8; ++keys)
    {
        rv = PR_SetThreadPrivate(keys, key_string[keys]);
        MY_ASSERT(PR_FAILURE == rv);
    }
    PrintProgress();
#endif
    
    did = PR_FALSE; should = PR_TRUE;
    for (keys = 0; keys < 4; ++keys)
    {
        rv = PR_SetThreadPrivate(key[keys], key_string[keys]);
        MY_ASSERT(PR_SUCCESS == rv);
    }
    PrintProgress();

    did = PR_FALSE; should = PR_TRUE;
    for (keys = 0; keys < 4; ++keys)
    {
        rv = PR_SetThreadPrivate(key[keys], NULL);
        MY_ASSERT(PR_SUCCESS == rv);
    }
    PrintProgress();

    did = should = PR_FALSE;
    for (keys = 0; keys < 4; ++keys)
    {
        rv = PR_SetThreadPrivate(key[keys], NULL);
        MY_ASSERT(PR_SUCCESS == rv);
    }
    PrintProgress();

    did = should = PR_FALSE;
    for (keys = 8; keys < 127; ++keys)
    {
        rv = PR_NewThreadPrivateIndex(&key[keys], Destructor);
        MY_ASSERT(PR_SUCCESS == rv);
        rv = PR_SetThreadPrivate(key[keys], "EXTENSION");
        MY_ASSERT(PR_SUCCESS == rv);
    }
    PrintProgress();

    did = PR_FALSE; should = PR_TRUE;
    for (keys = 8; keys < 127; ++keys)
    {
        rv = PR_SetThreadPrivate(key[keys], NULL);
        MY_ASSERT(PR_SUCCESS == rv);
    }
    PrintProgress();

    did = should = PR_FALSE;
    for (keys = 8; keys < 127; ++keys)
    {
        rv = PR_SetThreadPrivate(key[keys], NULL);
        MY_ASSERT(PR_SUCCESS == rv);
    }

    thread = PR_CreateThread(
        PR_USER_THREAD, Thread, NULL, PR_PRIORITY_NORMAL,
        PR_LOCAL_THREAD, PR_JOINABLE_THREAD, 0);

    (void)PR_JoinThread(thread);

#if defined(WIN16)
    printf(
        "%s\n",((PR_TRUE == failed) ? "FAILED" : "PASSED"));
#else
    (void)PR_fprintf(
        fout, "%s\n",((PR_TRUE == failed) ? "FAILED" : "PASSED"));
#endif    

    return 0;

}  /* Tpd */

PRIntn main(PRIntn argc, char *argv[])
{
	PLOptStatus os;
	PLOptState *opt = PL_CreateOptState(argc, argv, "dl:r:");
	while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
		if (PL_OPT_BAD == os) continue;
        switch (opt->option)
        {
        case 'd':  /* debug mode */
			debug = PR_TRUE;
            break;
         default:
            break;
        }
    }
	PL_DestroyOptState(opt);
    PR_STDIO_INIT();
    return PR_Initialize(Tpd, argc, argv, 0);
}  /* main */

/* tpd.c */
