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
 *   Dr Vipul Gupta <vipul.gupta@sun.com>, Sun Microsystems Laboratories
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
**
** Sample client side test program that uses SSL and libsec
**
*/

#include "secutil.h"

#if defined(XP_UNIX)
#include <unistd.h>
#else
#include <ctype.h>	/* for isalpha() */
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

#if defined(WIN32)
#include <fcntl.h>
#include <io.h>
#endif

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
#ifdef NSS_ENABLE_ECC
    /* NOTE: Since no new SSL2 ciphersuites are being 
     * invented, and we've run out of lowercase letters
     * for SSL3 ciphers, we use letters G and beyond
     * for new SSL3 ciphers.
     */
    TLS_ECDH_ECDSA_WITH_NULL_SHA,       	/* G */
    TLS_ECDH_ECDSA_WITH_RC4_128_SHA,       	/* H */
    TLS_ECDH_ECDSA_WITH_DES_CBC_SHA,       	/* I */
    TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA,    	/* J */
    TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA,     	/* K */
    TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA,     	/* L */
    TLS_ECDH_RSA_WITH_NULL_SHA,          	/* M */
    TLS_ECDH_RSA_WITH_RC4_128_SHA,       	/* N */
    TLS_ECDH_RSA_WITH_DES_CBC_SHA,       	/* O */
    TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA,      	/* P */
    TLS_ECDH_RSA_WITH_AES_128_CBC_SHA,       	/* Q */
    TLS_ECDH_RSA_WITH_AES_256_CBC_SHA,       	/* R */
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,    	/* S */
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,      	/* T */
#endif /* NSS_ENABLE_ECC */
    0
};

