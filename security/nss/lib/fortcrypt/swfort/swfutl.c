/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */
/*
 * This File includes utility functions used by cilib. and swfparse.c
 */

#include "prtypes.h"
#include "prsystem.h"
#include "prio.h"

#include "swforti.h"
#include "keyt.h"
/* #include "dh.h" */
#include "maci.h"
#include "secport.h"
#include "secrng.h"
#ifdef XP_OS2
#include <sys/stat.h>
#endif

#ifdef XP_WIN
#include <windows.h>
#include <winsock.h>
#include <direct.h>
#endif

/* no platform seem to agree on where this function is defined */
static char *local_index(char *source, char target) {
   while ((*source != target) && (*source != 0)) {
	source++;
   }
   return (*source != 0) ? source : NULL;
}

/*
 * Check to see if the index is ok, and that key is appropriately present or
 * absent.
 */
int
fort_KeyOK(FORTSWToken *token, int index, PRBool isPresent)
{
    if (index < 0) return CI_INV_KEY_INDEX;
    if (index >= KEY_REGISTERS) return CI_INV_KEY_INDEX;

    return (token->keyReg[index].present == isPresent) ? CI_OK : 
			(isPresent ? CI_NO_KEY : CI_REG_IN_USE);
}

/*
 * clear out a key register
 */
void
fort_ClearKey(FORTKeySlot *key)
{
    key->present = PR_FALSE;
    PORT_Memset(key->data, 0, sizeof (key->data));
    return;
}

/*
 * clear out an Ra register
 */
void
fort_ClearRaSlot(FORTRaRegisters *ra)
{
    PORT_Memset(ra->public, 0, sizeof(ra->public));
    PORT_Memset(ra->private, 0, sizeof(ra->private));
    return;
}

/*
 * provide a helper function to do all the loggin out functions.
 * NOTE: Logining in only happens in MACI_CheckPIN
 */
void
fort_Logout(FORTSWToken *token)
{
    int i;

    /* ditch all the stored keys */
    for (i=0; i < KEY_REGISTERS; i++) {
	fort_ClearKey(&token->keyReg[i]);
    }
    for (i=0; i < MAX_RA_SLOTS; i++) {
	fort_ClearRaSlot(&token->RaValues[i]);
    }

    /* mark as logged out */
    token->login = PR_FALSE;
    token->certIndex = 0;
    token->key = 0;
    return;
}

/*
 * update the new IV value based on the current cipherText (should be the last 
 * block of the cipher text).
 */
int
fort_UpdateIV(unsigned char *cipherText, unsigned int size,unsigned char *IV)
{
    if (size == 0) return CI_INV_SIZE;
    if ((size & (SKIPJACK_BLOCK_SIZE-1)) != 0) return CI_INV_SIZE;
    size -= SKIPJACK_BLOCK_SIZE;
    PORT_Memcpy(IV,&cipherText[size],SKIPJACK_BLOCK_SIZE);
    return CI_OK;
}


/*
 * verify that we have a card initialized, and if necessary, logged in.
 */
int
fort_CardExists(FORTSWToken *token,PRBool needLogin)
{
    if (token == NULL ) return CI_LIB_NOT_INIT; 
    if (token->config_file == NULL) return CI_NO_CARD;
    if (needLogin && !token->login) return CI_INV_STATE;
    return CI_OK;
}

/*
 * walk down the cert slot entries, counting them.
 * return that count.
 */
int
fort_GetCertCount(FORTSWFile *file)
{
    int i;

    if (file->slotEntries == NULL) return 0;

    for (i=0; file->slotEntries[i]; i++) 
	/* no body */ ;

    return i;
}

/*
 * copy an unsigned SECItem to a signed SecItem. (if the high bit is on,
 * pad with a leading 0.
 */
SECStatus
fort_CopyUnsigned(PRArenaPool *arena, SECItem *to, const SECItem *from)
{
    int offset = 0;

    if (from->data && from->len) {
	if (from->data[0] & 0x80) offset = 1;
	if ( arena ) {
	    to->data = (unsigned char*) PORT_ArenaZAlloc(arena,
							 from->len+offset);
	} else {
	    to->data = (unsigned char*) PORT_ZAlloc(from->len+offset);
	}
	
	if (!to->data) {
	    return SECFailure;
	}
	PORT_Memcpy(to->data+offset, from->data, from->len);
	to->len = from->len+offset;
    } else {
	to->data = 0;
	to->len = 0;
    }
    return SECSuccess;
}

