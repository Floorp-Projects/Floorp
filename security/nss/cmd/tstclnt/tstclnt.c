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
**
** Sample client side test program that uses SSL and libsec
**
*/

#include "secutil.h"

#if defined(XP_UNIX)
#include <unistd.h>
#else
#include "ctype.h"	/* for isalpha() */
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>

#include "nspr.h"
#include "prio.h"
#include "prnetdb.h"
#include "nss.h"
#include "ssl.h"
#include "sslproto.h"
#include "pk11func.h"
#include "plgetopt.h"

#define PRINTF  if (verbose)  printf
#define FPRINTF if (verbose) fprintf

#define MAX_WAIT_FOR_SERVER 600
#define WAIT_INTERVAL       100

int ssl2CipherSuites[] = {
    SSL_EN_RC4_128_WITH_MD5,			/* A */
    SSL_EN_RC4_128_EXPORT40_WITH_MD5,		/* B */
    SSL_EN_RC2_128_CBC_WITH_MD5,		/* C */
    SSL_EN_RC2_128_CBC_EXPORT40_WITH_MD5,	/* D */
    SSL_EN_DES_64_CBC_WITH_MD5,			/* E */
    SSL_EN_DES_192_EDE3_CBC_WITH_MD5,		/* F */
    0
};

int ssl3CipherSuites[] = {
    SSL_FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA,	/* a */
    SSL_FORTEZZA_DMS_WITH_RC4_128_SHA,		/* b */
    SSL_RSA_WITH_RC4_128_MD5,			/* c */
    SSL_RSA_WITH_3DES_EDE_CBC_SHA,		/* d */
    SSL_RSA_WITH_DES_CBC_SHA,			/* e */
    SSL_RSA_EXPORT_WITH_RC4_40_MD5,		/* f */
    SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5,		/* g */
    SSL_FORTEZZA_DMS_WITH_NULL_SHA,		/* h */
    SSL_RSA_WITH_NULL_MD5,			/* i */
    SSL_RSA_FIPS_WITH_3DES_EDE_CBC_SHA,		/* j */
    SSL_RSA_FIPS_WITH_DES_CBC_SHA,		/* k */
    TLS_RSA_EXPORT1024_WITH_DES_CBC_SHA,	/* l */
    TLS_RSA_EXPORT1024_WITH_RC4_56_SHA,	        /* m */
    SSL_RSA_WITH_RC4_128_SHA,			/* n */
    TLS_DHE_DSS_WITH_RC4_128_SHA,		/* o */
    SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA,		/* p */
    SSL_DHE_DSS_WITH_3DES_EDE_CBC_SHA,		/* q */
    SSL_DHE_RSA_WITH_DES_CBC_SHA,		/* r */
    SSL_DHE_DSS_WITH_DES_CBC_SHA,		/* s */
    TLS_DHE_DSS_WITH_AES_128_CBC_SHA, 	    	/* t */
    TLS_DHE_RSA_WITH_AES_128_CBC_SHA,       	/* u */
    TLS_RSA_WITH_AES_128_CBC_SHA,     	    	/* v */
    TLS_DHE_DSS_WITH_AES_256_CBC_SHA, 	    	/* w */
    TLS_DHE_RSA_WITH_AES_256_CBC_SHA,       	/* x */
    TLS_RSA_WITH_AES_256_CBC_SHA,     	    	/* y */
    0
};

unsigned long __cmp_umuls;
PRBool verbose;

static char *progName;

/* This exists only for the automated test suite. It allows us to
 * pass in a password on the command line. 
 */

char *password = NULL;

char * ownPasswd( PK11SlotInfo *slot, PRBool retry, void *arg)
{
	char *passwd = NULL;
	if ( (!retry) && arg ) {
		passwd = PL_strdup((char *)arg);
	}
	return passwd;
}

