/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
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

#include "prio.h"
#include "prprf.h"
#include "prlink.h"
#include "prvrsion.h"

#include "plerror.h"
#include "plgetopt.h"

PRIntn main(PRIntn argc, char **argv)
{
    PRIntn rv = 1;
    PLOptStatus os;
    PRIntn verbosity = 0;
    PRLibrary *runtime = NULL;
    const char *library_name = NULL;
    const PRVersionDescription *version_info;
	char buffer[100];
	PRExplodedTime exploded;
    PLOptState *opt = PL_CreateOptState(argc, argv, "d");

    PRFileDesc *err = PR_GetSpecialFD(PR_StandardError);

    while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
        if (PL_OPT_BAD == os) continue;
        switch (opt->option)
        {
        case 0:  /* fully qualified library name */
            library_name = opt->value;
            break;
        case 'd':  /* verbodity */
            verbosity += 1;
            break;
         default:
            PR_fprintf(err, "Usage: version [-d] {fully qualified library name}\n");
            return 2;  /* but not a lot else */
        }
    }
    PL_DestroyOptState(opt);

    if (NULL != library_name)
    {
        runtime = PR_LoadLibrary(library_name);
        if (NULL == runtime) {
			PL_FPrintError(err, "PR_LoadLibrary");
			return 3;
		} else {
            versionEntryPointType versionPoint = (versionEntryPointType)
                PR_FindSymbol(runtime, "libVersionPoint");
            if (NULL == versionPoint) {
				PL_FPrintError(err, "PR_FindSymbol");
				return 4;
			}	
			version_info = versionPoint();
		}
	} else
		version_info = libVersionPoint();	/* NSPR's version info */

	(void)PR_fprintf(err, "Runtime library version information\n");
	PR_ExplodeTime(
		version_info->buildTime, PR_GMTParameters, &exploded);
	(void)PR_FormatTime(
		buffer, sizeof(buffer), "%d %b %Y %H:%M:%S", &exploded);
	(void)PR_fprintf(err, "  Build time: %s GMT\n", buffer);
	(void)PR_fprintf(
		err, "  Build time: %s\n", version_info->buildTimeString);
	(void)PR_fprintf(
		err, "  %s V%u.%u.%u (%s%s%s)\n",
		version_info->description,
		version_info->vMajor,
		version_info->vMinor,
		version_info->vPatch,
		(version_info->beta ? " beta " : ""),
		(version_info->debug ? " debug " : ""),
		(version_info->special ? " special" : ""));
	(void)PR_fprintf(err, "  filename: %s\n", version_info->filename);
	(void)PR_fprintf(err, "  security: %s\n", version_info->security);
	(void)PR_fprintf(err, "  copyright: %s\n", version_info->copyright);
	(void)PR_fprintf(err, "  comment: %s\n", version_info->comment);
	rv = 0;
    return rv;
}

/* version.c */
