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

#include <stdlib.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>

#include "NSReg.h"
#include "VerReg.h"

extern void interp(void);

#define REGFILE "c:\\temp\\reg.dat"

char *gRegistry;

int main(int argc, char *argv[]);

char *errstr(REGERR err)
{

	switch( err )
	{
	case REGERR_OK:
		return "REGERR_OK";
	case REGERR_FAIL:
		return "REGERR_FAIL";
	case REGERR_NOMORE:
		return "REGERR_MORE";
	case REGERR_NOFIND:
		return "REGERR_NOFIND";
	case REGERR_BADREAD:
		return "REGERR_BADREAD";
	case REGERR_BADLOCN:
		return "REGERR_BADLOCN";
	case REGERR_PARAM:
		return "REGERR_PARAM";
	case REGERR_BADMAGIC:
		return "REGERR_BADMAGIC";
    case REGERR_BADCHECK:
        return "REGERR_BADCHECK";
    case REGERR_NOFILE:
        return "REGERR_NOFILE";
    case REGERR_MEMORY:
        return "REGERR_MEMORY";
    case REGERR_BUFTOOSMALL:
        return "REGERR_BUFTOOSMALL";
    case REGERR_NAMETOOLONG:
        return "REGERR_NAMETOOLONG";
    case REGERR_REGVERSION:
        return "REGERR_REGVERSION";
    case REGERR_DELETED:
        return "REGERR_DELETED";
    case REGERR_BADTYPE:
        return "REGERR_BADTYPE";
    case REGERR_NOPATH:
        return "REGERR_NOPATH";
    case REGERR_BADNAME:
        return "REGERR_BADNAME";
    case REGERR_READONLY:
        return "REGERR_READONLY";
    case REGERR_BADUTF8:
        return "REGERR_BADUTF8";
	default:
		return "<Unknown>";
	}

}	// errstr


int main(int argc, char *argv[])
{
	printf("Registry Test 4/10/99.\n");

    interp();

	return 0;
}


