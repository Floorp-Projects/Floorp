/*
 * loader.c - load platform dependent DSO containing freebl implementation.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define _GNU_SOURCE 1
#include "loader.h"
#include "prmem.h"
#include "prerror.h"
#include "prinit.h"
#include "prenv.h"
#include "blname.c"

#include "prio.h"
#include "prprf.h"
#include <stdio.h>
#include "prsystem.h"
#include "nsslowhash.h"
#include <dlfcn.h>
#include "pratom.h"

static PRLibrary *blLib;

#define LSB(x) ((x)&0xff)
#define MSB(x) ((x) >> 8)

static const NSSLOWVector *vector;
static const char *libraryName = NULL;

/* pretty much only glibc uses this, make sure we don't have any depenencies
 * on nspr.. */
#undef PORT_Alloc
#undef PORT_Free
#define PORT_Alloc malloc
#define PR_Malloc malloc
#define PORT_Free free
#define PR_Free free
#define PR_GetDirectorySeparator() '/'
#define PR_LoadLibraryWithFlags(libspec, flags) \
    (PRLibrary *)dlopen(libSpec.value.pathname, RTLD_NOW | RTLD_LOCAL)
#define PR_GetLibraryFilePathname(name, addr) \
    freebl_lowhash_getLibraryFilePath(addr)

static char *
freebl_lowhash_getLibraryFilePath(void *addr)
{
    Dl_info dli;
    if (dladdr(addr, &dli) == 0) {
        return NULL;
    }
    return strdup(dli.dli_fname);
}

/*
 * The PR_LoadLibraryWithFlags call above defines this variable away, so we
 * don't need it..
 */
#ifdef nodef
static const char *NameOfThisSharedLib =
    SHLIB_PREFIX "freebl" SHLIB_VERSION "." SHLIB_SUFFIX;
#endif

#include "genload.c"

/* This function must be run only once. */
/*  determine if hybrid platform, then actually load the DSO. */
static PRStatus
freebl_LoadDSO(void)
{
    PRLibrary *handle;
    const char *name = getLibName();

    if (!name) {
        /*PR_SetError(PR_LOAD_LIBRARY_ERROR,0); */
        return PR_FAILURE;
    }
    handle = loader_LoadLibrary(name);
    if (handle) {
        void *address = dlsym(handle, "NSSLOW_GetVector");
        if (address) {
            NSSLOWGetVectorFn *getVector = (NSSLOWGetVectorFn *)address;
            const NSSLOWVector *dsoVector = getVector();
            if (dsoVector) {
                unsigned short dsoVersion = dsoVector->version;
                unsigned short myVersion = NSSLOW_VERSION;
                if (MSB(dsoVersion) == MSB(myVersion) &&
                    LSB(dsoVersion) >= LSB(myVersion) &&
                    dsoVector->length >= sizeof(NSSLOWVector)) {
                    vector = dsoVector;
                    libraryName = name;
                    blLib = handle;
                    return PR_SUCCESS;
                }
            }
        }
        (void)dlclose(handle);
    }
    return PR_FAILURE;
}

static PRCallOnceType loadFreeBLOnce;

static PRStatus
freebl_RunLoaderOnce(void)
{
    /* Don't have NSPR, so can use the real PR_CallOnce, implement a stripped
     * down version. */
    if (loadFreeBLOnce.initialized) {
        return loadFreeBLOnce.status;
    }
    if (__sync_lock_test_and_set(&loadFreeBLOnce.inProgress, 1) == 0) {
        loadFreeBLOnce.status = freebl_LoadDSO();
        loadFreeBLOnce.initialized = 1;
    } else {
        /* shouldn't have a lot of takers on the else clause, which is good
         * since we don't have condition variables yet.
         * 'initialized' only ever gets set (not cleared) so we don't
         * need the traditional locks. */
        while (!loadFreeBLOnce.initialized) {
            sleep(1); /* don't have condition variables, just give up the CPU */
        }
    }

    return loadFreeBLOnce.status;
}

const FREEBLVector *
FREEBL_GetVector(void)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce()) {
        return NULL;
    }
    if (vector) {
        return (vector->p_FREEBL_GetVector)();
    }
    return NULL;
}

NSSLOWInitContext *
NSSLOW_Init(void)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return NULL;
    return (vector->p_NSSLOW_Init)();
}

void
NSSLOW_Shutdown(NSSLOWInitContext *context)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_NSSLOW_Shutdown)(context);
}

void
NSSLOW_Reset(NSSLOWInitContext *context)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_NSSLOW_Reset)(context);
}

NSSLOWHASHContext *
NSSLOWHASH_NewContext(
    NSSLOWInitContext *initContext,
    HASH_HashType hashType)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return NULL;
    return (vector->p_NSSLOWHASH_NewContext)(initContext, hashType);
}

void
NSSLOWHASH_Begin(NSSLOWHASHContext *context)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_NSSLOWHASH_Begin)(context);
}

void
NSSLOWHASH_Update(NSSLOWHASHContext *context,
                  const unsigned char *buf,
                  unsigned int len)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_NSSLOWHASH_Update)(context, buf, len);
}

void
NSSLOWHASH_End(NSSLOWHASHContext *context,
               unsigned char *buf,
               unsigned int *ret, unsigned int len)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_NSSLOWHASH_End)(context, buf, ret, len);
}

void
NSSLOWHASH_Destroy(NSSLOWHASHContext *context)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_NSSLOWHASH_Destroy)(context);
}

unsigned int
NSSLOWHASH_Length(NSSLOWHASHContext *context)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return -1;
    return (vector->p_NSSLOWHASH_Length)(context);
}
