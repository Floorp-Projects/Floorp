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
** secutil.c - various functions used by security stuff
**
*/

#include "prtypes.h"
#include "prtime.h"
#include "prlong.h"
#include "prerror.h"
#include "prprf.h"
#include "plgetopt.h"

#include "secutil.h"
#include "secpkcs7.h"
#include "secrng.h"
#include <sys/stat.h>
#include <stdarg.h>

#ifdef XP_UNIX
#include <unistd.h>
#endif

/* for SEC_TraverseNames */
#include "cert.h"
#include "certt.h"
#include "certdb.h"

typedef struct {
    char *		name;
    CERTCertTrust	trust;
} certNameAndTrustEntry;

typedef struct {
    int numCerts;
    certNameAndTrustEntry *nameAndTrustEntries;
} certNameAndTrustList;

SECStatus
sec_CountCerts(CERTCertificate *cert, SECItem *unknown, void *arg)
{
    (*(int*)arg)++;
    return SECSuccess;
}

SECStatus
sec_CollectCertNamesAndTrust(CERTCertificate *cert, SECItem *unknown, void *arg)
{
    certNameAndTrustList *pCertNames = (certNameAndTrustList*)arg;
    char *name;
    int i;

    i = pCertNames->numCerts;
    name = cert->nickname ? cert->nickname : cert->emailAddr;

    if (name)
	pCertNames->nameAndTrustEntries[i].name = PORT_Strdup(name);
    else
	pCertNames->nameAndTrustEntries[i].name = PORT_Strdup("<unknown>");

    PORT_Memcpy(&pCertNames->nameAndTrustEntries[i].trust, cert->trust, sizeof(*cert->trust));

    pCertNames->numCerts++;

    return SECSuccess;
}


static int
sec_name_and_trust_compare_by_name(const void *p1, const void *p2)
{
    certNameAndTrustEntry *e1 = (certNameAndTrustEntry *)p1;
    certNameAndTrustEntry *e2 = (certNameAndTrustEntry *)p2;
    return PORT_Strcmp(e1->name, e2->name);
}

static int
sec_combine_trust_flags(CERTCertTrust *trust)
{
    if (trust == NULL)
	return 0;
    return trust->sslFlags | trust->emailFlags | trust->objectSigningFlags;
}

static int
sec_name_and_trust_compare_by_trust(const void *p1, const void *p2)
{
    certNameAndTrustEntry *e1 = (certNameAndTrustEntry *)p1;
    certNameAndTrustEntry *e2 = (certNameAndTrustEntry *)p2;
    int e1_is_ca, e2_is_ca;
    int e1_is_user, e2_is_user;
    int rv;

    e1_is_ca = (sec_combine_trust_flags(&e1->trust) & CERTDB_VALID_CA) != 0;
    e2_is_ca = (sec_combine_trust_flags(&e2->trust) & CERTDB_VALID_CA) != 0;
    e1_is_user = (sec_combine_trust_flags(&e1->trust) & CERTDB_USER) != 0;
    e2_is_user = (sec_combine_trust_flags(&e2->trust) & CERTDB_USER) != 0;

    /* first, sort by user status, then CA status, */
    /*  then by actual comparison of CA flags, then by name */
    if ((rv = (e2_is_user - e1_is_user)) == 0 && (rv = (e1_is_ca - e2_is_ca)) == 0)
	if (e1_is_ca || (rv = memcmp(&e1->trust, &e2->trust, sizeof(CERTCertTrust))) == 0)
	    return PORT_Strcmp(e1->name, e2->name);
	else
	    return rv;
    else
	return rv;
}

SECStatus
SECU_PrintCertificateNames(CERTCertDBHandle *handle, PRFileDesc *out, 
                           PRBool sortByName, PRBool sortByTrust)
{
    certNameAndTrustList certNames = { 0, NULL };
    int numCerts, i;
    SECStatus rv;
    int (*comparefn)(const void *, const void *);
    char trusts[30];

    numCerts = 0;

    rv = SEC_TraversePermCerts(handle, sec_CountCerts, &numCerts);
    if (rv != SECSuccess)
	return SECFailure;

    certNames.nameAndTrustEntries = 
		(certNameAndTrustEntry *)PORT_Alloc(numCerts * sizeof(certNameAndTrustEntry));
    if (certNames.nameAndTrustEntries == NULL)
	return SECFailure;

    rv = SEC_TraversePermCerts(handle, sec_CollectCertNamesAndTrust, &certNames);
    if (rv != SECSuccess)
	return SECFailure;

    if (sortByName)
	comparefn = sec_name_and_trust_compare_by_name;
    else if (sortByTrust)
	comparefn = sec_name_and_trust_compare_by_trust;
    else
	comparefn = NULL;

    if (comparefn)
	qsort(certNames.nameAndTrustEntries, certNames.numCerts, 
			    sizeof(certNameAndTrustEntry), comparefn);

    PR_fprintf(out, "\n%-60s %-5s\n\n", "Certificate Name", "Trust Attributes");
    for (i = 0; i < certNames.numCerts; i++) {
	PORT_Memset (trusts, 0, sizeof(trusts));
	printflags(trusts, certNames.nameAndTrustEntries[i].trust.sslFlags);
	PORT_Strcat(trusts, ",");
	printflags(trusts, certNames.nameAndTrustEntries[i].trust.emailFlags);
	PORT_Strcat(trusts, ",");
	printflags(trusts, certNames.nameAndTrustEntries[i].trust.objectSigningFlags);
	PR_fprintf(out, "%-60s %-5s\n", 
	           certNames.nameAndTrustEntries[i].name, trusts);
    }
    PR_fprintf(out, "\n");
    PR_fprintf(out, "p    Valid peer\n");
    PR_fprintf(out, "P    Trusted peer (implies p)\n");
    PR_fprintf(out, "c    Valid CA\n");
    PR_fprintf(out, "T    Trusted CA to issue client certs (implies c)\n");
    PR_fprintf(out, "C    Trusted CA to certs(only server certs for ssl) (implies c)\n");
    PR_fprintf(out, "u    User cert\n");
    PR_fprintf(out, "w    Send warning\n");

    for (i = 0; i < certNames.numCerts; i++)
	PORT_Free(certNames.nameAndTrustEntries[i].name);
    PORT_Free(certNames.nameAndTrustEntries);

    return rv;
}
