/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Samir Gehani <sgehani@netscape.com>
 */

#include "XIErrors.h"

/*------------------------------------------------------------------*
 *   Default Error Handler
 *------------------------------------------------------------------*/
int ErrorHandler(int aErr)
{
    // XXX 1.  fix this to get a string associated with the error code
    // XXX     from a resourced string bundle
    // XXX
    // XXX 2.  fix this to throw up a dialog rather than writing to stdout

    if (aErr < FATAL_ERR_THRESHOLD)
    {
        printf("Fatal error[%d]: Doom and darkness has struck!\n", aErr); 
        exit(aErr);
    }
    else if (aErr < GENERAL_ERR_THRESHOLD)
        printf("Error[%d]: Regular error so moving right along.\n", aErr);
    else 
        printf("Warning[%d]: We spit crap to stdout cos we can!\n", aErr); 

    return aErr;
}

