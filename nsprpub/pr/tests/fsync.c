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
#include "prmem.h"
#include "prprf.h"
#include "prinrval.h"

#include "plerror.h"
#include "plgetopt.h"

static PRFileDesc *err = NULL;

static void Help(void)
{
    PR_fprintf(err, "Usage: [-S] [-K <n>] [-h] <filename>\n");
    PR_fprintf(err, "\t-c   Nuber of iterations     (default: 10)\n");
    PR_fprintf(err, "\t-S   Sync the file           (default: FALSE)\n");
    PR_fprintf(err, "\t-K   Size of file (K bytes)  (default: 10)\n");
    PR_fprintf(err, "\t     Name of file to write   (default: /usr/tmp/sync.dat)\n");
    PR_fprintf(err, "\t-h   This message and nothing else\n");
}  /* Help */

PRIntn main(PRIntn argc, char **argv)
{
    PRStatus rv;
    PLOptStatus os;
    PRUint8 *buffer;
    PRFileDesc *file = NULL;
    const char *filename = "sync.dat";
    PRUint32 index, loops, iterations = 10, filesize = 10;
    PRIntn flags = PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE;
    PLOptState *opt = PL_CreateOptState(argc, argv, "hSK:c:");
    PRIntervalTime time, total = 0, shortest = 0x7fffffff, longest = 0;

    err = PR_GetSpecialFD(PR_StandardError);

    while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
        if (PL_OPT_BAD == os) continue;
        switch (opt->option)
        {
        case 0:       /* Name of file to create */
            filename = opt->value;
            break;
        case 'S':       /* Use sych option on file */
            flags |= PR_SYNC;
            break;
        case 'K':       /* Size of file to write */
            filesize = atoi(opt->value);
            break;
        case 'c':       /* Number of iterations */
            iterations = atoi(opt->value);
            break;
        case 'h':       /* user wants some guidance */
        default:        /* user needs some guidance */
            Help();     /* so give him an earful */
            return 2;   /* but not a lot else */
        }
    }
    PL_DestroyOptState(opt);

    file = PR_Open(filename, flags, 0666);
    if (NULL == file)
    {
        PL_FPrintError(err, "Failed to open file");
        return 1;
    }

    buffer = (PRUint8*)PR_CALLOC(1024);
    if (NULL == buffer)
    {
        PL_FPrintError(err, "Cannot allocate buffer");
        return 1;
    }

    for (index = 0; index < sizeof(buffer); ++index)
        buffer[index] = (PRUint8)index;

    for (loops = 0; loops < iterations; ++loops)
    {
        time = PR_IntervalNow();
        for (index = 0; index < filesize; ++index)
        {
            PR_Write(file, buffer, 1024);
        }
        time = (PR_IntervalNow() - time);

        total += time;
        if (time < shortest) shortest = time;
        else if (time > longest) longest = time;
        if (0 != PR_Seek(file, 0, PR_SEEK_SET))
        {
           PL_FPrintError(err, "Rewinding file");
           return 1;
        }
    }

    total = total / iterations;
    PR_fprintf(
        err, "%u iterations over a %u kbyte %sfile: %u [%u] %u\n",
        iterations, filesize, ((flags & PR_SYNC) ? "SYNCH'd " : ""),
        PR_IntervalToMicroseconds(shortest),
        PR_IntervalToMicroseconds(total),
        PR_IntervalToMicroseconds(longest));

    PR_DELETE(buffer);
    rv = PR_Close(file);
    if (PR_SUCCESS != rv)
    {
        PL_FPrintError(err, "Closing file failed");
        return 1;
    }
    rv = PR_Delete(filename);
    if (PR_SUCCESS != rv)
    {
        PL_FPrintError(err, "Deleting file failed");
        return 1;
    }
    return 0;
}
