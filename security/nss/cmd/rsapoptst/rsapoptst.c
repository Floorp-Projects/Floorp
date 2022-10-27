/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>
#include "plgetopt.h"
#include "nss.h"
#include "secutil.h"
#include "pk11table.h"
#include "secmodt.h"
#include "pk11pub.h"

struct test_args {
    char *arg;
    int mask_value;
    char *description;
};

static const struct test_args test_array[] = {
    { "all", 0x1f, "run all the tests" },
    { "e_n_p", 0x01, "public exponent, modulus, prime1" },
    { "d_n_q", 0x02, "private exponent, modulus, prime2" },
    { "d_p_q", 0x04, "private exponent, prime1, prime2" },
    { "e_d_q", 0x08, "public exponent, private exponent, prime2" },
    { "e_d_n", 0x10, "public exponent, private exponent, modulus" }
};
static const int test_array_size =
    (sizeof(test_array) / sizeof(struct test_args));

static void
Usage(char *progName)
{
    int i;
#define PRINTUSAGE(subject, option, predicate) \
    fprintf(stderr, "%10s %s\t%s\n", subject, option, predicate);
    fprintf(stderr, "%s [-k keysize] [-e exp] [-r rounds] [-t tests]\n "
                    "Test creating RSA private keys from Partial components\n",
            progName);
    PRINTUSAGE("", "-k", "key size (in bit)");
    PRINTUSAGE("", "-e", "rsa public exponent");
    PRINTUSAGE("", "-r", "number times to repeat the test");
    PRINTUSAGE("", "-t", "run the specified tests");
    for (i = 0; i < test_array_size; i++) {
        PRINTUSAGE("", test_array[i].arg, test_array[i].description);
    }
    fprintf(stderr, "\n");
}

/*
 * Test the RSA populate command to see that it can really build
 * keys from it's components.
 */

const static CK_ATTRIBUTE rsaTemplate[] = {
    { CKA_CLASS, NULL, 0 },
    { CKA_KEY_TYPE, NULL, 0 },
    { CKA_TOKEN, NULL, 0 },
    { CKA_SENSITIVE, NULL, 0 },
    { CKA_PRIVATE, NULL, 0 },
    { CKA_ID, NULL, 0 },
    { CKA_MODULUS, NULL, 0 },
    { CKA_PUBLIC_EXPONENT, NULL, 0 },
    { CKA_PRIVATE_EXPONENT, NULL, 0 },
    { CKA_PRIME_1, NULL, 0 },
    { CKA_PRIME_2, NULL, 0 },
    { CKA_EXPONENT_1, NULL, 0 },
    { CKA_EXPONENT_2, NULL, 0 },
    { CKA_COEFFICIENT, NULL, 0 },
};

#define RSA_SIZE (sizeof(rsaTemplate))
#define RSA_ATTRIBUTES (sizeof(rsaTemplate) / sizeof(CK_ATTRIBUTE))

static void
resetTemplate(CK_ATTRIBUTE *attribute, int start, int end)
{
    int i;
    for (i = start; i < end; i++) {
        if (attribute[i].pValue) {
            PORT_Free(attribute[i].pValue);
        }
        attribute[i].pValue = NULL;
        attribute[i].ulValueLen = 0;
    }
}

static SECStatus
copyAttribute(PK11ObjectType objType, void *object, CK_ATTRIBUTE *template,
              int offset, CK_ATTRIBUTE_TYPE attrType)
{
    SECItem attributeItem = { 0, 0, 0 };
    SECStatus rv;

    rv = PK11_ReadRawAttribute(objType, object, attrType, &attributeItem);
    if (rv != SECSuccess) {
        return rv;
    }
    template[offset].type = attrType;
    template[offset].pValue = attributeItem.data;
    template[offset].ulValueLen = attributeItem.len;
    return SECSuccess;
}

static SECStatus
readKey(PK11ObjectType objType, void *object, CK_ATTRIBUTE *template,
        int start, int end)
{
    int i;
    SECStatus rv;

    for (i = start; i < end; i++) {
        rv = copyAttribute(objType, object, template, i, template[i].type);
        if (rv != SECSuccess) {
            goto fail;
        }
    }
    return SECSuccess;

fail:
    resetTemplate(template, start, i);
    return rv;
}

