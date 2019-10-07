/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
** File:        tpd.c
** Description: Exercising the thread private data bailywick.
*/

#include "prmem.h"
#include "prinit.h"
#include "prlog.h"
#include "prprf.h"
#include "prthread.h"
#include "prtypes.h"

#include "private/pprio.h"

#include "plgetopt.h"

static PRUintn key[128];
static PRIntn debug = 0;
static PRBool failed = PR_FALSE;
static PRBool should = PR_TRUE;
static PRBool did = PR_TRUE;
static PRFileDesc *fout = NULL;

static void PrintProgress(PRIntn line)
{
    failed = failed || (should && !did);
    failed = failed || (!should && did);
    if (debug > 0)
    {
#if defined(WIN16)
        printf(
            "@ line %d destructor should%s have been called and was%s\n",
            line, ((should) ? "" : " NOT"), ((did) ? "" : " NOT"));
#else
        PR_fprintf(
            fout, "@ line %d destructor should%s have been called and was%s\n",
            line, ((should) ? "" : " NOT"), ((did) ? "" : " NOT"));
#endif
    }
}  /* PrintProgress */

static void MyAssert(const char *expr, const char *file, PRIntn line)
{
    if (debug > 0) {
        (void)PR_fprintf(fout, "'%s' in file: %s: %d\n", expr, file, line);
    }
}  /* MyAssert */

