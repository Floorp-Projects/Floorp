/*
 * loader.c - load platform dependent DSO containing freebl implementation.
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dr Vipul Gupta <vipul.gupta@sun.com>, Sun Microsystems Laboratories
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
/* $Id: loader.c,v 1.24 2005/09/16 16:59:22 wtchang%redhat.com Exp $ */

#include "loader.h"
#include "prmem.h"
#include "prerror.h"
#include "prinit.h"

static const char* default_name =
    SHLIB_PREFIX"freebl"SHLIB_VERSION"."SHLIB_SUFFIX;

/* getLibName() returns the name of the library to load. */

#if defined(SOLARIS) && defined(__sparc)
#include <stddef.h>
#include <strings.h>
#include <sys/systeminfo.h>


#if defined(NSS_USE_64)

const static char fast_hybrid_shared_lib[] = "libfreebl_64fpu_3.so";
const static char slow_hybrid_shared_lib[] = "libfreebl_64int_3.so";
const static char  non_hybrid_shared_lib[] = "libfreebl_64fpu_3.so";

const static char slow_hybrid_isa[] = "sparcv9";
const static char fast_hybrid_isa[] = "sparcv9+vis";

#else

const static char fast_hybrid_shared_lib[] = "libfreebl_32fpu_3.so";
const static char slow_hybrid_shared_lib[] = "libfreebl_32int64_3.so";
const static char  non_hybrid_shared_lib[] = "libfreebl_32int_3.so";

const static char slow_hybrid_isa[] = "sparcv8plus";
const static char fast_hybrid_isa[] = "sparcv8plus+vis";

#endif

static const char *
getLibName(void)
{
    char * found_slow_hybrid;
    char * found_fast_hybrid;
    long buflen;
    char buf[256];

    buflen = sysinfo(SI_ISALIST, buf, sizeof buf);
    if (buflen <= 0) 
	return NULL;
    found_slow_hybrid = strstr(buf, slow_hybrid_isa);
    found_fast_hybrid = strstr(buf, fast_hybrid_isa);
    if (found_fast_hybrid && 
	(!found_slow_hybrid ||
	 (found_slow_hybrid - found_fast_hybrid) >= 0)) {
	return fast_hybrid_shared_lib;
    }
    if (found_slow_hybrid) {
	return slow_hybrid_shared_lib;
    }
    return non_hybrid_shared_lib;
}

#elif defined(HPUX) && !defined(NSS_USE_64) && !defined(__ia64)
/* This code tests to see if we're running on a PA2.x CPU.
** It returns true (1) if so, and false (0) otherwise.
*/
static const char *
getLibName(void)
{
    long cpu = sysconf(_SC_CPU_VERSION);
    return (cpu == CPU_PA_RISC2_0) 
		? "libfreebl_32fpu_3.sl"
	        : "libfreebl_32int32_3.sl" ;
}
#else
/* default case, for platforms/ABIs that have only one freebl shared lib. */
static const char * getLibName(void) { return default_name; }
#endif

/*
 * We use PR_GetLibraryFilePathname to get the pathname of the loaded 
 * shared lib that contains this function, and then do a PR_LoadLibrary
 * with an absolute pathname for the freebl shared library.
 */

#include "prio.h"
#include "prprf.h"
#include <stdio.h>
#include "prsystem.h"

const char* softoken=SHLIB_PREFIX"softokn"SOFTOKEN_SHLIB_VERSION"."SHLIB_SUFFIX;

typedef struct {
    PRLibrary *dlh;
} BLLibrary;

