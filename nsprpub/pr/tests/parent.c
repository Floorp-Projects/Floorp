/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

/*
** file:        parent.c
** description: test the process machinery
**              see also child.c
*/

#include "prmem.h"
#include "prprf.h"
#include "prinit.h"
#include "prproces.h"

typedef struct Child
{
    const char *name;
    PRProcess *process;
    PRProcessAttr *attr;
} Child;

static void PrintUsage(void)
{
    PR_fprintf(PR_GetSpecialFD(PR_StandardError),
        "Usage: parent [-d] child [options]\n");
}

PRIntn main (PRIntn argc, char **argv)
{
    PRStatus rv;
    PRInt32 test_status = 1;
    PRFileDesc *debug = NULL;
    Child *child = PR_NEWZAP(Child);

    if (1 == argc)
    {
        PrintUsage();
        return 2;
    }

    argv += 1;  /* don't care about our program name */
    while (argv[0][0] == '-')
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

    if (NULL == *argv)
    {
        PrintUsage();
        return 2;
    }

    child->name = *argv;
    if (NULL != debug) PR_fprintf(debug, "Forking %s\n", child->name);

    child->attr = PR_NewProcessAttr();
    PR_SetStdioRedirect(
        child->attr, PR_StandardOutput,
        PR_GetSpecialFD(PR_StandardOutput));
    PR_SetStdioRedirect(
        child->attr, PR_StandardError,
        PR_GetSpecialFD(PR_StandardError));

    child->process = PR_CreateProcess(
        child->name, argv, NULL, child->attr);

    test_status = (NULL == child->process) ? 1 : 0;
    if (NULL != debug)
        PR_fprintf(
            debug, "Child was %sforked\n",
            (0 == test_status) ? "" : "NOT ");

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
    PR_Cleanup();
    return test_status;
    
}  /* main */

/* parent.c */