void printSecurityInfo(PRFileDesc *fd)
{
    CERTCertificate * cert;
    SSL3Statistics * ssl3stats = SSL_GetStatistics();
    SECStatus result;
    SSLChannelInfo info;

    result = SSL_GetChannelInfo(fd, &info, sizeof info);
    if (result != SECSuccess)
    	return;
    if (info.length >= sizeof info - sizeof info.reserved) {
	fprintf(stderr, 
	       "SSL version %d.%d using %d-bit %s with %d-bit %s MAC\n",
	       info.protocolVersion >> 8, info.protocolVersion & 0xff,
	       info.effectiveKeyBits, info.symCipherName, 
	       info.macBits, info.macAlgorithmName);
	fprintf(stderr, 
	       "Server Authentication: %d-bit %s, Key Exchange: %d-bit %s\n",
	       info.authKeyBits, info.authAlgorithmName,
	       info.keaKeyBits,  info.keaTypeName);
    }
    cert = SSL_RevealCert(fd);
    if (cert) {
	char * ip = CERT_NameToAscii(&cert->issuer);
	char * sp = CERT_NameToAscii(&cert->subject);
        if (sp) {
	    fprintf(stderr, "subject DN: %s\n", sp);
	    PR_Free(sp);
	}
        if (ip) {
	    fprintf(stderr, "issuer  DN: %s\n", ip);
	    PR_Free(ip);
	}
	CERT_DestroyCertificate(cert);
	cert = NULL;
    }
    fprintf(stderr,
    	"%ld cache hits; %ld cache misses, %ld cache not reusable\n",
    	ssl3stats->hsh_sid_cache_hits, ssl3stats->hsh_sid_cache_misses,
	ssl3stats->hsh_sid_cache_not_ok);

}

void
handshakeCallback(PRFileDesc *fd, void *client_data)
{
    printSecurityInfo(fd);
}

static void Usage(const char *progName)
{
    printf(
"Usage:  %s -h host [-p port] [-d certdir] [-n nickname] [-23Tovx] \n"
"                   [-c ciphers] [-w passwd] [-q]\n", progName);
    printf("%-20s Hostname to connect with\n", "-h host");
    printf("%-20s Port number for SSL server\n", "-p port");
    printf("%-20s Directory with cert database (default is ~/.netscape)\n",
	  "-d certdir");
    printf("%-20s Nickname of key and cert for client auth\n", "-n nickname");
    printf("%-20s Disable SSL v2.\n", "-2");
    printf("%-20s Disable SSL v3.\n", "-3");
    printf("%-20s Disable TLS (SSL v3.1).\n", "-T");
    printf("%-20s Override bad server cert. Make it OK.\n", "-o");
    printf("%-20s Verbose progress reporting.\n", "-v");
    printf("%-20s Use export policy.\n", "-x");
    printf("%-20s Ping the server and then exit.\n", "-q");
    printf("%-20s Letter(s) chosen from the following list\n", "-c ciphers");
    printf(
"A    SSL2 RC4 128 WITH MD5\n"
"B    SSL2 RC4 128 EXPORT40 WITH MD5\n"
"C    SSL2 RC2 128 CBC WITH MD5\n"
"D    SSL2 RC2 128 CBC EXPORT40 WITH MD5\n"
"E    SSL2 DES 64 CBC WITH MD5\n"
"F    SSL2 DES 192 EDE3 CBC WITH MD5\n"
"\n"
"a    SSL3 FORTEZZA DMS WITH FORTEZZA CBC SHA\n"
"b    SSL3 FORTEZZA DMS WITH RC4 128 SHA\n"
"c    SSL3 RSA WITH RC4 128 MD5\n"
"d    SSL3 RSA WITH 3DES EDE CBC SHA\n"
"e    SSL3 RSA WITH DES CBC SHA\n"
"f    SSL3 RSA EXPORT WITH RC4 40 MD5\n"
"g    SSL3 RSA EXPORT WITH RC2 CBC 40 MD5\n"
"h    SSL3 FORTEZZA DMS WITH NULL SHA\n"
"i    SSL3 RSA WITH NULL MD5\n"
"j    SSL3 RSA FIPS WITH 3DES EDE CBC SHA\n"
"k    SSL3 RSA FIPS WITH DES CBC SHA\n"
"l    SSL3 RSA EXPORT WITH DES CBC SHA\t(new)\n"
"m    SSL3 RSA EXPORT WITH RC4 56 SHA\t(new)\n"
"n    SSL3 RSA WITH RC4 128 SHA\n"
"o    TLS  DHE DSS WITH RC4 128 SHA\n"
"p    SSL3 DHE RSA WITH 3DES EDE CBC SHA\n"
"q    SSL3 DHE DSS WITH 3DES EDE CBC SHA\n"
"r    SSL3 DHE RSA WITH DES CBC SHA\n"
"s    SSL3 DHE DSS WITH DES CBC SHA\n"
"t    TLS_DHE_DSS_WITH_AES_128_CBC_SHA\n"
"u    TLS_DHE_RSA_WITH_AES_128_CBC_SHA\n"
"v    TLS_RSA_WITH_AES_128_CBC_SHA\n"
"w    TLS_DHE_DSS_WITH_AES_256_CBC_SHA\n"
"x    TLS_DHE_RSA_WITH_AES_256_CBC_SHA\n"
"y    TLS_RSA_WITH_AES_256_CBC_SHA\n"
	);
    exit(1);
}