static BLLibrary *
bl_LoadLibrary(const char *name)
{
    BLLibrary *lib = NULL;
    PRFuncPtr fn_addr;
    char* softokenPath = NULL;
    char* fullName = NULL;
    PRLibSpec libSpec;

    libSpec.type = PR_LibSpec_Pathname;
    lib = PR_NEWZAP(BLLibrary);
    if (NULL == lib) {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
        return NULL;
    }

    /* Get the pathname for the loaded libsoftokn, i.e. /usr/lib/libsoftokn3.so
     * PR_GetLibraryFilePathname works with either the base library name or a
     * function pointer, depending on the platform. We can't query an exported
     * symbol such as NSC_GetFunctionList, because on some platforms we can't
     * find symbols in loaded implicit dependencies such as libsoftokn.
     * But we can just get the address of this function !
     */
    fn_addr = (PRFuncPtr) &bl_LoadLibrary;
    softokenPath = PR_GetLibraryFilePathname(softoken, fn_addr);

    /* Remove "libsoftokn" from the pathname and add the freebl libname */
    if (softokenPath) {
       char* c = strrchr(softokenPath, PR_GetDirectorySeparator());
       if (c) {
           size_t softoknPathSize = 1 + c - softokenPath;
           fullName = (char*) PORT_Alloc(strlen(name) + softoknPathSize + 1);
           if (fullName) {
               memcpy(fullName, softokenPath, softoknPathSize);
               strcpy(fullName + softoknPathSize, name); 
           }
       }
       PR_Free(softokenPath);
    }
    if (fullName) {
#ifdef DEBUG_LOADER
        PR_fprintf(PR_STDOUT, "\nAttempting to load fully-qualified %s\n", 
                   fullName);
#endif
        libSpec.value.pathname = fullName;
        lib->dlh = PR_LoadLibraryWithFlags(libSpec, PR_LD_NOW | PR_LD_LOCAL);
        PORT_Free(fullName);
    }
    if (!lib->dlh) {
#ifdef DEBUG_LOADER
        PR_fprintf(PR_STDOUT, "\nAttempting to load %s\n", name);
#endif
        libSpec.value.pathname = name;
        lib->dlh = PR_LoadLibraryWithFlags(libSpec, PR_LD_NOW | PR_LD_LOCAL);
    }
    if (NULL == lib->dlh) {
#ifdef DEBUG_LOADER
        PR_fprintf(PR_STDOUT, "\nLoading failed : %s.\n", name);
#endif
        PR_Free(lib);
        lib = NULL;
    }
    return lib;
}

static void *
bl_FindSymbol(BLLibrary *lib, const char *name)
{
    void *f;

    f = PR_FindSymbol(lib->dlh, name);
    return f;
}

static PRStatus
bl_UnloadLibrary(BLLibrary *lib)
{
    if (PR_SUCCESS != PR_UnloadLibrary(lib->dlh)) {
        return PR_FAILURE;
    }
    PR_Free(lib);
    return PR_SUCCESS;
}

#define LSB(x) ((x)&0xff)
#define MSB(x) ((x)>>8)

static const FREEBLVector *vector;
static const char *libraryName = NULL;

/* This function must be run only once. */
/*  determine if hybrid platform, then actually load the DSO. */
static PRStatus
freebl_LoadDSO( void ) 
{
  BLLibrary *  handle;
  const char * name = getLibName();

  if (!name) {
    PR_SetError(PR_LOAD_LIBRARY_ERROR, 0);
    return PR_FAILURE;
  }

  handle = bl_LoadLibrary(name);
  if (handle) {
    void * address = bl_FindSymbol(handle, "FREEBL_GetVector");
    if (address) {
      FREEBLGetVectorFn  * getVector = (FREEBLGetVectorFn *)address;
      const FREEBLVector * dsoVector = getVector();
      if (dsoVector) {
	unsigned short dsoVersion = dsoVector->version;
	unsigned short  myVersion = FREEBL_VERSION;
	if (MSB(dsoVersion) == MSB(myVersion) && 
	    LSB(dsoVersion) >= LSB(myVersion) &&
	    dsoVector->length >= sizeof(FREEBLVector)) {
          vector = dsoVector;
	  libraryName = name;
	  return PR_SUCCESS;
	}
      }
    }
    bl_UnloadLibrary(handle);
  }
  return PR_FAILURE;
}

static PRStatus
freebl_RunLoaderOnce( void )
{
  PRStatus status;
  static PRCallOnceType once;

  status = PR_CallOnce(&once, &freebl_LoadDSO);
  return status;
}


RSAPrivateKey * 
RSA_NewKey(int keySizeInBits, SECItem * publicExponent)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return NULL;
  return (vector->p_RSA_NewKey)(keySizeInBits, publicExponent);
}

SECStatus 
RSA_PublicKeyOp(RSAPublicKey *   key,
				 unsigned char *  output,
				 const unsigned char *  input)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_RSA_PublicKeyOp)(key, output, input);
}

SECStatus 
RSA_PrivateKeyOp(RSAPrivateKey *  key,
				  unsigned char *  output,
				  const unsigned char *  input)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_RSA_PrivateKeyOp)(key, output, input);
}

SECStatus
RSA_PrivateKeyOpDoubleChecked(RSAPrivateKey *key,
                              unsigned char *output,
                              const unsigned char *input)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_RSA_PrivateKeyOpDoubleChecked)(key, output, input);
}

