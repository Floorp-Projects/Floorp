/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
