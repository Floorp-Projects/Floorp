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
#include <stdio.h>

#include "prio.h"
#include "seccomon.h"
#include "swforti.h"
#include "cert.h"
#include "pk11func.h"
#include "nss.h"
#include "secutil.h"

#define CERTDB_VALID_CA        (1<<3)
#define CERTDB_TRUSTED_CA      (1<<4) /* trusted for issuing server certs */

void secmod_GetInternalModule(SECMODModule *module);
void sec_SetCheckKRLState(int i);

#define STEP 16
void
printItem(SECItem *key) {
 int i;
 unsigned char *block;
 int len;
 for (block=key->data,len=key->len; len > 0; len -= STEP,block += STEP) {
     for(i=0; i < STEP && i < len; i++) printf(" %02x ",block[i]);
     printf("\n");
 }
 printf("\n");
}

void
dump(unsigned char *block, int len) {
 int i;
 for (; len > 0; len -= STEP,block += STEP) {
     for(i=0; i < STEP && i < len; i++) printf(" %02x ",block[i]);
     printf("\n");
 }
 printf("\n");
}


/*
 * We need to move this to security/cmd .. so we can use the password
 * prompting infrastructure.
 */
char *GetUserInput(char * prompt) 
{
    char phrase[200];

	    fprintf(stderr, "%s", prompt);
            fflush (stderr);

	fgets ((char*) phrase, sizeof(phrase), stdin);

	/* stomp on newline */
	phrase[PORT_Strlen((char*)phrase)-1] = 0;

	/* Validate password */
	return (char*) PORT_Strdup((char*)phrase);
}

void ClearPass(char *pass) {
    PORT_Memset(pass,0,strlen(pass));
    PORT_Free(pass);
}

char *
formatDERIssuer(FORTSWFile *file,SECItem *derIssuer)
{
    CERTName name;
    SECStatus rv;

    PORT_Memset(&name,0,sizeof(name));;
    rv = SEC_ASN1DecodeItem(file->arena,&name,CERT_NameTemplate,derIssuer);
    if (rv != SECSuccess) {
	return NULL;
    }
    return CERT_NameToAscii(&name);
}

#define NETSCAPE_INIT_FILE "nsswft.swf"

char *getDefaultTarget(void)
{
    char *fname = NULL;
    char *home = NULL;
    static char unix_home[512];

    /* first try to get it from the environment */
    fname = getenv("SW_FORTEZZA_FILE");
    if (fname != NULL) {
	return PORT_Strdup(fname);
    }

#ifdef XP_UNIX
    home = getenv("HOME");
    if (home) {
	strncpy(unix_home,home, sizeof(unix_home)-sizeof("/.netscape/"NETSCAPE_INIT_FILE));
	strcat(unix_home,"/.netscape/"NETSCAPE_INIT_FILE);
	return unix_home;
    }
#endif
#ifdef XP_WIN
    home = getenv("windir");
    if (home) {
	strncpy(unix_home,home, sizeof(unix_home)-sizeof("\\"NETSCAPE_INIT_FILE));
	strcat(unix_home,"\\"NETSCAPE_INIT_FILE);
	return unix_home;
    }
#endif
    return (NETSCAPE_INIT_FILE);
}

void
usage(char *prog) {
   fprintf(stderr,"usage: %s [-v][-f][-t transport_pass][-u user_pass][-o output_file] source_file\n",prog);
   exit(1);
}
	
