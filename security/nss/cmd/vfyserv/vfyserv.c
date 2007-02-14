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
 *   Douglas Stebila <douglas@stebila.ca>, Sun Microsystems Laboratories
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

/****************************************************************************
 *  SSL client program that tests  a server for proper operation of SSL2,   *
 *  SSL3, and TLS. Test propder certificate installation.                   *
 *                                                                          *
 *  This code was modified from the SSLSample code also kept in the NSS     *
 *  directory.                                                              *
 ****************************************************************************/ 

#include <stdio.h>
#include <string.h>

#if defined(XP_UNIX)
#include <unistd.h>
#endif

#include "prerror.h"

#include "pk11func.h"
#include "secmod.h"
#include "secitem.h"


#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>

#include "nspr.h"
#include "plgetopt.h"
#include "prio.h"
#include "prnetdb.h"
#include "nss.h"
#include "secutil.h"
#include "ocsp.h"

#include "vfyserv.h"

#define RD_BUF_SIZE (60 * 1024)

extern int ssl2CipherSuites[];
extern int ssl3CipherSuites[];

GlobalThreadMgr threadMGR;
char *certNickname = NULL;
char *hostName = NULL;
char *password = NULL;
unsigned short port = 0;
PRBool dumpChain;

static void
Usage(const char *progName)
{
    PRFileDesc *pr_stderr;

    pr_stderr = PR_STDERR;

    PR_fprintf(pr_stderr, "Usage:\n"
               "   %s  [-c ] [-o] [-p port] [-d dbdir] [-w password]\n"
               "   \t\t[-C cipher(s)]  [-l <url> -t <nickname> ] hostname",
               progName);
    PR_fprintf (pr_stderr, "\nWhere:\n");
    PR_fprintf (pr_stderr,
                "  %-13s dump server cert chain into files\n",
                "-c");
    PR_fprintf (pr_stderr,
                "  %-13s perform server cert OCSP check\n",
                "-o");
    PR_fprintf (pr_stderr,
                "  %-13s server port to be used\n",
                "-p");
    PR_fprintf (pr_stderr,
                "  %-13s use security databases in \"dbdir\"\n",
                "-d dbdir");
    PR_fprintf (pr_stderr,
                "  %-13s key database password\n",
                "-w password");
    PR_fprintf (pr_stderr,
                "  %-13s communication cipher list\n",
                "-C cipher(s)");
    PR_fprintf (pr_stderr,
                "  %-13s OCSP responder location. This location is used to\n"
                "  %-13s check  status  of a server  certificate.  If  not \n"
                "  %-13s specified, location  will  be taken  from the AIA\n"
                "  %-13s server certificate extension.\n",
                "-l url", "", "", "");
    PR_fprintf (pr_stderr,
                "  %-13s OCSP Trusted Responder Cert nickname\n\n",
                "-t nickname");

	exit(1);
}

PRFileDesc *
setupSSLSocket(PRNetAddr *addr)
{
	PRFileDesc         *tcpSocket;
	PRFileDesc         *sslSocket;
	PRSocketOptionData	socketOption;
	PRStatus            prStatus;
	SECStatus           secStatus;


	tcpSocket = PR_NewTCPSocket();
	if (tcpSocket == NULL) {
		errWarn("PR_NewTCPSocket");
	}

	/* Make the socket blocking. */
	socketOption.option	            = PR_SockOpt_Nonblocking;
	socketOption.value.non_blocking = PR_FALSE;

	prStatus = PR_SetSocketOption(tcpSocket, &socketOption);
	if (prStatus != PR_SUCCESS) {
		errWarn("PR_SetSocketOption");
		goto loser;
	} 


	/* Import the socket into the SSL layer. */
	sslSocket = SSL_ImportFD(NULL, tcpSocket);
	if (!sslSocket) {
		errWarn("SSL_ImportFD");
		goto loser;
	}

	/* Set configuration options. */
	secStatus = SSL_OptionSet(sslSocket, SSL_SECURITY, PR_TRUE);
	if (secStatus != SECSuccess) {
		errWarn("SSL_OptionSet:SSL_SECURITY");
		goto loser;
	}

	secStatus = SSL_OptionSet(sslSocket, SSL_HANDSHAKE_AS_CLIENT, PR_TRUE);
	if (secStatus != SECSuccess) {
		errWarn("SSL_OptionSet:SSL_HANDSHAKE_AS_CLIENT");
		goto loser;
	}

	/* Set SSL callback routines. */
	secStatus = SSL_GetClientAuthDataHook(sslSocket,
	                          (SSLGetClientAuthData)myGetClientAuthData,
	                          (void *)certNickname);
	if (secStatus != SECSuccess) {
		errWarn("SSL_GetClientAuthDataHook");
		goto loser;
	}

	secStatus = SSL_AuthCertificateHook(sslSocket,
	                                   (SSLAuthCertificate)myAuthCertificate,
                                       (void *)CERT_GetDefaultCertDB());
	if (secStatus != SECSuccess) {
		errWarn("SSL_AuthCertificateHook");
		goto loser;
	}

	secStatus = SSL_BadCertHook(sslSocket, 
	                           (SSLBadCertHandler)myBadCertHandler, NULL);
	if (secStatus != SECSuccess) {
		errWarn("SSL_BadCertHook");
		goto loser;
	}

	secStatus = SSL_HandshakeCallback(sslSocket, 
	                                 (SSLHandshakeCallback)myHandshakeCallback,
	                                 NULL);
	if (secStatus != SECSuccess) {
		errWarn("SSL_HandshakeCallback");
		goto loser;
	}

	return sslSocket;

loser:

	PR_Close(tcpSocket);
	return NULL;
}


