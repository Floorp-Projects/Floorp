
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif

#include "shsign.h"
#include "prlink.h"
#include "prio.h"
#include "blapi.h"
#include "seccomon.h"
#include "secerr.h"
#include "stdio.h"
#include "prmem.h"
#include "hasht.h"
#include "pqg.h"
#include "blapii.h"
#include "secitem.h"
#include "pkcs11t.h"

#ifndef NSS_FIPS_DISABLED

/*
 * Most modern version of Linux support a speed optimization scheme where an
 * application called prelink modifies programs and shared libraries to quickly
 * load if they fit into an already designed address space. In short, prelink
 * scans the list of programs and libraries on your system, assigns them a
 * predefined space in the the address space, then provides the fixups to the
 * library.

 * The modification of the shared library is correctly detected by the freebl
 * FIPS checksum scheme where we check a signed hash of the library against the
 * library itself.
 *
 * The prelink command itself can reverse the process of modification and
 * output the prestine shared library as it was before prelink made it's
 * changes. If FREEBL_USE_PRELINK is set Freebl uses prelink to output the
 * original copy of the shared library before prelink modified it.
 */
#ifdef FREEBL_USE_PRELINK
#ifndef FREELB_PRELINK_COMMAND
#define FREEBL_PRELINK_COMMAND "/usr/sbin/prelink -u -o -"
#endif
#include "private/pprio.h"

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>

/*
 * This function returns an NSPR PRFileDesc * which the caller can read to
 * obtain the prestine value of the shared library, before any OS related
 * changes to it (usually address fixups).
 *
 * If prelink is installed, this
 * file descriptor is a pipe connecting the output of
 *            /usr/sbin/prelink -u -o - {Library}
 * and *pid returns the process id of the prelink child.
 *
 * If prelink is not installed, it returns a normal readonly handle to the
 * library itself and *pid is set to '0'.
 */
PRFileDesc *
bl_OpenUnPrelink(const char *shName, int *pid)
{
    char *command = strdup(FREEBL_PRELINK_COMMAND);
    char *argString = NULL;
    char **argv = NULL;
    char *shNameArg = NULL;
    char *cp;
    pid_t child;
    int argc = 0, argNext = 0;
    struct stat statBuf;
    int pipefd[2] = { -1, -1 };
    int ret;

    *pid = 0;

    /* make sure the prelink command exists first. If not, fall back to
     * just reading the file */
    for (cp = command; *cp; cp++) {
        if (*cp == ' ') {
            *cp++ = 0;
            argString = cp;
            break;
        }
    }
    memset(&statBuf, 0, sizeof(statBuf));
    /* stat the file, follow the link */
    ret = stat(command, &statBuf);
    if (ret < 0) {
        free(command);
        return PR_Open(shName, PR_RDONLY, 0);
    }
    /* file exits, make sure it's an executable */
    if (!S_ISREG(statBuf.st_mode) ||
        ((statBuf.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) == 0)) {
        free(command);
        return PR_Open(shName, PR_RDONLY, 0);
    }

    /* OK, the prelink command exists and looks correct, use it */
    /* build the arglist while we can still malloc */
    /* count the args if any */
    if (argString && *argString) {
        /* argString may have leading spaces, strip them off*/
        for (cp = argString; *cp && *cp == ' '; cp++)
            ;
        argString = cp;
        if (*cp) {
            /* there is at least one arg.. */
            argc = 1;
        }

        /* count the rest: Note there is no provision for escaped
         * spaces here */
        for (cp = argString; *cp; cp++) {
            if (*cp == ' ') {
                while (*cp && *cp == ' ')
                    cp++;
                if (*cp)
                    argc++;
            }
        }
    }

    /* add the additional args: argv[0] (command), shName, NULL*/
    argc += 3;
    argv = PORT_NewArray(char *, argc);
    if (argv == NULL) {
        goto loser;
    }

    /* fill in the arglist */
    argv[argNext++] = command;
    if (argString && *argString) {
        argv[argNext++] = argString;
        for (cp = argString; *cp; cp++) {
            if (*cp == ' ') {
                *cp++ = 0;
                while (*cp && *cp == ' ')
                    cp++;
                if (*cp)
                    argv[argNext++] = cp;
            }
        }
    }
    /* exec doesn't advertise taking const char **argv, do the paranoid
     * copy */
    shNameArg = strdup(shName);
    if (shNameArg == NULL) {
        goto loser;
    }
    argv[argNext++] = shNameArg;
    argv[argNext++] = 0;

    ret = pipe(pipefd);
    if (ret < 0) {
        goto loser;
    }

    /* use vfork() so we don't trigger the pthread_at_fork() handlers */
    child = vfork();
    if (child < 0)
        goto loser;
    if (child == 0) {
        /* set up the file descriptors */
        /* if we need to support BSD, this will need to be an open of
         * /dev/null and dup2(nullFD, 0)*/
        close(0);
        /* associate pipefd[1] with stdout */
        if (pipefd[1] != 1)
            dup2(pipefd[1], 1);
        close(2);
        close(pipefd[0]);
        /* should probably close the other file descriptors? */

        execv(command, argv);
        /* avoid at_exit() handlers */
        _exit(1); /* shouldn't reach here except on an error */
    }
    close(pipefd[1]);
    pipefd[1] = -1;

    /* this is safe because either vfork() as full fork() semantics, and thus
     * already has it's own address space, or because vfork() has paused
     * the parent util the exec or exit */
    free(command);
    free(shNameArg);
    PORT_Free(argv);

    *pid = child;

    return PR_ImportPipe(pipefd[0]);

loser:
    if (pipefd[0] != -1) {
        close(pipefd[0]);
    }
    if (pipefd[1] != -1) {
        close(pipefd[1]);
    }
    free(command);
    free(shNameArg);
    PORT_Free(argv);

    return NULL;
}

