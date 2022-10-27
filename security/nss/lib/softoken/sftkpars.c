/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 *  The following code handles the storage of PKCS 11 modules used by the
 * NSS. This file is written to abstract away how the modules are
 * stored so we can deside that later.
 */
#include "pkcs11i.h"
#include "sdb.h"
#include "prprf.h"
#include "prenv.h"
#include "utilpars.h"

#define FREE_CLEAR(p) \
    if (p) {          \
        PORT_Free(p); \
        p = NULL;     \
    }

static void
sftk_parseTokenFlags(char *tmp, sftk_token_parameters *parsed)
{
    parsed->readOnly = NSSUTIL_ArgHasFlag("flags", "readOnly", tmp);
    parsed->noCertDB = NSSUTIL_ArgHasFlag("flags", "noCertDB", tmp);
    parsed->noKeyDB = NSSUTIL_ArgHasFlag("flags", "noKeyDB", tmp);
    parsed->forceOpen = NSSUTIL_ArgHasFlag("flags", "forceOpen", tmp);
    parsed->pwRequired = NSSUTIL_ArgHasFlag("flags", "passwordRequired", tmp);
    parsed->optimizeSpace = NSSUTIL_ArgHasFlag("flags", "optimizeSpace", tmp);
    return;
}

static void
sftk_parseFlags(char *tmp, sftk_parameters *parsed)
{
    parsed->noModDB = NSSUTIL_ArgHasFlag("flags", "noModDB", tmp);
    parsed->readOnly = NSSUTIL_ArgHasFlag("flags", "readOnly", tmp);
    /* keep legacy interface working */
    parsed->noCertDB = NSSUTIL_ArgHasFlag("flags", "noCertDB", tmp);
    parsed->forceOpen = NSSUTIL_ArgHasFlag("flags", "forceOpen", tmp);
    parsed->pwRequired = NSSUTIL_ArgHasFlag("flags", "passwordRequired", tmp);
    parsed->optimizeSpace = NSSUTIL_ArgHasFlag("flags", "optimizeSpace", tmp);
    return;
}

static CK_RV
sftk_parseTokenParameters(char *param, sftk_token_parameters *parsed)
{
    int next;
    char *tmp = NULL;
    const char *index;
    index = NSSUTIL_ArgStrip(param);

    while (*index) {
        NSSUTIL_HANDLE_STRING_ARG(index, parsed->configdir, "configDir=", ;)
        NSSUTIL_HANDLE_STRING_ARG(index, parsed->updatedir, "updateDir=", ;)
        NSSUTIL_HANDLE_STRING_ARG(index, parsed->updCertPrefix, "updateCertPrefix=", ;)
        NSSUTIL_HANDLE_STRING_ARG(index, parsed->updKeyPrefix, "updateKeyPrefix=", ;)
        NSSUTIL_HANDLE_STRING_ARG(index, parsed->updateID, "updateID=", ;)
        NSSUTIL_HANDLE_STRING_ARG(index, parsed->certPrefix, "certPrefix=", ;)
        NSSUTIL_HANDLE_STRING_ARG(index, parsed->keyPrefix, "keyPrefix=", ;)
        NSSUTIL_HANDLE_STRING_ARG(index, parsed->tokdes, "tokenDescription=", ;)
        NSSUTIL_HANDLE_STRING_ARG(index, parsed->updtokdes, "updateTokenDescription=", ;)
        NSSUTIL_HANDLE_STRING_ARG(index, parsed->slotdes, "slotDescription=", ;)
        NSSUTIL_HANDLE_STRING_ARG(
            index, tmp, "minPWLen=",
            if (tmp) { parsed->minPW=atoi(tmp); PORT_Free(tmp); tmp = NULL; })
        NSSUTIL_HANDLE_STRING_ARG(
            index, tmp, "flags=",
            if (tmp) { sftk_parseTokenFlags(param,parsed); PORT_Free(tmp); tmp = NULL; })
        NSSUTIL_HANDLE_FINAL_ARG(index)
    }
    return CKR_OK;
}

