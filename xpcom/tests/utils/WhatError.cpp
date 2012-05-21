/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsError.h"
#include <stdlib.h>
#include <stdio.h>

int
main(int argc, char **argv)
{
    int errorCode = 0;
    char buffer[100];
    
    if (argc != 2)
        return -1;

    sscanf( argv[1], "0x%x", &errorCode);
    sprintf(buffer, "%d", errorCode);
    sscanf( buffer, "%d", &errorCode);
    
    printf( "Code: %d, Module: %d, Severity: %d\n", 
            NS_ERROR_GET_CODE(errorCode), 
            NS_ERROR_GET_MODULE(errorCode), 
            NS_ERROR_GET_SEVERITY(errorCode));

    return 0;
}