SECStatus
RSA_PrivateKeyCheck(RSAPrivateKey *key)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_RSA_PrivateKeyCheck)(key);
}

SECStatus 
DSA_NewKey(const PQGParams * params, DSAPrivateKey ** privKey)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_DSA_NewKey)(params, privKey);
}

SECStatus 
DSA_SignDigest(DSAPrivateKey * key, SECItem * signature, const SECItem * digest)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_DSA_SignDigest)( key,  signature,  digest);
}

SECStatus 
DSA_VerifyDigest(DSAPublicKey * key, const SECItem * signature, 
                 const SECItem * digest)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_DSA_VerifyDigest)( key,  signature,  digest);
}

SECStatus 
DSA_NewKeyFromSeed(const PQGParams *params, const unsigned char * seed,
                   DSAPrivateKey **privKey)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_DSA_NewKeyFromSeed)(params,  seed, privKey);
}

SECStatus 
DSA_SignDigestWithSeed(DSAPrivateKey * key, SECItem * signature,
		       const SECItem * digest, const unsigned char * seed)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_DSA_SignDigestWithSeed)( key, signature, digest, seed);
}

SECStatus 
DH_GenParam(int primeLen, DHParams ** params)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_DH_GenParam)(primeLen, params);
}

SECStatus 
DH_NewKey(DHParams * params, DHPrivateKey ** privKey)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_DH_NewKey)( params, privKey);
}

SECStatus 
DH_Derive(SECItem * publicValue, SECItem * prime, SECItem * privateValue, 
			 SECItem * derivedSecret, unsigned int maxOutBytes)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_DH_Derive)( publicValue, prime, privateValue, 
				derivedSecret, maxOutBytes);
}

SECStatus 
KEA_Derive(SECItem *prime, SECItem *public1, SECItem *public2, 
	   SECItem *private1, SECItem *private2, SECItem *derivedSecret)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_KEA_Derive)(prime, public1, public2, 
	                        private1, private2, derivedSecret);
}

PRBool 
KEA_Verify(SECItem *Y, SECItem *prime, SECItem *subPrime)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return PR_FALSE;
  return (vector->p_KEA_Verify)(Y, prime, subPrime);
}

RC4Context *
RC4_CreateContext(const unsigned char *key, int len)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return NULL;
  return (vector->p_RC4_CreateContext)(key, len);
}

void 
RC4_DestroyContext(RC4Context *cx, PRBool freeit)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return;
  (vector->p_RC4_DestroyContext)(cx, freeit);
}

SECStatus 
RC4_Encrypt(RC4Context *cx, unsigned char *output, unsigned int *outputLen, 
	    unsigned int maxOutputLen, const unsigned char *input, 
	    unsigned int inputLen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_RC4_Encrypt)(cx, output, outputLen, maxOutputLen, input, 
	                         inputLen);
}

SECStatus 
RC4_Decrypt(RC4Context *cx, unsigned char *output, unsigned int *outputLen, 
	    unsigned int maxOutputLen, const unsigned char *input, 
	    unsigned int inputLen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_RC4_Decrypt)(cx, output, outputLen, maxOutputLen, input, 
	                         inputLen);
}

RC2Context *
RC2_CreateContext(const unsigned char *key, unsigned int len,
		  const unsigned char *iv, int mode, unsigned effectiveKeyLen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return NULL;
  return (vector->p_RC2_CreateContext)(key, len, iv, mode, effectiveKeyLen);
}

void 
RC2_DestroyContext(RC2Context *cx, PRBool freeit)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return;
  (vector->p_RC2_DestroyContext)(cx, freeit);
}

SECStatus 
RC2_Encrypt(RC2Context *cx, unsigned char *output, unsigned int *outputLen, 
	    unsigned int maxOutputLen, const unsigned char *input, 
	    unsigned int inputLen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_RC2_Encrypt)(cx, output, outputLen, maxOutputLen, input, 
	                         inputLen);
}

SECStatus 
RC2_Decrypt(RC2Context *cx, unsigned char *output, unsigned int *outputLen, 
	    unsigned int maxOutputLen, const unsigned char *input, 
	    unsigned int inputLen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_RC2_Decrypt)(cx, output, outputLen, maxOutputLen, input, 
	                         inputLen);
}

RC5Context *
RC5_CreateContext(const SECItem *key, unsigned int rounds,
		  unsigned int wordSize, const unsigned char *iv, int mode)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return NULL;
  return (vector->p_RC5_CreateContext)(key, rounds, wordSize, iv, mode);
}