static void
sftk_parseTokens(char *tokenParams, sftk_parameters *parsed)
{
    const char *tokenIndex;
    sftk_token_parameters *tokens = NULL;
    int i = 0, count = 0, next;

    if ((tokenParams == NULL) || (*tokenParams == 0))
        return;

    /* first count the number of slots */
    for (tokenIndex = NSSUTIL_ArgStrip(tokenParams); *tokenIndex;
         tokenIndex = NSSUTIL_ArgStrip(NSSUTIL_ArgSkipParameter(tokenIndex))) {
        count++;
    }

    /* get the data structures */
    tokens = (sftk_token_parameters *)
        PORT_ZAlloc(count * sizeof(sftk_token_parameters));
    if (tokens == NULL)
        return;

    for (tokenIndex = NSSUTIL_ArgStrip(tokenParams), i = 0;
         *tokenIndex && i < count; i++) {
        char *name;
        name = NSSUTIL_ArgGetLabel(tokenIndex, &next);
        tokenIndex += next;

        tokens[i].slotID = NSSUTIL_ArgDecodeNumber(name);
        tokens[i].readOnly = PR_FALSE;
        tokens[i].noCertDB = PR_FALSE;
        tokens[i].noKeyDB = PR_FALSE;
        if (!NSSUTIL_ArgIsBlank(*tokenIndex)) {
            char *args = NSSUTIL_ArgFetchValue(tokenIndex, &next);
            tokenIndex += next;
            if (args) {
                sftk_parseTokenParameters(args, &tokens[i]);
                PORT_Free(args);
            }
        }
        if (name)
            PORT_Free(name);
        tokenIndex = NSSUTIL_ArgStrip(tokenIndex);
    }
    parsed->token_count = i;
    parsed->tokens = tokens;
    return;
}

