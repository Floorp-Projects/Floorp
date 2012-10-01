/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "seccomon.h"
#include "cert.h"
#include "secutil.h"
#include "nspr.h"
#include "nss.h"
#include "blapi.h"
#include "plgetopt.h"
#include "lowkeyi.h"
#include "pk11pub.h"


#define DEFAULT_ITERS           10
#define DEFAULT_DURATION        10
#define DEFAULT_KEY_BITS        1024
#define MIN_KEY_BITS            512
#define MAX_KEY_BITS            65536
#define BUFFER_BYTES            MAX_KEY_BITS / 8
#define DEFAULT_THREADS         1
#define DEFAULT_EXPONENT        0x10001

extern NSSLOWKEYPrivateKey * getDefaultRSAPrivateKey(void);
extern NSSLOWKEYPublicKey  * getDefaultRSAPublicKey(void);

secuPWData pwData = { PW_NONE, NULL };

typedef struct TimingContextStr TimingContext;

struct TimingContextStr {
    PRTime start;
    PRTime end;
    PRTime interval;

    long  days;     
    int   hours;    
    int   minutes;  
    int   seconds;  
    int   millisecs;
};

TimingContext *CreateTimingContext(void) {
    return PORT_Alloc(sizeof(TimingContext));
}

void DestroyTimingContext(TimingContext *ctx) {
    PORT_Free(ctx);
}

void TimingBegin(TimingContext *ctx, PRTime begin) {
    ctx->start = begin;
}

