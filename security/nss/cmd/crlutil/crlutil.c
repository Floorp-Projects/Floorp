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
#include "pk11func.h"

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
	
    crl = SEC_FindCrlByName(certHandle, &cert->derSubject, type);
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
	SEC_DestroyCrl (crl);
    }
}

static void ListCRLNames (CERTCertDBHandle *certHandle, int crlType, PRBool deletecrls)
{
    CERTCrlHeadNode *crlList = NULL;
    CERTCrlNode *crlNode = NULL;
    CERTName *name = NULL;
    PRArenaPool *arena = NULL;
    SECStatus rv;

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
	   char* asciiname = NULL;
	   name = &crlNode->crl->crl.name;
	   if (!name){
		fprintf(stderr, "%s: fail to get the CRL issuer name (%s)\n", progName,
		SECU_Strerror(PORT_GetError()));
		break;
	    }

	    asciiname = CERT_NameToAscii(name);
	    fprintf (stdout, "\n%-40s %-5s\n", asciiname, "CRL");
	    if (asciiname) {
		PORT_Free(asciiname);
	    }
            if ( PR_TRUE == deletecrls) {
                CERTSignedCrl* acrl = NULL;
                SECItem* issuer = &crlNode->crl->crl.derName;
                acrl = SEC_FindCrlByName(certHandle, issuer, crlType);
                if (acrl)
                {
                    SEC_DeletePermCRL(acrl);
                    SEC_DestroyCrl(acrl);
                }
            }
            crlNode = crlNode->next;
	} 
	
    } while (0);
    if (crlList)
	PORT_FreeArena (crlList->arena, PR_FALSE);
    PORT_FreeArena (arena, PR_FALSE);
}

static void ListCRL (CERTCertDBHandle *certHandle, char *nickName, int crlType)
{
    if (nickName == NULL)
	ListCRLNames (certHandle, crlType, PR_FALSE);
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
    SEC_DestroyCrl(crl);
    if (rv != SECSuccess) {
	SECU_PrintError
		(progName, "fail to delete the issuer %s's CRL from the perm database (reason: %s)",
		 name, SECU_Strerror(PORT_GetError()));
	return SECFailure;
    }
    return (rv);
}

SECStatus ImportCRL (CERTCertDBHandle *certHandle, char *url, int type, 
                     PRFileDesc *inFile, PRInt32 importOptions, PRInt32 decodeOptions)
{
    CERTCertificate *cert = NULL;
    CERTSignedCrl *crl = NULL;
    SECItem crlDER;
    PK11SlotInfo* slot = NULL;
    int rv;
#if defined(DEBUG_jpierre)
    PRIntervalTime starttime, endtime, elapsed;
    PRUint32 mins, secs, msecs;
#endif

    crlDER.data = NULL;


    /* Read in the entire file specified with the -f argument */
    rv = SECU_ReadDERFromFile(&crlDER, inFile, PR_FALSE);
    if (rv != SECSuccess) {
	SECU_PrintError(progName, "unable to read input file");
	return (SECFailure);
    }

    decodeOptions |= CRL_DECODE_DONT_COPY_DER;

    slot = PK11_GetInternalKeySlot();
 
#if defined(DEBUG_jpierre)
    starttime = PR_IntervalNow();
#endif
    crl = PK11_ImportCRL(slot, &crlDER, url, type,
          NULL, importOptions, NULL, decodeOptions);
#if defined(DEBUG_jpierre)
    endtime = PR_IntervalNow();
    elapsed = endtime - starttime;
    mins = PR_IntervalToSeconds(elapsed) / 60;
    secs = PR_IntervalToSeconds(elapsed) % 60;
    msecs = PR_IntervalToMilliseconds(elapsed) % 1000;
    printf("Elapsed : %2d:%2d.%3d\n", mins, secs, msecs);
#endif
    if (!crl) {
	const char *errString;

	rv = SECFailure;
	errString = SECU_Strerror(PORT_GetError());
	if ( errString && PORT_Strlen (errString) == 0)
	    SECU_PrintError (progName, 
	        "CRL is not imported (error: input CRL is not up to date.)");
	else    
	    SECU_PrintError (progName, "unable to import CRL");
    } else {
	SEC_DestroyCrl (crl);
    }
    if (slot) {
        PK11_FreeSlot(slot);
    }
    return (rv);
}
	    

