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
#include "prlink.h"
#include "prvrsion.h"

#include "plerror.h"
#include "plgetopt.h"

PR_IMPORT(const PRVersionDescription *) libVersionPoint(void);

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