void
milliPause(PRUint32 milli)
{
    PRIntervalTime ticks = PR_MillisecondsToInterval(milli);
    PR_Sleep(ticks);
}

void
disableAllSSLCiphers(void)
{
    const PRUint16 *cipherSuites = SSL_ImplementedCiphers;
    int             i            = SSL_NumImplementedCiphers;
    SECStatus       rv;

    /* disable all the SSL3 cipher suites */
    while (--i >= 0) {
	PRUint16 suite = cipherSuites[i];
        rv = SSL_CipherPrefSetDefault(suite, PR_FALSE);
	if (rv != SECSuccess) {
	    PRErrorCode err = PR_GetError();
	    printf("SSL_CipherPrefSet didn't like value 0x%04x (i = %d): %s\n",
	    	   suite, i, SECU_Strerror(err));
	    exit(2);
	}
    }
}

/*
 * Callback is called when incoming certificate is not valid.
 * Returns SECSuccess to accept the cert anyway, SECFailure to reject.
 */
static SECStatus 
ownBadCertHandler(void * arg, PRFileDesc * socket)
{
    PRErrorCode err = PR_GetError();
    /* can log invalid cert here */
    printf("Bad server certificate: %d, %s\n", err, SECU_Strerror(err));
    return SECSuccess;	/* override, say it's OK. */
}