/*
 * NOTE: these keys do not have the public values, and cannot be used to
 * extract the public key from the private key. Since we never do this in
 * this code, and this function is static, we're reasonably safe (as long as
 * any of your callees do not try to extract the public value as well).
 * Also -- the token must be logged in before this function is called.
 */
FORTEZZAPrivateKey *
fort_GetPrivKey(FORTSWToken *token,FORTEZZAKeyType keyType,
						fortSlotEntry *certEntry)
{
    FORTEZZAPrivateKey *returnKey = NULL;
    SECStatus rv = SECFailure;
    PRArenaPool *poolp;
    fortKeyInformation *keyInfo = NULL;
    unsigned char *keyData;
    int len, ret;


    /* select the right keyinfo */
    switch (keyType) {
    case fortezzaDSAKey:
	keyInfo = certEntry->signatureKeyInformation;
	if (keyInfo ==  NULL) keyInfo = certEntry->exchangeKeyInformation;
	break;
    case fortezzaDHKey:
	keyInfo = certEntry->exchangeKeyInformation;
	if (keyInfo == NULL) keyInfo = certEntry->signatureKeyInformation;
	break;
    }

    /* if we don't have any key information, blow out of here */
    if (keyInfo == NULL) return NULL;

    poolp = PORT_NewArena(2048);
    if(!poolp) {
	return NULL;
    }

    returnKey = (FORTEZZAPrivateKey*)PORT_ArenaZAlloc(poolp, sizeof(FORTEZZAPrivateKey));
    if(!returnKey) {
	rv = SECFailure;
	goto loser;
    }

    returnKey->keyType = keyType;
    returnKey->arena = poolp;

    /*
     * decrypt the private key
     */
    len = keyInfo->privateKeyWrappedWithKs.len;
    keyData = PORT_ArenaZAlloc(poolp,len);
    if (keyData == NULL) {
	rv = SECFailure;
	goto loser;
    }
    /* keys must be 160 bits (20 bytes) if that's not the case the Unwrap will
     * fail.. */
    ret = fort_skipjackUnwrap(token->keyReg[0].data, len, 
			keyInfo->privateKeyWrappedWithKs.data, keyData);
    if (ret != CI_OK) {
	rv = SECFailure;
	goto loser;
    }

    switch(keyType) {
	case dsaKey:
	    returnKey->u.dsa.privateValue.data = keyData;
	    returnKey->u.dsa.privateValue.len = 20;
	    returnKey->u.dsa.params.arena = poolp;
	    rv = fort_CopyUnsigned(poolp, &(returnKey->u.dsa.params.prime),
					&(keyInfo->p));
	    if(rv != SECSuccess) break;
	    rv = fort_CopyUnsigned(poolp, &(returnKey->u.dsa.params.subPrime),
					&(keyInfo->q));
	    if(rv != SECSuccess) break;
	    rv = fort_CopyUnsigned(poolp, &(returnKey->u.dsa.params.base),
					&(keyInfo->g));
	    if(rv != SECSuccess) break;
	    break;
	case dhKey:
	    returnKey->u.dh.arena = poolp;
	    returnKey->u.dh.privateValue.data = keyData;
	    returnKey->u.dh.privateValue.len = 20;
	    rv = fort_CopyUnsigned(poolp, &(returnKey->u.dh.prime),
					&(keyInfo->p));
	    if(rv != SECSuccess) break;
	    rv = fort_CopyUnsigned(poolp, &(returnKey->u.dh.base),
					&(keyInfo->g));
	    if(rv != SECSuccess) break;
	    rv = SECSuccess;
	    break;
	default:
	    rv = SECFailure;
    }

loser:

    if(rv != SECSuccess) {
	PORT_FreeArena(poolp, PR_TRUE);
	returnKey = NULL;
    }

    return returnKey;
}


void
fort_DestroyPrivateKey(FORTEZZAPrivateKey *key)
{
    if (key && key->arena) {
	PORT_FreeArena(key->arena, PR_TRUE);
    }
}

/*
 * find a particulare certificate entry from the config
 * file.
 */
