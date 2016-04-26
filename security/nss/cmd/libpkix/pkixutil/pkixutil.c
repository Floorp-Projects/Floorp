/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * testwrapper.c
 *
 * Wrpper programm for libpkix tests.
 *
 */

#include <stdio.h>

#include "nspr.h"
#include "plgetopt.h"

#include "nss.h"
#include "secport.h"

typedef int (*mainTestFn)(int argc, char *argv[]);

extern int libpkix_buildthreads(int argc, char *argv[]);
extern int nss_threads(int argc, char *argv[]);
extern int test_certselector(int argc, char *argv[]);
extern int test_comcertselparams(int argc, char *argv[]);
extern int test_certchainchecker(int argc, char *argv[]);
extern int test_comcrlselparams(int argc, char *argv[]);
extern int test_crlselector(int argc, char *argv[]);

extern int test_procparams(int argc, char *argv[]);
extern int test_resourcelimits(int argc, char *argv[]);
extern int test_trustanchor(int argc, char *argv[]);
extern int test_valparams(int argc, char *argv[]);
extern int test_buildresult(int argc, char *argv[]);
extern int test_policynode(int argc, char *argv[]);
extern int test_valresult(int argc, char *argv[]);
extern int test_verifynode(int argc, char *argv[]);
extern int test_store(int argc, char *argv[]);
extern int test_basicchecker(int argc, char *argv[]);
extern int test_basicconstraintschecker(int argc, char *argv[]);
extern int test_buildchain(int argc, char *argv[]);
extern int test_buildchain_partialchain(int argc, char *argv[]);
extern int test_buildchain_resourcelimits(int argc, char *argv[]);
extern int test_buildchain_uchecker(int argc, char *argv[]);
extern int test_customcrlchecker(int argc, char *argv[]);
extern int test_defaultcrlchecker2stores(int argc, char *argv[]);
extern int test_ocsp(int argc, char *argv[]);
extern int test_policychecker(int argc, char *argv[]);
extern int test_subjaltnamechecker(int argc, char *argv[]);
extern int test_validatechain(int argc, char *argv[]);
extern int test_validatechain_NB(int argc, char *argv[]);
extern int test_validatechain_bc(int argc, char *argv[]);
extern int test_error(int argc, char *argv[]);
extern int test_list(int argc, char *argv[]);
extern int test_list2(int argc, char *argv[]);
extern int test_logger(int argc, char *argv[]);
extern int test_colcertstore(int argc, char *argv[]);
extern int test_ekuchecker(int argc, char *argv[]);
extern int test_httpcertstore(int argc, char *argv[]);
extern int test_pk11certstore(int argc, char *argv[]);
extern int test_socket(int argc, char *argv[]);
extern int test_authorityinfoaccess(int argc, char *argv[]);
extern int test_cert(int argc, char *argv[]);
extern int test_crl(int argc, char *argv[]);
extern int test_crlentry(int argc, char *argv[]);
extern int test_date(int argc, char *argv[]);
extern int test_generalname(int argc, char *argv[]);
extern int test_nameconstraints(int argc, char *argv[]);
extern int test_subjectinfoaccess(int argc, char *argv[]);
extern int test_x500name(int argc, char *argv[]);
extern int stress_test(int argc, char *argv[]);
extern int test_bigint(int argc, char *argv[]);
extern int test_bytearray(int argc, char *argv[]);
extern int test_hashtable(int argc, char *argv[]);
extern int test_mem(int argc, char *argv[]);
extern int test_monitorlock(int argc, char *argv[]);
extern int test_mutex(int argc, char *argv[]);
extern int test_mutex2(int argc, char *argv[]);
extern int test_mutex3(int argc, char *argv[]);
extern int test_object(int argc, char *argv[]);
extern int test_oid(int argc, char *argv[]);

/* Taken out. Problem with build                   */
/* extern int test_rwlock(int argc, char *argv[]); */
extern int test_string(int argc, char *argv[]);
extern int test_string2(int argc, char *argv[]);
extern int build_chain(int argc, char *argv[]);
extern int dumpcert(int argc, char *argv[]);
extern int dumpcrl(int argc, char *argv[]);
extern int validate_chain(int argc, char *argv[]);

typedef struct {
    char *fnName;
    mainTestFn fnPointer;
} testFunctionRef;

