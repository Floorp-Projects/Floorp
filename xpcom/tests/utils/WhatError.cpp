/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

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