fortSlotEntry *
fort_GetCertEntry(FORTSWFile *file,int index)
{
    /* search for the index */
    int i,count= fort_GetCertCount(file);

    /* make sure the given index exists & has key material */
    for (i=0; i < count ;i ++) {
	if (file->slotEntries[i]->certIndex == index) {
	    return file->slotEntries[i];
	}
    }
    return NULL;
}

/*
 * use the token to determine it's CI_State.
 */
CI_STATE
fort_GetState(FORTSWToken *token)
{
    /* no file? then the token has not been initialized */
    if (!token->config_file) {
	return CI_UNINITIALIZED;
    }
    /* we're initialized, are we logged in (CI_USER_INITIALIZED is not logged
     * in) */
    if (!token->login) {
	return CI_USER_INITIALIZED;
    }
    /* We're logged in, do we have a personality set */
    if (token->certIndex) {
	return CI_READY;
    }
    /* We're logged in, with no personality set */
    return CI_STANDBY;
}

/*
 * find the private ra value for a given public Ra value.
 */
fortRaPrivatePtr
fort_LookupPrivR(FORTSWToken *token,CI_RA Ra)
{
    int i;

    /* probably a more efficient way of doing this would be to search first
     * several entries before nextRa (or search backwards from next Ra)
     */
    for (i=0; i < MAX_RA_SLOTS; i++) {
	if (PORT_Memcmp(token->RaValues[i].public,Ra,CI_RA_SIZE) == 0) {
	    return token->RaValues[i].private;
	}
    }
    return NULL;
}

/*
 * go add more noise to the random number generator
 */
void
fort_AddNoise(void)
{
    unsigned char seed[20];

    /* note: GetNoise doesn't always get 20 bytes, but adding more
     * random data from the stack doesn't subtract entropy from the
     * Random number generator, so just send it all.
     */
    RNG_GetNoise(seed,sizeof(seed));
    RNG_RandomUpdate(seed,sizeof(seed));
}

/*
 * Get a random number
 */
int
fort_GenerateRandom(unsigned char *buf, int bytes)
{
    SECStatus rv;

    fort_AddNoise();
    rv = RNG_GenerateGlobalRandomBytes(buf,bytes);
    if (rv != SECSuccess) return CI_EXEC_FAIL;
    return CI_OK;
}

/*
 * NOTE: that MAC is missing below.
 */
#if defined (XP_UNIX) || defined (XP_OS2)
#ifdef XP_UNIX
#define NS_PATH_SEP ':'
#define NS_DIR_SEP '/'
#define NS_DEFAULT_PATH ".:/bin/netscape:/etc/netscape/:/etc"
#endif

#ifdef XP_OS2		/* for OS/2 */
#define NS_PATH_SEP ';'
#define NS_DIR_SEP '\\'
#define NS_DEFAULT_PATH ".:\\bin\\netscape:\\etc\\netscape\\:\\etc"
#endif

PRInt32
local_getFileInfo(const char *fn, PRFileInfo *info)
{
    PRInt32 rv;
    struct stat sb;

    rv = stat(fn, &sb);
    if (rv < 0)
	return -1;
    else if (NULL != info)
    {
        if (S_IFREG & sb.st_mode)
            info->type = PR_FILE_FILE;
        else if (S_IFDIR & sb.st_mode)
            info->type = PR_FILE_DIRECTORY;
        else
            info->type = PR_FILE_OTHER;

#if defined(OSF1)
        if (0x7fffffffLL < sb.st_size)
        {
            return -1;
        }
#endif /* defined(OSF1) */
        info->size = sb.st_size;

    }
    return rv;
}
#endif /* UNIX & OS/2 */

#ifdef XP_WIN
#define NS_PATH_SEP ';'
#define NS_DIR_SEP '\\'
#define NS_DEFAULT_PATH ".;c:\\program files\\netscape\\communicator\\program\\pkcs11\\netscape;c:\\netscape\\communicator\\program\\pkcs11\\netscape;c:\\windows\\system"


/*
 * Since we're a pkcs #11 module, we may get 
 * loaded into lots of different binaries, each with different or no versions
 * of NSPR running... so we copy the one function we need.
 */

#define _PR_IS_SLASH(ch) ((ch) == '/' || (ch) == '\\')