void 
RC5_DestroyContext(RC5Context *cx, PRBool freeit)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return;
  (vector->p_RC5_DestroyContext)(cx, freeit);
}

SECStatus 
RC5_Encrypt(RC5Context *cx, unsigned char *output, unsigned int *outputLen, 
	    unsigned int maxOutputLen, const unsigned char *input, 
	    unsigned int inputLen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_RC5_Encrypt)(cx, output, outputLen, maxOutputLen, input, 
				 inputLen);
}

SECStatus 
RC5_Decrypt(RC5Context *cx, unsigned char *output, unsigned int *outputLen, 
	    unsigned int maxOutputLen, const unsigned char *input, 
	    unsigned int inputLen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_RC5_Decrypt)(cx, output, outputLen, maxOutputLen, input, 
	                         inputLen);
}

DESContext *
DES_CreateContext(const unsigned char *key, const unsigned char *iv,
		  int mode, PRBool encrypt)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return NULL;
  return (vector->p_DES_CreateContext)(key, iv, mode, encrypt);
}

void 
DES_DestroyContext(DESContext *cx, PRBool freeit)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return;
  (vector->p_DES_DestroyContext)(cx, freeit);
}

SECStatus 
DES_Encrypt(DESContext *cx, unsigned char *output, unsigned int *outputLen, 
	    unsigned int maxOutputLen, const unsigned char *input, 
	    unsigned int inputLen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_DES_Encrypt)(cx, output, outputLen, maxOutputLen, input, 
	                         inputLen);
}

SECStatus 
DES_Decrypt(DESContext *cx, unsigned char *output, unsigned int *outputLen, 
	    unsigned int maxOutputLen, const unsigned char *input, 
	    unsigned int inputLen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_DES_Decrypt)(cx, output, outputLen, maxOutputLen, input, 
	                         inputLen);
}

AESContext *
AES_CreateContext(const unsigned char *key, const unsigned char *iv, 
                  int mode, int encrypt,
                  unsigned int keylen, unsigned int blocklen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return NULL;
  return (vector->p_AES_CreateContext)(key, iv, mode, encrypt, keylen, 
				       blocklen);
}

void 
AES_DestroyContext(AESContext *cx, PRBool freeit)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return ;
  (vector->p_AES_DestroyContext)(cx, freeit);
}

SECStatus 
AES_Encrypt(AESContext *cx, unsigned char *output,
            unsigned int *outputLen, unsigned int maxOutputLen,
            const unsigned char *input, unsigned int inputLen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_AES_Encrypt)(cx, output, outputLen, maxOutputLen, 
				 input, inputLen);
}

SECStatus 
AES_Decrypt(AESContext *cx, unsigned char *output,
            unsigned int *outputLen, unsigned int maxOutputLen,
            const unsigned char *input, unsigned int inputLen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_AES_Decrypt)(cx, output, outputLen, maxOutputLen, 
				 input, inputLen);
}

SECStatus 
MD5_Hash(unsigned char *dest, const char *src)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_MD5_Hash)(dest, src);
}

SECStatus 
MD5_HashBuf(unsigned char *dest, const unsigned char *src, uint32 src_length)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_MD5_HashBuf)(dest, src, src_length);
}

MD5Context *
MD5_NewContext(void)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return NULL;
  return (vector->p_MD5_NewContext)();
}

void 
MD5_DestroyContext(MD5Context *cx, PRBool freeit)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return;
  (vector->p_MD5_DestroyContext)(cx, freeit);
}

void 
MD5_Begin(MD5Context *cx)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return;
  (vector->p_MD5_Begin)(cx);
}

void 
MD5_Update(MD5Context *cx, const unsigned char *input, unsigned int inputLen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return;
  (vector->p_MD5_Update)(cx, input, inputLen);
}

void 
MD5_End(MD5Context *cx, unsigned char *digest,
		    unsigned int *digestLen, unsigned int maxDigestLen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return;
  (vector->p_MD5_End)(cx, digest, digestLen, maxDigestLen);
}

unsigned int 
MD5_FlattenSize(MD5Context *cx)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return 0;
  return (vector->p_MD5_FlattenSize)(cx);
}

SECStatus 
MD5_Flatten(MD5Context *cx,unsigned char *space)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_MD5_Flatten)(cx, space);
}

MD5Context * 
MD5_Resurrect(unsigned char *space, void *arg)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return NULL;
  return (vector->p_MD5_Resurrect)(space, arg);
}

void 
MD5_TraceState(MD5Context *cx)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return ;
  (vector->p_MD5_TraceState)(cx);
}

