/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif
#include "prtypes.h"
#include "prenv.h"
#include "secerr.h"
#include "blapi.h"
#include "hasht.h"
#include "plhash.h"
#include "nsslowhash.h"
#include "blapii.h"

struct NSSLOWInitContextStr {
    int count;
};

struct NSSLOWHASHContextStr {
    const SECHashObject *hashObj;
    void *hashCtxt;
};

#ifndef NSS_FIPS_DISABLED
static int
nsslow_GetFIPSEnabled(void)
{
#ifdef LINUX
    FILE *f;
    char d;
    size_t size;
    const char *env;

    env = PR_GetEnvSecure("NSS_FIPS");
    if (env && (*env == 'y' || *env == 'f' || *env == '1' || *env == 't')) {
        return 1;
    }

    f = fopen("/proc/sys/crypto/fips_enabled", "r");
    if (!f)
        return 0;

    size = fread(&d, 1, 1, f);
    fclose(f);
    if (size != 1)
        return 0;
    if (d != '1')
        return 0;
#endif /* LINUX */
    return 1;
}
#endif /* NSS_FIPS_DISABLED */

static NSSLOWInitContext dummyContext = { 0 };
static PRBool post_failed = PR_TRUE;

NSSLOWInitContext *
NSSLOW_Init(void)
{
#ifdef FREEBL_NO_DEPEND
    (void)FREEBL_InitStubs();
#endif

#ifndef NSS_FIPS_DISABLED
    /* make sure the FIPS product is installed if we are trying to
     * go into FIPS mode */
    if (nsslow_GetFIPSEnabled()) {
        if (BL_FIPSEntryOK(PR_TRUE, PR_FALSE) != SECSuccess) {
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
            post_failed = PR_TRUE;
            return NULL;
        }
    }
#endif
    post_failed = PR_FALSE;

    return &dummyContext;
}

void
NSSLOW_Shutdown(NSSLOWInitContext *context)
{
    PORT_Assert(context == &dummyContext);
    return;
}

void
NSSLOW_Reset(NSSLOWInitContext *context)
{
    PORT_Assert(context == &dummyContext);
    return;
}

NSSLOWHASHContext *
NSSLOWHASH_NewContext(NSSLOWInitContext *initContext,
                      HASH_HashType hashType)
{
    NSSLOWHASHContext *context;

    if (post_failed) {
        PORT_SetError(SEC_ERROR_PKCS11_DEVICE_ERROR);
        return NULL;
    }

    if (initContext != &dummyContext) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return (NULL);
    }

    context = PORT_ZNew(NSSLOWHASHContext);
    if (!context) {
        return NULL;
    }
    context->hashObj = HASH_GetRawHashObject(hashType);
    if (!context->hashObj) {
        PORT_Free(context);
        return NULL;
    }
    context->hashCtxt = context->hashObj->create();
    if (!context->hashCtxt) {
        PORT_Free(context);
        return NULL;
    }

    return context;
}

void
NSSLOWHASH_Begin(NSSLOWHASHContext *context)
{
    return context->hashObj->begin(context->hashCtxt);
}

void
NSSLOWHASH_Update(NSSLOWHASHContext *context, const unsigned char *buf,
                  unsigned int len)
{
    return context->hashObj->update(context->hashCtxt, buf, len);
}

void
NSSLOWHASH_End(NSSLOWHASHContext *context, unsigned char *buf,
               unsigned int *ret, unsigned int len)
{
    return context->hashObj->end(context->hashCtxt, buf, ret, len);
}

void
NSSLOWHASH_Destroy(NSSLOWHASHContext *context)
{
    context->hashObj->destroy(context->hashCtxt, PR_TRUE);
    PORT_Free(context);
}

unsigned int
NSSLOWHASH_Length(NSSLOWHASHContext *context)
{
    return context->hashObj->length;
}
