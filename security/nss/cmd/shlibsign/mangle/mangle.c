/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2003 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

/*
 * Test program to mangle 1 bit in a binary
 *
 * $Id: mangle.c,v 1.2 2003/02/07 21:12:26 relyea%netscape.com Exp $
 */

#include "nspr.h"
#include "plstr.h"
#include "plgetopt.h"
#include "prio.h"

static void
usage (char *program_name)
{
    PRFileDesc *pr_stderr;

    pr_stderr = PR_STDERR;
    PR_fprintf (pr_stderr, "Usage:");
    PR_fprintf (pr_stderr, "%s -i shared_library_name -o byte_offset -b bit\n", program_name);
}

#ifdef notdef
static char *
mkoutput(const char *input)
{
    int in_len = PORT_Strlen(input);
    char *output = PORT_Alloc(in_len+sizeof(SGN_SUFFIX));
    int index = in_len + 1 - sizeof("."SHLIB_SUFFIX);

    if ((index > 0) && 
	(PORT_Strncmp(&input[index],
			"."SHLIB_SUFFIX,sizeof("."SHLIB_SUFFIX)) == 0)) {
	in_len = index;
    }
    PORT_Memcpy(output,input,in_len);
    PORT_Memcpy(&output[in_len],SGN_SUFFIX,sizeof(SGN_SUFFIX));
    return output;
}


static void
lperror(const char *string)
{
     int errNum = PORT_GetError();
     const char *error = SECU_Strerror(errNum);
     fprintf(stderr,"%s: %s\n",string, error);
}
#endif


int
main (int argc, char **argv)
{
    /* buffers and locals */
    PLOptState	*optstate;
    char	*programName;
    char	cbuf;

    /* parameter set variables */
    const char  *libFile = NULL; 	
    int offset = -1;
    int bitOffset = -1;

    /* return values */
    int		 retval = -1;  /* 0 - test succeeded.  -1 - test failed */
    PRFileDesc *fd;
    int bytesRead;
    int bytesWritten;
    int pos;

    programName = PL_strrchr(argv[0], '/');
    programName = programName ? (programName + 1) : argv[0];

    optstate = PL_CreateOptState (argc, argv, "i:o:b:");
    if (optstate == NULL) {
	SECU_PrintError (programName, "PL_CreateOptState failed");
	return -1;
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
	return -1;
    }
    if ((bitOffset >= 8) || (bitOffset < 0)) {
	usage(programName);
	return -1;
    }

    if (offset < 0) {
	usage(programName);
	return -1;
    }

    /* open the target signature file */
    fd = PR_OpenFile(libFile,PR_RDWR,0666);
    if (fd == NULL ) {
	/* lperror(libFile); */
	goto loser;
    }

    /* read the byte */
    pos = PR_Seek(fd,offset, PR_SEEK_SET);
    if (pos != offset) {
	goto loser;
    }
    bytesRead = PR_Read(fd, &cbuf, 1);
    if (bytesRead != 1) {
	goto loser;
    }

    printf("Changing byte 0x%08x (%d): from %02x (%d) to ", 
					offset, offset, cbuf, cbuf);
    /* change it */
    cbuf ^= 1 << bitOffset;
    printf("%02x (%d)\n", cbuf, cbuf);

    /* write it back out */
    pos = PR_Seek(fd, offset, PR_SEEK_SET);
    if (pos != offset) {
	goto loser;
    }
    bytesWritten = PR_Write(fd, &cbuf, 1);
    if (bytesWritten != 1) {
	goto loser;
    }

    PR_Close(fd);
    retval = 0;


loser:

    PR_Cleanup ();
    return retval;
}

/*#DEFINES += -DSHLIB_SUFFIX=\"$(DLL_SUFFIX)\" -DSHLIB_PREFIX=\"$(DLL_PREFIX)\" */