#define ATTR_STRING(x) getNameFromAttribute(x)

static void
dumphex(FILE *file, const unsigned char *cpval, int start, int end)
{
    int i;
    for (i = start; i < end; i++) {
        if ((i % 16) == 0)
            fprintf(file, "\n ");
        fprintf(file, " %02x", cpval[i]);
    }
    return;
}

void
dumpTemplate(FILE *file, const CK_ATTRIBUTE *template, int start, int end)
{
    int i;
    for (i = start; i < end; i++) {
        unsigned char cval;
        CK_ULONG ulval;
        const unsigned char *cpval;

        fprintf(file, "%s:", ATTR_STRING(template[i].type));
        switch (template[i].ulValueLen) {
            case 1:
                cval = *(unsigned char *)template[i].pValue;
                switch (cval) {
                    case 0:
                        fprintf(file, " false");
                        break;
                    case 1:
                        fprintf(file, " true");
                        break;
                    default:
                        fprintf(file, " %d (=0x%02x,'%c')", cval, cval, cval);
                        break;
                }
                break;
            case sizeof(CK_ULONG):
                ulval = *(CK_ULONG *)template[i].pValue;
                fprintf(file, " %ld (=0x%04lx)", ulval, ulval);
                break;
            default:
                cpval = (const unsigned char *)template[i].pValue;
                dumphex(file, cpval, 0, template[i].ulValueLen);
                break;
        }
        fprintf(file, "\n");
    }
}

void
dumpItem(FILE *file, const SECItem *item)
{
    const unsigned char *cpval;

    if (item == NULL) {
        fprintf(file, " pNULL ");
        return;
    }
    if (item->data == NULL) {
        fprintf(file, " NULL ");
        return;
    }
    if (item->len == 0) {
        fprintf(file, " Empty ");
        return;
    }
    cpval = item->data;
    dumphex(file, cpval, 0, item->len);
    fprintf(file, " ");
    return;
}

PRBool
rsaKeysAreEqual(PK11ObjectType srcType, void *src,
                PK11ObjectType destType, void *dest)
{

    CK_ATTRIBUTE srcTemplate[RSA_ATTRIBUTES];
    CK_ATTRIBUTE destTemplate[RSA_ATTRIBUTES];
    PRBool areEqual = PR_TRUE;
    SECStatus rv;
    int i;

    memcpy(srcTemplate, rsaTemplate, RSA_SIZE);
    memcpy(destTemplate, rsaTemplate, RSA_SIZE);

    rv = readKey(srcType, src, srcTemplate, 0, RSA_ATTRIBUTES);
    if (rv != SECSuccess) {
        printf("Could read source key\n");
        return PR_FALSE;
    }
    rv = readKey(destType, dest, destTemplate, 0, RSA_ATTRIBUTES);
    if (rv != SECSuccess) {
        printf("Could read dest key\n");
        return PR_FALSE;
    }

    for (i = 0; i < RSA_ATTRIBUTES; i++) {
        if (srcTemplate[i].type == CKA_ID) {
            continue; /* we purposefully make the CKA_ID different */
        }
        if (srcTemplate[i].ulValueLen != destTemplate[i].ulValueLen) {
            printf("key->%s not equal src_len = %ld, dest_len=%ld\n",
                   ATTR_STRING(srcTemplate[i].type),
                   srcTemplate[i].ulValueLen, destTemplate[i].ulValueLen);
            areEqual = 0;
        } else if (memcmp(srcTemplate[i].pValue, destTemplate[i].pValue,
                          destTemplate[i].ulValueLen) != 0) {
            printf("key->%s not equal.\n", ATTR_STRING(srcTemplate[i].type));
            areEqual = 0;
        }
    }
    if (!areEqual) {
        fprintf(stderr, "original key:\n");
        dumpTemplate(stderr, srcTemplate, 0, RSA_ATTRIBUTES);
        fprintf(stderr, "created key:\n");
        dumpTemplate(stderr, destTemplate, 0, RSA_ATTRIBUTES);
    }
    resetTemplate(srcTemplate, 0, RSA_ATTRIBUTES);
    resetTemplate(destTemplate, 0, RSA_ATTRIBUTES);
    return areEqual;
}