/*
 * IsRootDirectory --
 *
 * Return PR_TRUE if the pathname 'fn' is a valid root directory,
 * else return PR_FALSE.  The char buffer pointed to by 'fn' must
 * be writable.  During the execution of this function, the contents
 * of the buffer pointed to by 'fn' may be modified, but on return
 * the original contents will be restored.  'buflen' is the size of
 * the buffer pointed to by 'fn'.
 *
 * Root directories come in three formats:
 * 1. / or \, meaning the root directory of the current drive.
 * 2. C:/ or C:\, where C is a drive letter.
 * 3. \\<server name>\<share point name>\ or
 *    \\<server name>\<share point name>, meaning the root directory
 *    of a UNC (Universal Naming Convention) name.
 */

static PRBool
IsRootDirectory(char *fn, size_t buflen)
{
    char *p;
    PRBool slashAdded = PR_FALSE;
    PRBool rv = PR_FALSE;

    if (_PR_IS_SLASH(fn[0]) && fn[1] == '\0') {
        return PR_TRUE;
    }

    if (isalpha(fn[0]) && fn[1] == ':' && _PR_IS_SLASH(fn[2])
            && fn[3] == '\0') {
        rv = GetDriveType(fn) > 1 ? PR_TRUE : PR_FALSE;
        return rv;
    }

    /* The UNC root directory */

    if (_PR_IS_SLASH(fn[0]) && _PR_IS_SLASH(fn[1])) {
        /* The 'server' part should have at least one character. */
        p = &fn[2];
        if (*p == '\0' || _PR_IS_SLASH(*p)) {
            return PR_FALSE;
        }

        /* look for the next slash */
        do {
            p++;
        } while (*p != '\0' && !_PR_IS_SLASH(*p));
        if (*p == '\0') {
            return PR_FALSE;
        }

        /* The 'share' part should have at least one character. */
        p++;
        if (*p == '\0' || _PR_IS_SLASH(*p)) {
            return PR_FALSE;
        }

        /* look for the final slash */
        do {
            p++;
        } while (*p != '\0' && !_PR_IS_SLASH(*p));
        if (_PR_IS_SLASH(*p) && p[1] != '\0') {
            return PR_FALSE;
        }
        if (*p == '\0') {
            /*
             * GetDriveType() doesn't work correctly if the
             * path is of the form \\server\share, so we add
             * a final slash temporarily.
             */
            if ((p + 1) < (fn + buflen)) {
                *p++ = '\\';
                *p = '\0';
                slashAdded = PR_TRUE;
            } else {
                return PR_FALSE; /* name too long */
            }
        }
        rv = GetDriveType(fn) > 1 ? PR_TRUE : PR_FALSE;
        /* restore the 'fn' buffer */
        if (slashAdded) {
            *--p = '\0';
        }
    }
    return rv;
}

PRInt32
local_getFileInfo(const char *fn, PRFileInfo *info)
{
    HANDLE hFindFile;
    WIN32_FIND_DATA findFileData;
    char pathbuf[MAX_PATH + 1];
    
    if (NULL == fn || '\0' == *fn) {
        return -1;
    }

    /*
     * FindFirstFile() expands wildcard characters.  So
     * we make sure the pathname contains no wildcard.
     */
    if (NULL != strpbrk(fn, "?*")) {
        return -1;
    }

    hFindFile = FindFirstFile(fn, &findFileData);
    if (INVALID_HANDLE_VALUE == hFindFile) {
        DWORD len;
        char *filePart;

        /*
         * FindFirstFile() does not work correctly on root directories.
         * It also doesn't work correctly on a pathname that ends in a
         * slash.  So we first check to see if the pathname specifies a
         * root directory.  If not, and if the pathname ends in a slash,
         * we remove the final slash and try again.
         */

        /*
         * If the pathname does not contain ., \, and /, it cannot be
         * a root directory or a pathname that ends in a slash.
         */
        if (NULL == strpbrk(fn, ".\\/")) {
            return -1;
        } 
        len = GetFullPathName(fn, sizeof(pathbuf), pathbuf,
                &filePart);
        if (len > sizeof(pathbuf)) {
            return -1;
        }
        if (IsRootDirectory(pathbuf, sizeof(pathbuf))) {
            info->type = PR_FILE_DIRECTORY;
            info->size = 0;
            /*
             * These timestamps don't make sense for root directories.
             */
            info->modifyTime = 0;
            info->creationTime = 0;
            return 0;
        }
        if (!((pathbuf[len - 1] == '/') || (pathbuf[len-1] == '\\'))) {
            return -1;
        } else {
            pathbuf[len - 1] = '\0';
            hFindFile = FindFirstFile(pathbuf, &findFileData);
            if (INVALID_HANDLE_VALUE == hFindFile) {
                return -1;
            }
        }
    }

    FindClose(hFindFile);

    if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        info->type = PR_FILE_DIRECTORY;
    } else {
        info->type = PR_FILE_FILE;
    }

    info->size = findFileData.nFileSizeLow;

    return 0;
}

