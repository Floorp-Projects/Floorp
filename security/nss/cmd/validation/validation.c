/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

#include "nspr.h"
#include "secutil.h"
#include "pk11func.h"
#include "nss.h"
#include "secport.h"
#include "secpkcs5.h"
#include "sechash.h"
#include "certdb.h"
#include "secmod.h"

static char *progName;
PRBool debug = PR_FALSE;

#define ERR_USAGE 2
#define ERR_PK11GETSLOT 13

static void
Usage()
{
#define FPS PR_fprintf(PR_STDERR,
    FPS "Usage:	 %s [-d certdir] [-P dbprefix] [-h tokenname]\n",
				 progName);
    FPS "\t\t [-k slotpwfile | -K slotpw] [-v]\n");

    exit(ERR_USAGE);
}

typedef enum {
    tagULong,
    tagVersion,
    tagUtf8
} tagType;

typedef struct {
    const char *attributeName;
    tagType attributeStorageType;
} attributeTag;

enum {
    opt_CertDir = 0,
    opt_TokenName,
    opt_SlotPWFile,
    opt_SlotPW,
    opt_DBPrefix,
    opt_Debug
};

static secuCommandFlag validation_options[] = {
    { /* opt_CertDir        */ 'd', PR_TRUE, 0, PR_FALSE },
    { /* opt_TokenName      */ 'h', PR_TRUE, 0, PR_FALSE },
    { /* opt_SlotPWFile     */ 'k', PR_TRUE, 0, PR_FALSE },
    { /* opt_SlotPW         */ 'K', PR_TRUE, 0, PR_FALSE },
    { /* opt_DBPrefix       */ 'P', PR_TRUE, 0, PR_FALSE },
    { /* opt_Debug          */ 'v', PR_FALSE, 0, PR_FALSE }
};

void
dump_Raw(char *label, CK_ATTRIBUTE *attr)
{
    int i;
    unsigned char *value = (unsigned char *)attr->pValue;
    printf("0x");
    for (i = 0; i < attr->ulValueLen; i++) {
        printf("%02x", value[i]);
    }
    printf("<%s>\n", label);
}

SECStatus
dump_validations(CK_OBJECT_CLASS objc, CK_ATTRIBUTE *template, int count,
                 attributeTag *tags, PK11SlotInfo *slot)
{
    PK11GenericObject *objs, *obj;

    objs = PK11_FindGenericObjects(slot, objc);

    for (obj = objs; obj != NULL; obj = PK11_GetNextGenericObject(obj)) {
        int i;
        printf("Validation Object:\n");
        PK11_ReadRawAttributes(NULL, PK11_TypeGeneric, obj, template, count);
        for (i = 0; i < count; i++) {
            CK_ULONG ulong;
            CK_VERSION version;
            int len = template[i].ulValueLen;
            printf("    %s: ", tags[i].attributeName);
            if (len < 0) {
                printf("<failed>\n");
            } else if (len == 0) {
                printf("<empty>\n");
            } else
                switch (tags[i].attributeStorageType) {
                    case tagULong:
                        if (len != sizeof(CK_ULONG)) {
                            dump_Raw("bad ulong", &template[i]);
                            break;
                        }
                        ulong = *(CK_ULONG *)template[i].pValue;
                        printf("%ld\n", ulong);
                        break;
                    case tagVersion:
                        if (len != sizeof(CK_VERSION)) {
                            dump_Raw("bad version", &template[i]);
                            break;
                        }
                        version = *(CK_VERSION *)template[i].pValue;
                        printf("%d.%d\n", version.major, version.minor);
                        break;
                    case tagUtf8:
                        printf("%.*s\n", len, (char *)template[i].pValue);
                        break;
                    default:
                        dump_Raw("unknown tag", &template[i]);
                        break;
                }
            PORT_Free(template[i].pValue);
            template[i].pValue = NULL;
            template[i].ulValueLen = 0;
        }
    }
    PK11_DestroyGenericObjects(objs);
    return SECSuccess;
}