const char requestString[] = {"GET /testfile HTTP/1.0\r\n\r\n" };

SECStatus
handle_connection(PRFileDesc *sslSocket, int connection)
{
	int	countRead = 0;
	PRInt32  numBytes;
	char    *readBuffer;

	readBuffer = PORT_Alloc(RD_BUF_SIZE);
	if (!readBuffer) {
		exitErr("PORT_Alloc");
	}

	/* compose the http request here. */

	numBytes = PR_Write(sslSocket, requestString, strlen(requestString));
	if (numBytes <= 0) {
		errWarn("PR_Write");
		PR_Free(readBuffer);
		readBuffer = NULL;
		return SECFailure;
	}

	/* read until EOF */
	while (PR_TRUE) {
		numBytes = PR_Read(sslSocket, readBuffer, RD_BUF_SIZE);
		if (numBytes == 0) {
			break;	/* EOF */
		}
		if (numBytes < 0) {
			errWarn("PR_Read");
			break;
		}
		countRead += numBytes;
	}

	printSecurityInfo(stderr, sslSocket);
	
	PR_Free(readBuffer);
	readBuffer = NULL;

	/* Caller closes the socket. */

	fprintf(stderr, 
	        "***** Connection %d read %d bytes total.\n", 
	        connection, countRead);

	return SECSuccess;	/* success */
}

#define BYTE(n,i) (((i)>>((n)*8))&0xff)

/* one copy of this function is launched in a separate thread for each
** connection to be made.
*/
SECStatus
do_connects(void *a, int connection)
{
	PRNetAddr  *addr = (PRNetAddr *)a;
	PRFileDesc *sslSocket;
	PRHostEnt   hostEntry;
	char        buffer[PR_NETDB_BUF_SIZE];
	PRStatus    prStatus;
	PRIntn      hostenum;
	PRInt32    ip;
	SECStatus   secStatus;

	/* Set up SSL secure socket. */
	sslSocket = setupSSLSocket(addr);
	if (sslSocket == NULL) {
		errWarn("setupSSLSocket");
		return SECFailure;
	}

	secStatus = SSL_SetPKCS11PinArg(sslSocket, password);
	if (secStatus != SECSuccess) {
		errWarn("SSL_SetPKCS11PinArg");
		return secStatus;
	}

	secStatus = SSL_SetURL(sslSocket, hostName);
	if (secStatus != SECSuccess) {
		errWarn("SSL_SetURL");
		return secStatus;
	}

	/* Prepare and setup network connection. */
	prStatus = PR_GetHostByName(hostName, buffer, sizeof(buffer), &hostEntry);
	if (prStatus != PR_SUCCESS) {
		errWarn("PR_GetHostByName");
		return SECFailure;
	}

	hostenum = PR_EnumerateHostEnt(0, &hostEntry, port, addr);
	if (hostenum == -1) {
		errWarn("PR_EnumerateHostEnt");
		return SECFailure;
	}

 	ip = PR_ntohl(addr->inet.ip);
	fprintf(stderr,
	 	"Connecting to host %s (addr %d.%d.%d.%d) on port %d\n",
			hostName, BYTE(3,ip), BYTE(2,ip), BYTE(1,ip), 
			BYTE(0,ip), PR_ntohs(addr->inet.port)); 

	prStatus = PR_Connect(sslSocket, addr, PR_INTERVAL_NO_TIMEOUT);
	if (prStatus != PR_SUCCESS) {
		errWarn("PR_Connect");
		return SECFailure;
	}

	/* Established SSL connection, ready to send data. */
#if 0
	secStatus = SSL_ForceHandshake(sslSocket);
	if (secStatus != SECSuccess) {
		errWarn("SSL_ForceHandshake");
		return secStatus;
	}
#endif

	secStatus = SSL_ResetHandshake(sslSocket, /* asServer */ PR_FALSE);
	if (secStatus != SECSuccess) {
		errWarn("SSL_ResetHandshake");
		prStatus = PR_Close(sslSocket);
		if (prStatus != PR_SUCCESS) {
			errWarn("PR_Close");
		}
		return secStatus;
	}

	secStatus = handle_connection(sslSocket, connection);
	if (secStatus != SECSuccess) {
		/* error already printed out in handle_connection */
		/* errWarn("handle_connection"); */
		prStatus = PR_Close(sslSocket);
		if (prStatus != PR_SUCCESS) {
			errWarn("PR_Close");
		}
		return secStatus;
	}

	PR_Close(sslSocket);
	return SECSuccess;
}

