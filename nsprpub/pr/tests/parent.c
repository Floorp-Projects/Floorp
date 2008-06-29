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

/*
** file:        parent.c
** description: test the process machinery
*/

#include "prmem.h"
#include "prprf.h"
#include "prinit.h"
#include "prproces.h"
#include "prinrval.h"

typedef struct Child
{
    const char *name;
    char **argv;
    PRProcess *process;
    PRProcessAttr *attr;
} Child;

/* for the default test 'cvar -c 2000' */
static char *default_argv[] = {"cvar", "-c", "2000", NULL};

static void PrintUsage(void)
{
    PR_fprintf(PR_GetSpecialFD(PR_StandardError),
        "Usage: parent [-d] child [options]\n");
}

PRIntn main (PRIntn argc, char **argv)
{
    PRStatus rv;
    PRInt32 test_status = 1;
    PRIntervalTime t_start, t_elapsed;
    PRFileDesc *debug = NULL;
    Child *child = PR_NEWZAP(Child);

    if (1 == argc)
    {
        /* no command-line arguments: run the default test */
        child->argv = default_argv;
    }
    else
    {
        argv += 1;  /* don't care about our program name */
        while (*argv != NULL && argv[0][0] == '-')
        {
            if (argv[0][1] == 'd')
                debug = PR_GetSpecialFD(PR_StandardError);
            else
            {
                PrintUsage();
                return 2;  /* not sufficient */
            }
            argv += 1;
        }
        child->argv = argv;
    }

    if (NULL == *child->argv)
    {
        PrintUsage();
        return 2;
    }

    child->name = *child->argv;
    if (NULL != debug) PR_fprintf(debug, "Forking %s\n", child->name);

    child->attr = PR_NewProcessAttr();
    PR_ProcessAttrSetStdioRedirect(
        child->attr, PR_StandardOutput,
        PR_GetSpecialFD(PR_StandardOutput));
    PR_ProcessAttrSetStdioRedirect(
        child->attr, PR_StandardError,
        PR_GetSpecialFD(PR_StandardError));

    t_start = PR_IntervalNow();
    child->process = PR_CreateProcess(
        child->name, child->argv, NULL, child->attr);
    t_elapsed = (PRIntervalTime) (PR_IntervalNow() - t_start);

    PR_DestroyProcessAttr(child->attr);

    test_status = (NULL == child->process) ? 1 : 0;
    if (NULL != debug)
    {
        PR_fprintf(
            debug, "Child was %sforked\n",
            (0 == test_status) ? "" : "NOT ");
        if (0 == test_status)
            PR_fprintf(
                debug, "PR_CreateProcess took %lu microseconds\n",
                PR_IntervalToMicroseconds(t_elapsed));
    }

    if (0 == test_status)
    {
        if (NULL != debug) PR_fprintf(debug, "Waiting for child to exit\n");
        rv = PR_WaitProcess(child->process, &test_status);
        if (PR_SUCCESS == rv)
        {
            if (NULL != debug)
                PR_fprintf(
                    debug, "Child exited %s\n",
                    (0 == test_status) ? "successfully" : "with error");
        }
        else
        {
            test_status = 1;
            if (NULL != debug)
                PR_fprintf(debug, "PR_WaitProcess failed\n");
        }
    }
    PR_DELETE(child);
    PR_Cleanup();
    return test_status;
    
}  /* main */

/* parent.c */

