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
** certutil.c
**
** utility for managing certificates and the cert database
**
*/
/* test only */

#include "nspr.h"
#include "plgetopt.h"
#include "secutil.h"
#include "cert.h"
#include "certdb.h"
#include "nss.h"

#define SEC_CERT_DB_EXISTS 0
#define SEC_CREATE_CERT_DB 1

static char *progName;

static CERTSignedCrl *FindCRL
   (CERTCertDBHandle *certHandle, char *name, int type)
{
    CERTSignedCrl *crl = NULL;    
    CERTCertificate *cert = NULL;


    cert = CERT_FindCertByNickname(certHandle, name);
    if (!cert) {
	SECU_PrintError(progName, "could not find certificate named %s", name);
	return ((CERTSignedCrl *)NULL);
    }
	
    crl = SEC_FindCrlByKey(certHandle, &cert->derSubject, type);
    if (crl ==NULL) 
	SECU_PrintError
		(progName, "could not find %s's CRL", name);
    CERT_DestroyCertificate (cert);
    return (crl);
}

static void DisplayCRL (CERTCertDBHandle *certHandle, char *nickName, int crlType)
{
    CERTCertificate *cert = NULL;
    CERTSignedCrl *crl = NULL;

    crl = FindCRL (certHandle, nickName, crlType);
	
    if (crl) {
	SECU_PrintCRLInfo (stdout, &crl->crl, "CRL Info:\n", 0);
	CERT_DestroyCrl (crl);
    }
}

static void ListCRLNames (CERTCertDBHandle *certHandle, int crlType)
{
    CERTCrlHeadNode *crlList = NULL;
    CERTCrlNode *crlNode = NULL;
    CERTName *name = NULL;
    PRArenaPool *arena = NULL;
    SECStatus rv;
    void *mark;

    do {
	arena = PORT_NewArena (SEC_ASN1_DEFAULT_ARENA_SIZE);
	if (arena == NULL) {
	    fprintf(stderr, "%s: fail to allocate memory\n", progName);
	    break;
	}
	
	name = PORT_ArenaZAlloc (arena, sizeof(*name));
	if (name == NULL) {
	    fprintf(stderr, "%s: fail to allocate memory\n", progName);
	    break;
	}
	name->arena = arena;
	    
	rv = SEC_LookupCrls (certHandle, &crlList, crlType);
	if (rv != SECSuccess) {
	    fprintf(stderr, "%s: fail to look up CRLs (%s)\n", progName,
	    SECU_Strerror(PORT_GetError()));
	    break;
	}
	
	/* just in case */
	if (!crlList)
	    break;

	crlNode  = crlList->first;

        fprintf (stdout, "\n");
	fprintf (stdout, "\n%-40s %-5s\n\n", "CRL names", "CRL Type");
	while (crlNode) {
	    mark = PORT_ArenaMark (arena); 	    
	    rv = SEC_ASN1DecodeItem
		   (arena, name, CERT_NameTemplate, &(crlNode->crl->crl.derName));
	    if (!name){
		fprintf(stderr, "%s: fail to get the CRL issuer name\n", progName,
		SECU_Strerror(PORT_GetError()));
		break;
	    }
		
	    fprintf (stdout, "\n%-40s %-5s\n", CERT_NameToAscii(name), "CRL");
	    crlNode = crlNode->next;
	    PORT_ArenaRelease (arena, mark);
	} 
	
    } while (0);
    if (crlList)
	PORT_FreeArena (crlList->arena, PR_FALSE);
    PORT_FreeArena (arena, PR_FALSE);
}

static void ListCRL (CERTCertDBHandle *certHandle, char *nickName, int crlType)
{
    if (nickName == NULL)
	ListCRLNames (certHandle, crlType);
    else
	DisplayCRL (certHandle, nickName, crlType);
}



static SECStatus DeleteCRL (CERTCertDBHandle *certHandle, char *name, int type)
{
    CERTSignedCrl *crl = NULL;    
    SECStatus rv = SECFailure;

    crl = FindCRL (certHandle, name, type);
    if (!crl) {
	SECU_PrintError
		(progName, "could not find the issuer %s's CRL", name);
	return SECFailure;
    }
    rv = SEC_DeletePermCRL (crl);
    if (rv != SECSuccess) {
	SECU_PrintError
		(progName, "fail to delete the issuer %s's CRL from the perm dbase (reason: %s)",
		 name, SECU_Strerror(PORT_GetError()));
	return SECFailure;
    }

    rv = SEC_DeleteTempCrl (crl);
    if (rv != SECSuccess) {
	SECU_PrintError
		(progName, "fail to delete the issuer %s's CRL from the temp dbase (reason: %s)",
		 name, SECU_Strerror(PORT_GetError()));
	return SECFailure;
    }
    return (rv);
}

