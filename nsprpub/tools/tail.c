/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
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

#include "prio.h"
#include "prprf.h"
#include "prinit.h"
#include "prthread.h"
#include "prinrval.h"

#include "plerror.h"
#include "plgetopt.h"

#include <stdlib.h>

#define BUFFER_SIZE 500

static PRFileDesc *out = NULL, *err = NULL;

static void Help(void)
{
    PR_fprintf(err, "Usage: tail [-n <n>] [-f] [-h] <filename>\n");
    PR_fprintf(err, "\t-t <n>	Dally time in milliseconds\n");
    PR_fprintf(err, "\t-n <n>	Number of bytes before <eof>\n");
    PR_fprintf(err, "\t-f   	Follow the <eof>\n");
    PR_fprintf(err, "\t-h   	This message and nothing else\n");
}  /* Help */

PRIntn main(PRIntn argc, char **argv)
{
	PRIntn rv = 0;
    PLOptStatus os;
	PRStatus status;
	PRFileDesc *file;
	PRFileInfo fileInfo;
	PRIntervalTime dally;
	char buffer[BUFFER_SIZE];
	PRBool follow = PR_FALSE;
	const char *filename = NULL;
	PRUint32 position = 0, seek = 0, time = 0;
    PLOptState *opt = PL_CreateOptState(argc, argv, "hfn:");

    out = PR_GetSpecialFD(PR_StandardOutput);
    err = PR_GetSpecialFD(PR_StandardError);

    while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
        if (PL_OPT_BAD == os) continue;
        switch (opt->option)
        {
		case 0:  /* it's the filename */
			filename = opt->value;
			break;
        case 'n':  /* bytes before end of file */
            seek = atoi(opt->value);
            break;
        case 't':  /* dally time */
            time = atoi(opt->value);
            break;
        case 'f':  /* follow the end of file */
            follow = PR_TRUE;
            break;
        case 'h':  /* user wants some guidance */
            Help();  /* so give him an earful */
            return 2;  /* but not a lot else */
            break;
         default:
            break;
        }
    }
    PL_DestroyOptState(opt);

	if (0 == time) time = 1000;
	dally = PR_MillisecondsToInterval(time);

    if (NULL == filename)
    {
        (void)PR_fprintf(out, "Input file not specified\n");
        rv = 1; goto done;
    }
	file = PR_Open(filename, PR_RDONLY, 0);
	if (NULL == file)
	{
		PL_FPrintError(err, "File cannot be opened for reading");
		return 1;
	}

	status = PR_GetOpenFileInfo(file, &fileInfo);
	if (PR_FAILURE == status)
	{
		PL_FPrintError(err, "Cannot acquire status of file");
		rv = 1; goto done;
	}
	if (seek > 0)
	{
	    if (seek > fileInfo.size) seek = 0;
		position = PR_Seek(file, (fileInfo.size - seek), PR_SEEK_SET);
		if (-1 == (PRInt32)position)
			PL_FPrintError(err, "Cannot seek to starting position");
	}

	do
	{
		while (position < fileInfo.size)
		{
			PRInt32 read, bytes = fileInfo.size - position;
			if (bytes > sizeof(buffer)) bytes = sizeof(buffer);
			read = PR_Read(file, buffer, bytes);
			if (read != bytes)
				PL_FPrintError(err, "Cannot read to eof");
			position += read;
			PR_Write(out, buffer, read);
		}

		if (follow)
		{
			PR_Sleep(dally);
			status = PR_GetOpenFileInfo(file, &fileInfo);
			if (PR_FAILURE == status)
			{
				PL_FPrintError(err, "Cannot acquire status of file");
				rv = 1; goto done;
			}
		}
	} while (follow);

done:
	PR_Close(file);

	return rv;
}  /* main */

/* tail.c */