/*
 * bl_CloseUnPrelink -
 *
 * This closes the file descripter and reaps and children openned and crated by
 * b;_OpenUnprelink. It's primary difference between it and just close is
 * that it calls wait on the pid if one is supplied, preventing zombie children
 * from hanging around.
 */
void
bl_CloseUnPrelink(PRFileDesc *file, int pid)
{
    /* close the file descriptor */
    PR_Close(file);
    /* reap the child */
    if (pid) {
        waitpid(pid, NULL, 0);
    }
}
#endif

/* #define DEBUG_SHVERIFY 1 */

static char *
mkCheckFileName(const char *libName)
{
    int ln_len = PORT_Strlen(libName);
    int index = ln_len + 1 - sizeof("." SHLIB_SUFFIX);
    char *output = PORT_Alloc(ln_len + sizeof(SGN_SUFFIX));
    if (!output) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return NULL;
    }

    if ((index > 0) &&
        (PORT_Strncmp(&libName[index],
                      "." SHLIB_SUFFIX, sizeof("." SHLIB_SUFFIX)) == 0)) {
        ln_len = index;
    }
    PORT_Memcpy(output, libName, ln_len);
    PORT_Memcpy(&output[ln_len], SGN_SUFFIX, sizeof(SGN_SUFFIX));
    return output;
}

static int
decodeInt(unsigned char *buf)
{
    return (buf[3]) | (buf[2] << 8) | (buf[1] << 16) | (buf[0] << 24);
}

static SECStatus
readItem(PRFileDesc *fd, SECItem *item)
{
    unsigned char buf[4];
    int bytesRead;

    bytesRead = PR_Read(fd, buf, 4);
    if (bytesRead != 4) {
        return SECFailure;
    }
    item->len = decodeInt(buf);

    item->data = PORT_Alloc(item->len);
    if (item->data == NULL) {
        item->len = 0;
        return SECFailure;
    }
    bytesRead = PR_Read(fd, item->data, item->len);
    if (bytesRead != item->len) {
        PORT_Free(item->data);
        item->data = NULL;
        item->len = 0;
        return SECFailure;
    }
    return SECSuccess;
}

