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
 * The Original Code is InnoTek Plugin Wrapper code.
 *
 * The Initial Developer of the Original Code is
 * InnoTek Systemberatung GmbH.
 * Portions created by the Initial Developer are Copyright (C) 2003-2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   InnoTek Systemberatung GmbH / Knut St. Osmundsen
 *   Peter Weilbacher <mozilla@weilbacher.org>
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
 *  Debug Stuff.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef __IBMC__
#include <builtin.h>
#endif

#define INCL_BASE
#include <os2.h>

#define INCL_DEBUGONLY
#include "nsInnoTekPluginWrapper.h"

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
static const char szPrefix[] = "ipluginw: ";


/**
 * Writes to log.
 */
int     npdprintf(const char *pszFormat, ...)
{
    static int      fInited = 0;
    static FILE *   phFile;
    char    szMsg[4096];
    int     rc;
    va_list args;

    strcpy(szMsg, szPrefix);
    rc = strlen(szMsg);
    va_start(args, pszFormat);
    rc += vsprintf(&szMsg[rc], pszFormat, args);
    va_end(args);
    if (rc > (int)sizeof(szMsg) - 32)
    {
        /* we're most likely toasted now. */
#ifdef __IBMC__
        _interrupt(3);
#else
        asm("int $3");
#endif
    }

    if (rc > 0 && szMsg[rc - 1] != '\n')
    {
        szMsg[rc++] = '\n';
        szMsg[rc] = '\0';
    }

    fwrite(&szMsg[0], 1, rc, stderr);
    if (!fInited)
    {
        fInited = 1;
        phFile = fopen("ipluginw.log", "w");
        if (phFile)
        {
            DATETIME dt;
            DosGetDateTime(&dt);
            fprintf(phFile, "*** Log Opened on %04d-%02d-%02d %02d:%02d:%02d ***\n",
                    dt.year, dt.month, dt.day, dt.hours, dt.minutes, dt.seconds);
            fprintf(phFile, "*** Build Date: " __DATE__ " " __TIME__ " ***\n");
        }
    }
    if (phFile)
    {
        fwrite(&szMsg[0], 1, rc, phFile);
        fflush(phFile);
    }

    return rc;
}


/**
 * Release int 3.
 */
void _Optlink ReleaseInt3(unsigned uEAX, unsigned uEDX, unsigned uECX)
{
#ifdef __IBMC__
    _interrupt(3);
#else
    asm("int $3");
#endif
}
