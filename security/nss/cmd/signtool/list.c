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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
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

#include "signtool.h"
#include "pk11func.h"
#include "certdb.h"

static int	num_trav_certs = 0;
static SECStatus cert_trav_callback(CERTCertificate *cert, SECItem *k,
			void *data);

/*********************************************************************
 *
 * L i s t C e r t s
 */
int
ListCerts(char *key, int list_certs)
{
    int	failed = 0;
    SECStatus rv;
    char	*ugly_list;
    CERTCertDBHandle * db;

    CERTCertificate * cert;
    CERTVerifyLog errlog;

    errlog.arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( errlog.arena == NULL) {
	out_of_memory();
    }
    errlog.head = NULL;
    errlog.tail = NULL;
    errlog.count = 0;

    ugly_list = PORT_ZAlloc (16);

    if (ugly_list == NULL) {
	out_of_memory();
    }

    *ugly_list = 0;

    db = CERT_GetDefaultCertDB();

    if (list_certs == 2) {
	PR_fprintf(outputFD, "\nS Certificates\n");
	PR_fprintf(outputFD, "- ------------\n");
    } else {
	PR_fprintf(outputFD, "\nObject signing certificates\n");
	PR_fprintf(outputFD, "---------------------------------------\n");
    }

    num_trav_certs = 0;

    /* Traverse non-internal DBs */
    rv = PK11_TraverseSlotCerts(cert_trav_callback, (void * )&list_certs,
         		NULL /*wincx*/);

    if (rv) {
	PR_fprintf(outputFD, "**Traverse of non-internal DBs failed**\n");
	return - 1;
    }

    if (num_trav_certs == 0) {
	PR_fprintf(outputFD,
	    "You don't appear to have any object signing certificates.\n");
    }

    if (list_certs == 2) {
	PR_fprintf(outputFD, "- ------------\n");
    } else {
	PR_fprintf(outputFD, "---------------------------------------\n");
    }

    if (list_certs == 1) {
	PR_fprintf(outputFD,
	    "For a list including CA's, use \"%s -L\"\n", PROGRAM_NAME);
    }

    if (list_certs == 2) {
	PR_fprintf(outputFD,
	    "Certificates that can be used to sign objects have *'s to "
	    "their left.\n");
    }

    if (key) {
	/* Do an analysis of the given cert */

	cert = PK11_FindCertFromNickname(key, NULL /*wincx*/);

	if (cert) {
	    PR_fprintf(outputFD,
	        "\nThe certificate with nickname \"%s\" was found:\n",
	         			 cert->nickname);
	    PR_fprintf(outputFD, "\tsubject name: %s\n", cert->subjectName);
	    PR_fprintf(outputFD, "\tissuer name: %s\n", cert->issuerName);

	    PR_fprintf(outputFD, "\n");

	    rv = CERT_CertTimesValid (cert);
	    if (rv != SECSuccess) {
		PR_fprintf(outputFD, "**This certificate is expired**\n");
	    } else {
		PR_fprintf(outputFD, "This certificate is not expired.\n");
	    }

	    rv = CERT_VerifyCert (db, cert, PR_TRUE,
	        certUsageObjectSigner, PR_Now(), NULL, &errlog);

	    if (rv != SECSuccess) {
		failed = 1;
		if (errlog.count > 0) {
		    PR_fprintf(outputFD,
		        "**Certificate validation failed for the "
		        "following reason(s):**\n");
		} else {
		    PR_fprintf(outputFD, "**Certificate validation failed**");
		}
	    } else {
		PR_fprintf(outputFD, "This certificate is valid.\n");
	    }
	    displayVerifyLog(&errlog);


	} else {
	    failed = 1;
	    PR_fprintf(outputFD,
	        "The certificate with nickname \"%s\" was NOT FOUND\n", key);
	}
    }

    if (errlog.arena != NULL) {
	PORT_FreeArena(errlog.arena, PR_FALSE);
    }

    if (failed) {
	return - 1;
    }
    return 0;
}


/********************************************************************
 *
 * c e r t _ t r a v _ c a l l b a c k
 */
static SECStatus
cert_trav_callback(CERTCertificate *cert, SECItem *k, void *data)
{
    int	isSigningCert;
    int	list_certs = 1;

    char	*name, *issuerCN, *expires;
    CERTCertificate * issuerCert = NULL;

    if (data) {
	list_certs = *((int * )data);
    }

    if (cert->nickname) {
	name = cert->nickname;

	isSigningCert = cert->nsCertType & NS_CERT_TYPE_OBJECT_SIGNING;
	issuerCert = CERT_FindCertIssuer (cert, PR_Now(), certUsageObjectSigner);
	issuerCN = CERT_GetCommonName (&cert->issuer);

	if (!isSigningCert && list_certs == 1)
	    return (SECSuccess);

	/* Add this name or email to list */

	if (name) {
	    int	rv;

	    num_trav_certs++;
	    if (list_certs == 2) {
		PR_fprintf(outputFD, "%s ", isSigningCert ? "*" : " ");
	    }
	    PR_fprintf(outputFD, "%s\n", name);

	    if (list_certs == 1) {
		if (issuerCert == NULL) {
		    PR_fprintf(outputFD,
		        "\t++ Error ++ Unable to find issuer certificate\n");
		    return SECSuccess; 
			   /*function was a success even if cert is bogus*/
		}
		if (issuerCN == NULL)
		    PR_fprintf(outputFD, "    Issued by: %s\n",
		         issuerCert->nickname);
		else
		    PR_fprintf(outputFD,
		        "    Issued by: %s (%s)\n", issuerCert->nickname,
		         issuerCN);

		expires = DER_TimeChoiceDayToAscii(&cert->validity.notAfter);

		if (expires)
		    PR_fprintf(outputFD, "    Expires: %s\n", expires);

		rv = CERT_CertTimesValid (cert);

		if (rv != SECSuccess)
		    PR_fprintf(outputFD, 
			"    ++ Error ++ THIS CERTIFICATE IS EXPIRED\n");

		if (rv == SECSuccess) {
		    rv = CERT_VerifyCertNow (cert->dbhandle, cert,
		        PR_TRUE, certUsageObjectSigner, NULL);

		    if (rv != SECSuccess) {
			rv = PORT_GetError();
			PR_fprintf(outputFD,
			"    ++ Error ++ THIS CERTIFICATE IS NOT VALID (%s)\n",
			     				secErrorString(rv));            
		    }
		}

		expires = DER_TimeChoiceDayToAscii(&issuerCert->validity.notAfter);
		if (expires == NULL) 
		    expires = "(unknown)";

		rv = CERT_CertTimesValid (issuerCert);

		if (rv != SECSuccess)
		    PR_fprintf(outputFD,
		        "    ++ Error ++ ISSUER CERT \"%s\" EXPIRED ON %s\n",
			issuerCert->nickname, expires);

		if (rv == SECSuccess) {
		    rv = CERT_VerifyCertNow (issuerCert->dbhandle, issuerCert, 
		        PR_TRUE, certUsageVerifyCA, NULL);
		    if (rv != SECSuccess) {
			rv = PORT_GetError();
			PR_fprintf(outputFD,
			"    ++ Error ++ ISSUER CERT \"%s\" IS NOT VALID (%s)\n",
			     issuerCert->nickname, secErrorString(rv));
		    }
		}
	    }
	}
    }

    return (SECSuccess);
}