static void timingUpdate(TimingContext *ctx) {
    PRInt64 tmp, remaining;
    PRInt64 L1000,L60,L24;

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

void TimingEnd(TimingContext *ctx, PRTime end) {
    ctx->end = end;
    LL_SUB(ctx->interval, ctx->end, ctx->start);
    PORT_Assert(LL_GE_ZERO(ctx->interval));
    timingUpdate(ctx);
}

void TimingDivide(TimingContext *ctx, int divisor) {
    PRInt64 tmp;

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
    fprintf(stderr, "Usage: %s [-s | -e] [-i iterations | -p period] "
            "[-t threads]\n[-n none [-k keylength] [ [-g] -x exponent] |\n"
            " -n token:nickname [-d certdir] [-w password] |\n"
            " -h token [-d certdir] [-w password] [-g] [-k keylength] "
            "[-x exponent] [-f pwfile]\n",
	    progName);
    fprintf(stderr, "%-20s Cert database directory (default is ~/.netscape)\n",
	    "-d certdir");
    fprintf(stderr, "%-20s How many operations to perform\n", "-i iterations");
    fprintf(stderr, "%-20s How many seconds to run\n", "-p period");
    fprintf(stderr, "%-20s Perform signing (private key) operations\n", "-s");
    fprintf(stderr, "%-20s Perform encryption (public key) operations\n","-e");
    fprintf(stderr, "%-20s Nickname of certificate or key, prefixed "
            "by optional token name\n", "-n nickname");
    fprintf(stderr, "%-20s PKCS#11 token to perform operation with.\n",
            "-h token");
    fprintf(stderr, "%-20s key size in bits, from %d to %d\n", "-k keylength",
            MIN_KEY_BITS, MAX_KEY_BITS);
    fprintf(stderr, "%-20s token password\n", "-w password");
    fprintf(stderr, "%-20s temporary key generation. Not for token keys.\n",
            "-g");
    fprintf(stderr, "%-20s set public exponent for keygen\n", "-x");
    fprintf(stderr, "%-20s Number of execution threads (default 1)\n",
            "-t threads");
    exit(-1);
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

typedef SECStatus (* RSAOp)(void *               key, 
			    unsigned char *      output,
		            unsigned char *      input);

typedef struct {
    SECKEYPublicKey* pubKey;
    SECKEYPrivateKey* privKey;
} PK11Keys;


SECStatus PK11_PublicKeyOp (SECKEYPublicKey*     key,
			    unsigned char *      output,
		            unsigned char *      input)
{
    return PK11_PubEncryptRaw(key, output, input, key->u.rsa.modulus.len,
                              NULL);
}

SECStatus PK11_PrivateKeyOp (PK11Keys*            keys,
			     unsigned char *      output,
		             unsigned char *      input)
{
    unsigned outLen = 0;
    return PK11_PrivDecryptRaw(keys->privKey,
                               output, &outLen,
                               keys->pubKey->u.rsa.modulus.len, input,
                               keys->pubKey->u.rsa.modulus.len);
}
typedef struct ThreadRunDataStr ThreadRunData;

struct ThreadRunDataStr {
    const PRBool        *doIters;
    const void          *rsaKey;
    const unsigned char *buf;
    RSAOp                fn;
    int                  seconds;
    long                 iters;
    long                 iterRes;
    PRErrorCode          errNum;
    SECStatus            status;
};


void ThreadExecFunction(void *data)
{
    ThreadRunData *tdata = (ThreadRunData*)data;
    unsigned char buf2[BUFFER_BYTES];

    tdata->status = SECSuccess;
    if (*tdata->doIters) {
        long i = tdata->iters;
        tdata->iterRes = 0;
        while (i--) {
            SECStatus rv = tdata->fn((void*)tdata->rsaKey, buf2,
                                     (unsigned char*)tdata->buf);
            if (rv != SECSuccess) {
                tdata->errNum = PORT_GetError();
                tdata->status = rv;
                break;
            }
            tdata->iterRes++;
        }
    } else {
        PRIntervalTime total = PR_SecondsToInterval(tdata->seconds);
        PRIntervalTime start = PR_IntervalNow();
        tdata->iterRes = 0;
        while (PR_IntervalNow() - start < total) {
            SECStatus rv = tdata->fn((void*)tdata->rsaKey, buf2,
                                     (unsigned char*)tdata->buf);
            if (rv != SECSuccess) {
                tdata->errNum = PORT_GetError();
                tdata->status = rv;
                break;
            }
            tdata->iterRes++;
        }
    }
}

#define INT_ARG(arg,def) atol(arg)>0?atol(arg):def

int
main(int argc, char **argv)
{
    TimingContext *	  timeCtx       = NULL;
    SECKEYPublicKey *	  pubHighKey    = NULL;
    SECKEYPrivateKey *    privHighKey   = NULL;
    NSSLOWKEYPrivateKey * privKey       = NULL;
    NSSLOWKEYPublicKey *  pubKey        = NULL;
    CERTCertificate *	  cert          = NULL;
    char *		  progName      = NULL;
    char *		  secDir 	= NULL;
    char *		  nickname 	= NULL;
    char *                slotname      = NULL;
    long                  keybits     = 0;
    RSAOp                 fn;
    void *                rsaKey        = NULL;
    PLOptState *          optstate;
    PLOptStatus           optstatus;
    long 		  iters 	= DEFAULT_ITERS;
    int		 	  i;
    PRBool 		  doPriv 	= PR_FALSE;
    PRBool 		  doPub 	= PR_FALSE;
    int 		  rv;
    unsigned char 	  buf[BUFFER_BYTES];
    unsigned char 	  buf2[BUFFER_BYTES];
    int                   seconds       = DEFAULT_DURATION;
    PRBool                doIters       = PR_FALSE;
    PRBool                doTime        = PR_FALSE;
    PRBool                useTokenKey   = PR_FALSE; /* use PKCS#11 token
                                                       object key */
    PRBool                useSessionKey = PR_FALSE; /* use PKCS#11 session
                                                       object key */
    PRBool                useBLKey = PR_FALSE;      /* use freebl */
    PK11SlotInfo*         slot          = NULL;     /* slot for session
                                                       object key operations */
    PRBool                doKeyGen      = PR_FALSE;
    int                   publicExponent  = DEFAULT_EXPONENT;
    PK11Keys keys;
    int peCount = 0;
    CK_BYTE pubEx[4];
    SECItem pe;
    RSAPublicKey          pubKeyStr;
    int                   threadNum     = DEFAULT_THREADS;
    ThreadRunData      ** runDataArr = NULL;
    PRThread           ** threadsArr = NULL;
    int                   calcThreads = 0;

    progName = strrchr(argv[0], '/');
    if (!progName)
	progName = strrchr(argv[0], '\\');
    progName = progName ? progName+1 : argv[0];

    optstate = PL_CreateOptState(argc, argv, "d:ef:gh:i:k:n:p:st:w:x:");
    while ((optstatus = PL_GetNextOpt(optstate)) == PL_OPT_OK) {
	switch (optstate->option) {
	case '?':
	    Usage(progName);
	    break;
	case 'd':
	    secDir = PORT_Strdup(optstate->value);
	    break;
	case 'i':
	    iters = INT_ARG(optstate->value, DEFAULT_ITERS);
	    doIters = PR_TRUE;
	    break;
	case 's':
	    doPriv = PR_TRUE;
	    break;
	case 'e':
	    doPub = PR_TRUE;
	    break;
	case 'g':
	    doKeyGen = PR_TRUE;
	    break;
	case 'n':
	    nickname = PORT_Strdup(optstate->value);
            /* for compatibility, nickname of "none" means go to freebl */
            if (nickname && strcmp(nickname, "none")) {
	        useTokenKey = PR_TRUE;
            } else {
                useBLKey = PR_TRUE;
            }
	    break;
	case 'p':
	    seconds = INT_ARG(optstate->value, DEFAULT_DURATION);
	    doTime = PR_TRUE;
            break;
	case 'h':
	    slotname = PORT_Strdup(optstate->value);
	    useSessionKey = PR_TRUE;
	    break;
	case 'k':
	    keybits = INT_ARG(optstate->value, DEFAULT_KEY_BITS);
	    break;
	case 'w':
	    pwData.data = PORT_Strdup(optstate->value);;
	    pwData.source = PW_PLAINTEXT;
	    break;
        case 'f':
            pwData.data = PORT_Strdup(optstate->value);
            pwData.source = PW_FROMFILE;
            break;
	case 'x':
	    /*  -x public exponent (for RSA keygen)  */
	    publicExponent = INT_ARG(optstate->value, DEFAULT_EXPONENT);
	    break;
	case 't':
	    threadNum = INT_ARG(optstate->value, DEFAULT_THREADS);
	    break;
	}
    }
    if (optstatus == PL_OPT_BAD)
	Usage(progName);

    if ((doPriv && doPub) || (doIters && doTime) ||
        ((useTokenKey + useSessionKey + useBLKey) != PR_TRUE) ||
        (useTokenKey && keybits) || (useTokenKey && doKeyGen) ||
        (keybits && (keybits<MIN_KEY_BITS || keybits>MAX_KEY_BITS))) {
        Usage(progName);
    }

    if (!doPriv && !doPub) doPriv = PR_TRUE;

    if (doIters && doTime) Usage(progName);

    if (!doTime) {
        doIters = PR_TRUE;
    }

    PR_Init( PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);

    PK11_SetPasswordFunc(SECU_GetModulePassword);
    secDir = SECU_ConfigDirectory(secDir);

    if (useTokenKey || useSessionKey) {
	rv = NSS_Init(secDir);
	if (rv != SECSuccess) {
	    fprintf(stderr, "NSS_Init failed.\n");
	    exit(1);
	}
    } else {
	rv = NSS_NoDB_Init(NULL);
	if (rv != SECSuccess) {
	    fprintf(stderr, "NSS_NoDB_Init failed.\n");
	    exit(1);
	}
    }

    if (useTokenKey) {
        CK_OBJECT_HANDLE kh = CK_INVALID_HANDLE;
        CERTCertDBHandle* certdb = NULL;
	certdb = CERT_GetDefaultCertDB();
        
        cert = PK11_FindCertFromNickname(nickname, &pwData);
        if (cert == NULL) {
            fprintf(stderr,
                    "Can't find certificate by name \"%s\"\n", nickname);
            exit(1);
        }
        pubHighKey = CERT_ExtractPublicKey(cert);
        if (pubHighKey == NULL) {
            fprintf(stderr, "Can't extract public key from certificate");
            exit(1);
        }

        if (doPub) {
            /* do public key ops */
            fn = (RSAOp)PK11_PublicKeyOp;
            rsaKey = (void *) pubHighKey;
                
            kh = PK11_ImportPublicKey(cert->slot, pubHighKey, PR_FALSE);
            if (CK_INVALID_HANDLE == kh) {
                fprintf(stderr,
                        "Unable to import public key to certificate slot.");
                exit(1);
            }
            pubHighKey->pkcs11Slot = PK11_ReferenceSlot(cert->slot);
            pubHighKey->pkcs11ID = kh;
            printf("Using PKCS#11 for RSA encryption with token %s.\n",
                   PK11_GetTokenName(cert->slot));
        } else {
            /* do private key ops */
            privHighKey = PK11_FindKeyByAnyCert(cert, &pwData);
            if (privHighKey == NULL) {
                fprintf(stderr,
                        "Can't find private key by name \"%s\"\n", nickname);
                exit(1);
            }
    
            SECKEY_CacheStaticFlags(privHighKey);
            fn = (RSAOp)PK11_PrivateKeyOp;
            keys.privKey = privHighKey;
            keys.pubKey = pubHighKey;
            rsaKey = (void *) &keys;
            printf("Using PKCS#11 for RSA decryption with token %s.\n",
                   PK11_GetTokenName(privHighKey->pkcs11Slot));
        }        
    } else

    if (useSessionKey) {
        /* use PKCS#11 session key objects */
        PK11RSAGenParams   rsaparams;
        void             * params;

        slot = PK11_FindSlotByName(slotname); /* locate target slot */
        if (!slot) {
            fprintf(stderr, "Can't find slot \"%s\"\n", slotname);
            exit(1);
        }

        doKeyGen = PR_TRUE; /* Always do a keygen for session keys.
                               Import of hardcoded key is not supported */
        /* do a temporary keygen in selected slot */        
        if (!keybits) {
            keybits = DEFAULT_KEY_BITS;
        }

        printf("Using PKCS#11 with %ld bits session key in token %s.\n",
               keybits, PK11_GetTokenName(slot));

        rsaparams.keySizeInBits = keybits;
        rsaparams.pe = publicExponent;
        params = &rsaparams;

        fprintf(stderr,"\nGenerating RSA key. This may take a few moments.\n");

        privHighKey = PK11_GenerateKeyPair(slot, CKM_RSA_PKCS_KEY_PAIR_GEN,
                                           params, &pubHighKey, PR_FALSE,
                                           PR_FALSE, (void*)&pwData);
        if (!privHighKey) {
            fprintf(stderr,
                    "Key generation failed in token \"%s\"\n",
                    PK11_GetTokenName(slot));
            exit(1);
        }

        SECKEY_CacheStaticFlags(privHighKey);
        
        fprintf(stderr,"Keygen completed.\n");

        if (doPub) {
            /* do public key operations */
            fn = (RSAOp)PK11_PublicKeyOp;
            rsaKey = (void *) pubHighKey;
        } else {
            /* do private key operations */
            fn = (RSAOp)PK11_PrivateKeyOp;
            keys.privKey = privHighKey;
            keys.pubKey = pubHighKey;
            rsaKey = (void *) &keys;
        }        
    } else
        
    {
        /* use freebl directly */
        if (!keybits) {
            keybits = DEFAULT_KEY_BITS;
        }
        if (!doKeyGen) {
            if (keybits != DEFAULT_KEY_BITS) {
                doKeyGen = PR_TRUE;
            }
        }
        printf("Using freebl with %ld bits key.\n", keybits);
        if (doKeyGen) {
            fprintf(stderr,"\nGenerating RSA key. "
                    "This may take a few moments.\n");
            for (i=0; i < 4; i++) {
                if (peCount || (publicExponent & ((unsigned long)0xff000000L >>
                                                  (i*8)))) {
                    pubEx[peCount] =  (CK_BYTE)((publicExponent >>
                                                 (3-i)*8) & 0xff);
                    peCount++;
                }
            }
            pe.len = peCount;
            pe.data = &pubEx[0];
            pe.type = siBuffer;

            rsaKey = RSA_NewKey(keybits, &pe);
            fprintf(stderr,"Keygen completed.\n");
        } else {
            /* use a hardcoded key */
            printf("Using hardcoded %ld bits key.\n", keybits);
            if (doPub) {
                pubKey = getDefaultRSAPublicKey();
            } else {
                privKey = getDefaultRSAPrivateKey();
            }
        }

        if (doPub) {
            /* do public key operations */
            fn = (RSAOp)RSA_PublicKeyOp;
            if (rsaKey) {
                /* convert the RSAPrivateKey to RSAPublicKey */
                pubKeyStr.arena = NULL;
                pubKeyStr.modulus = ((RSAPrivateKey*)rsaKey)->modulus;
                pubKeyStr.publicExponent =
                    ((RSAPrivateKey*)rsaKey)->publicExponent;
                rsaKey = &pubKeyStr;
            } else {
                /* convert NSSLOWKeyPublicKey to RSAPublicKey */
                rsaKey = (void *)(&pubKey->u.rsa);
            }
            PORT_Assert(rsaKey);
        } else {
            /* do private key operations */
            fn = (RSAOp)RSA_PrivateKeyOp;
            if (privKey) {
                /* convert NSSLOWKeyPrivateKey to RSAPrivateKey */
                rsaKey = (void *)(&privKey->u.rsa);
            }
            PORT_Assert(rsaKey);
        }
    }

    memset(buf, 1, sizeof buf);
    rv = fn(rsaKey, buf2, buf);
    if (rv != SECSuccess) {
	PRErrorCode errNum;
	const char * errStr = NULL;

	errNum = PORT_GetError();
	if (errNum)
	    errStr = SECU_Strerror(errNum);
	else
	    errNum = rv;
	if (!errStr)
	    errStr = "(null)";
	fprintf(stderr, "Error in RSA operation: %d : %s\n", errNum, errStr);
	exit(1);
    }

    threadsArr = (PRThread**)PORT_Alloc(threadNum*sizeof(PRThread*));
    runDataArr = (ThreadRunData**)PORT_Alloc(threadNum*sizeof(ThreadRunData*));
    timeCtx = CreateTimingContext();
    TimingBegin(timeCtx, PR_Now());
    for (i = 0;i < threadNum;i++) {
        runDataArr[i] = (ThreadRunData*)PORT_Alloc(sizeof(ThreadRunData));
        runDataArr[i]->fn = fn;
        runDataArr[i]->buf = buf;
        runDataArr[i]->doIters = &doIters;
        runDataArr[i]->rsaKey = rsaKey;
        runDataArr[i]->seconds = seconds;
        runDataArr[i]->iters = iters;
        threadsArr[i] = 
            PR_CreateThread(PR_USER_THREAD,
                 ThreadExecFunction,
                 (void*) runDataArr[i],
                 PR_PRIORITY_NORMAL,
                 PR_GLOBAL_THREAD,
                 PR_JOINABLE_THREAD,
                 0);
    }
    iters = 0;
    calcThreads = 0;
    for (i = 0;i < threadNum;i++, calcThreads++)
    {
        PR_JoinThread(threadsArr[i]);
        if (runDataArr[i]->status != SECSuccess) {
            const char * errStr = SECU_Strerror(runDataArr[i]->errNum);
            fprintf(stderr, "Thread %d: Error in RSA operation: %d : %s\n",
                    i, runDataArr[i]->errNum, errStr);
            calcThreads -= 1;
        } else {
            iters += runDataArr[i]->iterRes;
        }
        PORT_Free((void*)runDataArr[i]);
    }
    PORT_Free(runDataArr);
    PORT_Free(threadsArr);

    TimingEnd(timeCtx, PR_Now());
    
    printf("%ld iterations in %s\n",
	   iters, TimingGenerateString(timeCtx));
    printf("%.2f operations/s .\n", ((double)(iters)*(double)1000000.0) /
           (double)timeCtx->interval );
    TimingDivide(timeCtx, iters);
    printf("one operation every %s\n", TimingGenerateString(timeCtx));

    if (pubHighKey) {
        SECKEY_DestroyPublicKey(pubHighKey);
    }

    if (privHighKey) {
         SECKEY_DestroyPrivateKey(privHighKey);
    }

    if (cert) {
        CERT_DestroyCertificate(cert);
    }

    if (NSS_Shutdown() != SECSuccess) {
        exit(1);
    }

    return 0;
}