static PRBool blapi_SHVerifyFile(const char *shName, PRBool self);

static PRBool
blapi_SHVerify(const char *name, PRFuncPtr addr, PRBool self)
{
    PRBool result = PR_FALSE; /* if anything goes wrong,
                   * the signature does not verify */
    /* find our shared library name */
    char *shName = PR_GetLibraryFilePathname(name, addr);
    if (!shName) {
        goto loser;
    }
    result = blapi_SHVerifyFile(shName, self);

loser:
    if (shName != NULL) {
        PR_Free(shName);
    }

    return result;
}

PRBool
BLAPI_SHVerify(const char *name, PRFuncPtr addr)
{
    return blapi_SHVerify(name, addr, PR_FALSE);
}

PRBool
BLAPI_SHVerifyFile(const char *shName)
{
    return blapi_SHVerifyFile(shName, PR_FALSE);
}

#ifndef NSS_STRICT_INTEGRITY
/* This allows checks with old shlibsign .chk files. If NSS_STRICT_INTEGRITY
 * is set, we don't accept DSA */
static PRBool
blapi_SHVerifyDSACheck(PRFileDesc *shFD, const SECHashObject *hashObj,
                       DSAPublicKey *key, const SECItem *signature)
{
    void *hashcx = NULL;
    SECItem hash;
    int bytesRead;
    unsigned char hashBuf[HASH_LENGTH_MAX];
    unsigned char buf[4096];
    SECStatus rv;

    hash.type = siBuffer;
    hash.data = hashBuf;
    hash.len = sizeof(hashBuf);

    /* hash our library file */
    hashcx = hashObj->create();
    if (hashcx == NULL) {
        return PR_FALSE;
    }
    hashObj->begin(hashcx);

    while ((bytesRead = PR_Read(shFD, buf, sizeof(buf))) > 0) {
        hashObj->update(hashcx, buf, bytesRead);
    }
    hashObj->end(hashcx, hash.data, &hash.len, hash.len);
    hashObj->destroy(hashcx, PR_TRUE);

    /* verify the hash against the check file */
    rv = DSA_VerifyDigest(key, signature, &hash);
    PORT_Memset(hashBuf, 0, sizeof hashBuf);
    return (rv == SECSuccess) ? PR_TRUE : PR_FALSE;
}
#endif

#ifdef NSS_STRICT_INTEGRITY
/* don't allow MD2, MD5, SHA1 or SHA224 as your integrity hash */
static PRBool
blapi_HashAllowed(SECHashObject *hashObj)
{
    switch (hashObj->type) {
        case HASH_AlgSHA256:
        case HASH_AlgSHA384:
        case HASH_AlgSHA512:
            return PR_TRUE;
        default:
            break;
    }
    return PR_FALSE;
}
#endif