CK_RV
sftk_parseParameters(char *param, sftk_parameters *parsed, PRBool isFIPS)
{
    int next;
    char *tmp = NULL;
    const char *index;
    char *certPrefix = NULL, *keyPrefix = NULL;
    char *tokdes = NULL, *ptokdes = NULL, *pupdtokdes = NULL;
    char *slotdes = NULL, *pslotdes = NULL;
    char *fslotdes = NULL, *ftokdes = NULL;
    char *minPW = NULL;
    index = NSSUTIL_ArgStrip(param);

    PORT_Memset(parsed, 0, sizeof(sftk_parameters));

    while (*index) {
        NSSUTIL_HANDLE_STRING_ARG(index, parsed->configdir, "configDir=", ;)
        NSSUTIL_HANDLE_STRING_ARG(index, parsed->updatedir, "updateDir=", ;)
        NSSUTIL_HANDLE_STRING_ARG(index, parsed->updateID, "updateID=", ;)
        NSSUTIL_HANDLE_STRING_ARG(index, parsed->secmodName, "secmod=", ;)
        NSSUTIL_HANDLE_STRING_ARG(index, parsed->man, "manufacturerID=", ;)
        NSSUTIL_HANDLE_STRING_ARG(index, parsed->libdes, "libraryDescription=", ;)
        /* constructed values, used so legacy interfaces still work */
        NSSUTIL_HANDLE_STRING_ARG(index, certPrefix, "certPrefix=", ;)
        NSSUTIL_HANDLE_STRING_ARG(index, keyPrefix, "keyPrefix=", ;)
        NSSUTIL_HANDLE_STRING_ARG(index, tokdes, "cryptoTokenDescription=", ;)
        NSSUTIL_HANDLE_STRING_ARG(index, ptokdes, "dbTokenDescription=", ;)
        NSSUTIL_HANDLE_STRING_ARG(index, slotdes, "cryptoSlotDescription=", ;)
        NSSUTIL_HANDLE_STRING_ARG(index, pslotdes, "dbSlotDescription=", ;)
        NSSUTIL_HANDLE_STRING_ARG(index, fslotdes, "FIPSSlotDescription=", ;)
        NSSUTIL_HANDLE_STRING_ARG(index, ftokdes, "FIPSTokenDescription=", ;)
        NSSUTIL_HANDLE_STRING_ARG(index, pupdtokdes, "updateTokenDescription=", ;)
        NSSUTIL_HANDLE_STRING_ARG(index, minPW, "minPWLen=", ;)

        NSSUTIL_HANDLE_STRING_ARG(
            index, tmp, "flags=",
            if (tmp) { sftk_parseFlags(param,parsed); PORT_Free(tmp); tmp = NULL; })
        NSSUTIL_HANDLE_STRING_ARG(
            index, tmp, "tokens=",
            if (tmp) { sftk_parseTokens(tmp,parsed); PORT_Free(tmp); tmp = NULL; })
        NSSUTIL_HANDLE_FINAL_ARG(index)
    }
    if (parsed->tokens == NULL) {
        int count = isFIPS ? 1 : 2;
        int i = count - 1;
        sftk_token_parameters *tokens = NULL;

        tokens = (sftk_token_parameters *)
            PORT_ZAlloc(count * sizeof(sftk_token_parameters));
        if (tokens == NULL) {
            goto loser;
        }
        parsed->tokens = tokens;
        parsed->token_count = count;
        tokens[i].slotID = isFIPS ? FIPS_SLOT_ID : PRIVATE_KEY_SLOT_ID;
        tokens[i].certPrefix = certPrefix;
        tokens[i].keyPrefix = keyPrefix;
        tokens[i].minPW = minPW ? atoi(minPW) : 0;
        tokens[i].readOnly = parsed->readOnly;
        tokens[i].noCertDB = parsed->noCertDB;
        tokens[i].noKeyDB = parsed->noCertDB;
        tokens[i].forceOpen = parsed->forceOpen;
        tokens[i].pwRequired = parsed->pwRequired;
        tokens[i].optimizeSpace = parsed->optimizeSpace;
        tokens[0].optimizeSpace = parsed->optimizeSpace;
        certPrefix = NULL;
        keyPrefix = NULL;
        if (isFIPS) {
            tokens[i].tokdes = ftokdes;
            tokens[i].updtokdes = pupdtokdes;
            tokens[i].slotdes = fslotdes;
            fslotdes = NULL;
            ftokdes = NULL;
            pupdtokdes = NULL;
        } else {
            tokens[i].tokdes = ptokdes;
            tokens[i].updtokdes = pupdtokdes;
            tokens[i].slotdes = pslotdes;
            tokens[0].slotID = NETSCAPE_SLOT_ID;
            tokens[0].tokdes = tokdes;
            tokens[0].slotdes = slotdes;
            tokens[0].noCertDB = PR_TRUE;
            tokens[0].noKeyDB = PR_TRUE;
            pupdtokdes = NULL;
            ptokdes = NULL;
            pslotdes = NULL;
            tokdes = NULL;
            slotdes = NULL;
        }
    }

loser:
    FREE_CLEAR(certPrefix);
    FREE_CLEAR(keyPrefix);
    FREE_CLEAR(tokdes);
    FREE_CLEAR(ptokdes);
    FREE_CLEAR(pupdtokdes);
    FREE_CLEAR(slotdes);
    FREE_CLEAR(pslotdes);
    FREE_CLEAR(fslotdes);
    FREE_CLEAR(ftokdes);
    FREE_CLEAR(minPW);
    return CKR_OK;
}

void
sftk_freeParams(sftk_parameters *params)
{
    int i;

    for (i = 0; i < params->token_count; i++) {
        FREE_CLEAR(params->tokens[i].configdir);
        FREE_CLEAR(params->tokens[i].certPrefix);
        FREE_CLEAR(params->tokens[i].keyPrefix);
        FREE_CLEAR(params->tokens[i].tokdes);
        FREE_CLEAR(params->tokens[i].slotdes);
        FREE_CLEAR(params->tokens[i].updatedir);
        FREE_CLEAR(params->tokens[i].updCertPrefix);
        FREE_CLEAR(params->tokens[i].updKeyPrefix);
        FREE_CLEAR(params->tokens[i].updateID);
        FREE_CLEAR(params->tokens[i].updtokdes);
    }

    FREE_CLEAR(params->configdir);
    FREE_CLEAR(params->secmodName);
    FREE_CLEAR(params->man);
    FREE_CLEAR(params->libdes);
    FREE_CLEAR(params->tokens);
    FREE_CLEAR(params->updatedir);
    FREE_CLEAR(params->updateID);
}
