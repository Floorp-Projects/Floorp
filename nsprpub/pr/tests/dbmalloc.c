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

/***********************************************************************
**
** Name: dbmalloc.c
**
** Description: Testing malloc (OBSOLETE)
**
** Modification History:
** 14-May-97 AGarcia- Converted the test to accomodate the debug_mode flag.
**	         The debug mode will print all of the printfs associated with this test.
**			 The regress mode will be the default mode. Since the regress tool limits
**           the output to a one line status:PASS or FAIL,all of the printf statements
**			 have been handled with an if (debug_mode) statement. 
***********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "nspr.h"

void
usage
(
    void
)
{
    fprintf(stderr, "Usage: dbmalloc ('-m'|'-s') '-f' num_fails ('-d'|'-n') filename [...]\n");
    exit(0);
}

typedef struct node_struct
{
    struct node_struct *next, *prev;
    int line;
    char value[4];
}
    node_t,
   *node_pt;

node_pt get_node(const char *line)
{
    node_pt rv;
    int l = strlen(line);
    rv = (node_pt)PR_MALLOC(sizeof(node_t) + l + 1 - 4);
    if( (node_pt)0 == rv ) return (node_pt)0;
    memcpy(&rv->value[0], line, l+1);
    rv->next = rv->prev = (node_pt)0;
    return rv;
}

void
dump
(
    const char *name,
    node_pt     node,
    int         mf,
    int         debug
)
{
    if( (node_pt)0 != node->prev ) dump(name, node->prev, mf, debug);
    if( 0 != debug ) printf("[%s]: %6d: %s", name, node->line, node->value);
    if( node->line == mf ) fprintf(stderr, "[%s]: Line %d was allocated!\n", name, node->line);
    if( (node_pt)0 != node->next ) dump(name, node->next, mf, debug);
    return;
}

void
release
(
    node_pt node
)
{
    if( (node_pt)0 != node->prev ) release(node->prev);
    if( (node_pt)0 != node->next ) release(node->next);
    PR_DELETE(node);
}

int
t2
(
    const char *name,
    int         mf,
    int         debug
)
{
    int rv;
    FILE *fp;
    int l = 0;
    node_pt head = (node_pt)0;
    char buffer[ BUFSIZ ];

    fp = fopen(name, "r");
    if( (FILE *)0 == fp )
    {
        fprintf(stderr, "[%s]: Cannot open \"%s.\"\n", name, name);
        return -1;
    }

    /* fgets mallocs a buffer, first time through. */
    if( (char *)0 == fgets(buffer, BUFSIZ, fp) )
    {
        fprintf(stderr, "[%s]: \"%s\" is empty.\n", name, name);
        (void)fclose(fp);
        return -1;
    }

    rewind(fp);

    if( PR_SUCCESS != PR_ClearMallocCount() )
    {
        fprintf(stderr, "[%s]: Cannot clear malloc count.\n", name);
        (void)fclose(fp);
        return -1;
    }

    if( PR_SUCCESS != PR_SetMallocCountdown(mf) )
    {
        fprintf(stderr, "[%s]: Cannot set malloc countdown to %d\n", name, mf);
        (void)fclose(fp);
        return -1;
    }

    while( fgets(buffer, BUFSIZ, fp) )
    {
        node_pt n;
        node_pt *w = &head;

        if( (strlen(buffer) == (BUFSIZ-1)) && (buffer[BUFSIZ-2] != '\n') )
            buffer[BUFSIZ-2] == '\n';

        l++;

        n = get_node(buffer);
        if( (node_pt)0 == n ) 
        {
            printf("[%s]: Line %d: malloc failure!\n", name, l);
            continue;
        }

        n->line = l;

        while( 1 )
        {
            int comp;

            if( (node_pt)0 == *w )
            {
                *w = n;
                break;
            }

            comp = strcmp((*w)->value, n->value);
            if( comp < 0 ) w = &(*w)->next;
            else w = &(*w)->prev;
        }
    }

    (void)fclose(fp);

    dump(name, head, mf, debug);

    rv = PR_GetMallocCount();
    PR_ClearMallocCountdown();

    release(head);

    return rv;
}

int nf = 0;
int debug = 0;

void
test
(
    const char *name
)
{
    int n, i;

    extern int nf, debug;

    printf("[%s]: starting test 0\n", name);
    n = t2(name, 0, debug);
    if( -1 == n ) return;
    printf("[%s]: test 0 had %ld allocations.\n", name, n);

    if( 0 >= n ) return;

    for( i = 0; i < nf; i++ )
    {
        int which = rand() % n;
        if( 0 == which ) printf("[%s]: starting test %d -- no allocation should fail\n", name, i+1);
        else printf("[%s]: starting test %d -- allocation %d should fail\n", name, i+1, which);
        (void)t2(name, which, debug);
        printf("[%s]: test %d done.\n", name, i+1);
    }

    return;
}

int
main
(
    int     argc,
    char   *argv[]
)
{
    int okay = 0;
    int multithread = 0;

    struct threadlist
    {
        struct threadlist *next;
        PRThread *thread;
    }
        *threadhead = (struct threadlist *)0;

    extern int nf, debug;

    srand(time(0));

    PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
    PR_STDIO_INIT();

    printf("[main]: We %s using the debugging malloc.\n",
           PR_IsDebuggingMalloc() ? "ARE" : "ARE NOT");

    while( argv++, --argc )
    {
        if( '-' == argv[0][0] )
        {
            switch( argv[0][1] )
            {
                case 'f':
                    nf = atoi(argv[0][2] ? &argv[0][2] :
                              --argc ? *++argv : "0");
                    break;
                case 'd':
                    debug = 1;
                    break;
                case 'n':
                    debug = 0;
                    break;
                case 'm':
                    multithread = 1;
                    break;
                case 's':
                    multithread = 0;
                    break;
                default:
                    usage();
                    break;
            }
        }
        else
        {
            FILE *fp = fopen(*argv, "r");
            if( (FILE *)0 == fp )
            {
                fprintf(stderr, "Cannot open \"%s.\"\n", *argv);
                continue;
            }

            okay++;
            (void)fclose(fp);
            if( multithread )
            {
                struct threadlist *n;

                n = (struct threadlist *)malloc(sizeof(struct threadlist));
                if( (struct threadlist *)0 == n ) 
                {
                    fprintf(stderr, "This is getting tedious. \"%s\"\n", *argv);
                    continue;
                }

                n->next = threadhead;
                n->thread = PR_CreateThread(PR_USER_THREAD, (void (*)(void *))test, 
                                            *argv, PR_PRIORITY_NORMAL, 
                                            PR_LOCAL_THREAD, PR_JOINABLE_THREAD,
                                            0);
                if( (PRThread *)0 == n->thread )
                {
                    fprintf(stderr, "Can't create thread for \"%s.\"\n", *argv);
                    continue;
                }
                else
                {
                    threadhead = n;
                }
            }
            else
            {
                test(*argv);
            }
        }
    }

    if( okay == 0 ) usage();
    else while( (struct threadlist *)0 != threadhead )
    {
        struct threadlist *x = threadhead->next;
        (void)PR_JoinThread(threadhead->thread);
        PR_DELETE(threadhead);
        threadhead = x;
    }

    return 0;
}

