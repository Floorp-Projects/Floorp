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
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
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

#include "seccomon.h"
#include "cert.h"
#include "secutil.h"
#include "nspr.h"
#include "nss.h"
#include "blapi.h"
#include "plgetopt.h"
#include "lowkeyi.h"


#define MAX_RSA_MODULUS_BYTES (1024/8)
#define DEFAULT_ITERS 10

extern NSSLOWKEYPrivateKey * getDefaultRSAPrivateKey(void);
extern NSSLOWKEYPublicKey  * getDefaultRSAPublicKey(void);

typedef struct TimingContextStr TimingContext;

struct TimingContextStr {
    int64 start;
    int64 end;
    int64 interval;

    long  days;     
    int   hours;    
    int   minutes;  
    int   seconds;  
    int   millisecs;
};

TimingContext *CreateTimingContext(void) {
    return PR_Malloc(sizeof(TimingContext));
}

void DestroyTimingContext(TimingContext *ctx) {
    PR_Free(ctx);
}

void TimingBegin(TimingContext *ctx) {
    ctx->start = PR_Now();
}

static void timingUpdate(TimingContext *ctx) {
    int64 tmp, remaining;
    int64 L1000,L60,L24;

    LL_I2L(L1000,1000);
    LL_I2L(L60,60);
    LL_I2L(L24,24);

    LL_DIV(remaining, ctx->interval, L1000);
    LL_MOD(tmp, remaining, L1000);
    LL_L2I(ctx->millisecs, tmp);
    LL_DIV(remaining, remaining, L1000);
    LL_MOD(tmp, remaining, L60);
    LL_L2I(ctx->seconds, tmp);
    LL_DIV(remaining, remaining, L60);
    LL_MOD(tmp, remaining, L60);
    LL_L2I(ctx->minutes, tmp);
    LL_DIV(remaining, remaining, L60);
    LL_MOD(tmp, remaining, L24);
    LL_L2I(ctx->hours, tmp);
    LL_DIV(remaining, remaining, L24);
    LL_L2I(ctx->days, remaining);
}

void TimingEnd(TimingContext *ctx) {
    ctx->end = PR_Now();
    LL_SUB(ctx->interval, ctx->end, ctx->start);
    PORT_Assert(LL_GE_ZERO(ctx->interval));
    timingUpdate(ctx);
}

void TimingDivide(TimingContext *ctx, int divisor) {
    int64 tmp;

    LL_I2L(tmp, divisor);
    LL_DIV(ctx->interval, ctx->interval, tmp);

    timingUpdate(ctx);
}

char *TimingGenerateString(TimingContext *ctx) {
    char *buf = NULL;

    if (ctx->days != 0) {
	buf = PR_sprintf_append(buf, "%d days", ctx->days);
    }
    if (ctx->hours != 0) {
	if (buf != NULL) buf = PR_sprintf_append(buf, ", ");
	buf = PR_sprintf_append(buf, "%d hours", ctx->hours);
    }
    if (ctx->minutes != 0) {
	if (buf != NULL) buf = PR_sprintf_append(buf, ", ");
	buf = PR_sprintf_append(buf, "%d minutes", ctx->minutes);
    }
    if (buf != NULL) buf = PR_sprintf_append(buf, ", and ");
    if (!buf && ctx->seconds == 0) {
	int interval;
	LL_L2I(interval, ctx->interval);
    	if (ctx->millisecs < 100) 
	    buf = PR_sprintf_append(buf, "%d microseconds", interval);
	else
	    buf = PR_sprintf_append(buf, "%d milliseconds", ctx->millisecs);
    } else if (ctx->millisecs == 0) {
	buf = PR_sprintf_append(buf, "%d seconds", ctx->seconds);
    } else {
	buf = PR_sprintf_append(buf, "%d.%03d seconds",
				ctx->seconds, ctx->millisecs);
    }
    return buf;
}

