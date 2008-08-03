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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2003
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
 * Test program to mangle 1 bit in a binary
 *
 * $Id: mangle.c,v 1.7 2007/01/26 19:38:05 nelson%bolyard.com Exp $
 */

#include "nspr.h"
#include "plstr.h"
#include "plgetopt.h"
#include "prio.h"

static PRFileDesc *pr_stderr;
static void
usage (char *program_name)
{

    PR_fprintf (pr_stderr, "Usage:");
    PR_fprintf (pr_stderr, "%s -i shared_library_name -o byte_offset -b bit\n", program_name);
}


int
main (int argc, char **argv)
{
    /* buffers and locals */
    PLOptState	*optstate;
    char	*programName;
    char	cbuf;

    /* parameter set variables */
    const char  *libFile = NULL; 	
    int bitOffset = -1;

    /* return values */
    int		 retval = 2;  /* 0 - test succeeded.
			       * 1 - illegal args 
			       * 2 - function failed */
    PRFileDesc *fd = NULL;
    int bytesRead;
    int bytesWritten;
    PROffset32   offset = -1;
    PROffset32   pos;

    programName = PL_strrchr(argv[0], '/');
    programName = programName ? (programName + 1) : argv[0];

    pr_stderr = PR_STDERR;

    optstate = PL_CreateOptState (argc, argv, "i:o:b:");
    if (optstate == NULL) {
	return 1;
    }

    while (PL_GetNextOpt (optstate) == PL_OPT_OK) {
	switch (optstate->option) {
          case 'i':
            libFile = optstate->value;
            break;

          case 'o':
            offset = atoi(optstate->value);
            break;

          case 'b':
            bitOffset = atoi(optstate->value);
            break;
	}
    }

    if (libFile == NULL) {
	usage(programName);
	return 1;
    }
    if ((bitOffset >= 8) || (bitOffset < 0)) {
	usage(programName);
	return 1;
    }

    /* open the target signature file */
    fd = PR_OpenFile(libFile,PR_RDWR,0666);
    if (fd == NULL ) {
	/* lperror(libFile); */
	PR_fprintf(pr_stderr,"Couldn't Open %s\n",libFile);
	goto loser;
    }

    if (offset < 0) { /* convert to positive offset */
	pos = PR_Seek(fd, offset, PR_SEEK_END);
	if (pos == -1) {
	    PR_fprintf(pr_stderr,"Seek for read on %s (to %d) failed\n", 
		       libFile, offset);
	    goto loser;
	}
	offset = pos;
    }

    /* read the byte */
    pos = PR_Seek(fd, offset, PR_SEEK_SET);
    if (pos != offset) {
	PR_fprintf(pr_stderr,"Seek for read on %s (to %d) failed\n", 
	           libFile, offset);
	goto loser;
    }
    bytesRead = PR_Read(fd, &cbuf, 1);
    if (bytesRead != 1) {
	PR_fprintf(pr_stderr,"Read on %s (to %d) failed\n", libFile, offset);
	goto loser;
    }

    PR_fprintf(pr_stderr,"Changing byte 0x%08x (%d): from %02x (%d) to ", 
		offset, offset, (unsigned char)cbuf, (unsigned char)cbuf);
    /* change it */
    cbuf ^= 1 << bitOffset;
    PR_fprintf(pr_stderr,"%02x (%d)\n", 
               (unsigned char)cbuf, (unsigned char)cbuf);

    /* write it back out */
    pos = PR_Seek(fd, offset, PR_SEEK_SET);
    if (pos != offset) {
	PR_fprintf(pr_stderr,"Seek for write on %s (to %d) failed\n", 
	           libFile, offset);
	goto loser;
    }
    bytesWritten = PR_Write(fd, &cbuf, 1);
    if (bytesWritten != 1) {
	PR_fprintf(pr_stderr,"Write on %s (to %d) failed\n", libFile, offset);
	goto loser;
    }

    retval = 0;

loser:
    if (fd)
	PR_Close(fd);
    PR_Cleanup ();
    return retval;
}

/*#DEFINES += -DSHLIB_SUFFIX=\"$(DLL_SUFFIX)\" -DSHLIB_PREFIX=\"$(DLL_PREFIX)\" */