int
main(int argc, char **argv)
{
    secuPWData slotPw = { PW_NONE, NULL };
    secuPWData p12FilePw = { PW_NONE, NULL };
    PK11SlotInfo *slot = NULL;
    char *slotname = NULL;
    char *dbprefix = "";
    char *nssdir = NULL;
    SECStatus rv;
    secuCommand validation;
    int local_errno = 0;

    CK_ATTRIBUTE validation_template[] = {
        { CKA_NSS_VALIDATION_TYPE, NULL, 0 },
        { CKA_NSS_VALIDATION_VERSION, NULL, 0 },
        { CKA_NSS_VALIDATION_LEVEL, NULL, 0 },
        { CKA_NSS_VALIDATION_MODULE_ID, NULL, 0 }
    };
    attributeTag validation_tags[] = {
        { "Validation Type", tagULong },
        { "Validation Version", tagVersion },
        { "Validation Level", tagULong },
        { "Validation Module ID", tagUtf8 },
    };

#ifdef _CRTDBG_MAP_ALLOC
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    validation.numCommands = 0;
    validation.commands = 0;
    validation.numOptions = PR_ARRAY_SIZE(validation_options);
    validation.options = validation_options;

    progName = strrchr(argv[0], '/');
    progName = progName ? progName + 1 : argv[0];

    rv = SECU_ParseCommandLine(argc, argv, progName, &validation);

    if (rv != SECSuccess)
        Usage();

    debug = validation.options[opt_Debug].activated;

    slotname = SECU_GetOptionArg(&validation, opt_TokenName);

    if (validation.options[opt_SlotPWFile].activated) {
        slotPw.source = PW_FROMFILE;
        slotPw.data = PORT_Strdup(validation.options[opt_SlotPWFile].arg);
    }

    if (validation.options[opt_SlotPW].activated) {
        slotPw.source = PW_PLAINTEXT;
        slotPw.data = PORT_Strdup(validation.options[opt_SlotPW].arg);
    }

    if (validation.options[opt_CertDir].activated) {
        nssdir = validation.options[opt_CertDir].arg;
    }
    if (validation.options[opt_DBPrefix].activated) {
        dbprefix = validation.options[opt_DBPrefix].arg;
    }

    PR_Init(PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);
    if (nssdir == NULL && NSS_NoDB_Init("") == SECSuccess) {
        rv = SECSuccess;
        /* if the system isn't already in FIPS mode, we need
         * to switch to FIPS mode */
        if (!PK11_IsFIPS()) {
            /* flip to FIPS mode */
            SECMODModule *module = SECMOD_GetInternalModule();
            rv = SECMOD_DeleteInternalModule(module->commonName);
        }
    } else if (nssdir != NULL) {
        rv = NSS_Initialize(nssdir, dbprefix, dbprefix, "secmod.db", 0);
    }
    if (rv != SECSuccess) {
        SECU_PrintPRandOSError(progName);
        local_errno = -1;
        goto done;
    }

    if (!slotname || PL_strcmp(slotname, "internal") == 0)
        slot = PK11_GetInternalKeySlot();
    else
        slot = PK11_FindSlotByName(slotname);

    if (!slot) {
        SECU_PrintError(progName, "Invalid slot \"%s\"",
                        slotname ? "internal" : slotname);
        local_errno = ERR_PK11GETSLOT;
        goto done;
    }

    rv = dump_validations(CKO_NSS_VALIDATION,
                          validation_template,
                          PR_ARRAY_SIZE(validation_template),
                          validation_tags,
                          slot);

done:
    if (slotPw.data != NULL)
        PORT_ZFree(slotPw.data, PL_strlen(slotPw.data));
    if (p12FilePw.data != NULL)
        PORT_ZFree(p12FilePw.data, PL_strlen(p12FilePw.data));
    if (slotname) {
        PORT_Free(slotname);
    }
    if (slot)
        PK11_FreeSlot(slot);
    if (NSS_Shutdown() != SECSuccess) {
        local_errno = 1;
    }
    PL_ArenaFinish();
    PR_Cleanup();
    return local_errno;
}
