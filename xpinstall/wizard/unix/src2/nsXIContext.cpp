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

#include "nsXIContext.h"

nsXIContext::nsXIContext()
{
    me = NULL;

    ldlg = NULL;
    wdlg = NULL;
    sdlg = NULL;
    cdlg = NULL;
    idlg = NULL;

    opt = new nsXIOptions();

    window = NULL;
    back = NULL;
    next = NULL;
    nextLabel = NULL;
    backLabel = NULL;
    acceptLabel = NULL;
    declineLabel = NULL;
    installLabel = NULL;
    logo = NULL; 

    backID = 0;
    nextID = 0;
    bMoving = FALSE;
}

nsXIContext::~nsXIContext()
{
    // NOTE: don't try to delete "me" cause I control thee

    XI_IF_DELETE(ldlg);
    XI_IF_DELETE(wdlg);
    XI_IF_DELETE(sdlg);
    XI_IF_DELETE(cdlg);
    XI_IF_DELETE(idlg);

    XI_IF_DELETE(opt);

    XI_IF_FREE(window);
    XI_IF_FREE(back);
    XI_IF_FREE(next);
    XI_IF_FREE(nextLabel);
    XI_IF_FREE(backLabel);
    XI_IF_FREE(acceptLabel);
    XI_IF_FREE(declineLabel);
    XI_IF_FREE(installLabel);
    XI_IF_FREE(logo);
}

char *
nsXIContext::itoa(int n)
{
	char *s;
	int i, j, sign, tmp;
	
	/* check sign and convert to positive to stringify numbers */
	if ( (sign = n) < 0)
		n = -n;
	i = 0;
	s = (char*) malloc(sizeof(char));
	
	/* grow string as needed to add numbers from powers of 10 
     * down till none left 
     */
	do
	{
		s = (char*) realloc(s, (i+1)*sizeof(char));
		s[i++] = n % 10 + '0';  /* '0' or 30 is where ASCII numbers start */
		s[i] = '\0';
	}
	while( (n /= 10) > 0);	
	
	/* tack on minus sign if we found earlier that this was negative */
	if (sign < 0)
	{
		s = (char*) realloc(s, (i+1)*sizeof(char));
		s[i++] = '-';
	}
	s[i] = '\0';
	
	/* pop numbers (and sign) off of string to push back into right direction */
	for (i = 0, j = strlen(s) - 1; i < j; i++, j--)
	{
		tmp = s[i];
		s[i] = s[j];
		s[j] = tmp;
	}
	
	return s;
}