static int exp_exp_prime_fail_count = 0;

#define LEAK_ID 0xf

static int
doRSAPopulateTest(unsigned int keySize, unsigned long exponent,
                  int mask, int round, void *pwarg)
{
    SECKEYPrivateKey *rsaPrivKey;
    SECKEYPublicKey *rsaPubKey;
    PK11GenericObject *tstPrivKey;
    CK_ATTRIBUTE tstTemplate[RSA_ATTRIBUTES];
    int tstHeaderCount;
    PK11SlotInfo *slot = NULL;
    PK11RSAGenParams rsaParams;
    CK_OBJECT_CLASS obj_class = CKO_PRIVATE_KEY;
    CK_KEY_TYPE key_type = CKK_RSA;
    CK_BBOOL ck_false = CK_FALSE;
    CK_BYTE cka_id[2] = { 0, 0 };
    int failed = 0;
    int leak_found;      /* did we find the expected leak */
    int expect_leak = 0; /* are we expecting a leak? */

    rsaParams.pe = exponent;
    rsaParams.keySizeInBits = keySize;

    slot = PK11_GetInternalSlot();
    if (slot == NULL) {
        fprintf(stderr, "Couldn't get the internal slot for the test \n");
        return -1;
    }

    rsaPrivKey = PK11_GenerateKeyPair(slot, CKM_RSA_PKCS_KEY_PAIR_GEN,
                                      &rsaParams, &rsaPubKey, PR_FALSE,
                                      PR_FALSE, pwarg);
    if (rsaPrivKey == NULL) {
        fprintf(stderr, "RSA Key Gen failed");
        PK11_FreeSlot(slot);
        return -1;
    }

    memcpy(tstTemplate, rsaTemplate, RSA_SIZE);

    tstTemplate[0].pValue = &obj_class;
    tstTemplate[0].ulValueLen = sizeof(obj_class);
    tstTemplate[1].pValue = &key_type;
    tstTemplate[1].ulValueLen = sizeof(key_type);
    tstTemplate[2].pValue = &ck_false;
    tstTemplate[2].ulValueLen = sizeof(ck_false);
    tstTemplate[3].pValue = &ck_false;
    tstTemplate[3].ulValueLen = sizeof(ck_false);
    tstTemplate[4].pValue = &ck_false;
    tstTemplate[4].ulValueLen = sizeof(ck_false);
    tstTemplate[5].pValue = &cka_id[0];
    tstTemplate[5].ulValueLen = sizeof(cka_id);
    tstHeaderCount = 6;
    cka_id[0] = round;

    if (mask & 1) {
        printf("%s\n", test_array[1].description);
        resetTemplate(tstTemplate, tstHeaderCount, RSA_ATTRIBUTES);
        cka_id[1] = 0;
        copyAttribute(PK11_TypePrivKey, rsaPrivKey, tstTemplate,
                      tstHeaderCount, CKA_PUBLIC_EXPONENT);
        copyAttribute(PK11_TypePrivKey, rsaPrivKey, tstTemplate,
                      tstHeaderCount + 1, CKA_MODULUS);
        copyAttribute(PK11_TypePrivKey, rsaPrivKey, tstTemplate,
                      tstHeaderCount + 2, CKA_PRIME_1);

        tstPrivKey = PK11_CreateManagedGenericObject(slot, tstTemplate,
                                                     tstHeaderCount +
                                                         3,
                                                     PR_FALSE);
        if (tstPrivKey == NULL) {
            fprintf(stderr, "RSA Populate failed: pubExp mod p\n");
            failed = 1;
        } else if (!rsaKeysAreEqual(PK11_TypePrivKey, rsaPrivKey,
                                    PK11_TypeGeneric, tstPrivKey)) {
            fprintf(stderr, "RSA Populate key mismatch: pubExp mod p\n");
            failed = 1;
        }
        if (tstPrivKey)
            PK11_DestroyGenericObject(tstPrivKey);
    }
    if (mask & 2) {
        printf("%s\n", test_array[2].description);
        /* test the basic2 case, public exponent, modulus, prime2 */
        resetTemplate(tstTemplate, tstHeaderCount, RSA_ATTRIBUTES);
        cka_id[1] = 1;
        copyAttribute(PK11_TypePrivKey, rsaPrivKey, tstTemplate,
                      tstHeaderCount, CKA_PUBLIC_EXPONENT);
        copyAttribute(PK11_TypePrivKey, rsaPrivKey, tstTemplate,
                      tstHeaderCount + 1, CKA_MODULUS);
        copyAttribute(PK11_TypePrivKey, rsaPrivKey, tstTemplate,
                      tstHeaderCount + 2, CKA_PRIME_2);
        /* test with q in the prime1 position */
        tstTemplate[tstHeaderCount + 2].type = CKA_PRIME_1;

        tstPrivKey = PK11_CreateManagedGenericObject(slot, tstTemplate,
                                                     tstHeaderCount +
                                                         3,
                                                     PR_FALSE);
        if (tstPrivKey == NULL) {
            fprintf(stderr, "RSA Populate failed: pubExp mod q\n");
            failed = 1;
        } else if (!rsaKeysAreEqual(PK11_TypePrivKey, rsaPrivKey,
                                    PK11_TypeGeneric, tstPrivKey)) {
            fprintf(stderr, "RSA Populate key mismatch: pubExp mod q\n");
            failed = 1;
        }
        if (tstPrivKey)
            PK11_DestroyGenericObject(tstPrivKey);
    }
    if (mask & 4) {
        printf("%s\n", test_array[3].description);
        /* test the medium case, private exponent, prime1, prime2 */
        resetTemplate(tstTemplate, tstHeaderCount, RSA_ATTRIBUTES);
        cka_id[1] = 2;

        copyAttribute(PK11_TypePrivKey, rsaPrivKey, tstTemplate,
                      tstHeaderCount, CKA_PRIVATE_EXPONENT);
        copyAttribute(PK11_TypePrivKey, rsaPrivKey, tstTemplate,
                      tstHeaderCount + 1, CKA_PRIME_1);
        copyAttribute(PK11_TypePrivKey, rsaPrivKey, tstTemplate,
                      tstHeaderCount + 2, CKA_PRIME_2);
        /* test with p & q swapped. Underlying code should swap these back */
        tstTemplate[tstHeaderCount + 2].type = CKA_PRIME_1;
        tstTemplate[tstHeaderCount + 1].type = CKA_PRIME_2;

        tstPrivKey = PK11_CreateManagedGenericObject(slot, tstTemplate,
                                                     tstHeaderCount +
                                                         3,
                                                     PR_FALSE);
        if (tstPrivKey == NULL) {
            fprintf(stderr, "RSA Populate failed: privExp p q\n");
            failed = 1;
        } else if (!rsaKeysAreEqual(PK11_TypePrivKey, rsaPrivKey,
                                    PK11_TypeGeneric, tstPrivKey)) {
            fprintf(stderr, "RSA Populate key mismatch: privExp p q\n");
            failed = 1;
        }
        if (tstPrivKey)
            PK11_DestroyGenericObject(tstPrivKey);
    }
    if (mask & 8) {
        printf("%s\n", test_array[4].description);
        /* test the advanced case, public exponent, private exponent, prime2 */
        resetTemplate(tstTemplate, tstHeaderCount, RSA_ATTRIBUTES);
        cka_id[1] = 3;
        copyAttribute(PK11_TypePrivKey, rsaPrivKey, tstTemplate,
                      tstHeaderCount, CKA_PRIVATE_EXPONENT);
        copyAttribute(PK11_TypePrivKey, rsaPrivKey, tstTemplate,
                      tstHeaderCount + 1, CKA_PUBLIC_EXPONENT);
        copyAttribute(PK11_TypePrivKey, rsaPrivKey, tstTemplate,
                      tstHeaderCount + 2, CKA_PRIME_2);

        tstPrivKey = PK11_CreateManagedGenericObject(slot, tstTemplate,
                                                     tstHeaderCount +
                                                         3,
                                                     PR_FALSE);
        if (tstPrivKey == NULL) {
            fprintf(stderr, "RSA Populate failed: pubExp privExp q\n");
            fprintf(stderr, " this is expected periodically. It means we\n");
            fprintf(stderr, " had more than one key that meets the "
                            "specification\n");
            exp_exp_prime_fail_count++;
        } else if (!rsaKeysAreEqual(PK11_TypePrivKey, rsaPrivKey,
                                    PK11_TypeGeneric, tstPrivKey)) {
            fprintf(stderr, "RSA Populate key mismatch: pubExp privExp q\n");
            failed = 1;
        }
        if (tstPrivKey)
            PK11_DestroyGenericObject(tstPrivKey);
    }
    if (mask & 0x10) {
        printf("%s\n", test_array[5].description);
        /* test the advanced case2, public exponent, private exponent, modulus
         */
        resetTemplate(tstTemplate, tstHeaderCount, RSA_ATTRIBUTES);
        cka_id[1] = LEAK_ID;

        copyAttribute(PK11_TypePrivKey, rsaPrivKey, tstTemplate,
                      tstHeaderCount, CKA_PRIVATE_EXPONENT);
        copyAttribute(PK11_TypePrivKey, rsaPrivKey, tstTemplate,
                      tstHeaderCount + 1, CKA_PUBLIC_EXPONENT);
        copyAttribute(PK11_TypePrivKey, rsaPrivKey, tstTemplate,
                      tstHeaderCount + 2, CKA_MODULUS);

        /* purposefully use the old version. This will create a leak */
        tstPrivKey = PK11_CreateGenericObject(slot, tstTemplate,
                                              tstHeaderCount +
                                                  3,
                                              PR_FALSE);
        if (tstPrivKey == NULL) {
            fprintf(stderr, "RSA Populate failed: pubExp privExp mod\n");
            failed = 1;
        } else if (!rsaKeysAreEqual(PK11_TypePrivKey, rsaPrivKey,
                                    PK11_TypeGeneric, tstPrivKey)) {
            fprintf(stderr, "RSA Populate key mismatch: pubExp privExp mod\n");
            failed = 1;
        }
        expect_leak = 1;
        if (tstPrivKey)
            PK11_DestroyGenericObject(tstPrivKey);
    }
    resetTemplate(tstTemplate, tstHeaderCount, RSA_ATTRIBUTES);
    SECKEY_DestroyPrivateKey(rsaPrivKey);
    SECKEY_DestroyPublicKey(rsaPubKey);

    /* make sure we didn't leak */
    leak_found = 0;
    tstPrivKey = PK11_FindGenericObjects(slot, CKO_PRIVATE_KEY);
    if (tstPrivKey) {
        SECStatus rv;
        PK11GenericObject *thisKey;
        int i;

        fprintf(stderr, "Leaking keys...\n");
        for (i = 0, thisKey = tstPrivKey; thisKey; i++,
            thisKey = PK11_GetNextGenericObject(thisKey)) {
            SECItem id = { 0, NULL, 0 };

            rv = PK11_ReadRawAttribute(PK11_TypeGeneric, thisKey,
                                       CKA_ID, &id);
            if (rv != SECSuccess) {
                fprintf(stderr, "Key %d: couldn't read CKA_ID: %s\n",
                        i, PORT_ErrorToString(PORT_GetError()));
                continue;
            }
            fprintf(stderr, "id = { ");
            dumpItem(stderr, &id);
            fprintf(stderr, "};");
            if (id.data[1] == LEAK_ID) {
                fprintf(stderr, " ---> leak expected\n");
                if (id.data[0] == round)
                    leak_found = 1;
            } else {
                if (id.len != sizeof(cka_id)) {
                    fprintf(stderr,
                            " ---> ERROR unexpected leak in generated key\n");
                } else {
                    fprintf(stderr,
                            " ---> ERROR unexpected leak in constructed key\n");
                }
                failed = 1;
            }
            SECITEM_FreeItem(&id, PR_FALSE);
        }
        PK11_DestroyGenericObjects(tstPrivKey);
    }
    if (expect_leak && !leak_found) {
        fprintf(stderr, "ERROR expected leak not found\n");
        failed = 1;
    }

    PK11_FreeSlot(slot);
    return failed ? -1 : 0;
}