testFunctionRef testFnRefTable[] = {
    { "libpkix_buildthreads", libpkix_buildthreads },
    { "nss_threads", nss_threads },
    { "test_certselector", test_certselector },
    { "test_comcertselparams", test_comcertselparams },
    { "test_certchainchecker", test_certchainchecker },
    { "test_comcrlselparams", test_comcrlselparams },
    { "test_crlselector", test_crlselector },
    { "test_procparams", test_procparams },
    { "test_resourcelimits", test_resourcelimits },
    { "test_trustanchor", test_trustanchor },
    { "test_valparams", test_valparams },
    { "test_buildresult", test_buildresult },
    { "test_policynode", test_policynode },
    { "test_valresult", test_valresult },
    { "test_verifynode", test_verifynode },
    { "test_store", test_store },
    { "test_basicchecker", test_basicchecker },
    { "test_basicconstraintschecker", test_basicconstraintschecker },
    { "test_buildchain", test_buildchain },
    { "test_buildchain_partialchain", test_buildchain_partialchain },
    { "test_buildchain_resourcelimits", test_buildchain_resourcelimits },
    { "test_buildchain_uchecker", test_buildchain_uchecker },
    { "test_customcrlchecker", test_customcrlchecker },
    { "test_defaultcrlchecker2stores", test_defaultcrlchecker2stores },
    { "test_ocsp", test_ocsp },
    { "test_policychecker", test_policychecker },
    { "test_subjaltnamechecker", test_subjaltnamechecker },
    { "test_validatechain", test_validatechain },
    { "test_validatechain_NB", test_validatechain_NB },
    { "test_validatechain_bc", test_validatechain_bc },
    { "test_error", test_error },
    { "test_list", test_list },
    { "test_list2", test_list2 },
    { "test_logger", test_logger },
    { "test_colcertstore", test_colcertstore },
    { "test_ekuchecker", test_ekuchecker },
    { "test_httpcertstore", test_httpcertstore },
    { "test_pk11certstore", test_pk11certstore },
    { "test_socket", test_socket },
    { "test_authorityinfoaccess", test_authorityinfoaccess },
    { "test_cert", test_cert },
    { "test_crl", test_crl },
    { "test_crlentry", test_crlentry },
    { "test_date", test_date },
    { "test_generalname", test_generalname },
    { "test_nameconstraints", test_nameconstraints },
    { "test_subjectinfoaccess", test_subjectinfoaccess },
    { "test_x500name", test_x500name },
    { "stress_test", stress_test },
    { "test_bigint", test_bigint },
    { "test_bytearray", test_bytearray },
    { "test_hashtable", test_hashtable },
    { "test_mem", test_mem },
    { "test_monitorlock", test_monitorlock },
    { "test_mutex", test_mutex },
    { "test_mutex2", test_mutex2 },
    { "test_mutex3", test_mutex3 },
    { "test_object", test_object },
    { "test_oid", test_oid },
    /*  {"test_rwlock",                    test_rwlock }*/
    { "test_string", test_string },
    { "test_string2", test_string2 },
    { "build_chain", build_chain },
    { "dumpcert", dumpcert },
    { "dumpcrl", dumpcrl },
    { "validate_chain", validate_chain },
    { NULL, NULL },
};

static void
printUsage(char *cmdName)
{
    int fnCounter = 0;

    fprintf(stderr, "Usage: %s [test name] [arg1]...[argN]\n\n", cmdName);
    fprintf(stderr, "List of possible names for the tests:");
    while (testFnRefTable[fnCounter].fnName != NULL) {
        if (fnCounter % 2 == 0) {
            fprintf(stderr, "\n");
        }
        fprintf(stderr, "  %-35s ", testFnRefTable[fnCounter].fnName);
        fnCounter += 1;
    }
    fprintf(stderr, "\n");
}

static SECStatus
getTestArguments(int argc,
                 char **argv,
                 mainTestFn *ptestFn,
                 char **pdbPath,
                 int *pargc,
                 char ***pargv)
{
    PLOptState *optstate = NULL;
    PLOptStatus status;
    mainTestFn testFunction = NULL;
    char **wArgv = NULL;
    char *dbPath = NULL;
    char *fnName = NULL;
    int wArgc = 0;
    int fnCounter = 0;

    if (argc < 2) {
        printf("ERROR: insufficient number of arguments: %s.\n", fnName);
        return SECFailure;
    }

    fnName = argv[1];
    while (testFnRefTable[fnCounter].fnName != NULL) {
        if (!PORT_Strcmp(fnName, testFnRefTable[fnCounter].fnName)) {
            testFunction = testFnRefTable[fnCounter].fnPointer;
            break;
        }
        fnCounter += 1;
    }
    if (!testFunction) {
        printf("ERROR: unknown name of the test: %s.\n", fnName);
        return SECFailure;
    }

    wArgv = PORT_ZNewArray(char *, argc);
    if (!wArgv) {
        return SECFailure;
    }

    /* set name of the function as a first arg and increment arg count. */
    wArgv[0] = fnName;
    wArgc += 1;

    optstate = PL_CreateOptState(argc - 1, argv + 1, "d:");
    while ((status = PL_GetNextOpt(optstate)) == PL_OPT_OK) {
        switch (optstate->option) {
            case 'd':
                dbPath = (char *)optstate->value;
                break;

            default:
                wArgv[wArgc] = (char *)optstate->value;
                wArgc += 1;
                break;
        }
    }
    PL_DestroyOptState(optstate);

    *ptestFn = testFunction;
    *pdbPath = dbPath;
    *pargc = wArgc;
    *pargv = wArgv;

    return SECSuccess;
}

static int
runCmd(mainTestFn fnPointer,
       int argc,
       char **argv,
       char *dbPath)
{
    int retStat = 0;

    /*  Initialize NSPR and NSS.  */
    PR_Init(PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);

    /* if using databases, use NSS_Init and not NSS_NoDB_Init */
    if (dbPath && PORT_Strlen(dbPath) != 0) {
        if (NSS_Init(dbPath) != SECSuccess)
            return SECFailure;
    } else {
        if (NSS_NoDB_Init(NULL) != 0)
            return SECFailure;
    }
    retStat = fnPointer(argc, argv);

    if (NSS_Shutdown() != SECSuccess) {
        exit(1);
    }
    PR_Cleanup();
    return retStat;
}

int
main(int argc, char **argv)
{
    mainTestFn testFunction = NULL;
    char *dbPath = NULL;
    char **testArgv = NULL;
    int testArgc = 0;
    int rv = 0;

    rv = getTestArguments(argc, argv, &testFunction, &dbPath,
                          &testArgc, &testArgv);
    if (rv != SECSuccess) {
        printUsage(argv[0]);
        return 1;
    }

    rv = runCmd(testFunction, testArgc, testArgv, dbPath);

    PORT_Free(testArgv);

    return rv;
}
