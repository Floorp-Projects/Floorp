/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/***********************************************************************
**  1996 - Netscape Communications Corporation
**
**
** Name:        depend.c
** Description: Test to enumerate the dependencies
*
** Modification History:
** 14-May-97 AGarcia- Converted the test to accomodate the debug_mode flag.
**           The debug mode will print all of the printfs associated with this test.
**           The regress mode will be the default mode. Since the regress tool limits
**           the output to a one line status:PASS or FAIL,all of the printf statements
**           have been handled with an if (debug_mode) statement.
***********************************************************************/
#include "prinit.h"

/***********************************************************************
** Includes
***********************************************************************/
/* Used to get the command line option */
#include "plgetopt.h"

#include <stdio.h>
#include <stdlib.h>

static void PrintVersion(
    const char *msg, const PRVersion* info, PRIntn tab)
{
    static const len = 20;
    static const char *tabs = {"                    "};

    tab *= 2;
    if (tab > len) {
        tab = len;
    }
    printf("%s", &tabs[len - tab]);
    printf("%s ", msg);
    printf("%s ", info->id);
    printf("%d.%d", info->major, info->minor);
    if (0 != info->patch) {
        printf(".p%d", info->patch);
    }
    printf("\n");
}  /* PrintDependency */

static void ChaseDependents(const PRVersionInfo *info, PRIntn tab)
{
    PrintVersion("exports", &info->selfExport, tab);
    if (NULL != info->importEnumerator)
    {
        const PRDependencyInfo *dependent = NULL;
        while (NULL != (dependent = info->importEnumerator(dependent)))
        {
            const PRVersionInfo *import = dependent->exportInfoFn();
            PrintVersion("imports", &dependent->importNeeded, tab);
            ChaseDependents(import, tab + 1);
        }
    }
}  /* ChaseDependents */

static PRVersionInfo hack_export;
static PRVersionInfo dummy_export;
static PRDependencyInfo dummy_imports[2];

static const PRVersionInfo *HackExportInfo(void)
{
    hack_export.selfExport.major = 11;
    hack_export.selfExport.minor = 10;
    hack_export.selfExport.patch = 200;
    hack_export.selfExport.id = "Hack";
    hack_export.importEnumerator = NULL;
    return &hack_export;
}

static const PRDependencyInfo *DummyImports(
    const PRDependencyInfo *previous)
{
    if (NULL == previous) {
        return &dummy_imports[0];
    }
    else if (&dummy_imports[0] == previous) {
        return &dummy_imports[1];
    }
    else if (&dummy_imports[1] == previous) {
        return NULL;
    }
}  /* DummyImports */

static const PRVersionInfo *DummyLibVersion(void)
{
    dummy_export.selfExport.major = 1;
    dummy_export.selfExport.minor = 0;
    dummy_export.selfExport.patch = 0;
    dummy_export.selfExport.id = "Dumbass application";
    dummy_export.importEnumerator = DummyImports;

    dummy_imports[0].importNeeded.major = 2;
    dummy_imports[0].importNeeded.minor = 0;
    dummy_imports[0].importNeeded.patch = 0;
    dummy_imports[0].importNeeded.id = "Netscape Portable Runtime";
    dummy_imports[0].exportInfoFn = PR_ExportInfo;

    dummy_imports[1].importNeeded.major = 5;
    dummy_imports[1].importNeeded.minor = 1;
    dummy_imports[1].importNeeded.patch = 2;
    dummy_imports[1].importNeeded.id = "Hack Library";
    dummy_imports[1].exportInfoFn = HackExportInfo;

    return &dummy_export;
}  /* DummyLibVersion */

int main(int argc, char **argv)
{
    PRIntn tab = 0;
    const PRVersionInfo *info = DummyLibVersion();
    const char *buildDate = __DATE__, *buildTime = __TIME__;

    printf("Depend.c build time is %s %s\n", buildDate, buildTime);

    if (NULL != info) {
        ChaseDependents(info, tab);
    }

    return 0;
}  /* main */

/* depend.c */