SECStatus 
MD2_Hash(unsigned char *dest, const char *src)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_MD2_Hash)(dest, src);
}

MD2Context *
MD2_NewContext(void)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return NULL;
  return (vector->p_MD2_NewContext)();
}

void 
MD2_DestroyContext(MD2Context *cx, PRBool freeit)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return ;
  (vector->p_MD2_DestroyContext)(cx, freeit);
}

void 
MD2_Begin(MD2Context *cx)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return ;
  (vector->p_MD2_Begin)(cx);
}

void 
MD2_Update(MD2Context *cx, const unsigned char *input, unsigned int inputLen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return ;
  (vector->p_MD2_Update)(cx, input, inputLen);
}

void 
MD2_End(MD2Context *cx, unsigned char *digest,
		    unsigned int *digestLen, unsigned int maxDigestLen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return ;
  (vector->p_MD2_End)(cx, digest, digestLen, maxDigestLen);
}

unsigned int 
MD2_FlattenSize(MD2Context *cx)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return 0;
  return (vector->p_MD2_FlattenSize)(cx);
}

SECStatus 
MD2_Flatten(MD2Context *cx,unsigned char *space)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_MD2_Flatten)(cx, space);
}

MD2Context * 
MD2_Resurrect(unsigned char *space, void *arg)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return NULL;
  return (vector->p_MD2_Resurrect)(space, arg);
}


SECStatus 
SHA1_Hash(unsigned char *dest, const char *src)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_SHA1_Hash)(dest, src);
}

SECStatus 
SHA1_HashBuf(unsigned char *dest, const unsigned char *src, uint32 src_length)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_SHA1_HashBuf)(dest, src, src_length);
}

SHA1Context *
SHA1_NewContext(void)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return NULL;
  return (vector->p_SHA1_NewContext)();
}

void 
SHA1_DestroyContext(SHA1Context *cx, PRBool freeit)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return ;
  (vector->p_SHA1_DestroyContext)(cx, freeit);
}

void 
SHA1_Begin(SHA1Context *cx)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return ;
  (vector->p_SHA1_Begin)(cx);
}

void 
SHA1_Update(SHA1Context *cx, const unsigned char *input,
			unsigned int inputLen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return ;
  (vector->p_SHA1_Update)(cx, input, inputLen);
}

void 
SHA1_End(SHA1Context *cx, unsigned char *digest,
		     unsigned int *digestLen, unsigned int maxDigestLen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return ;
  (vector->p_SHA1_End)(cx, digest, digestLen, maxDigestLen);
}

void 
SHA1_TraceState(SHA1Context *cx)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return ;
  (vector->p_SHA1_TraceState)(cx);
}

unsigned int 
SHA1_FlattenSize(SHA1Context *cx)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return 0;
  return (vector->p_SHA1_FlattenSize)(cx);
}

SECStatus 
SHA1_Flatten(SHA1Context *cx,unsigned char *space)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_SHA1_Flatten)(cx, space);
}

SHA1Context * 
SHA1_Resurrect(unsigned char *space, void *arg)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return NULL;
  return (vector->p_SHA1_Resurrect)(space, arg);
}

SECStatus 
RNG_RNGInit(void)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_RNG_RNGInit)();
}

SECStatus 
RNG_RandomUpdate(const void *data, size_t bytes)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_RNG_RandomUpdate)(data, bytes);
}

SECStatus 
RNG_GenerateGlobalRandomBytes(void *dest, size_t len)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_RNG_GenerateGlobalRandomBytes)(dest, len);
}

void  
RNG_RNGShutdown(void)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return ;
  (vector->p_RNG_RNGShutdown)();
}

SECStatus
PQG_ParamGen(unsigned int j, PQGParams **pParams, PQGVerify **pVfy)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_PQG_ParamGen)(j, pParams, pVfy); 
}

SECStatus
PQG_ParamGenSeedLen( unsigned int j, unsigned int seedBytes, 
                     PQGParams **pParams, PQGVerify **pVfy)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_PQG_ParamGenSeedLen)(j, seedBytes, pParams, pVfy);
}

SECStatus   
PQG_VerifyParams(const PQGParams *params, const PQGVerify *vfy, 
		 SECStatus *result)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_PQG_VerifyParams)(params, vfy, result);
}

void 
BL_Cleanup(void)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return;
  (vector->p_BL_Cleanup)();
}

/* ============== New for 3.003 =============================== */

SECStatus 
SHA256_Hash(unsigned char *dest, const char *src)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_SHA256_Hash)(dest, src);
}

