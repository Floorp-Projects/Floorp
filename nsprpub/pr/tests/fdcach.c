/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * File: fdcach.c
 * Description:
 *   This test verifies that the fd cache and stack are working
 *   correctly.
 */

#include "nspr.h"

#include <stdio.h>
#include <stdlib.h>

/*
 * Define ORDER_PRESERVED if the implementation of PR_SetFDCacheSize
 * preserves the ordering of the fd's when moving them between the
 * cache and the stack.
 */
#define ORDER_PRESERVED 1

/*
 * NUM_FDS must be <= FD_CACHE_SIZE.
 */
#define FD_CACHE_SIZE 1024
#define NUM_FDS 20

int main(int argc, char **argv)
{
    int i;
    PRFileDesc *fds[NUM_FDS];
    PRFileDesc *savefds[NUM_FDS];
    int numfds = sizeof(fds)/sizeof(fds[0]);

    /*
     * Switch between cache and stack when they are empty.
     * Then start with the fd cache.
     */
    PR_SetFDCacheSize(0, FD_CACHE_SIZE);
    PR_SetFDCacheSize(0, 0);
    PR_SetFDCacheSize(0, FD_CACHE_SIZE);

    /* Add some fd's to the fd cache. */
    for (i = 0; i < numfds; i++) {
        savefds[i] = PR_NewTCPSocket();
        if (NULL == savefds[i]) {
            fprintf(stderr, "PR_NewTCPSocket failed\n");
            exit(1);
        }
    }
    for (i = 0; i < numfds; i++) {
        if (PR_Close(savefds[i]) == PR_FAILURE) {
            fprintf(stderr, "PR_Close failed\n");
            exit(1);
        } 
    }

    /*
     * Create some fd's.  These fd's should come from
     * the fd cache.  Verify the FIFO ordering of the fd
     * cache.
     */
    for (i = 0; i < numfds; i++) {
        fds[i] = PR_NewTCPSocket();
        if (NULL == fds[i]) {
            fprintf(stderr, "PR_NewTCPSocket failed\n");
            exit(1);
        }
        if (fds[i] != savefds[i]) {
            fprintf(stderr, "fd cache malfunctioned\n");
            exit(1);
        }
    }
    /* Put the fd's back to the fd cache. */
    for (i = 0; i < numfds; i++) {
        if (PR_Close(savefds[i]) == PR_FAILURE) {
            fprintf(stderr, "PR_Close failed\n");
            exit(1);
        } 
    }

    /* Switch to the fd stack. */
    PR_SetFDCacheSize(0, 0);

    /*
     * Create some fd's.  These fd's should come from
     * the fd stack.
     */
    for (i = 0; i < numfds; i++) {
        fds[i] = PR_NewTCPSocket();
        if (NULL == fds[i]) {
            fprintf(stderr, "PR_NewTCPSocket failed\n");
            exit(1);
        }
#ifdef ORDER_PRESERVED
        if (fds[i] != savefds[numfds-1-i]) {
            fprintf(stderr, "fd stack malfunctioned\n");
            exit(1);
        }
#else
        savefds[numfds-1-i] = fds[i];
#endif
    }
    /* Put the fd's back to the fd stack. */
    for (i = 0; i < numfds; i++) {
        if (PR_Close(savefds[i]) == PR_FAILURE) {
            fprintf(stderr, "PR_Close failed\n");
            exit(1);
        } 
    }

    /*
     * Now create some fd's and verify the LIFO ordering of
     * the fd stack.
     */
    for (i = 0; i < numfds; i++) {
        fds[i] = PR_NewTCPSocket();
        if (NULL == fds[i]) {
            fprintf(stderr, "PR_NewTCPSocket failed\n");
            exit(1);
        }
        if (fds[i] != savefds[numfds-1-i]) {
            fprintf(stderr, "fd stack malfunctioned\n");
            exit(1);
        }
    }
    /* Put the fd's back to the fd stack. */
    for (i = 0; i < numfds; i++) {
        if (PR_Close(savefds[i]) == PR_FAILURE) {
            fprintf(stderr, "PR_Close failed\n");
            exit(1);
        } 
    }

    /* Switch to the fd cache. */
    PR_SetFDCacheSize(0, FD_CACHE_SIZE);

    for (i = 0; i < numfds; i++) {
        fds[i] = PR_NewTCPSocket();
        if (NULL == fds[i]) {
            fprintf(stderr, "PR_NewTCPSocket failed\n");
            exit(1);
        }
#ifdef ORDER_PRESERVED
        if (fds[i] != savefds[i]) {
            fprintf(stderr, "fd cache malfunctioned\n");
            exit(1);
        }
#else
        savefds[i] = fds[i];
#endif
    }
    for (i = 0; i < numfds; i++) {
        if (PR_Close(savefds[i]) == PR_FAILURE) {
            fprintf(stderr, "PR_Close failed\n");
            exit(1);
        } 
    }

    for (i = 0; i < numfds; i++) {
        fds[i] = PR_NewTCPSocket();
        if (NULL == fds[i]) {
            fprintf(stderr, "PR_NewTCPSocket failed\n");
            exit(1);
        }
        if (fds[i] != savefds[i]) {
            fprintf(stderr, "fd cache malfunctioned\n");
            exit(1);
        }
    }
    for (i = 0; i < numfds; i++) {
        if (PR_Close(savefds[i]) == PR_FAILURE) {
            fprintf(stderr, "PR_Close failed\n");
            exit(1);
        } 
    }

    /* Switch to the fd stack. */
    PR_SetFDCacheSize(0, 0);

    for (i = 0; i < numfds; i++) {
        fds[i] = PR_NewTCPSocket();
        if (NULL == fds[i]) {
            fprintf(stderr, "PR_NewTCPSocket failed\n");
            exit(1);
        }
#ifdef ORDER_PRESERVED
        if (fds[i] != savefds[numfds-1-i]) {
            fprintf(stderr, "fd stack malfunctioned\n");
            exit(1);
        }
#else
        savefds[numfds-1-i];
#endif
    }
    for (i = 0; i < numfds; i++) {
        if (PR_Close(savefds[i]) == PR_FAILURE) {
            fprintf(stderr, "PR_Close failed\n");
            exit(1);
        } 
    }

    for (i = 0; i < numfds; i++) {
        fds[i] = PR_NewTCPSocket();
        if (NULL == fds[i]) {
            fprintf(stderr, "PR_NewTCPSocket failed\n");
            exit(1);
        }
        if (fds[i] != savefds[numfds-1-i]) {
            fprintf(stderr, "fd stack malfunctioned\n");
            exit(1);
        }
    }
    for (i = 0; i < numfds; i++) {
        if (PR_Close(savefds[i]) == PR_FAILURE) {
            fprintf(stderr, "PR_Close failed\n");
            exit(1);
        } 
    }

    PR_Cleanup();
    printf("PASS\n");
    return 0;
}
