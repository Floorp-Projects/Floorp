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
 *  JARSIGN
 *
 *  Routines used in signing archives.
 */


#define USE_MOZ_THREAD

#include "jar.h"
#include "jarint.h"

#ifdef USE_MOZ_THREAD
#include "jarevil.h"
#endif

#include "pk11func.h"
#include "sechash.h"

/* from libevent.h */
typedef void (*ETVoidPtrFunc) (void * data);

#ifdef MOZILLA_CLIENT_OLD

extern void ET_moz_CallFunction (ETVoidPtrFunc fn, void *data);

/* from proto.h */
/* extern MWContext *XP_FindSomeContext(void); */
extern void *XP_FindSomeContext(void);

#endif

/* key database wrapper */

/* static SECKEYKeyDBHandle *jar_open_key_database (void); */

/* CHUNQ is our bite size */

#define CHUNQ 64000
#define FILECHUNQ 32768

/*
 *  J A R _ c a l c u l a t e _ d i g e s t 
 *
 *  Quick calculation of a digest for
 *  the specified block of memory. Will calculate
 *  for all supported algorithms, now MD5.
 *
 *  This version supports huge pointers for WIN16.
 * 
 */

JAR_Digest * PR_CALLBACK JAR_calculate_digest (void ZHUGEP *data, long length)
  {
  long chunq;
  JAR_Digest *dig;

  unsigned int md5_length, sha1_length;

  PK11Context *md5  = 0;
  PK11Context *sha1 = 0;

  dig = (JAR_Digest *) PORT_ZAlloc (sizeof (JAR_Digest));

  if (dig == NULL) 
    {
    /* out of memory allocating digest */
    return NULL;
    }

#if defined(XP_WIN16)
  PORT_Assert ( !IsBadHugeReadPtr(data, length) );
#endif

  md5  = PK11_CreateDigestContext (SEC_OID_MD5);
  sha1 = PK11_CreateDigestContext (SEC_OID_SHA1);

  if (length >= 0) 
    {
    PK11_DigestBegin (md5);
    PK11_DigestBegin (sha1);

    do {
       chunq = length;

#ifdef XP_WIN16
       if (length > CHUNQ) chunq = CHUNQ;

       /*
        *  If the block of data crosses one or more segment 
        *  boundaries then only pass the chunk of data in the 
        *  first segment.
        * 
        *  This allows the data to be treated as FAR by the
        *  PK11_DigestOp(...) routine.
        *
        */

       if (OFFSETOF(data) + chunq >= 0x10000) 
         chunq = 0x10000 - OFFSETOF(data);
#endif

       PK11_DigestOp (md5,  (unsigned char*)data, chunq);
       PK11_DigestOp (sha1, (unsigned char*)data, chunq);

       length -= chunq;
       data = ((char ZHUGEP *) data + chunq);
       } 
    while (length > 0);

    PK11_DigestFinal (md5,  dig->md5,  &md5_length,  MD5_LENGTH);
    PK11_DigestFinal (sha1, dig->sha1, &sha1_length, SHA1_LENGTH);

    PK11_DestroyContext (md5,  PR_TRUE);
    PK11_DestroyContext (sha1, PR_TRUE);
    }

  return dig;
  }

/*
 *  J A R _ d i g e s t _ f i l e
 *
 *  Calculates the MD5 and SHA1 digests for a file 
 *  present on disk, and returns these in JAR_Digest struct.
 *
 */

int JAR_digest_file (char *filename, JAR_Digest *dig)
    {
    JAR_FILE fp;

    int num;
    unsigned char *buf;

    PK11Context *md5 = 0;
    PK11Context *sha1 = 0;

    unsigned int md5_length, sha1_length;

    buf = (unsigned char *) PORT_ZAlloc (FILECHUNQ);
    if (buf == NULL)
      {
      /* out of memory */
      return JAR_ERR_MEMORY;
      }
 
    if ((fp = JAR_FOPEN (filename, "rb")) == 0)
      {
      /* perror (filename); FIX XXX XXX XXX XXX XXX XXX */
      PORT_Free (buf);
      return JAR_ERR_FNF;
      }

    md5 = PK11_CreateDigestContext (SEC_OID_MD5);
    sha1 = PK11_CreateDigestContext (SEC_OID_SHA1);

    if (md5 == NULL || sha1 == NULL) 
      {
      /* can't generate digest contexts */
      PORT_Free (buf);
      JAR_FCLOSE (fp);
      return JAR_ERR_GENERAL;
      }

    PK11_DigestBegin (md5);
    PK11_DigestBegin (sha1);

    while (1)
      {
      if ((num = JAR_FREAD (fp, buf, FILECHUNQ)) == 0)
        break;

      PK11_DigestOp (md5, buf, num);
      PK11_DigestOp (sha1, buf, num);
      }

    PK11_DigestFinal (md5, dig->md5, &md5_length, MD5_LENGTH);
    PK11_DigestFinal (sha1, dig->sha1, &sha1_length, SHA1_LENGTH);

    PK11_DestroyContext (md5, PR_TRUE);
    PK11_DestroyContext (sha1, PR_TRUE);

    PORT_Free (buf);
    JAR_FCLOSE (fp);

    return 0;
    }

