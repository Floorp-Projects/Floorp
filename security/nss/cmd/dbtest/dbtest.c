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
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * Sonja Mirtitsch Sun Microsystems
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
** dbtest.c
**
** QA test for cert and key databases, especially to open
** database readonly (NSS_INIT_READONLY) and force initializations
** even if the databases cannot be opened (NSS_INIT_FORCEOPEN)
**
*/
#include <stdio.h>
#include <string.h>

#if defined(WIN32)
#include "fcntl.h"
#include "io.h"
#endif

#include "secutil.h"

#if defined(XP_UNIX)
#include <unistd.h>
#endif

#include "nspr.h"
#include "prtypes.h"
#include "certdb.h"
#include "nss.h"
#include "../modutil/modutil.h"

#include "plgetopt.h"

static char *progName;

char *dbDir  =  NULL;

static char *dbName[]={"secmod.db", "cert8.db", "key3.db"}; 
static char* dbprefix = "";
static char* secmodName = "secmod.db";
PRBool verbose;


static void Usage(const char *progName)
{
    printf("Usage:  %s [-r] [-f] [-d dbdir ] \n",
         progName);
    printf("%-20s open database readonly (NSS_INIT_READONLY)\n", "-r");
    printf("%-20s Continue to force initializations even if the\n", "-f");
    printf("%-20s databases cannot be opened (NSS_INIT_FORCEOPEN)\n", " ");
    printf("%-20s Directory with cert database (default is .\n",
          "-d certdir");
    exit(1);
}

int main(int argc, char **argv)
{
    PLOptState *optstate;
    PLOptStatus optstatus;

    PRUint32 flags = 0;
    Error ret;
    SECStatus rv;
    char * dbString = NULL;
    int i;

    progName = strrchr(argv[0], '/');
    if (!progName)
        progName = strrchr(argv[0], '\\');
    progName = progName ? progName+1 : argv[0];

    optstate = PL_CreateOptState(argc, argv, "rfd:h");

    while ((optstatus = PL_GetNextOpt(optstate)) == PL_OPT_OK) {
        switch (optstate->option) {
          case 'h':
          default : Usage(progName);                    break;

          case 'r': flags |= NSS_INIT_READONLY;         break;

          case 'f': flags |= NSS_INIT_FORCEOPEN;        break;

          case 'd':
                dbDir = PORT_Strdup(optstate->value);
                break;

        }
    }
    if (optstatus == PL_OPT_BAD)
        Usage(progName);

    if (!dbDir) {
        dbDir = SECU_DefaultSSLDir(); /* Look in $SSL_DIR */
    }
    dbDir = SECU_ConfigDirectory(dbDir);
    PR_fprintf(PR_STDERR, "dbdir selected is %s\n\n", dbDir);

    if( dbDir[0] == '\0') {
        PR_fprintf(PR_STDERR, errStrings[DIR_DOESNT_EXIST_ERR], dbDir);
        ret= DIR_DOESNT_EXIST_ERR;
        goto loser;
    }


    PR_Init( PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);

    /* get the status of the directory and databases and output message */
    if(PR_Access(dbDir, PR_ACCESS_EXISTS) != PR_SUCCESS) {
        PR_fprintf(PR_STDERR, errStrings[DIR_DOESNT_EXIST_ERR], dbDir);
    } else if(PR_Access(dbDir, PR_ACCESS_READ_OK) != PR_SUCCESS) {
        PR_fprintf(PR_STDERR, errStrings[DIR_NOT_READABLE_ERR], dbDir);
    } else {
        if( !( flags & NSS_INIT_READONLY ) &&
                PR_Access(dbDir, PR_ACCESS_WRITE_OK) != PR_SUCCESS) {
            PR_fprintf(PR_STDERR, errStrings[DIR_NOT_WRITEABLE_ERR], dbDir);
        }
        for (i=0;i<3;i++) {
            dbString=PR_smprintf("%s/%s",dbDir,dbName[i]);
            PR_fprintf(PR_STDOUT, "database checked is %s\n",dbString);
            if(PR_Access(dbString, PR_ACCESS_EXISTS) != PR_SUCCESS) {
                PR_fprintf(PR_STDERR, errStrings[FILE_DOESNT_EXIST_ERR], 
                                      dbString);
            } else if(PR_Access(dbString, PR_ACCESS_READ_OK) != PR_SUCCESS) {
                PR_fprintf(PR_STDERR, errStrings[FILE_NOT_READABLE_ERR], 
                                      dbString);
            } else if( !( flags & NSS_INIT_READONLY ) &&
                    PR_Access(dbString, PR_ACCESS_WRITE_OK) != PR_SUCCESS) {
                PR_fprintf(PR_STDERR, errStrings[FILE_NOT_WRITEABLE_ERR], 
                                      dbString);
            }
        }
    }

    rv = NSS_Initialize(SECU_ConfigDirectory(dbDir), dbprefix, dbprefix,
                   secmodName, flags);
    if (rv != SECSuccess) {
        SECU_PrintPRandOSError(progName);
        ret=NSS_INITIALIZE_FAILED_ERR;
    } else {
        if (NSS_Shutdown() != SECSuccess) {
            exit(1);
        }
        ret=SUCCESS;
    }

loser:
    return ret;
}