SECStatus ImportCRL (CERTCertDBHandle *certHandle, char *url, int type, 
                     PRFileDesc *inFile)
{
    CERTCertificate *cert = NULL;
    CERTSignedCrl *crl = NULL;
    SECItem crlDER;
    int rv;

    crlDER.data = NULL;


    /* Read in the entire file specified with the -f argument */
	rv = SECU_ReadDERFromFile(&crlDER, inFile, PR_FALSE);
    if (rv != SECSuccess) {
	SECU_PrintError(progName, "unable to read input file");
	return (SECFailure);
    }
    
    crl = CERT_ImportCRL (certHandle, &crlDER, url, type, NULL);
    if (!crl) {
	const char *errString;

	errString = SECU_Strerror(PORT_GetError());
	if (PORT_Strlen (errString) == 0)
	    SECU_PrintError
		    (progName, "CRL is not import (error: input CRL is not up to date.)");
	else    
	    SECU_PrintError
		    (progName, "unable to import CRL");
    }
    PORT_Free (crlDER.data);
    CERT_DestroyCrl (crl);
    return (rv);
}
	    

static void Usage(char *progName)
{
    fprintf(stderr,
	    "Usage:  %s -L [-n nickname[ [-d keydir] [-t crlType]\n"
	    "        %s -D -n nickname [-d keydir]\n"
	    "        %s -I -i crl -t crlType [-u url] [-d keydir]\n",
	    progName, progName, progName);

    fprintf (stderr, "%-15s List CRL\n", "-L");
    fprintf(stderr, "%-20s Specify the nickname of the CA certificate\n",
	    "-n nickname");
    fprintf(stderr, "%-20s Key database directory (default is ~/.netscape)\n",
	    "-d keydir");
   
    fprintf (stderr, "%-15s Delete a CRL from the cert dbase\n", "-D");    
    fprintf(stderr, "%-20s Specify the nickname for the CA certificate\n",
	    "-n nickname");
    fprintf(stderr, "%-20s Specify the crl type.\n", "-t crlType");

    fprintf (stderr, "%-15s Import a CRL to the cert dbase\n", "-I");    
    fprintf(stderr, "%-20s Specify the file which contains the CRL to import\n",
	    "-i crl");
    fprintf(stderr, "%-20s Specify the url.\n", "-u url");
    fprintf(stderr, "%-20s Specify the crl type.\n", "-t crlType");

    fprintf(stderr, "%-20s CRL Types (default is SEC_CRL_TYPE):\n", " ");
    fprintf(stderr, "%-20s \t 0 - SEC_KRL_TYPE\n", " ");
    fprintf(stderr, "%-20s \t 1 - SEC_CRL_TYPE\n", " ");        

    exit(-1);
}

int main(int argc, char **argv)
{
    SECItem privKeyDER;
    CERTCertDBHandle *certHandle;
    FILE *certFile;
    PRFileDesc *inFile;
    int listCRL;
    int importCRL;
    int opt;
    int deleteCRL;
    int rv;
    char *nickName;
    char *progName;
    char *url;
    int crlType;
    PLOptState *optstate;
    PLOptStatus status;
    SECStatus secstatus;

    progName = strrchr(argv[0], '/');
    progName = progName ? progName+1 : argv[0];

    rv = 0;
    deleteCRL = importCRL = listCRL = 0;
    certFile = NULL;
    inFile = NULL;
    nickName = url = NULL;
    privKeyDER.data = NULL;
    certHandle = NULL;
    crlType = SEC_CRL_TYPE;
    /*
     * Parse command line arguments
     */
    optstate = PL_CreateOptState(argc, argv, "IALd:i:Dn:Ct:u:");
    while ((status = PL_GetNextOpt(optstate)) == PL_OPT_OK) {
	switch (optstate->option) {
	  case '?':
	    Usage(progName);
	    break;

	  case 'C':
	      listCRL = 1;
	      break;

	  case 'D':
	      deleteCRL = 1;
	      break;

	  case 'I':
	      importCRL = 1;
	      break;
	           
	  case 'L':
	      listCRL = 1;
	      break;
	           
	  case 'd':
	    SECU_ConfigDirectory(optstate->value);
	    break;

	  case 'i':
	    inFile = PR_Open(optstate->value, PR_RDONLY, 0);
	    if (!inFile) {
		fprintf(stderr, "%s: unable to open \"%s\" for reading\n",
			progName, optstate->value);
		return -1;
	    }
	    break;
	    
	  case 'n':
	    nickName = strdup(optstate->value);
	    break;
	    
	  case 'u':
	    url = strdup(optstate->value);
	    break;

	  case 't': {
	    char *type;
	    
	    type = strdup(optstate->value);
	    crlType = atoi (type);
	    if (crlType != SEC_CRL_TYPE && crlType != SEC_KRL_TYPE) {
		fprintf(stderr, "%s: invalid crl type\n", progName);
		return -1;
	    }
	    break;
          }
	}
    }

    if (deleteCRL && !nickName) Usage (progName);
    if (!(listCRL || deleteCRL || importCRL)) Usage (progName);
    if (importCRL && !inFile) Usage (progName);
    
    PR_Init( PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);
    secstatus = NSS_InitReadWrite(SECU_ConfigDirectory(NULL));
    if (secstatus != SECSuccess) {
	SECU_PrintPRandOSError(progName);
	return -1;
    }

    certHandle = CERT_GetDefaultCertDB();
    if (certHandle == NULL) {
	SECU_PrintError(progName, "unable to open the cert db");	    	
	return (-1);
    }

    /* Read in the private key info */
    if (deleteCRL) 
	DeleteCRL (certHandle, nickName, crlType);
    else if (listCRL)
	ListCRL (certHandle, nickName, crlType);
    else if (importCRL) 
	rv = ImportCRL (certHandle, url, crlType, inFile);
    
    return (rv);
}