#define MY_ASSERT(_expr) \
    ((_expr)?((void)0):MyAssert(# _expr,__FILE__,__LINE__))


static void PR_CALLBACK Destructor(void *data)
{
    MY_ASSERT(NULL != data);
    if (should) {
        did = PR_TRUE;
    }
    else {
        failed = PR_TRUE;
    }
    /*
     * We don't actually free the storage since it's actually allocated
     * on the stack. Normally, this would not be the case and this is
     * the opportunity to free whatever.
    PR_Free(data);
     */
}  /* Destructor */

static void PR_CALLBACK Thread(void *null)
{
    void *pd;
    PRStatus rv;
    PRUintn keys;
    char *key_string[] = {
        "Key #0", "Key #1", "Key #2", "Key #3",
        "Bogus #5", "Bogus #6", "Bogus #7", "Bogus #8"
    };

    did = should = PR_FALSE;
    for (keys = 0; keys < 8; ++keys)
    {
        pd = PR_GetThreadPrivate(key[keys]);
        MY_ASSERT(NULL == pd);
    }
    PrintProgress(__LINE__);

    did = should = PR_FALSE;
    for (keys = 0; keys < 4; ++keys)
    {
        rv = PR_SetThreadPrivate(key[keys], key_string[keys]);
        MY_ASSERT(PR_SUCCESS == rv);
    }
    PrintProgress(__LINE__);

    did = should = PR_FALSE;
    for (keys = 4; keys < 8; ++keys)
    {
        rv = PR_SetThreadPrivate(key[keys], key_string[keys]);
        MY_ASSERT(PR_FAILURE == rv);
    }
    PrintProgress(__LINE__);

    did = PR_FALSE; should = PR_TRUE;
    for (keys = 0; keys < 4; ++keys)
    {
        rv = PR_SetThreadPrivate(key[keys], key_string[keys]);
        MY_ASSERT(PR_SUCCESS == rv);
    }
    PrintProgress(__LINE__);

    did = PR_FALSE; should = PR_TRUE;
    for (keys = 0; keys < 4; ++keys)
    {
        rv = PR_SetThreadPrivate(key[keys], NULL);
        MY_ASSERT(PR_SUCCESS == rv);
    }
    PrintProgress(__LINE__);

    did = should = PR_FALSE;
    for (keys = 0; keys < 4; ++keys)
    {
        rv = PR_SetThreadPrivate(key[keys], NULL);
        MY_ASSERT(PR_SUCCESS == rv);
    }
    PrintProgress(__LINE__);

    did = should = PR_FALSE;
    for (keys = 8; keys < 127; ++keys)
    {
        rv = PR_SetThreadPrivate(key[keys], "EXTENSION");
        MY_ASSERT(PR_SUCCESS == rv);
    }
    PrintProgress(__LINE__);

    did = PR_FALSE; should = PR_TRUE;
    for (keys = 8; keys < 127; ++keys)
    {
        rv = PR_SetThreadPrivate(key[keys], NULL);
        MY_ASSERT(PR_SUCCESS == rv);
    }
    PrintProgress(__LINE__);

    did = should = PR_FALSE;
    for (keys = 8; keys < 127; ++keys)
    {
        rv = PR_SetThreadPrivate(key[keys], NULL);
        MY_ASSERT(PR_SUCCESS == rv);
    }

    /* put in keys and leave them there for thread exit */
    did = should = PR_FALSE;
    for (keys = 0; keys < 4; ++keys)
    {
        rv = PR_SetThreadPrivate(key[keys], key_string[keys]);
        MY_ASSERT(PR_SUCCESS == rv);
    }
    PrintProgress(__LINE__);
    did = PR_FALSE; should = PR_TRUE;

}  /* Thread */

static PRIntn PR_CALLBACK Tpd(PRIntn argc, char **argv)
{
    void *pd;
    PRStatus rv;
    PRUintn keys;
    PRThread *thread;
    char *key_string[] = {
        "Key #0", "Key #1", "Key #2", "Key #3",
        "Bogus #5", "Bogus #6", "Bogus #7", "Bogus #8"
    };

    fout = PR_STDOUT;

    did = should = PR_FALSE;
    for (keys = 0; keys < 4; ++keys)
    {
        rv = PR_NewThreadPrivateIndex(&key[keys], Destructor);
        MY_ASSERT(PR_SUCCESS == rv);
    }
    PrintProgress(__LINE__);

    did = should = PR_FALSE;
    for (keys = 0; keys < 8; ++keys)
    {
        pd = PR_GetThreadPrivate(key[keys]);
        MY_ASSERT(NULL == pd);
    }
    PrintProgress(__LINE__);

    did = should = PR_FALSE;
    for (keys = 0; keys < 4; ++keys)
    {
        rv = PR_SetThreadPrivate(key[keys], key_string[keys]);
        MY_ASSERT(PR_SUCCESS == rv);
    }
    PrintProgress(__LINE__);

    for (keys = 4; keys < 8; ++keys) {
        key[keys] = 4096;    /* set to invalid value */
    }
    did = should = PR_FALSE;
    for (keys = 4; keys < 8; ++keys)
    {
        rv = PR_SetThreadPrivate(key[keys], key_string[keys]);
        MY_ASSERT(PR_FAILURE == rv);
    }
    PrintProgress(__LINE__);

    did = PR_FALSE; should = PR_TRUE;
    for (keys = 0; keys < 4; ++keys)
    {
        rv = PR_SetThreadPrivate(key[keys], key_string[keys]);
        MY_ASSERT(PR_SUCCESS == rv);
    }
    PrintProgress(__LINE__);

    did = PR_FALSE; should = PR_TRUE;
    for (keys = 0; keys < 4; ++keys)
    {
        rv = PR_SetThreadPrivate(key[keys], NULL);
        MY_ASSERT(PR_SUCCESS == rv);
    }
    PrintProgress(__LINE__);

    did = should = PR_FALSE;
    for (keys = 0; keys < 4; ++keys)
    {
        rv = PR_SetThreadPrivate(key[keys], NULL);
        MY_ASSERT(PR_SUCCESS == rv);
    }
    PrintProgress(__LINE__);

    did = should = PR_FALSE;
    for (keys = 8; keys < 127; ++keys)
    {
        rv = PR_NewThreadPrivateIndex(&key[keys], Destructor);
        MY_ASSERT(PR_SUCCESS == rv);
        rv = PR_SetThreadPrivate(key[keys], "EXTENSION");
        MY_ASSERT(PR_SUCCESS == rv);
    }
    PrintProgress(__LINE__);

    did = PR_FALSE; should = PR_TRUE;
    for (keys = 8; keys < 127; ++keys)
    {
        rv = PR_SetThreadPrivate(key[keys], NULL);
        MY_ASSERT(PR_SUCCESS == rv);
    }
    PrintProgress(__LINE__);

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

    PrintProgress(__LINE__);

#if defined(WIN16)
    printf(
        "%s\n",((PR_TRUE == failed) ? "FAILED" : "PASSED"));
#else
    (void)PR_fprintf(
        fout, "%s\n",((PR_TRUE == failed) ? "FAILED" : "PASSED"));
#endif

    return 0;

}  /* Tpd */

int main(int argc, char **argv)
{
    PLOptStatus os;
    PLOptState *opt = PL_CreateOptState(argc, argv, "dl:r:");
    while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
        if (PL_OPT_BAD == os) {
            continue;
        }
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