/* populate options */
enum {
    opt_Exponent = 0,
    opt_KeySize,
    opt_Repeat,
    opt_Tests
};

static secuCommandFlag populate_options[] = {
    { /* opt_Exponent   */ 'e', PR_TRUE, 0, PR_FALSE },
    { /* opt_KeySize    */ 'k', PR_TRUE, 0, PR_FALSE },
    { /* opt_Repeat     */ 'r', PR_TRUE, 0, PR_FALSE },
    { /* opt_Tests      */ 't', PR_TRUE, 0, PR_FALSE },
};

int
is_delimiter(char c)
{
    if ((c == '+') || (c == ',') || (c == '|')) {
        return 1;
    }
    return 0;
}

int
parse_tests(char *test_string)
{
    int mask = 0;
    int i;

    while (*test_string) {
        if (is_delimiter(*test_string)) {
            test_string++;
        }
        for (i = 0; i < test_array_size; i++) {
            char *arg = test_array[i].arg;
            int len = strlen(arg);
            if (strncmp(test_string, arg, len) == 0) {
                test_string += len;
                mask |= test_array[i].mask_value;
                break;
            }
        }
        if (i == test_array_size) {
            break;
        }
    }
    return mask;
}

int
main(int argc, char **argv)
{
    unsigned int keySize = 1024;
    unsigned long exponent = 65537;
    int i, repeat = 1, ret = 0;
    SECStatus rv = SECFailure;
    secuCommand populateArgs;
    char *progName;
    int mask = 0xff;

    populateArgs.numCommands = 0;
    populateArgs.numOptions = sizeof(populate_options) /
                              sizeof(secuCommandFlag);
    populateArgs.commands = NULL;
    populateArgs.options = populate_options;

    progName = strrchr(argv[0], '/');
    if (!progName)
        progName = strrchr(argv[0], '\\');
    progName = progName ? progName + 1 : argv[0];

    rv = NSS_NoDB_Init(NULL);
    if (rv != SECSuccess) {
        SECU_PrintPRandOSError(progName);
        return -1;
    }

    rv = SECU_ParseCommandLine(argc, argv, progName, &populateArgs);
    if (rv == SECFailure) {
        fprintf(stderr, "%s: command line parsing error!\n", progName);
        Usage(progName);
        return -1;
    }
    rv = SECFailure;

    if (populateArgs.options[opt_KeySize].activated) {
        keySize = PORT_Atoi(populateArgs.options[opt_KeySize].arg);
    }
    if (populateArgs.options[opt_Repeat].activated) {
        repeat = PORT_Atoi(populateArgs.options[opt_Repeat].arg);
    }
    if (populateArgs.options[opt_Exponent].activated) {
        exponent = PORT_Atoi(populateArgs.options[opt_Exponent].arg);
    }
    if (populateArgs.options[opt_Tests].activated) {
        char *test_string = populateArgs.options[opt_Tests].arg;
        mask = PORT_Atoi(test_string);
        if (mask == 0) {
            mask = parse_tests(test_string);
        }
        if (mask == 0) {
            Usage(progName);
            return -1;
        }
    }

    exp_exp_prime_fail_count = 0;
    for (i = 0; i < repeat; i++) {
        printf("Running RSA Populate test run %d\n", i);
        ret = doRSAPopulateTest(keySize, exponent, mask, i, NULL);
        if (ret != 0) {
            i++;
            break;
        }
    }
    if (ret != 0) {
        fprintf(stderr, "RSA Populate test round %d: FAILED\n", i);
    }
    if (repeat > 1) {
        printf(" pub priv prime test:  %d failures out of %d runs (%f %%)\n",
               exp_exp_prime_fail_count, i,
               (((double)exp_exp_prime_fail_count) * 100.0) / (double)i);
    }
    if (NSS_Shutdown() != SECSuccess) {
        fprintf(stderr, "Shutdown failed\n");
        ret = -1;
    }
    return ret;
}