SECStatus 
SHA256_HashBuf(unsigned char *dest, const unsigned char *src, uint32 src_length)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_SHA256_HashBuf)(dest, src, src_length);
}

SHA256Context *
SHA256_NewContext(void)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return NULL;
  return (vector->p_SHA256_NewContext)();
}

void 
SHA256_DestroyContext(SHA256Context *cx, PRBool freeit)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return ;
  (vector->p_SHA256_DestroyContext)(cx, freeit);
}

void 
SHA256_Begin(SHA256Context *cx)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return ;
  (vector->p_SHA256_Begin)(cx);
}

void 
SHA256_Update(SHA256Context *cx, const unsigned char *input,
			unsigned int inputLen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return ;
  (vector->p_SHA256_Update)(cx, input, inputLen);
}

void 
SHA256_End(SHA256Context *cx, unsigned char *digest,
		     unsigned int *digestLen, unsigned int maxDigestLen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return ;
  (vector->p_SHA256_End)(cx, digest, digestLen, maxDigestLen);
}

void 
SHA256_TraceState(SHA256Context *cx)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return ;
  (vector->p_SHA256_TraceState)(cx);
}

unsigned int 
SHA256_FlattenSize(SHA256Context *cx)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return 0;
  return (vector->p_SHA256_FlattenSize)(cx);
}

SECStatus 
SHA256_Flatten(SHA256Context *cx,unsigned char *space)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_SHA256_Flatten)(cx, space);
}

SHA256Context * 
SHA256_Resurrect(unsigned char *space, void *arg)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return NULL;
  return (vector->p_SHA256_Resurrect)(space, arg);
}

SECStatus 
SHA512_Hash(unsigned char *dest, const char *src)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_SHA512_Hash)(dest, src);
}

SECStatus 
SHA512_HashBuf(unsigned char *dest, const unsigned char *src, uint32 src_length)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_SHA512_HashBuf)(dest, src, src_length);
}

SHA512Context *
SHA512_NewContext(void)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return NULL;
  return (vector->p_SHA512_NewContext)();
}

void 
SHA512_DestroyContext(SHA512Context *cx, PRBool freeit)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return ;
  (vector->p_SHA512_DestroyContext)(cx, freeit);
}

void 
SHA512_Begin(SHA512Context *cx)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return ;
  (vector->p_SHA512_Begin)(cx);
}

void 
SHA512_Update(SHA512Context *cx, const unsigned char *input,
			unsigned int inputLen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return ;
  (vector->p_SHA512_Update)(cx, input, inputLen);
}

void 
SHA512_End(SHA512Context *cx, unsigned char *digest,
		     unsigned int *digestLen, unsigned int maxDigestLen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return ;
  (vector->p_SHA512_End)(cx, digest, digestLen, maxDigestLen);
}

void 
SHA512_TraceState(SHA512Context *cx)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return ;
  (vector->p_SHA512_TraceState)(cx);
}

unsigned int 
SHA512_FlattenSize(SHA512Context *cx)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return 0;
  return (vector->p_SHA512_FlattenSize)(cx);
}

SECStatus 
SHA512_Flatten(SHA512Context *cx,unsigned char *space)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_SHA512_Flatten)(cx, space);
}

SHA512Context * 
SHA512_Resurrect(unsigned char *space, void *arg)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return NULL;
  return (vector->p_SHA512_Resurrect)(space, arg);
}


SECStatus 
SHA384_Hash(unsigned char *dest, const char *src)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_SHA384_Hash)(dest, src);
}

SECStatus 
SHA384_HashBuf(unsigned char *dest, const unsigned char *src, uint32 src_length)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_SHA384_HashBuf)(dest, src, src_length);
}

SHA384Context *
SHA384_NewContext(void)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return NULL;
  return (vector->p_SHA384_NewContext)();
}

void 
SHA384_DestroyContext(SHA384Context *cx, PRBool freeit)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return ;
  (vector->p_SHA384_DestroyContext)(cx, freeit);
}

void 
SHA384_Begin(SHA384Context *cx)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return ;
  (vector->p_SHA384_Begin)(cx);
}

void 
SHA384_Update(SHA384Context *cx, const unsigned char *input,
			unsigned int inputLen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return ;
  (vector->p_SHA384_Update)(cx, input, inputLen);
}

void 
SHA384_End(SHA384Context *cx, unsigned char *digest,
		     unsigned int *digestLen, unsigned int maxDigestLen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return ;
  (vector->p_SHA384_End)(cx, digest, digestLen, maxDigestLen);
}

