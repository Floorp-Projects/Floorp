/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *    Doug Turner <dougt@netscape.com>
 */

#include "nspr.h"

int PrintTime(PRTime* time)
{
    char buffer[30];
   
    PR_snprintf(buffer, 30, "%lld", *time);
    
    printf("time: %s", buffer);

    return 0;
}

int ParseTime(const char* string, PRTime* time)
{
    PRStatus status = PR_ParseTimeString (string, PR_FALSE, time);
    
    if (status == PR_FAILURE)
        printf("ERROR: Could not parse time. \n");
    return 0;
}

int Usage()
{
    printf("Usage: timebombgen [-ct] string\n");
    printf("       All time relative to local time zone.\n");
    return 0;
}

int
main(int argc, char **argv)
{
    PRTime time = LL_Zero();
   
    if (argc < 2) 
        return Usage();
   
    char *command = argv[1];

    if (command && command[0] == '-')
    {
        if (command[1] == 'c')
        {
            time = PR_Now();
            return PrintTime(&time);
        }
        else if (command[1] == 't')
        {
            char * timeString = argv[2];

            if (argc < 3 || !timeString  || timeString[0] == '\0')
                return Usage();

            ParseTime(timeString, &time);
            return PrintTime(&time);
        }
    }
    
    return Usage();
}