void
client_main(unsigned short      port, 
            int	                connections, 
            const char *        hostName)
{
	int			i;
	SECStatus	secStatus;
	PRStatus    prStatus;
	PRInt32     rv;
	PRNetAddr	addr;
	PRHostEnt   hostEntry;
	char        buffer[256];

	/* Setup network connection. */
	prStatus = PR_GetHostByName(hostName, buffer, 256, &hostEntry);
	if (prStatus != PR_SUCCESS) {
		exitErr("PR_GetHostByName");
	}

	rv = PR_EnumerateHostEnt(0, &hostEntry, port, &addr);
	if (rv < 0) {
		exitErr("PR_EnumerateHostEnt");
	}

	secStatus = launch_thread(&threadMGR, do_connects, &addr, 1);
	if (secStatus != SECSuccess) {
		exitErr("launch_thread");
	}

	if (connections > 1) {
		/* wait for the first connection to terminate, then launch the rest. */
		reap_threads(&threadMGR);
		/* Start up the connections */
		for (i = 2; i <= connections; ++i) {
			secStatus = launch_thread(&threadMGR, do_connects, &addr, i);
			if (secStatus != SECSuccess) {
				errWarn("launch_thread");
			}
		}
	}

	reap_threads(&threadMGR);
	destroy_thread_data(&threadMGR);
}

#define HEXCHAR_TO_INT(c, i) \
    if (((c) >= '0') && ((c) <= '9')) { \
	i = (c) - '0'; \
    } else if (((c) >= 'a') && ((c) <= 'f')) { \
	i = (c) - 'a' + 10; \
    } else if (((c) >= 'A') && ((c) <= 'F')) { \
	i = (c) - 'A' + 10; \
    } else { \
	Usage(progName); \
    }

