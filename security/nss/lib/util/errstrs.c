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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Red Hat, Inc
 * Portions created by the Initial Developer are Copyright (C) 2009
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
#include "prerror.h"
#include "secerr.h"
#include "secport.h"
#include "prinit.h"
#include "prprf.h"
#include "prtypes.h"
#include "prlog.h"
#include "plstr.h"
#include "nssutil.h"
#include <string.h>

#define ER3(name, value, str) {#name, str},

static const struct PRErrorMessage sectext[] = {
#include "SECerrs.h"
    {0,0}
};

static const struct PRErrorTable sec_et = {
    sectext, "secerrstrings", SEC_ERROR_BASE, 
        (sizeof sectext)/(sizeof sectext[0]) 
};

static PRStatus 
nss_InitializePRErrorTableOnce(void) {
    return PR_ErrorInstallTable(&sec_et);
}

static PRCallOnceType once;

PRStatus
NSS_InitializePRErrorTable(void)
{
    return PR_CallOnce(&once, nss_InitializePRErrorTableOnce);
}

/* Returns a UTF-8 encoded constant error string for "errNum".
 * Returns NULL if either initialization of the error tables
 * or formatting fails due to insufficient memory. 
 *
 * This is the simpler common function that the others call.
 * It is thread safe and does not preappend anything to the
 * mapped error string.
 */
static char *
nss_Strerror(PRErrorCode errNum)
{
    static int initDone;

    if (!initDone) {
    /* nspr_InitializePRErrorTable(); done by PR_Init */
    PRStatus rv = NSS_InitializePRErrorTable();
    /* If this calls fails for insufficient memory, just return NULL */
    if (rv != PR_SUCCESS) return NULL;
	initDone = 1;
    }

    return (char *) PR_ErrorToString(errNum, PR_LANGUAGE_I_DEFAULT);
}

/* Hope this size is sufficient even with localization */
#define EBUFF_SIZE 512
static char ebuf[EBUFF_SIZE];

/* Returns a UTF-8 encoded constant error string for "errNum".
 * Returns NULL if either initialization of the error tables
 * or formatting fails due to insufficient memory.
 *
 * The format argument indicates whether extra error information
 * is desired. This is useful when localizations are not yet
 * available and the mapping would return nothing for a locale. 
 *
 * Specify formatSimple to get just the error string as mapped.
 * Specify formatIncludeErrorCode to format the error code 
 * numeric value plus a bracketed stringized error name
 * preappended to the mapped error string. 
 * 
 * Additional formatting options may be added in teh future
 *
 * This string must not be modified by the application, but may be modified by
 * a subsequent call to NSS_Perror() or NSS_Strerror().
 */
char *
NSS_Strerror(PRErrorCode errNum, ReportFormatType format)
{
    PRUint32 count;
    char *errname = (char *) PR_ErrorToName(errNum);
    char *errstr = nss_Strerror(errNum);

    if (!errstr) return NULL;

    if (format == formatSimple) return errstr;

    count = PR_snprintf(ebuf, EBUFF_SIZE, "[%d %s] %s",
	errNum, errname, errstr);

    PR_ASSERT(count != -1);

    return ebuf;
}

/* NSS_StrerrorTS is a thread safe version of NSS_Strerror.
 * It formats output into a buffer allocated at run time.
 * The buffer is allocated with PR_smprintf thus the string
 * returned should be freed with PR_smprintf_free.
 */
char *
NSS_StrerrorTS(PRErrorCode errNum, ReportFormatType format)
{
    char *errstr = NSS_Strerror(errNum, format);

    return PR_smprintf("[%d %s] %s",
	errNum, PR_ErrorToName(errNum), errstr ? errstr : "");
}

/* Prints an error message on the standard error output, describing the last
 * error encountered during a call to an NSS library function.
 *
 * A language-dependent error message is written and formatted to
 * the standard error stream as follows:
 *
 *  If s is not a NULL or empty, prints the string pointed to by s followed
 *  by a colon and a space and then the error message string followed by a
 *  newline.
 *
 * NSS_Perror is partially modeled after the posix function perror.
 */
void
NSS_Perror(const char *s, ReportFormatType format)
{
    PRErrorCode err;
    char *errString;

    if (!s || PORT_Strlen(s) == 0) {
	return;
    }

    err = PORT_GetError();
    errString = NSS_Strerror(err, format);

    fprintf(stderr, "%s: ", s);

    if (errString != NULL && PORT_Strlen(errString) > 0) {
	fprintf(stderr, "%s\n", errString);
    } else {
	fprintf(stderr, "Unknown error: %d\n", (int)err);
    }
}