int main(int argc, char **argv)
{
    PRFileDesc *       s;
    PRFileDesc *       std_out;
    CERTCertDBHandle * handle;
    char *             host	=  NULL;
    char *             port	=  "443";
    char *             certDir  =  NULL;
    char *             nickname =  NULL;
    char *             cipherString = NULL;
    int                multiplier = 0;
    SECStatus          rv;
    PRStatus           status;
    PRInt32            filesReady;
    PRInt32            ip;
    int                npds;
    int                override = 0;
    int                disableSSL2 = 0;
    int                disableSSL3 = 0;
    int                disableTLS  = 0;
    int                useExportPolicy = 0;
    int                file_read = 0;
    PRSocketOptionData opt;
    PRNetAddr          addr;
    PRHostEnt          hp;
    PRPollDesc         pollset[2];
    char               buf[PR_NETDB_BUF_SIZE];
    PRBool             useCommandLinePassword = PR_FALSE;
    PRBool             pingServerFirst = PR_FALSE;
    int                error=0;
    PLOptState *optstate;
    PLOptStatus optstatus;
    PRStatus prStatus;

    progName = strrchr(argv[0], '/');
    if (!progName)
	progName = strrchr(argv[0], '\\');
    progName = progName ? progName+1 : argv[0];

    optstate = PL_CreateOptState(argc, argv, "23Tfc:h:p:d:m:n:oqvw:x");
    while ((optstatus = PL_GetNextOpt(optstate)) == PL_OPT_OK) {
	switch (optstate->option) {
	  case '?':
	  default : Usage(progName); 			break;

          case '2': disableSSL2 = 1; 			break;

          case '3': disableSSL3 = 1; 			break;

          case 'T': disableTLS  = 1; 			break;

          case 'c': cipherString = strdup(optstate->value); break;

          case 'h': host = strdup(optstate->value);	break;
#ifdef _WINDOWS
	  case 'f': file_read = 1; 			break;
#else
	  case 'f': break;
#endif

	  case 'd':
	    certDir = strdup(optstate->value);
	    certDir = SECU_ConfigDirectory(certDir);
	    break;

	  case 'm':
	    multiplier = atoi(optstate->value);
	    if (multiplier < 0)
	    	multiplier = 0;
	    break;

	  case 'n': nickname = strdup(optstate->value);	break;

	  case 'o': override = 1; 			break;

	  case 'p': port = strdup(optstate->value);	break;

	  case 'q': pingServerFirst = PR_TRUE;          break;

	  case 'v': verbose++;	 			break;

	  case 'w':
		password = PORT_Strdup(optstate->value);
		useCommandLinePassword = PR_TRUE;
		break;

	  case 'x': useExportPolicy = 1; 		break;
	}
    }
    if (optstatus == PL_OPT_BAD)
	Usage(progName);

    if (!host || !port) Usage(progName);

    if (!certDir) {
	certDir = SECU_DefaultSSLDir();	/* Look in $SSL_DIR */
	certDir = SECU_ConfigDirectory(certDir); /* call even if it's NULL */
    }

    PR_Init( PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);

    /* set our password function */
	if ( useCommandLinePassword ) {
		PK11_SetPasswordFunc(ownPasswd);
	} else {
    	PK11_SetPasswordFunc(SECU_GetModulePassword);
	}

    /* open the cert DB, the key DB, and the secmod DB. */
    rv = NSS_Init(certDir);
    if (rv != SECSuccess) {
	SECU_PrintError(progName, "unable to open cert database");
#if 0
    rv = CERT_OpenVolatileCertDB(handle);
	CERT_SetDefaultCertDB(handle);
#else
	return 1;
#endif
    }
    handle = CERT_GetDefaultCertDB();

    /* set the policy bits true for all the cipher suites. */
    if (useExportPolicy)
	NSS_SetExportPolicy();
    else
	NSS_SetDomesticPolicy();

    /* all the SSL2 and SSL3 cipher suites are enabled by default. */
    if (cipherString) {
	/* disable all the ciphers, then enable the ones we want. */
	disableAllSSLCiphers();
    }

    /* Lookup host */
    status = PR_GetHostByName(host, buf, sizeof(buf), &hp);
    if (status != PR_SUCCESS) {
	SECU_PrintError(progName, "error looking up host");
	return 1;
    }
    if (PR_EnumerateHostEnt(0, &hp, atoi(port), &addr) == -1) {
	SECU_PrintError(progName, "error looking up host address");
	return 1;
    }

    ip = PR_ntohl(addr.inet.ip);
    PRINTF("%s: connecting to %s:%d (address=%d.%d.%d.%d)\n",
	   progName, host, PR_ntohs(addr.inet.port),
	   (ip >> 24) & 0xff,
	   (ip >> 16) & 0xff,
	   (ip >>  8) & 0xff,
	   (ip >>  0) & 0xff);

    if (pingServerFirst) {
	int iter = 0;
	PRErrorCode err;
	do {
	    s = PR_NewTCPSocket();
	    if (s == NULL) {
		SECU_PrintError(progName, "Failed to create a TCP socket");
	    }
	    opt.option             = PR_SockOpt_Nonblocking;
	    opt.value.non_blocking = PR_FALSE;
	    prStatus = PR_SetSocketOption(s, &opt);
	    if (prStatus != PR_SUCCESS) {
		PR_Close(s);
		SECU_PrintError(progName, 
		                "Failed to set blocking socket option");
		return 1;
	    }
	    prStatus = PR_Connect(s, &addr, PR_INTERVAL_NO_TIMEOUT);
	    if (prStatus == PR_SUCCESS) {
    		PR_Shutdown(s, PR_SHUTDOWN_BOTH);
    		PR_Close(s);
    		NSS_Shutdown();
    		PR_Cleanup();
		return 0;
	    }
	    err = PR_GetError();
	    if ((err != PR_CONNECT_REFUSED_ERROR) && 
	        (err != PR_CONNECT_RESET_ERROR)) {
		SECU_PrintError(progName, "TCP Connection failed");
		return 1;
	    }
	    PR_Close(s);
	    PR_Sleep(PR_MillisecondsToInterval(WAIT_INTERVAL));
	} while (++iter < MAX_WAIT_FOR_SERVER);
	SECU_PrintError(progName, 
                     "Client timed out while waiting for connection to server");
	return 1;
    }

    /* Create socket */
    s = PR_NewTCPSocket();
    if (s == NULL) {
	SECU_PrintError(progName, "error creating socket");
	return 1;
    }

    opt.option = PR_SockOpt_Nonblocking;
    opt.value.non_blocking = PR_TRUE;
    PR_SetSocketOption(s, &opt);
    /*PR_SetSocketOption(PR_GetSpecialFD(PR_StandardInput), &opt);*/

    s = SSL_ImportFD(NULL, s);
    if (s == NULL) {
	SECU_PrintError(progName, "error importing socket");
	return 1;
    }

    rv = SSL_OptionSet(s, SSL_SECURITY, 1);
    if (rv != SECSuccess) {
        SECU_PrintError(progName, "error enabling socket");
	return 1;
    }

    rv = SSL_OptionSet(s, SSL_HANDSHAKE_AS_CLIENT, 1);
    if (rv != SECSuccess) {
	SECU_PrintError(progName, "error enabling client handshake");
	return 1;
    }

    /* all the SSL2 and SSL3 cipher suites are enabled by default. */
    if (cipherString) {
    	int ndx;

	while (0 != (ndx = *cipherString++)) {
	    int *cptr;
	    int  cipher;

	    if (! isalpha(ndx))
	     	Usage(progName);
	    cptr = islower(ndx) ? ssl3CipherSuites : ssl2CipherSuites;
	    for (ndx &= 0x1f; (cipher = *cptr++) != 0 && --ndx > 0; ) 
	    	/* do nothing */;
	    if (cipher) {
		SECStatus status;
		status = SSL_CipherPrefSet(s, cipher, SSL_ALLOWED);
		if (status != SECSuccess) 
		    SECU_PrintError(progName, "SSL_CipherPrefSet()");
	    }
	}
    }

    rv = SSL_OptionSet(s, SSL_ENABLE_SSL2, !disableSSL2);
    if (rv != SECSuccess) {
	SECU_PrintError(progName, "error enabling SSLv2 ");
	return 1;
    }

    rv = SSL_OptionSet(s, SSL_ENABLE_SSL3, !disableSSL3);
    if (rv != SECSuccess) {
	SECU_PrintError(progName, "error enabling SSLv3 ");
	return 1;
    }

    rv = SSL_OptionSet(s, SSL_ENABLE_TLS, !disableTLS);
    if (rv != SECSuccess) {
	SECU_PrintError(progName, "error enabling TLS ");
	return 1;
    }

    /* disable ssl2 and ssl2-compatible client hellos. */
    rv = SSL_OptionSet(s, SSL_V2_COMPATIBLE_HELLO, !disableSSL2);
    if (rv != SECSuccess) {
	SECU_PrintError(progName, "error disabling v2 compatibility");
	return 1;
    }

    if (useCommandLinePassword) {
	SSL_SetPKCS11PinArg(s, password);
    }

    SSL_AuthCertificateHook(s, SSL_AuthCertificate, (void *)handle);
    if (override) {
	SSL_BadCertHook(s, ownBadCertHandler, NULL);
    }
    SSL_GetClientAuthDataHook(s, NSS_GetClientAuthData, (void *)nickname);
    SSL_HandshakeCallback(s, handshakeCallback, NULL);
    SSL_SetURL(s, host);

    /* Try to connect to the server */
    status = PR_Connect(s, &addr, PR_INTERVAL_NO_TIMEOUT);
    if (status != PR_SUCCESS) {
	if (PR_GetError() == PR_IN_PROGRESS_ERROR) {
	    if (verbose)
		SECU_PrintError(progName, "connect");
	    milliPause(50 * multiplier);
	    pollset[0].fd = s;
	    pollset[0].in_flags = PR_POLL_WRITE | PR_POLL_EXCEPT;
	    pollset[0].out_flags = 0;
	    while(1) {
		PRINTF("%s: about to call PR_Poll for connect completion!\n", progName);
		filesReady = PR_Poll(pollset, 1, PR_INTERVAL_NO_TIMEOUT);
		if (filesReady < 0) {
		    SECU_PrintError(progName, "unable to connect (poll)");
		    return 1;
		}
		PRINTF("%s: PR_Poll returned 0x%02x for socket out_flags.\n",
			progName, pollset[0].out_flags);
		if (filesReady == 0) {	/* shouldn't happen! */
		    PRINTF("%s: PR_Poll returned zero!\n", progName);
		    return 1;
		}
		/* Must milliPause between PR_Poll and PR_GetConnectStatus,
		 * Or else winsock gets mighty confused.
		 * Sleep(0);
		 */
		milliPause(1);
		status = PR_GetConnectStatus(pollset);
		if (status == PR_SUCCESS) {
		    break;
		}
		if (PR_GetError() != PR_IN_PROGRESS_ERROR) {
		    SECU_PrintError(progName, "unable to connect (poll)");
		    return 1;
		}
		SECU_PrintError(progName, "poll");
		milliPause(50 * multiplier);
	    }
	} else {
	    SECU_PrintError(progName, "unable to connect");
	    return 1;
	}
    }

    pollset[0].fd        = s;
    pollset[0].in_flags  = PR_POLL_READ;
    pollset[1].fd        = PR_GetSpecialFD(PR_StandardInput);
    pollset[1].in_flags  = PR_POLL_READ;
    npds                 = 2;
    std_out              = PR_GetSpecialFD(PR_StandardOutput);


    if (file_read) {
	pollset[1].out_flags = PR_POLL_READ;
	npds=1;
    }

    /*
    ** Select on stdin and on the socket. Write data from stdin to
    ** socket, read data from socket and write to stdout.
    */
    PRINTF("%s: ready...\n", progName);

    while (pollset[0].in_flags || pollset[1].in_flags) {
	char buf[4000];	/* buffer for stdin */
	int nb;		/* num bytes read from stdin. */

	pollset[0].out_flags = 0;
        if (!file_read) {
	    pollset[1].out_flags = 0;
	}

	PRINTF("%s: about to call PR_Poll !\n", progName);
        if (pollset[1].in_flags && file_read) {
		filesReady = PR_Poll(pollset, npds, PR_INTERVAL_NO_WAIT);
		filesReady++;
	} else {
		filesReady = PR_Poll(pollset, npds, PR_INTERVAL_NO_TIMEOUT);
	}
	if (filesReady < 0) {
	   SECU_PrintError(progName, "select failed");
	   error=1;
	   goto done;
	}
	if (filesReady == 0) {	/* shouldn't happen! */
	    PRINTF("%s: PR_Poll returned zero!\n", progName);
	    return 1;
	}
	PRINTF("%s: PR_Poll returned!\n", progName);
	if (pollset[1].in_flags) {
	        PRINTF("%s: PR_Poll returned 0x%02x for stdin out_flags.\n",
		    progName, pollset[1].out_flags);
#ifndef _WINDOWS 
	}
	if (pollset[1].out_flags & PR_POLL_READ) {
#endif
	    /* Read from stdin and write to socket */
	    nb = PR_Read(pollset[1].fd, buf, sizeof(buf));
	    PRINTF("%s: stdin read %d bytes\n", progName, nb);
	    if (nb < 0) {
		if (PR_GetError() != PR_WOULD_BLOCK_ERROR) {
		    SECU_PrintError(progName, "read from stdin failed");
	            error=1;
		    break;
		}
	    } else if (nb == 0) {
		pollset[1].in_flags = 0;
	    } else {
		char * bufp = buf;
		PRINTF("%s: Writing %d bytes to server\n", progName, nb);
		do {
		    PRInt32 cc = PR_Write(s, bufp, nb);
		    if (cc < 0) {
		    	PRErrorCode err = PR_GetError();
			if (err != PR_WOULD_BLOCK_ERROR) {
			    SECU_PrintError(progName, 
			                    "write to SSL socket failed");
			    error=254;
			    goto done;
			}
			cc = 0;
		    }
		    bufp += cc;
		    nb   -= cc;
		    if (nb <= 0) 
		    	break;
		    pollset[0].in_flags = PR_POLL_WRITE | PR_POLL_EXCEPT;
		    pollset[0].out_flags = 0;
		    PRINTF("%s: about to call PR_Poll on writable socket !\n", progName);
		    cc = PR_Poll(pollset, 1, PR_INTERVAL_NO_TIMEOUT);
		    PRINTF("%s: PR_Poll returned with writable socket !\n", progName);
		} while (1);
		pollset[0].in_flags  = PR_POLL_READ;
	    }
	}

	if (pollset[0].in_flags) {
	    PRINTF("%s: PR_Poll returned 0x%02x for socket out_flags.\n",
		   progName, pollset[0].out_flags);
	}
	if (   (pollset[0].out_flags & PR_POLL_READ) 
	    || (pollset[0].out_flags & PR_POLL_ERR)  
#ifdef PR_POLL_HUP
	    || (pollset[0].out_flags & PR_POLL_HUP)
#endif
	    ) {
	    /* Read from socket and write to stdout */
	    nb = PR_Read(pollset[0].fd, buf, sizeof(buf));
	    PRINTF("%s: Read from server %d bytes\n", progName, nb);
	    if (nb < 0) {
		if (PR_GetError() != PR_WOULD_BLOCK_ERROR) {
		    SECU_PrintError(progName, "read from socket failed");
		    error=1;
		    goto done;
	    	}
	    } else if (nb == 0) {
		/* EOF from socket... bye bye */
		pollset[0].in_flags = 0;
	    } else {
		PR_Write(std_out, buf, nb);
		puts("\n\n");
	    }
	}
	milliPause(50 * multiplier);
    }

  done:
    PR_Close(s);
    NSS_Shutdown();
    PR_Cleanup();
    return error;
}