int ssl3CipherSuites[] = {
    -1, /* SSL_FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA* a */
    -1, /* SSL_FORTEZZA_DMS_WITH_RC4_128_SHA,	 * b */
    SSL_RSA_WITH_RC4_128_MD5,			/* c */
    SSL_RSA_WITH_3DES_EDE_CBC_SHA,		/* d */
    SSL_RSA_WITH_DES_CBC_SHA,			/* e */
    SSL_RSA_EXPORT_WITH_RC4_40_MD5,		/* f */
    SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5,		/* g */
    -1, /* SSL_FORTEZZA_DMS_WITH_NULL_SHA,	 * h */
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
    SSL_RSA_WITH_NULL_SHA,			/* z */
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
    SSLChannelInfo    channel;
    SSLCipherSuiteInfo suite;

    result = SSL_GetChannelInfo(fd, &channel, sizeof channel);
    if (result == SECSuccess && 
        channel.length == sizeof channel && 
	channel.cipherSuite) {
	result = SSL_GetCipherSuiteInfo(channel.cipherSuite, 
					&suite, sizeof suite);
	if (result == SECSuccess) {
	    FPRINTF(stderr, 
	    "tstclnt: SSL version %d.%d using %d-bit %s with %d-bit %s MAC\n",
	       channel.protocolVersion >> 8, channel.protocolVersion & 0xff,
	       suite.effectiveKeyBits, suite.symCipherName, 
	       suite.macBits, suite.macAlgorithmName);
	    FPRINTF(stderr, 
	    "tstclnt: Server Auth: %d-bit %s, Key Exchange: %d-bit %s\n",
	       channel.authKeyBits, suite.authAlgorithmName,
	       channel.keaKeyBits,  suite.keaTypeName);
    	}
    }
    cert = SSL_RevealCert(fd);
    if (cert) {
	char * ip = CERT_NameToAscii(&cert->issuer);
	char * sp = CERT_NameToAscii(&cert->subject);
        if (sp) {
	    fprintf(stderr, "subject DN: %s\n", sp);
	    PORT_Free(sp);
	}
        if (ip) {
	    fprintf(stderr, "issuer  DN: %s\n", ip);
	    PORT_Free(ip);
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
    fprintf(stderr, 
"Usage:  %s -h host [-p port] [-d certdir] [-n nickname] [-23Tfovx] \n"
"                   [-c ciphers] [-w passwd] [-q]\n", progName);
    fprintf(stderr, "%-20s Hostname to connect with\n", "-h host");
    fprintf(stderr, "%-20s Port number for SSL server\n", "-p port");
    fprintf(stderr, 
            "%-20s Directory with cert database (default is ~/.netscape)\n",
	    "-d certdir");
    fprintf(stderr, "%-20s Nickname of key and cert for client auth\n", 
                    "-n nickname");
    fprintf(stderr, "%-20s Disable SSL v2.\n", "-2");
    fprintf(stderr, "%-20s Disable SSL v3.\n", "-3");
    fprintf(stderr, "%-20s Disable TLS (SSL v3.1).\n", "-T");
    fprintf(stderr, "%-20s Client speaks first. \n", "-f");
    fprintf(stderr, "%-20s Override bad server cert. Make it OK.\n", "-o");
    fprintf(stderr, "%-20s Verbose progress reporting.\n", "-v");
    fprintf(stderr, "%-20s Use export policy.\n", "-x");
    fprintf(stderr, "%-20s Ping the server and then exit.\n", "-q");
    fprintf(stderr, "%-20s Letter(s) chosen from the following list\n", 
                    "-c ciphers");
    fprintf(stderr, 
"A    SSL2 RC4 128 WITH MD5\n"
"B    SSL2 RC4 128 EXPORT40 WITH MD5\n"
"C    SSL2 RC2 128 CBC WITH MD5\n"
"D    SSL2 RC2 128 CBC EXPORT40 WITH MD5\n"
"E    SSL2 DES 64 CBC WITH MD5\n"
"F    SSL2 DES 192 EDE3 CBC WITH MD5\n"
#ifdef NSS_ENABLE_ECC
"G    TLS ECDH ECDSA WITH NULL SHA\n"
"H    TLS ECDH ECDSA WITH RC4 128 SHA\n"
"I    TLS ECDH ECDSA WITH DES CBC SHA\n"
"J    TLS ECDH ECDSA WITH 3DES EDE CBC SHA\n"
"K    TLS ECDH ECDSA WITH AES 128 CBC SHA\n"
"L    TLS ECDH ECDSA WITH AES 256 CBC SHA\n"
"M    TLS ECDH RSA WITH NULL SHA\n"
"N    TLS ECDH RSA WITH RC4 128 SHA\n"
"O    TLS ECDH RSA WITH DES CBC SHA\n"
"P    TLS ECDH RSA WITH 3DES EDE CBC SHA\n"
"Q    TLS ECDH RSA WITH AES 128 CBC SHA\n"
"R    TLS ECDH RSA WITH AES 256 CBC SHA\n"
"S    TLS ECDHE ECDSA WITH AES 128 CBC SHA\n"
"T    TLS ECDHE RSA WITH AES 128 CBC SHA\n"
#endif /* NSS_ENABLE_ECC */
"\n"
"c    SSL3 RSA WITH RC4 128 MD5\n"
"d    SSL3 RSA WITH 3DES EDE CBC SHA\n"
"e    SSL3 RSA WITH DES CBC SHA\n"
"f    SSL3 RSA EXPORT WITH RC4 40 MD5\n"
"g    SSL3 RSA EXPORT WITH RC2 CBC 40 MD5\n"
"i    SSL3 RSA WITH NULL MD5\n"
"j    SSL3 RSA FIPS WITH 3DES EDE CBC SHA\n"
"k    SSL3 RSA FIPS WITH DES CBC SHA\n"
"l    SSL3 RSA EXPORT WITH DES CBC SHA\t(new)\n"
"m    SSL3 RSA EXPORT WITH RC4 56 SHA\t(new)\n"
"n    SSL3 RSA WITH RC4 128 SHA\n"
"o    SSL3 DHE DSS WITH RC4 128 SHA\n"
"p    SSL3 DHE RSA WITH 3DES EDE CBC SHA\n"
"q    SSL3 DHE DSS WITH 3DES EDE CBC SHA\n"
"r    SSL3 DHE RSA WITH DES CBC SHA\n"
"s    SSL3 DHE DSS WITH DES CBC SHA\n"
"t    SSL3 DHE DSS WITH AES 128 CBC SHA\n"
"u    SSL3 DHE RSA WITH AES 128 CBC SHA\n"
"v    SSL3 RSA WITH AES 128 CBC SHA\n"
"w    SSL3 DHE DSS WITH AES 256 CBC SHA\n"
"x    SSL3 DHE RSA WITH AES 256 CBC SHA\n"
"y    SSL3 RSA WITH AES 256 CBC SHA\n"
"z    SSL3 RSA WITH NULL SHA\n"
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
	    fprintf(stderr,
	            "SSL_CipherPrefSet didn't like value 0x%04x (i = %d): %s\n",
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
    fprintf(stderr, "Bad server certificate: %d, %s\n", err, 
            SECU_Strerror(err));
    return SECSuccess;	/* override, say it's OK. */
}

SECStatus
own_GetClientAuthData(void *                       arg,
                      PRFileDesc *                 socket,
                      struct CERTDistNamesStr *    caNames,
                      struct CERTCertificateStr ** pRetCert,
                      struct SECKEYPrivateKeyStr **pRetKey)
{
    if (verbose > 1) {
	SECStatus rv;
        fprintf(stderr, "Server requested Client Authentication\n");
	if (caNames && caNames->nnames > 0) {
	    PLArenaPool *arena = caNames->arena;
	    if (!arena)
		arena = PORT_NewArena(2048);
	    if (arena) {
		int i;
		for (i = 0; i < caNames->nnames; ++i) {
		    char *nameString;
		    CERTName dn;
		    rv = SEC_QuickDERDecodeItem(arena, 
					    &dn,
					    SEC_ASN1_GET(CERT_NameTemplate), 
					    caNames->names + i);
		    if (rv != SECSuccess)
			continue;
		    nameString = CERT_NameToAscii(&dn);
		    if (!nameString)
			continue;
		    fprintf(stderr, "CA[%d]: %s\n", i + 1, nameString);
		    PORT_Free(nameString);
		}
		if (!caNames->arena) {
		    PORT_FreeArena(arena, PR_FALSE);
		}
	    }
	}
	rv = NSS_GetClientAuthData(arg, socket, caNames, pRetCert, pRetKey);
	if (rv == SECSuccess && *pRetCert) {
	    char *nameString = CERT_NameToAscii(&((*pRetCert)->subject));
	    if (nameString) {
		fprintf(stderr, "sent cert: %s\n", nameString);
		PORT_Free(nameString);
	    }
	} else {
	    fprintf(stderr, "send no cert\n");
	}
	return rv;
    }
    return NSS_GetClientAuthData(arg, socket, caNames, pRetCert, pRetKey);
}

#if defined(WIN32) || defined(OS2)
void
thread_main(void * arg)
{
    PRFileDesc * ps     = (PRFileDesc *)arg;
    PRFileDesc * std_in = PR_GetSpecialFD(PR_StandardInput);
    int wc, rc;
    char buf[256];

#ifdef WIN32
    {
	/* Put stdin into O_BINARY mode 
	** or else incoming \r\n's will become \n's.
	*/
	int smrv = _setmode(_fileno(stdin), _O_BINARY);
	if (smrv == -1) {
	    fprintf(stderr,
	    "%s: Cannot change stdin to binary mode. Use -i option instead.\n",
	            progName);
	    /* plow ahead anyway */
	}
    }
#endif

    do {
	rc = PR_Read(std_in, buf, sizeof buf);
	if (rc <= 0)
	    break;
	wc = PR_Write(ps, buf, rc);
    } while (wc == rc);
    PR_Close(ps);
}

#endif

static void
printHostNameAndAddr(const char * host, const PRNetAddr * addr)
{
    PRUint16 port = PR_NetAddrInetPort(addr);
    char addrBuf[80];
    PRStatus st = PR_NetAddrToString(addr, addrBuf, sizeof addrBuf);

    if (st == PR_SUCCESS) {
	port = PR_ntohs(port);
	FPRINTF(stderr, "%s: connecting to %s:%hu (address=%s)\n",
	       progName, host, port, addrBuf);
    }
}

#define SSOCK_FD 0
#define STDIN_FD 1

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
    int                npds;
    int                override = 0;
    int                disableSSL2 = 0;
    int                disableSSL3 = 0;
    int                disableTLS  = 0;
    int                useExportPolicy = 0;
    PRSocketOptionData opt;
    PRNetAddr          addr;
    PRHostEnt          hp;
    PRPollDesc         pollset[2];
    PRBool             useCommandLinePassword = PR_FALSE;
    PRBool             pingServerFirst = PR_FALSE;
    PRBool             clientSpeaksFirst = PR_FALSE;
    int                error = 0;
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

          case 'f':  clientSpeaksFirst = PR_TRUE;       break;

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

    status = PR_StringToNetAddr(host, &addr);
    if (status == PR_SUCCESS) {
	int portno = atoi(port);
    	addr.inet.port = PR_htons((PRUint16)portno);
    } else {
	/* Lookup host */
	char buf[PR_NETDB_BUF_SIZE];
	status = PR_GetIPNodeByName(host, PR_AF_INET6, PR_AI_DEFAULT, 
				    buf, sizeof buf, &hp);
	if (status != PR_SUCCESS) {
	    SECU_PrintError(progName, "error looking up host");
	    return 1;
	}
	if (PR_EnumerateHostEnt(0, &hp, (PRUint16)atoi(port), &addr) == -1) {
	    SECU_PrintError(progName, "error looking up host address");
	    return 1;
	}
    }

    if (PR_IsNetAddrType(&addr, PR_IpAddrV4Mapped)) {
    	/* convert to IPv4.  */
	addr.inet.family = PR_AF_INET;
	memcpy(&addr.inet.ip, &addr.ipv6.ip.pr_s6_addr[12], 4);
	memset(&addr.inet.pad[0], 0, sizeof addr.inet.pad);
    }

    printHostNameAndAddr(host, &addr);

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
               if (NSS_Shutdown() != SECSuccess) {
                   exit(1);
               }
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
	    if (cipher > 0) {
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
    SSL_GetClientAuthDataHook(s, own_GetClientAuthData, (void *)nickname);
    SSL_HandshakeCallback(s, handshakeCallback, NULL);
    SSL_SetURL(s, host);

    /* Try to connect to the server */
    status = PR_Connect(s, &addr, PR_INTERVAL_NO_TIMEOUT);
    if (status != PR_SUCCESS) {
	if (PR_GetError() == PR_IN_PROGRESS_ERROR) {
	    if (verbose)
		SECU_PrintError(progName, "connect");
	    milliPause(50 * multiplier);
	    pollset[SSOCK_FD].in_flags = PR_POLL_WRITE | PR_POLL_EXCEPT;
	    pollset[SSOCK_FD].out_flags = 0;
	    pollset[SSOCK_FD].fd = s;
	    while(1) {
		FPRINTF(stderr, 
		        "%s: about to call PR_Poll for connect completion!\n", 
			progName);
		filesReady = PR_Poll(pollset, 1, PR_INTERVAL_NO_TIMEOUT);
		if (filesReady < 0) {
		    SECU_PrintError(progName, "unable to connect (poll)");
		    return 1;
		}
		FPRINTF(stderr,
		        "%s: PR_Poll returned 0x%02x for socket out_flags.\n",
			progName, pollset[SSOCK_FD].out_flags);
		if (filesReady == 0) {	/* shouldn't happen! */
		    FPRINTF(stderr, "%s: PR_Poll returned zero!\n", progName);
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

    pollset[SSOCK_FD].fd        = s;
    pollset[SSOCK_FD].in_flags  = clientSpeaksFirst ? 0 : PR_POLL_READ;
    pollset[STDIN_FD].fd        = PR_GetSpecialFD(PR_StandardInput);
    pollset[STDIN_FD].in_flags  = PR_POLL_READ;
    npds                 = 2;
    std_out              = PR_GetSpecialFD(PR_StandardOutput);

#if defined(WIN32) || defined(OS2)
    /* PR_Poll cannot be used with stdin on Windows or OS/2.  (sigh). 
    ** But use of PR_Poll and non-blocking sockets is a major feature
    ** of this program.  So, we simulate a pollable stdin with a 
    ** TCP socket pair and a  thread that reads stdin and writes to 
    ** that socket pair.
    */
  {
    PRFileDesc * fds[2];
    PRThread *   thread;

    int nspr_rv = PR_NewTCPSocketPair(fds);
    if (nspr_rv != PR_SUCCESS) {
        SECU_PrintError(progName, "PR_NewTCPSocketPair failed");
	error = 1;
	goto done;
    }
    pollset[STDIN_FD].fd = fds[1];

    thread = PR_CreateThread(PR_USER_THREAD, thread_main, fds[0], 
                             PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD, 
                             PR_UNJOINABLE_THREAD, 0);
    if (!thread) {
        SECU_PrintError(progName, "PR_CreateThread failed");
	error = 1;
	goto done;
    }
  }
#endif

    /*
    ** Select on stdin and on the socket. Write data from stdin to
    ** socket, read data from socket and write to stdout.
    */
    FPRINTF(stderr, "%s: ready...\n", progName);

    while (pollset[SSOCK_FD].in_flags | pollset[STDIN_FD].in_flags) {
	char buf[4000];	/* buffer for stdin */
	int nb;		/* num bytes read from stdin. */

	pollset[SSOCK_FD].out_flags = 0;
	pollset[STDIN_FD].out_flags = 0;

	FPRINTF(stderr, "%s: about to call PR_Poll !\n", progName);
	filesReady = PR_Poll(pollset, npds, PR_INTERVAL_NO_TIMEOUT);
	if (filesReady < 0) {
	    SECU_PrintError(progName, "select failed");
	    error = 1;
	    goto done;
	}
	if (filesReady == 0) {	/* shouldn't happen! */
	    FPRINTF(stderr, "%s: PR_Poll returned zero!\n", progName);
	    return 1;
	}
	FPRINTF(stderr, "%s: PR_Poll returned!\n", progName);
	if (pollset[STDIN_FD].in_flags) {
	    FPRINTF(stderr,
		    "%s: PR_Poll returned 0x%02x for stdin out_flags.\n",
		    progName, pollset[STDIN_FD].out_flags);
	}
	if (pollset[SSOCK_FD].in_flags) {
	    FPRINTF(stderr, 
	            "%s: PR_Poll returned 0x%02x for socket out_flags.\n",
		    progName, pollset[SSOCK_FD].out_flags);
	}
	if (pollset[STDIN_FD].out_flags & PR_POLL_READ) {
	    /* Read from stdin and write to socket */
	    nb = PR_Read(pollset[STDIN_FD].fd, buf, sizeof(buf));
	    FPRINTF(stderr, "%s: stdin read %d bytes\n", progName, nb);
	    if (nb < 0) {
		if (PR_GetError() != PR_WOULD_BLOCK_ERROR) {
		    SECU_PrintError(progName, "read from stdin failed");
	            error = 1;
		    break;
		}
	    } else if (nb == 0) {
		/* EOF on stdin, stop polling stdin for read. */
		pollset[STDIN_FD].in_flags = 0;
	    } else {
		char * bufp = buf;
		FPRINTF(stderr, "%s: Writing %d bytes to server\n", 
		        progName, nb);
		do {
		    PRInt32 cc = PR_Write(s, bufp, nb);
		    if (cc < 0) {
		    	PRErrorCode err = PR_GetError();
			if (err != PR_WOULD_BLOCK_ERROR) {
			    SECU_PrintError(progName, 
			                    "write to SSL socket failed");
			    error = 254;
			    goto done;
			}
			cc = 0;
		    }
		    bufp += cc;
		    nb   -= cc;
		    if (nb <= 0) 
		    	break;
		    pollset[SSOCK_FD].in_flags = PR_POLL_WRITE | PR_POLL_EXCEPT;
		    pollset[SSOCK_FD].out_flags = 0;
		    FPRINTF(stderr,
		            "%s: about to call PR_Poll on writable socket !\n", 
			    progName);
		    cc = PR_Poll(pollset, 1, PR_INTERVAL_NO_TIMEOUT);
		    FPRINTF(stderr,
		            "%s: PR_Poll returned with writable socket !\n", 
			    progName);
		} while (1);
		pollset[SSOCK_FD].in_flags  = PR_POLL_READ;
	    }
	}

	if (pollset[SSOCK_FD].in_flags) {
	    FPRINTF(stderr, 
	            "%s: PR_Poll returned 0x%02x for socket out_flags.\n",
		    progName, pollset[SSOCK_FD].out_flags);
	}
	if (   (pollset[SSOCK_FD].out_flags & PR_POLL_READ) 
	    || (pollset[SSOCK_FD].out_flags & PR_POLL_ERR)  
#ifdef PR_POLL_HUP
	    || (pollset[SSOCK_FD].out_flags & PR_POLL_HUP)
#endif
	    ) {
	    /* Read from socket and write to stdout */
	    nb = PR_Read(pollset[SSOCK_FD].fd, buf, sizeof(buf));
	    FPRINTF(stderr, "%s: Read from server %d bytes\n", progName, nb);
	    if (nb < 0) {
		if (PR_GetError() != PR_WOULD_BLOCK_ERROR) {
		    SECU_PrintError(progName, "read from socket failed");
		    error = 1;
		    goto done;
	    	}
	    } else if (nb == 0) {
		/* EOF from socket... stop polling socket for read */
		pollset[SSOCK_FD].in_flags = 0;
	    } else {
		PR_Write(std_out, buf, nb);
		if (verbose)
		    fputs("\n\n", stderr);
	    }
	}
	milliPause(50 * multiplier);
    }

  done:
    PR_Close(s);
    SSL_ClearSessionCache();
    if (NSS_Shutdown() != SECSuccess) {
        exit(1);
    }

    PR_Cleanup();
    return error;
}