int
main(int argc, char **argv)
{
	char *               certDir = NULL;
	char *               progName     = NULL;
	int                  connections  = 1;
	char *               cipherString = NULL;
	char *               respUrl = NULL;
	char *               respCertName = NULL;
	SECStatus            secStatus;
	PLOptState *         optstate;
	PLOptStatus          status;
	PRBool               doOcspCheck = PR_FALSE;

	/* Call the NSPR initialization routines */
	PR_Init( PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);

	progName = PORT_Strdup(argv[0]);

	hostName = NULL;
	optstate = PL_CreateOptState(argc, argv, "C:cd:l:n:p:ot:w:");
	while ((status = PL_GetNextOpt(optstate)) == PL_OPT_OK) {
		switch(optstate->option) {
		case 'C' : cipherString = PL_strdup(optstate->value); break;
 		case 'c' : dumpChain = PR_TRUE;                       break;
		case 'd' : certDir = PL_strdup(optstate->value);      break;
		case 'l' : respUrl = PL_strdup(optstate->value);      break;
		case 'p' : port = PORT_Atoi(optstate->value);         break;
		case 'o' : doOcspCheck = PR_TRUE;                     break;
		case 't' : respCertName = PL_strdup(optstate->value); break;
		case 'w' : password = PL_strdup(optstate->value);     break;
		case '\0': hostName = PL_strdup(optstate->value);     break;
		default  : Usage(progName);
		}
	}

	if (port == 0) {
		port = 443;
	}

	if (port == 0 || hostName == NULL)
		Usage(progName);

        if (doOcspCheck &&
            ((respCertName != NULL && respUrl == NULL) ||
             (respUrl != NULL && respCertName == NULL))) {
	    SECU_PrintError (progName, "options -l <url> and -t "
	                     "<responder> must be used together");
	    Usage(progName);
        }
    
	/* Set our password function callback. */
	PK11_SetPasswordFunc(myPasswd);

	/* Initialize the NSS libraries. */
	if (certDir) {
	    secStatus = NSS_Init(certDir);
	} else {
	    secStatus = NSS_NoDB_Init(NULL);

	    /* load the builtins */
	    SECMOD_AddNewModule("Builtins",
				DLL_PREFIX"nssckbi."DLL_SUFFIX, 0, 0);
	}
	if (secStatus != SECSuccess) {
		exitErr("NSS_Init");
	}
	SECU_RegisterDynamicOids();

	if (doOcspCheck == PR_TRUE) {
            SECStatus rv;
            CERTCertDBHandle *handle = CERT_GetDefaultCertDB();
            if (handle == NULL) {
                SECU_PrintError (progName, "problem getting certdb handle");
                goto cleanup;
            }
            
            rv = CERT_EnableOCSPChecking (handle);
            if (rv != SECSuccess) {
                SECU_PrintError (progName, "error enabling OCSP checking");
                goto cleanup;
            }

            if (respUrl != NULL) {
                rv = CERT_SetOCSPDefaultResponder (handle, respUrl,
                                                   respCertName);
                if (rv != SECSuccess) {
                    SECU_PrintError (progName,
                                     "error setting default responder");
                    goto cleanup;
                }
                
                rv = CERT_EnableOCSPDefaultResponder (handle);
                if (rv != SECSuccess) {
                    SECU_PrintError (progName,
                                     "error enabling default responder");
                    goto cleanup;
                }
            }
	}

	/* All cipher suites except RSA_NULL_MD5 are enabled by 
	 * Domestic Policy. */
	NSS_SetDomesticPolicy();
	SSL_CipherPrefSetDefault(SSL_RSA_WITH_NULL_MD5, PR_TRUE);

	/* all the SSL2 and SSL3 cipher suites are enabled by default. */
	if (cipherString) {
	    int ndx;

	    /* disable all the ciphers, then enable the ones we want. */
	    disableAllSSLCiphers();

	    while (0 != (ndx = *cipherString++)) {
		int  cipher;

		if (ndx == ':') {
		    int ctmp;

		    cipher = 0;
		    HEXCHAR_TO_INT(*cipherString, ctmp)
		    cipher |= (ctmp << 12);
		    cipherString++;
		    HEXCHAR_TO_INT(*cipherString, ctmp)
		    cipher |= (ctmp << 8);
		    cipherString++;
		    HEXCHAR_TO_INT(*cipherString, ctmp)
		    cipher |= (ctmp << 4);
		    cipherString++;
		    HEXCHAR_TO_INT(*cipherString, ctmp)
		    cipher |= ctmp;
		    cipherString++;
		} else {
		    const int *cptr;
		    if (! isalpha(ndx))
			Usage(progName);
		    cptr = islower(ndx) ? ssl3CipherSuites : ssl2CipherSuites;
		    for (ndx &= 0x1f; (cipher = *cptr++) != 0 && --ndx > 0; )
			/* do nothing */;
		}
		if (cipher > 0) {
		    SSL_CipherPrefSetDefault(cipher, PR_TRUE);
		} else {
		    Usage(progName);
		}
	    }
	}

	client_main(port, connections, hostName);

cleanup:
        if (doOcspCheck) {
            CERTCertDBHandle *handle = CERT_GetDefaultCertDB();
            CERT_DisableOCSPDefaultResponder(handle);        
            CERT_DisableOCSPChecking (handle);
        }

        if (NSS_Shutdown() != SECSuccess) {
            exit(1);
        }

	PR_Cleanup();
	PORT_Free(progName);
	return 0;
}