static void Usage(char *progName)
{
    fprintf(stderr,
	    "Usage:  %s -L [-n nickname] [-d keydir] [-P dbprefix] [-t crlType]\n"
	    "        %s -D -n nickname [-d keydir] [-P dbprefix]\n"
	    "        %s -I -i crl -t crlType [-u url] [-d keydir] [-P dbprefix] [-B]\n"
	    "        %s -E -t crlType [-d keydir] [-P dbprefix]\n"
	    "        %s -T\n", progName, progName, progName, progName, progName);

    fprintf (stderr, "%-15s List CRL\n", "-L");
    fprintf(stderr, "%-20s Specify the nickname of the CA certificate\n",
	    "-n nickname");
    fprintf(stderr, "%-20s Key database directory (default is ~/.netscape)\n",
	    "-d keydir");
    fprintf(stderr, "%-20s Cert & Key database prefix (default is \"\")\n",
	    "-P dbprefix");
   
    fprintf (stderr, "%-15s Delete a CRL from the cert database\n", "-D");    
    fprintf(stderr, "%-20s Specify the nickname for the CA certificate\n",
	    "-n nickname");
    fprintf(stderr, "%-20s Specify the crl type.\n", "-t crlType");
    fprintf(stderr, "%-20s Key database directory (default is ~/.netscape)\n",
	    "-d keydir");
    fprintf(stderr, "%-20s Cert & Key database prefix (default is \"\")\n",
	    "-P dbprefix");

    fprintf (stderr, "%-15s Erase all CRLs of specified type from hte cert database\n", "-E");
    fprintf(stderr, "%-20s Specify the crl type.\n", "-t crlType");
    fprintf(stderr, "%-20s Key database directory (default is ~/.netscape)\n",
	    "-d keydir");
    fprintf(stderr, "%-20s Cert & Key database prefix (default is \"\")\n",
	    "-P dbprefix");

    fprintf (stderr, "%-15s Import a CRL to the cert database\n", "-I");    
    fprintf(stderr, "%-20s Specify the file which contains the CRL to import\n",
	    "-i crl");
    fprintf(stderr, "%-20s Specify the url.\n", "-u url");
    fprintf(stderr, "%-20s Specify the crl type.\n", "-t crlType");
    fprintf(stderr, "%-20s Key database directory (default is ~/.netscape)\n",
	    "-d keydir");
    fprintf(stderr, "%-20s Cert & Key database prefix (default is \"\")\n",
	    "-P dbprefix");
#ifdef DEBUG
    fprintf (stderr, "%-15s Test . Only for debugging purposes. See source code\n", "-T");
#endif
    fprintf(stderr, "%-20s CRL Types (default is SEC_CRL_TYPE):\n", " ");
    fprintf(stderr, "%-20s \t 0 - SEC_KRL_TYPE\n", " ");
    fprintf(stderr, "%-20s \t 1 - SEC_CRL_TYPE\n", " ");        
    fprintf(stderr, "\n%-20s Bypass CA certificate checks.\n", "-B");
    fprintf(stderr, "\n%-20s Partial decode for faster operation.\n", "-p");
    fprintf(stderr, "%-20s Repeat the operation.\n", "-r <iterations>");

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
    int deleteCRL;
    int rv;
    char *nickName;
    char *url;
    char *dbPrefix = "";
    int crlType;
    PLOptState *optstate;
    PLOptStatus status;
    SECStatus secstatus;
    PRBool bypassChecks = PR_FALSE;
    PRInt32 decodeOptions = CRL_DECODE_DEFAULT_OPTIONS;
    PRInt32 importOptions = CRL_IMPORT_DEFAULT_OPTIONS;
    PRBool test = PR_FALSE;
    PRBool erase = PR_FALSE;
    PRInt32 i = 0;
    PRInt32 iterations = 1;

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
    optstate = PL_CreateOptState(argc, argv, "BCDILP:d:i:n:pt:u:TEr:");
    while ((status = PL_GetNextOpt(optstate)) == PL_OPT_OK) {
	switch (optstate->option) {
	  case '?':
	    Usage(progName);
	    break;

          case 'T':
            test = PR_TRUE;
            break;

          case 'E':
            erase = PR_TRUE;
            break;

	  case 'B':
            importOptions |= CRL_IMPORT_BYPASS_CHECKS;
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

	  case 'P':
	    dbPrefix = strdup(optstate->value);
	    break;
	           
	  case 'd':
	    SECU_ConfigDirectory(optstate->value);
	    break;

	  case 'i':
	    inFile = PR_Open(optstate->value, PR_RDONLY, 0);
	    if (!inFile) {
		fprintf(stderr, "%s: unable to open \"%s\" for reading\n",
			progName, optstate->value);
		PL_DestroyOptState(optstate);
		return -1;
	    }
	    break;
	    
	  case 'n':
	    nickName = strdup(optstate->value);
	    break;

	  case 'p':
	    decodeOptions |= CRL_DECODE_SKIP_ENTRIES;
	    break;

	  case 'r': {
	    const char* str = optstate->value;
	    if (str && atoi(str)>0)
		iterations = atoi(str);
	    }
	    break;
	    
	  case 't': {
	    char *type;
	    
	    type = strdup(optstate->value);
	    crlType = atoi (type);
	    if (crlType != SEC_CRL_TYPE && crlType != SEC_KRL_TYPE) {
		fprintf(stderr, "%s: invalid crl type\n", progName);
		PL_DestroyOptState(optstate);
		return -1;
	    }
	    break;

	  case 'u':
	    url = strdup(optstate->value);
	    break;
          }
	}
    }
    PL_DestroyOptState(optstate);

    if (deleteCRL && !nickName) Usage (progName);
    if (!(listCRL || deleteCRL || importCRL || test || erase)) Usage (progName);
    if (importCRL && !inFile) Usage (progName);
    
    PR_Init( PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);
    secstatus = NSS_Initialize(SECU_ConfigDirectory(NULL), dbPrefix, dbPrefix,
			       "secmod.db", 0);
    if (secstatus != SECSuccess) {
	SECU_PrintPRandOSError(progName);
	return -1;
    }
    SECU_RegisterDynamicOids();

    certHandle = CERT_GetDefaultCertDB();
    if (certHandle == NULL) {
	SECU_PrintError(progName, "unable to open the cert db");	    	
	/*ignoring return value of NSS_Shutdown() as code returns -1*/
	(void) NSS_Shutdown();
	return (-1);
    }

    for (i=0; i<iterations; i++) {
	/* Read in the private key info */
	if (deleteCRL) 
	    DeleteCRL (certHandle, nickName, crlType);
	else if (listCRL) {
	    ListCRL (certHandle, nickName, crlType);
	}
	else if (importCRL) {
	    rv = ImportCRL (certHandle, url, crlType, inFile, importOptions,
			    decodeOptions);
	}
	else if (erase) {
	    /* list and delete all CRLs */
	    ListCRLNames (certHandle, crlType, PR_TRUE);
	}
#ifdef DEBUG
	else if (test) {
	    /* list and delete all CRLs */
	    ListCRLNames (certHandle, crlType, PR_TRUE);
	    /* list CRLs */
	    ListCRLNames (certHandle, crlType, PR_FALSE);
	    /* import CRL as a blob */
	    rv = ImportCRL (certHandle, url, crlType, inFile, importOptions,
			    decodeOptions);
	    /* list CRLs */
	    ListCRLNames (certHandle, crlType, PR_FALSE);
	}
#endif    
    }
    if (NSS_Shutdown() != SECSuccess) {
        rv = SECFailure;
    }

    return (rv != SECSuccess);
}
