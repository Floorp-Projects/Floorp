#if defined(XP_UNIX)
#include <unistd.h>
#endif
#include <stdio.h>
#include <nss.h>
#include <prtypes.h>
#include <prerr.h>
#include <prerror.h>
#include <prthread.h>
#include <pk11pub.h>
#include <keyhi.h>

#define MAX_THREAD_COUNT 100

/* globals */
int THREAD_COUNT = 30;
int FAILED = 0;
int ERROR = 0;
int LOOP_COUNT = 100;
int KEY_SIZE = 3072;
int STACK_SIZE = 0;
int VERBOSE = 0;
char *NSSDIR = ".";
PRBool ISTOKEN = PR_TRUE;
CK_MECHANISM_TYPE MECHANISM = CKM_RSA_PKCS_KEY_PAIR_GEN;

void
usage(char *prog, char *error)
{
    if (error) {
        fprintf(stderr, "Bad Arguments: %s", error);
    }
    fprintf(stderr, "usage: %s [-l loop_count] [-t thread_count] "
                    "[-k key_size] [-s stack_size] [-d nss_dir] [-e] [-v] [-h]\n",
            prog);
    fprintf(stderr, "    loop_count   -- "
                    "number of keys to generate on each thread (default=%d)\n",
            LOOP_COUNT);
    fprintf(stderr, "    thread_count -- "
                    "number of of concurrent threads to run (def=%d,max=%d)\n",
            THREAD_COUNT, MAX_THREAD_COUNT);
    fprintf(stderr, "    key_size     -- "
                    "rsa key size in bits (default=%d)\n",
            KEY_SIZE);
    fprintf(stderr, "    stack_size     -- "
                    "thread stack size in bytes, 0=optimal (default=%d)\n",
            STACK_SIZE);
    fprintf(stderr, "    nss_dir     -- "
                    "location of the nss directory (default=%s)\n",
            NSSDIR);
    fprintf(stderr, "    -e use session keys rather than token keys\n");
    fprintf(stderr, "    -v verbose, print progress indicators\n");
    fprintf(stderr, "    -h print this message\n");
    exit(2);
}

void
create_key_loop(void *arg)
{
    int i;
    PK11SlotInfo *slot = PK11_GetInternalKeySlot();
    PK11RSAGenParams param;
    int threadnumber = *(int *)arg;
    int failures = 0;
    int progress = 5;
    PRIntervalTime epoch = PR_IntervalNow();
    param.keySizeInBits = KEY_SIZE;
    param.pe = 0x10001L;
    printf(" - thread %d starting\n", threadnumber);
    progress = 30 / THREAD_COUNT;
    if (progress < 2)
        progress = 2;
    for (i = 0; i < LOOP_COUNT; i++) {
        SECKEYPrivateKey *privKey;
        SECKEYPublicKey *pubKey;
        privKey = PK11_GenerateKeyPair(slot, MECHANISM, &param, &pubKey,
                                       ISTOKEN, PR_TRUE, NULL);
        if (privKey == NULL) {
            fprintf(stderr,
                    "keypair gen in thread %d failed %s\n", threadnumber,
                    PORT_ErrorToString(PORT_GetError()));
            FAILED++;
            failures++;
        }
        if (VERBOSE && (i % progress) == 0) {
            PRIntervalTime current = PR_IntervalNow();
            PRIntervalTime interval = current - epoch;
            int seconds = (interval / PR_TicksPerSecond());
            int mseconds = ((interval * 1000) / PR_TicksPerSecond()) - (seconds * 1000);
            epoch = current;
            printf(" - thread %d @ %d iterations %d.%03d sec\n", threadnumber,
                   i, seconds, mseconds);
        }
        if (ISTOKEN && privKey) {
            SECKEY_DestroyPublicKey(pubKey);
            SECKEY_DestroyPrivateKey(privKey);
        }
    }
    PK11_FreeSlot(slot);
    printf(" * thread %d ending with %d failures\n", threadnumber, failures);
    return;
}

int
main(int argc, char **argv)
{
    PRThread *thread[MAX_THREAD_COUNT];
    int threadnumber[MAX_THREAD_COUNT];
    int i;
    PRStatus status;
    SECStatus rv;
    char *prog = *argv++;
    char buf[2048];
    char *arg;

    while ((arg = *argv++) != NULL) {
        if (*arg == '-') {
            switch (arg[1]) {
                case 'l':
                    if (*argv == NULL)
                        usage(prog, "missing loop count");
                    LOOP_COUNT = atoi(*argv++);
                    break;
                case 'k':
                    if (*argv == NULL)
                        usage(prog, "missing key size");
                    KEY_SIZE = atoi(*argv++);
                    break;
                case 's':
                    if (*argv == NULL)
                        usage(prog, "missing stack size");
                    STACK_SIZE = atoi(*argv++);
                    break;
                case 't':
                    if (*argv == NULL)
                        usage(prog, "missing thread count");
                    THREAD_COUNT = atoi(*argv++);
                    if (THREAD_COUNT > MAX_THREAD_COUNT) {
                        usage(prog, "max thread count exceeded");
                    }
                    break;
                case 'v':
                    VERBOSE = 1;
                    break;
                case 'd':
                    if (*argv == NULL)
                        usage(prog, "missing directory");
                    NSSDIR = *argv++;
                    break;
                case 'e':
                    ISTOKEN = PR_FALSE;
                    break;
                case 'h':
                    usage(prog, NULL);
                    break;
                default:
                    sprintf(buf, "unknown option %c", arg[1]);
                    usage(prog, buf);
            }
        } else {
            sprintf(buf, "unknown argument %s", arg);
            usage(prog, buf);
        }
    }
    /* initialize NSS */
    rv = NSS_InitReadWrite(NSSDIR);
    if (rv != SECSuccess) {
        fprintf(stderr,
                "NSS_InitReadWrite(%s) failed(%s)\n", NSSDIR,
                PORT_ErrorToString(PORT_GetError()));
        exit(2);
    }

    /* need to initialize the database here if it's not already */

    printf("creating %d threads\n", THREAD_COUNT);
    for (i = 0; i < THREAD_COUNT; i++) {
        threadnumber[i] = i;
        thread[i] = PR_CreateThread(PR_USER_THREAD, create_key_loop,
                                    &threadnumber[i], PR_PRIORITY_NORMAL,
                                    PR_GLOBAL_THREAD,
                                    PR_JOINABLE_THREAD, STACK_SIZE);
        if (thread[i] == NULL) {
            ERROR++;
            fprintf(stderr,
                    "PR_CreateThread failed iteration %d, %s\n", i,
                    PORT_ErrorToString(PORT_GetError()));
        }
    }
    printf("waiting on %d threads\n", THREAD_COUNT);
    for (i = 0; i < THREAD_COUNT; i++) {
        if (thread[i] == NULL) {
            continue;
        }
        status = PR_JoinThread(thread[i]);
        if (status != PR_SUCCESS) {
            ERROR++;
            fprintf(stderr,
                    "PR_CreateThread filed iteration %d, %s\n", i,
                    PORT_ErrorToString(PORT_GetError()));
        }
    }
    if (NSS_Shutdown() != SECSuccess) {
        ERROR++;
        fprintf(stderr, "NSS_Shutdown failed: %s\n",
                PORT_ErrorToString(PORT_GetError()));
    }
    printf("%d failures and %d errors found\n", FAILED, ERROR);
    /* clean up */
    if (FAILED) {
        exit(1);
    }
    if (ERROR) {
        exit(2);
    }
    exit(0);
}