/*
 *  J A R _ o p e n _ k e y _ d a t a b a s e
 *
 */

void* jar_open_key_database (void)
  {
    return NULL;
  }

int jar_close_key_database (void *keydb)
  {
  /* We never do close it */
  return 0;
  }


/*
 *  j a r _ c r e a t e _ p k 7
 *
 */

static void jar_pk7_out (void *arg, const char *buf, unsigned long len)
  {
  JAR_FWRITE ((JAR_FILE) arg, buf, len);
  }

int jar_create_pk7 
   (CERTCertDBHandle *certdb, void *keydb, 
       CERTCertificate *cert, char *password, JAR_FILE infp, JAR_FILE outfp)
  {
  int nb;
  unsigned char buffer [4096], digestdata[32];
  const SECHashObject *hashObj;
  void *hashcx;
  unsigned int len;

  int status = 0;
  char *errstring;

  SECItem digest;
  SEC_PKCS7ContentInfo *cinfo;
  SECStatus rv;

  void /*MWContext*/ *mw;

  if (outfp == NULL || infp == NULL || cert == NULL)
    return JAR_ERR_GENERAL;

  /* we sign with SHA */
  hashObj = HASH_GetHashObject(HASH_AlgSHA1);

  hashcx = (* hashObj->create)();
  if (hashcx == NULL)
    return JAR_ERR_GENERAL;

  (* hashObj->begin)(hashcx);

  while (1)
    {
    /* nspr2.0 doesn't support feof 
       if (feof (infp)) break; */

    nb = JAR_FREAD (infp, buffer, sizeof (buffer));
    if (nb == 0) 
      {
#if 0
      if (ferror(infp)) 
        {
        /* PORT_SetError(SEC_ERROR_IO); */ /* FIX */
	(* hashObj->destroy) (hashcx, PR_TRUE);
	return JAR_ERR_GENERAL;
        }
#endif
      /* eof */
      break;
      }
    (* hashObj->update) (hashcx, buffer, nb);
    }

  (* hashObj->end) (hashcx, digestdata, &len, 32);
  (* hashObj->destroy) (hashcx, PR_TRUE);

  digest.data = digestdata;
  digest.len = len;

  /* signtool must use any old context it can find since it's
     calling from inside javaland. */

#ifdef MOZILLA_CLIENT_OLD
  mw = XP_FindSomeContext();
#else
  mw = NULL;
#endif

  PORT_SetError (0);

  cinfo = SEC_PKCS7CreateSignedData 
             (cert, certUsageObjectSigner, NULL, 
                SEC_OID_SHA1, &digest, NULL, (void *) mw);

  if (cinfo == NULL)
    return JAR_ERR_PK7;

  rv = SEC_PKCS7IncludeCertChain (cinfo, NULL);
  if (rv != SECSuccess) 
    {
    status = PORT_GetError();
    SEC_PKCS7DestroyContentInfo (cinfo);
    return status;
    }

  /* Having this here forces signtool to always include
     signing time. */

  rv = SEC_PKCS7AddSigningTime (cinfo);
  if (rv != SECSuccess)
    {
    /* don't check error */
    }

  PORT_SetError (0);

#ifdef USE_MOZ_THREAD
  /* if calling from mozilla */
  rv = jar_moz_encode
             (cinfo, jar_pk7_out, outfp, 
                 NULL,  /* pwfn */ NULL,  /* pwarg */ (void *) mw);
#else
  /* if calling from mozilla thread*/
  rv = SEC_PKCS7Encode 
             (cinfo, jar_pk7_out, outfp, 
                 NULL,  /* pwfn */ NULL,  /* pwarg */ (void *) mw):
#endif

  if (rv != SECSuccess)
    status = PORT_GetError();

  SEC_PKCS7DestroyContentInfo (cinfo);

  if (rv != SECSuccess)
    {
    errstring = JAR_get_error (status);
    /*XP_TRACE (("Jar signing failed (reason %d = %s)", status, errstring));*/
    return status < 0 ? status : JAR_ERR_GENERAL;
    }

  return 0;
  }