#endif /* XP_WIN */

#ifdef XP_MAC
#error Need to write fort_FindFileInPath for Mac
#define NS_PATH_SEP ','
#define NS_DIR_SEP ':'
#define NS_DEFAULT_PATH ",System Folder,System Folder:Netscape f:pkcs11:netscape"
#endif

#define NETSCAPE_INIT_FILE "nsswft.swf"

/*
 * OK, We're deep in the bottom of MACI and PKCS #11... We need to
 * find our fortezza key file. We have no clue of where the our binary lives
 * or where our key file lives. This function lets us search manual paths 
 * to find our key file.
 */
char *fort_FindFileInPath(char *path, char *fn)
{
    char *next;
    char *holdData;
    char *ret = NULL;
    int len = 0;
    int fn_len = PORT_Strlen(fn)+1; /* include the NULL */
    PRFileInfo info;
    char dirSep = NS_DIR_SEP;

    holdData = PORT_Alloc(strlen(path)+1+fn_len);

    while ((next = local_index(path,NS_PATH_SEP)) != NULL) {
	len = next - path;

	PORT_Memcpy(holdData,path,len);
	if ((len != 0) && (holdData[len-1] != dirSep)) {
	  PORT_Memcpy(&holdData[len],&dirSep,1);
	  len++;
	}
	PORT_Memcpy(&holdData[len],fn,fn_len);
	
	if ((local_getFileInfo(holdData,&info) == 0) &&
		(info.type == PR_FILE_FILE) && (info.size != 0)) {
	    ret = PORT_Strdup(holdData);
	    PORT_Free(holdData);
	    return ret;
	}
	path = next+1;
    }

    len = strlen(path);
    PORT_Memcpy(holdData,path,len);
    if ((len != 0) && (holdData[len-1] != dirSep)) {
	  PORT_Memcpy(&holdData[len],&dirSep,1);
	  len++;
    }
    PORT_Memcpy(&holdData[len],fn,fn_len);
	
    if ((local_getFileInfo(holdData,&info) == 0) &&
		(info.type == PR_FILE_FILE) && (info.size != 0)) {
	    ret = PORT_Strdup(holdData);
    }
    PORT_Free(holdData);
    return ret;
}

static char *path_table[] = {
   "PATH","LD_LIBRARY_PATH","LIBPATH"
};

static int path_table_size = sizeof(path_table)/sizeof(path_table[0]);

char *fort_LookupFORTEZZAInitFile(void)
{
    char *fname = NULL;
    char *home = NULL;
#ifdef XP_UNIX
    char unix_home[512];
#endif
    int i;

    /* first try to get it from the environment */
    fname = getenv("SW_FORTEZZA_FILE");
    if (fname != NULL) {
	return PORT_Strdup(fname);
    }

#ifdef XP_UNIX
    home = getenv("HOME");
    if (home) {
	strncpy(unix_home,home, sizeof(unix_home)-sizeof("/.netscape"));
	strcat(unix_home,"/.netscape");
	fname = fort_FindFileInPath(unix_home,NETSCAPE_INIT_FILE);
	if (fname) return fname;
    }
#endif
#ifdef XP_WIN
    home = getenv("windir");
    if (home) {
	fname = fort_FindFileInPath(home,NETSCAPE_INIT_FILE);
	if (fname) return fname;
    }
#endif
	
    fname = fort_FindFileInPath(NS_DEFAULT_PATH,NETSCAPE_INIT_FILE);
    if (fname) return fname;

    /* now search the system paths */
    for (i=0; i < path_table_size; i++) {
    	char *path = getenv(path_table[i]);

	if (path != NULL) {
	    fname = fort_FindFileInPath(path,NETSCAPE_INIT_FILE);
	    if (fname) return fname;
	}
    }


    return NULL;
}