void
Usage(char *progName)
{
    fprintf(stderr, "Usage: %s [-d certdir] [-i iterations] [-s | -e]"
	            " -n nickname\n",
	    progName);
    fprintf(stderr, "%-20s Cert database directory (default is ~/.netscape)\n",
	    "-d certdir");
    fprintf(stderr, "%-20s How many operations to perform\n", "-i iterations");
    fprintf(stderr, "%-20s Perform signing (private key) operations\n", "-s");
    fprintf(stderr, "%-20s Perform encryption (public key) operations\n", "-e");
    fprintf(stderr, "%-20s Nickname of certificate or key\n", "-n nickname");
    exit(-1);
}

void
printCount(int iterations)
{

}


static void
dumpBytes( unsigned char * b, int l)
{
    int i;
    if (l <= 0)
        return;
    for (i = 0; i < l; ++i) {
        if (i % 16 == 0)
            printf("\t");
        printf(" %02x", b[i]);
        if (i % 16 == 15)
            printf("\n");
    }
    if ((i % 16) != 0)
        printf("\n");
}

static void
dumpItem( SECItem * item, const char * description)
{
    if (item->len & 1 && item->data[0] == 0) {
	printf("%s: (%d bytes)\n", description, item->len - 1);
	dumpBytes(item->data + 1, item->len - 1);
    } else {
	printf("%s: (%d bytes)\n", description, item->len);
	dumpBytes(item->data, item->len);
    }
}

void
printPrivKey(NSSLOWKEYPrivateKey * privKey)
{
    RSAPrivateKey *rsa = &privKey->u.rsa;

    dumpItem( &rsa->modulus, 		"n");
    dumpItem( &rsa->publicExponent, 	"e");
    dumpItem( &rsa->privateExponent, 	"d");
    dumpItem( &rsa->prime1, 		"P");
    dumpItem( &rsa->prime2, 		"Q");
    dumpItem( &rsa->exponent1, 	        "d % (P-1)");
    dumpItem( &rsa->exponent2, 	        "d % (Q-1)");
    dumpItem( &rsa->coefficient, 	"(Q ** -1) % P");
    puts("");
}

void
printFnCounts(long doPrint)
{

}

typedef SECStatus (* RSAOp)(void *               key, 
			    unsigned char *      output,
		            unsigned char *      input);


