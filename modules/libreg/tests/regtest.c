/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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