static PRBool
blapi_SHVerifyHMACCheck(PRFileDesc *shFD, const SECHashObject *hashObj,
                        const SECItem *key, const SECItem *signature)
{
    HMACContext *hmaccx = NULL;
    SECItem hash;
    int bytesRead;
    unsigned char hashBuf[HASH_LENGTH_MAX];
    unsigned char buf[4096];
    SECStatus rv;
    PRBool result = PR_FALSE;

#ifdef NSS_STRICT_INTEGRITY
    if (!blapi_HashAllowed(hashObj)) {
        return PR_FALSE;
#endif

        hash.type = siBuffer;
        hash.data = hashBuf;
        hash.len = hashObj->length;

        /* create an hmac for the library file */
        hmaccx = HMAC_Create(hashObj, key->data, key->len, PR_TRUE);
        if (hmaccx == NULL) {
            return PR_FALSE;
        }
        HMAC_Begin(hmaccx);

        while ((bytesRead = PR_Read(shFD, buf, sizeof(buf))) > 0) {
            HMAC_Update(hmaccx, buf, bytesRead);
        }
        rv = HMAC_Finish(hmaccx, hash.data, &hash.len, hash.len);

        HMAC_Destroy(hmaccx, PR_TRUE);

        /* verify the hmac against the check file */
        if (rv == SECSuccess) {
            result = SECITEM_ItemsAreEqual(signature, &hash);
        }
        PORT_Memset(hashBuf, 0, sizeof hashBuf);
        return result;
    }

    static PRBool
    blapi_SHVerifyFile(const char *shName, PRBool self)
    {
        char *checkName = NULL;
        PRFileDesc *checkFD = NULL;
        PRFileDesc *shFD = NULL;
        const SECHashObject *hashObj = NULL;
        SECItem signature = { 0, NULL, 0 };
        int bytesRead, offset, type;
        SECStatus rv;
        SECItem hmacKey = { 0, NULL, 0 };
#ifdef FREEBL_USE_PRELINK
        int pid = 0;
#endif
        PRBool result = PR_FALSE; /* if anything goes wrong,
                   * the signature does not verify */
        NSSSignChkHeader header;
#ifndef NSS_STRICT_INTEGRITY
        DSAPublicKey key;

        PORT_Memset(&key, 0, sizeof(key));
#endif

        /* If our integrity check was never ran or failed, fail any other
     * integrity checks to prevent any token going into FIPS mode. */
        if (!self && (BL_FIPSEntryOK(PR_FALSE) != SECSuccess)) {
            return PR_FALSE;
        }

        if (!shName) {
            goto loser;
        }

        /* figure out the name of our check file */
        checkName = mkCheckFileName(shName);
        if (!checkName) {
            goto loser;
        }

        /* open the check File */
        checkFD = PR_Open(checkName, PR_RDONLY, 0);
        if (checkFD == NULL) {
#ifdef DEBUG_SHVERIFY
            fprintf(stderr, "Failed to open the check file %s: (%d, %d)\n",
                    checkName, (int)PR_GetError(), (int)PR_GetOSError());
#endif /* DEBUG_SHVERIFY */
            goto loser;
        }

        /* read and Verify the headerthe header */
        bytesRead = PR_Read(checkFD, &header, sizeof(header));
        if (bytesRead != sizeof(header)) {
            goto loser;
        }
        if ((header.magic1 != NSS_SIGN_CHK_MAGIC1) ||
            (header.magic2 != NSS_SIGN_CHK_MAGIC2)) {
            goto loser;
        }
        /* we've bumped the version number so that newly signed .check
     * files will fail nicely on old version of nss */
        if (header.majorVersion > NSS_SIGN_CHK_MAJOR_VERSION) {
            goto loser;
        }
        if (header.minorVersion < NSS_SIGN_CHK_MINOR_VERSION) {
            goto loser;
        }
        type = decodeInt(header.type);

        /* seek past any future header extensions */
        offset = decodeInt(header.offset);
        if (PR_Seek(checkFD, offset, PR_SEEK_SET) < 0) {
            goto loser;
        }

        switch (type) {
            case CKK_DSA:
#ifdef NSS_STRICT_INTEGRITY
                goto loser;
#else
            /* accept old dsa check files if NSS_STRICT_INTEGRITY is not set*/
            /* read the key */
            rv = readItem(checkFD, &key.params.prime);
            if (rv != SECSuccess) {
                goto loser;
            }
            rv = readItem(checkFD, &key.params.subPrime);
            if (rv != SECSuccess) {
                goto loser;
            }
            rv = readItem(checkFD, &key.params.base);
            if (rv != SECSuccess) {
                goto loser;
            }
            rv = readItem(checkFD, &key.publicValue);
            if (rv != SECSuccess) {
                goto loser;
            }
            /* read the signature */
            rv = readItem(checkFD, &signature);
            if (rv != SECSuccess) {
                goto loser;
            }
            hashObj = HASH_GetRawHashObject(PQG_GetHashType(&key.params));
            break;
#endif
            default:
                if ((type & NSS_SIGN_CHK_TYPE_FLAGS) != NSS_SIGN_CHK_FLAG_HMAC) {
                    goto loser;
                }
                /* read the HMAC Key */
                rv = readItem(checkFD, &hmacKey);
                if (rv != SECSuccess) {
                    goto loser;
                }
                /* read the siganture */
                rv = readItem(checkFD, &signature);
                if (rv != SECSuccess) {
                    goto loser;
                }
                hashObj = HASH_GetRawHashObject(type & ~NSS_SIGN_CHK_TYPE_FLAGS);
        }

        /* done with the check file */
        PR_Close(checkFD);
        checkFD = NULL;

        if (hashObj == NULL) {
            goto loser;
        }

/* open our library file */
#ifdef FREEBL_USE_PRELINK
        shFD = bl_OpenUnPrelink(shName, &pid);
#else
    shFD = PR_Open(shName, PR_RDONLY, 0);
#endif
        if (shFD == NULL) {
#ifdef DEBUG_SHVERIFY
            fprintf(stderr, "Failed to open the library file %s: (%d, %d)\n",
                    shName, (int)PR_GetError(), (int)PR_GetOSError());
#endif /* DEBUG_SHVERIFY */
            goto loser;
        }

        switch (type) {
            case CKK_DSA:
#ifndef NSS_STRICT_INTEGRITY
                result = blapi_SHVerifyDSACheck(shFD, hashObj, &key, &signature);
#endif
                break;
            default:
                if ((type & NSS_SIGN_CHK_TYPE_FLAGS) != NSS_SIGN_CHK_FLAG_HMAC) {
                    break;
                }
                result = blapi_SHVerifyHMACCheck(shFD, hashObj, &hmacKey, &signature);
                break;
        }

#ifdef FREEBL_USE_PRELINK
        bl_CloseUnPrelink(shFD, pid);
#else
    PR_Close(shFD);
#endif
        shFD = NULL;

    loser:
        PORT_Memset(&header, 0, sizeof header);
        if (checkName != NULL) {
            PORT_Free(checkName);
        }
        if (checkFD != NULL) {
            PR_Close(checkFD);
        }
        if (shFD != NULL) {
            PR_Close(shFD);
        }
        if (hmacKey.data != NULL) {
            SECITEM_ZfreeItem(&hmacKey, PR_FALSE);
        }
        if (signature.data != NULL) {
            SECITEM_ZfreeItem(&signature, PR_FALSE);
        }
#ifndef NSS_STRICT_INTEGRITY
        if (key.params.prime.data != NULL) {
            SECITEM_ZfreeItem(&key.params.prime, PR_FALSE);
        }
        if (key.params.subPrime.data != NULL) {
            SECITEM_ZfreeItem(&key.params.subPrime, PR_FALSE);
        }
        if (key.params.base.data != NULL) {
            SECITEM_ZfreeItem(&key.params.base, PR_FALSE);
        }
        if (key.publicValue.data != NULL) {
            SECITEM_ZfreeItem(&key.publicValue, PR_FALSE);
        }
#endif
        return result;
    }

    PRBool
    BLAPI_VerifySelf(const char *name)
    {
        if (name == NULL) {
            /*
         * If name is NULL, freebl is statically linked into softoken.
         * softoken will call BLAPI_SHVerify next to verify itself.
         */
            return PR_TRUE;
        }
        return blapi_SHVerify(name, (PRFuncPtr)decodeInt, PR_TRUE);
    }

#else /* NSS_FIPS_DISABLED */

PRBool
BLAPI_SHVerifyFile(const char *shName)
{
    return PR_FALSE;
}
PRBool
BLAPI_SHVerify(const char *name, PRFuncPtr addr)
{
    return PR_FALSE;
}
PRBool
BLAPI_VerifySelf(const char *name)
{
    return PR_FALSE;
}

#endif /* NSS_FIPS_DISABLED */