int
main(int argc, char **argv)
{
    TimingContext *	  timeCtx;
    SECKEYPublicKey *	  pubHighKey;
    NSSLOWKEYPrivateKey * privKey;
    NSSLOWKEYPublicKey *  pubKey;
    CERTCertificate *	  cert;
    char *		  progName;
    char *		  secDir 	= NULL;
    char *		  nickname 	= NULL;
    RSAOp                 fn;
    void *                rsaKey;
    PLOptState *          optstate;
    PLOptStatus           optstatus;
    long 		  iters 	= DEFAULT_ITERS;
    int		 	  i;
    PRBool 		  doPriv 	= PR_FALSE;
    PRBool 		  doPub 	= PR_FALSE;
    int 		  rv;
    CERTCertDBHandle 	* certdb;
    unsigned char 	  buf[1024];
    unsigned char 	  buf2[1024];

    progName = strrchr(argv[0], '/');
    if (!progName)
	progName = strrchr(argv[0], '\\');
    progName = progName ? progName+1 : argv[0];

    optstate = PL_CreateOptState(argc, argv, "d:i:sen:");
    while ((optstatus = PL_GetNextOpt(optstate)) == PL_OPT_OK) {
	switch (optstate->option) {
	case '?':
	    Usage(progName);
	    break;
	case 'd':
	    secDir = PORT_Strdup(optstate->value);
	    break;
	case 'i':
	    iters = (atol(optstate->value)>0?atol(optstate->value):iters);
	    break;
	case 's':
	    doPriv = PR_TRUE;
	    break;
	case 'e':
	    doPub = PR_TRUE;
	    break;
	case 'n':
	    nickname = PORT_Strdup(optstate->value);
	    break;
	}
    }
    if (optstatus == PL_OPT_BAD)
	Usage(progName);

    if ((doPriv && doPub) || (nickname == NULL)) Usage(progName);

    if (!doPriv && !doPub) doPriv = PR_TRUE;

    PR_Init( PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);

    secDir = SECU_ConfigDirectory(secDir);
    if (strcmp(nickname, "none")) {
	rv = NSS_Init(secDir);
	if (rv != SECSuccess) {
	    fprintf(stderr, "NSS_Init failed.\n");
	    exit(1);
	}
	certdb = CERT_GetDefaultCertDB();
    } else {
	rv = NSS_NoDB_Init(secDir);
	if (rv != SECSuccess) {
	    fprintf(stderr, "NSS_NoDB_Init failed.\n");
	    exit(1);
	}
    }
#if defined(SECU_GetPassword)
    if (doPub) {
	if (!strcmp(nickname, "none")) {
	    pubKey = getDefaultRSAPublicKey();
	} else {
	    cert = CERT_FindCertByNickname(certdb, nickname);
	    if (cert == NULL) {
		fprintf(stderr,
			"Can't find certificate by name \"%s\"\n", nickname);
		exit(1);
	    }
	    pubHighKey = CERT_ExtractPublicKey(cert);
	    pubKey = SECU_ConvHighToLow(pubHighKey);
	}
	if (pubKey == NULL) {
	    fprintf(stderr, "Can't extract public key from certificate");
	    exit(1);
	}
	fn = (RSAOp)RSA_PublicKeyOp;
	rsaKey = (void *)(&pubKey->u.rsa);
    }

    if (doPriv) {

	if (!strcmp(nickname, "none")) {
	    privKey = getDefaultRSAPrivateKey();
	} else {
	    cert = CERT_FindCertByNickname(certdb, nickname);
	    if (cert == NULL) {
		fprintf(stderr,
			"Can't find certificate by name \"%s\"\n", nickname);
		exit(1);
	    }

	    privKey = SECKEY_FindKeyByCert(keydb, cert, SECU_GetPassword, NULL);
	}
	if (privKey == NULL) {
	    fprintf(stderr,
		    "Can't find private key by name \"%s\"\n", nickname);
	    exit(1);
	}

	fn = (RSAOp)RSA_PrivateKeyOp;
	rsaKey = (void *)(&privKey->u.rsa);
    }
#else
    if (doPub) {
	pubKey = getDefaultRSAPublicKey();
	fn = (RSAOp)RSA_PublicKeyOp;
	rsaKey = (void *)(&pubKey->u.rsa);
    }
    if (doPriv) {
	privKey = getDefaultRSAPrivateKey();
	fn = (RSAOp)RSA_PrivateKeyOp;
	rsaKey = (void *)(&privKey->u.rsa);
    }
#endif

    memset(buf, 1, sizeof buf);
    rv = fn(rsaKey, buf2, buf);
    if (rv != SECSuccess) {
	PRErrorCode errNum;
	const char * errStr = NULL;

	errNum = PR_GetError();
	if (errNum)
	    errStr = SECU_Strerror(errNum);
	else
	    errNum = rv;
	if (!errStr)
	    errStr = "(null)";
	fprintf(stderr, "Error in RSA operation: %d : %s\n", errNum, errStr);
	exit(1);
    }

/*  printf("START\n");	*/

    timeCtx = CreateTimingContext();
    TimingBegin(timeCtx);
    i = iters;
    while (i--) {
	rv = fn(rsaKey, buf2, buf);
	if (rv != SECSuccess) {
	    PRErrorCode errNum = PR_GetError();
	    const char * errStr = SECU_Strerror(errNum);
	    fprintf(stderr, "Error in RSA operation: %d : %s\n", 
		    errNum, errStr);
	    exit(1);
	}
    }
    TimingEnd(timeCtx);
    printf("%ld iterations in %s\n",
	   iters, TimingGenerateString(timeCtx));
    TimingDivide(timeCtx, iters);
    printf("one operation every %s\n", TimingGenerateString(timeCtx));

    if (NSS_Shutdown() != SECSuccess) {
        exit(1);
    }

    return 0;
}