void 
SHA384_TraceState(SHA384Context *cx)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return ;
  (vector->p_SHA384_TraceState)(cx);
}

unsigned int 
SHA384_FlattenSize(SHA384Context *cx)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return 0;
  return (vector->p_SHA384_FlattenSize)(cx);
}

SECStatus 
SHA384_Flatten(SHA384Context *cx,unsigned char *space)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_SHA384_Flatten)(cx, space);
}

SHA384Context * 
SHA384_Resurrect(unsigned char *space, void *arg)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return NULL;
  return (vector->p_SHA384_Resurrect)(space, arg);
}


AESKeyWrapContext *
AESKeyWrap_CreateContext(const unsigned char *key, const unsigned char *iv, 
                         int encrypt, unsigned int keylen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return NULL;
  return vector->p_AESKeyWrap_CreateContext(key, iv, encrypt, keylen);
}

void 
AESKeyWrap_DestroyContext(AESKeyWrapContext *cx, PRBool freeit)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return;
  vector->p_AESKeyWrap_DestroyContext(cx, freeit);
}

SECStatus 
AESKeyWrap_Encrypt(AESKeyWrapContext *cx, unsigned char *output,
		   unsigned int *outputLen, unsigned int maxOutputLen,
		   const unsigned char *input, unsigned int inputLen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return vector->p_AESKeyWrap_Encrypt(cx, output, outputLen, maxOutputLen,
                                      input, inputLen);
}
SECStatus 
AESKeyWrap_Decrypt(AESKeyWrapContext *cx, unsigned char *output,
		   unsigned int *outputLen, unsigned int maxOutputLen,
		   const unsigned char *input, unsigned int inputLen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return vector->p_AESKeyWrap_Decrypt(cx, output, outputLen, maxOutputLen,
		                      input, inputLen);
}

PRBool
BLAPI_SHVerify(const char *name, PRFuncPtr addr)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return PR_FALSE;
  return vector->p_BLAPI_SHVerify(name, addr);
}

/*
 * The Caller is expected to pass NULL as the name, which will
 * trigger the p_BLAPI_VerifySelf() to return 'TRUE'. If we really loaded
 * from a shared library, BLAPI_VerifySelf will get pick up the real name
 * from the static set in freebl_LoadDSO( void ) 
 */
PRBool
BLAPI_VerifySelf(const char *name)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return PR_FALSE;
  return vector->p_BLAPI_VerifySelf(libraryName);
}

/* ============== New for 3.006 =============================== */

SECStatus 
EC_NewKey(ECParams * params, ECPrivateKey ** privKey)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_EC_NewKey)( params, privKey );
}

SECStatus 
EC_NewKeyFromSeed(ECParams * params, ECPrivateKey ** privKey,
    const unsigned char *seed, int seedlen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_EC_NewKeyFromSeed)( params, privKey, seed, seedlen );
}

SECStatus 
EC_ValidatePublicKey(ECParams * params, SECItem * publicValue)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_EC_ValidatePublicKey)( params, publicValue );
}

SECStatus 
ECDH_Derive(SECItem * publicValue, ECParams * params, SECItem * privateValue,
            PRBool withCofactor, SECItem * derivedSecret)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_ECDH_Derive)( publicValue, params, privateValue,
                                  withCofactor, derivedSecret );
}

SECStatus
ECDSA_SignDigest(ECPrivateKey * key, SECItem * signature,
    const SECItem * digest)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_ECDSA_SignDigest)( key, signature, digest );
}

SECStatus
ECDSA_VerifyDigest(ECPublicKey * key, const SECItem * signature,
    const SECItem * digest)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_ECDSA_VerifyDigest)( key, signature, digest );
}

SECStatus
ECDSA_SignDigestWithSeed(ECPrivateKey * key, SECItem * signature,
    const SECItem * digest, const unsigned char *seed, const int seedlen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_ECDSA_SignDigestWithSeed)( key, signature, digest, 
      seed, seedlen );
}

/* ============== New for 3.008 =============================== */

AESContext *
AES_AllocateContext(void)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return NULL;
  return (vector->p_AES_AllocateContext)();
}

AESKeyWrapContext *
AESKeyWrap_AllocateContext(void)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return NULL;
  return (vector->p_AESKeyWrap_AllocateContext)();
}

DESContext *
DES_AllocateContext(void)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return NULL;
  return (vector->p_DES_AllocateContext)();
}

RC2Context *
RC2_AllocateContext(void)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return NULL;
  return (vector->p_RC2_AllocateContext)();
}

