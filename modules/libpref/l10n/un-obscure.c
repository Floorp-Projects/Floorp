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
 un-obscure.c

 Prints a netscape.cfg file in plaintext.

 Usage:
		un-obscure netscape.cfg
**********************************************************************/

#include <stdio.h>
#include <errno.h>


/*
 * main
 */
int
main(int argc, char* argv[])
{
    int i, n;
    FILE* file;
    unsigned char buf[1024];

    if ( argc != 2 ) {
        fprintf(stderr, "Usage: %s filename\n", argv[0]);
    }

    if ( (file = fopen(argv[1], "r")) == NULL ) {
        perror(argv[1]);
        exit(errno);
    }

    while ( (n = fread(buf, sizeof(char), sizeof(buf), file)) > 0 ) {
        for ( i = 0; i < n; i++ ) {
            buf[i]-= 7;
        }
        fwrite(buf, sizeof(char), n, stdout);
    }

    return 0;
}