main(int argc, char ** argv)
{

    FORTSignedSWFile * swfile;
    int size;
    SECItem file;
    char *progname = *argv++;
    char *filename = NULL;
    char *outname = NULL;
    char *cp;
    int verbose = 0;
    int force = 0;
    CERTCertDBHandle *certhandle = NULL;
    CERTCertificate *cert;
    CERTCertTrust *trust;
    char * pass;
    SECStatus rv;
    int i;
    SECMODModule *module;
    int64 now; /* XXXX */
    char *issuer;
    char *transport_pass = NULL;
    char *user_pass = NULL;
    SECItem *outItem = NULL;
    PRFileDesc *fd;
    PRFileInfo info;
    PRStatus prv;



   
    /* put better argument parsing here */
    while ((cp = *argv++) != NULL) {
	if (*cp == '-') {
	    while (*++cp) {
		switch (*cp) {
		/* verbose mode */
		case 'v':
		    verbose++;
		    break;
		/* explicitly set the target */
		case 'o':
		    outname = *argv++;
		    break;
		case 'f':
		/* skip errors in signatures without prompts */
		    force++;
		    break;
		case 't':
		/* provide password on command line */
		    transport_pass = *argv++;
		    break;
		case 'u':
		/* provide user password on command line */
		    user_pass = *argv++;
		    break;
		default:
		    usage(progname);
		    break;
		}
	    }
	} else if (filename) {
	    usage(progname);
	} else {
	    filename = cp;
	}
    }

    if (filename == NULL) usage(progname);
    if (outname == NULL) outname = getDefaultTarget();


    now = PR_Now();
    /* read the file in */ 
    fd = PR_Open(filename,PR_RDONLY,0);
    if (fd == NULL) { 
	fprintf(stderr,"%s: couldn't open file \"%s\".\n",progname,filename); 
	exit(1);
    }

    prv = PR_GetOpenFileInfo(fd,&info);
    if (prv != PR_SUCCESS) {
	fprintf(stderr,"%s: couldn't get info on file \"%s\".\n",
							progname,filename); 
	exit(1);
    }

    size = info.size;

    file.data = malloc(size);
    file.len = size;

    file.len = PR_Read(fd,file.data,file.len);
    if (file.len < 0) { 
	fprintf(stderr,"%s: couldn't read file \"%s\".\n",progname, filename); 
	exit(1);
    }

    PR_Close(fd);

    /* Parse the file */
    swfile = FORT_GetSWFile(&file);
    if (swfile == NULL) {
	fprintf(stderr,
	   "%s: File \"%s\" not a valid FORTEZZA initialization file.\n",
		progname,filename);
	exit(1);
    }

    issuer = formatDERIssuer(&swfile->file,&swfile->file.derIssuer);
    if (issuer == NULL) {
	issuer = "<Invalid Issuer DER>";
    }

    if (verbose) {
	printf("Processing file %s ....\n",filename);
	printf("  Version %d\n",DER_GetInteger(&swfile->file.version));
	printf("  Issuer: %s\n",issuer);
	printf("  Serial Number: ");
	for (i=0; i < (int)swfile->file.serialID.len; i++) {
	    printf(" %02x",swfile->file.serialID.data[i]);
	}
	printf("\n");
    }


    /* Check the Initalization phrase and save Kinit */
    if (!transport_pass) {
    	pass = SECU_GetPasswordString(NULL,"Enter the Initialization Memphrase:");
	transport_pass = pass;
    }
    rv = FORT_CheckInitPhrase(swfile,transport_pass);
    if (rv != SECSuccess) {
	fprintf(stderr,
		"%s: Invalid Initialization Memphrase for file \"%s\".\n",
		progname,filename);
	exit(1);
    }

    /* Check the user or init phrase and save Ks, use Kinit to unwrap the
     * remaining data. */
    if (!user_pass) {
    	pass = SECU_GetPasswordString(NULL,"Enter the User Memphrase or the User PIN:");
	user_pass = pass;
    }
    rv = FORT_CheckUserPhrase(swfile,user_pass);
    if (rv != SECSuccess) {
	fprintf(stderr,"%s: Invalid User Memphrase or PIN for file \"%s\".\n",
		progname,filename);
	exit(1);
    }

    NSS_NoDB_Init(NULL);
    sec_SetCheckKRLState(1);
    certhandle = CERT_GetDefaultCertDB();

    /* now dump the certs into the temparary data base */
    for (i=0; swfile->file.slotEntries[i]; i++) {
	int trusted = 0;
    	SECItem *derCert = FORT_GetDERCert(swfile,
			swfile->file.slotEntries[i]->certIndex);

	if (derCert == NULL) {
	    if (verbose) {
	       printf(" Cert %02d: %s \"%s\" \n",
		swfile->file.slotEntries[i]->certIndex,
			"untrusted", "Couldn't decrypt Cert");
	    }
	    continue;
	}
	cert = CERT_NewTempCertificate(certhandle,derCert, NULL, 
							PR_FALSE, PR_TRUE);
	if (cert == NULL) {
	    if (verbose) {
	       printf(" Cert %02d: %s \"%s\" \n",
		swfile->file.slotEntries[i]->certIndex,
			"untrusted", "Couldn't decode Cert");
	    }
	    continue;
	}
	if (swfile->file.slotEntries[i]->trusted.data[0]) {
	    /* Add TRUST */
	    trust = PORT_ArenaAlloc(cert->arena,sizeof(CERTCertTrust));
	    if (trust != NULL) {
	        trust->sslFlags = CERTDB_VALID_CA|CERTDB_TRUSTED_CA;
	        trust->emailFlags = CERTDB_VALID_CA|CERTDB_TRUSTED_CA;
	        trust->objectSigningFlags = CERTDB_VALID_CA|CERTDB_TRUSTED_CA;
	        cert->trust = trust;
		trusted++;
	    }
	}
	if (verbose) {
	    printf(" Cert %02d: %s \"%s\" \n",
		swfile->file.slotEntries[i]->certIndex,
		trusted?" trusted ":"untrusted",
			CERT_NameToAscii(&cert->subject));
	}
    }

    fflush(stdout);


    cert = CERT_FindCertByName(certhandle,&swfile->file.derIssuer);
    if (cert == NULL) {
	fprintf(stderr,"%s: Couldn't find signer certificate \"%s\".\n",
		progname,issuer);
	rv = SECFailure;
	goto noverify;
    }
    rv = CERT_VerifySignedData(&swfile->signatureWrap,cert, now, NULL);
    if (rv != SECSuccess) {
	fprintf(stderr,
	  "%s: Couldn't verify the signature on file \"%s\" with certificate \"%s\".\n",
		progname,filename,issuer);
	goto noverify;
    }
    rv = CERT_VerifyCert(certhandle, cert, PR_TRUE, certUsageSSLServer,
								now ,NULL,NULL);
    /* not an normal cert, see if it's a CA? */
    if (rv != SECSuccess) {
	rv = CERT_VerifyCert(certhandle, cert, PR_TRUE, certUsageAnyCA,
								now ,NULL,NULL);
    }
    if (rv != SECSuccess) {
	fprintf(stderr,"%s: Couldn't verify the signer certificate \"%s\".\n",
		progname,issuer);
	goto noverify;
    }

noverify:
    if (rv != SECSuccess) {
	if (!force) {
	    pass = GetUserInput(
	       "Signature verify failed, continue without verification? ");
	    if (!(pass && ((*pass == 'Y') || (*pass == 'y')))) {
		exit(1);
            }
	}
    }


    /* now write out the modified init file for future use */
    outItem = FORT_PutSWFile(swfile);
    if (outItem == NULL) {
	fprintf(stderr,"%s: Couldn't format target init file.\n",
		progname);
	goto noverify;
    }

    if (verbose) {
	printf("writing modified file out to \"%s\".\n",outname);
    }

    /* now write it out */
    fd = PR_Open(outname,PR_WRONLY|PR_CREATE_FILE|PR_TRUNCATE,0700);
    if (fd == NULL) { 
	fprintf(stderr,"%s: couldn't open file \"%s\".\n",progname,outname); 
	exit(1);
    }

    file.len = PR_Write(fd,outItem->data,outItem->len);
    if (file.len < 0) { 
	fprintf(stderr,"%s: couldn't read file \"%s\".\n",progname, filename); 
	exit(1);
    }

    PR_Close(fd);
    
    exit(0);
    return (0);
}