RC4Context *
RC4_AllocateContext(void)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return NULL;
  return (vector->p_RC4_AllocateContext)();
}

SECStatus 
AES_InitContext(AESContext *cx, const unsigned char *key, 
		unsigned int keylen, const unsigned char *iv, int mode,
		unsigned int encrypt, unsigned int blocklen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_AES_InitContext)(cx, key, keylen, iv, mode, encrypt, 
				     blocklen);
}

SECStatus 
AESKeyWrap_InitContext(AESKeyWrapContext *cx, const unsigned char *key, 
		unsigned int keylen, const unsigned char *iv, int mode,
		unsigned int encrypt, unsigned int blocklen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_AESKeyWrap_InitContext)(cx, key, keylen, iv, mode, 
					    encrypt, blocklen);
}

SECStatus 
DES_InitContext(DESContext *cx, const unsigned char *key, 
		unsigned int keylen, const unsigned char *iv, int mode,
		unsigned int encrypt, unsigned int xtra)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_DES_InitContext)(cx, key, keylen, iv, mode, encrypt, xtra);
}

SECStatus 
RC2_InitContext(RC2Context *cx, const unsigned char *key, 
		unsigned int keylen, const unsigned char *iv, int mode,
		unsigned int effectiveKeyLen, unsigned int xtra)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_RC2_InitContext)(cx, key, keylen, iv, mode, 
				     effectiveKeyLen, xtra);
}

SECStatus 
RC4_InitContext(RC4Context *cx, const unsigned char *key, 
		unsigned int keylen, const unsigned char *x1, int x2,
		unsigned int x3, unsigned int x4)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_RC4_InitContext)(cx, key, keylen, x1, x2, x3, x4);
}

void 
MD2_Clone(MD2Context *dest, MD2Context *src)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return;
  (vector->p_MD2_Clone)(dest, src);
}

void 
MD5_Clone(MD5Context *dest, MD5Context *src)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return;
  (vector->p_MD5_Clone)(dest, src);
}

void 
SHA1_Clone(SHA1Context *dest, SHA1Context *src)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return;
  (vector->p_SHA1_Clone)(dest, src);
}

void 
SHA256_Clone(SHA256Context *dest, SHA256Context *src)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return;
  (vector->p_SHA256_Clone)(dest, src);
}

void 
SHA384_Clone(SHA384Context *dest, SHA384Context *src)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return;
  (vector->p_SHA384_Clone)(dest, src);
}

void 
SHA512_Clone(SHA512Context *dest, SHA512Context *src)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return;
  (vector->p_SHA512_Clone)(dest, src);
}

SECStatus 
TLS_PRF(const SECItem *secret, const char *label, 
		     SECItem *seed, SECItem *result, PRBool isFIPS)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_TLS_PRF)(secret, label, seed, result, isFIPS);
}

const SECHashObject *
HASH_GetRawHashObject(HASH_HashType hashType)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return NULL;
  return (vector->p_HASH_GetRawHashObject)(hashType);
}


void
HMAC_Destroy(HMACContext *cx, PRBool freeit)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return;
  (vector->p_HMAC_Destroy)(cx, freeit);
}

HMACContext *
HMAC_Create(const SECHashObject *hashObj, const unsigned char *secret, 
	    unsigned int secret_len, PRBool isFIPS)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return NULL;
  return (vector->p_HMAC_Create)(hashObj, secret, secret_len, isFIPS);
}

SECStatus
HMAC_Init(HMACContext *cx, const SECHashObject *hashObj, 
	  const unsigned char *secret, unsigned int secret_len, PRBool isFIPS)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_HMAC_Init)(cx, hashObj, secret, secret_len, isFIPS);
}

void
HMAC_Begin(HMACContext *cx)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return;
  (vector->p_HMAC_Begin)(cx);
}

void 
HMAC_Update(HMACContext *cx, const unsigned char *data, unsigned int data_len)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return;
  (vector->p_HMAC_Update)(cx, data, data_len);
}

SECStatus
HMAC_Finish(HMACContext *cx, unsigned char *result, unsigned int *result_len,
	    unsigned int max_result_len)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_HMAC_Finish)(cx, result, result_len, max_result_len);
}

HMACContext *
HMAC_Clone(HMACContext *cx)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return NULL;
  return (vector->p_HMAC_Clone)(cx);
}

void
RNG_SystemInfoForRNG(void)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return ;
  (vector->p_RNG_SystemInfoForRNG)();

}
