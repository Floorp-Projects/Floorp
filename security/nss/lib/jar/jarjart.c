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
 *  JARJART
 *
 *  JAR functions used by Jartool
 */

/* This allows manifest files above 64k to be
   processed on non-win16 platforms */

#include "jar.h"
#include "jarint.h"
#include "jarjart.h"
#include "blapi.h"	/* JAR is supposed to be above the line!! */
#include "pk11func.h"	/* PK11 wrapper funcs are all above the line. */

/* from certdb.h */
#define CERTDB_USER (1<<6)

#if 0
/* from certdb.h */
typedef SECStatus (* PermCertCallback)(CERTCertificate *cert, SECItem *k, void *pdata);
/* from certdb.h */
SECStatus SEC_TraversePermCerts
   (CERTCertDBHandle *handle, PermCertCallback certfunc, void *udata);
#endif

/*
 *  S O B _ l i s t _ c e r t s
 *
 *  Return a list of newline separated certificate nicknames
 *  (this function used by the Jartool)
 * 
 */

static SECStatus jar_list_cert_callback 
     (CERTCertificate *cert, SECItem *k, void *data)
  {
  char *name;
  char **ugly_list;

  int trusted;

  ugly_list = (char **) data;

  if (cert && cert->dbEntry)
    {
    /* name = cert->dbEntry->nickname; */
    name = cert->nickname;

    trusted = cert->trust->objectSigningFlags & CERTDB_USER;

    /* Add this name or email to list */

    if (name && trusted)
      {
      *ugly_list = (char*)PORT_Realloc
           (*ugly_list, PORT_Strlen (*ugly_list) + PORT_Strlen (name) + 2);

      if (*ugly_list)
        {
        if (**ugly_list)
          PORT_Strcat (*ugly_list, "\n");

        PORT_Strcat (*ugly_list, name);
        }
      }
    }

  return (SECSuccess);
  }

/*
 *  S O B _ J A R _ l i s t _ c e r t s
 *
 *  Return a linfeed separated ascii list of certificate
 *  nicknames for the Jartool.
 *
 */

char *JAR_JAR_list_certs (void)
  {
  SECStatus status;
  CERTCertDBHandle *certdb;

  char *ugly_list;

  certdb = JAR_open_database();

  /* a little something */
  ugly_list = (char*)PORT_ZAlloc (16);

  if (ugly_list)
    {
    *ugly_list = 0;

    status = SEC_TraversePermCerts 
            (certdb, jar_list_cert_callback, (void *) &ugly_list);
    }

  JAR_close_database (certdb);

  return status ? NULL : ugly_list;
  }

int JAR_JAR_validate_archive (char *filename)
  {
  JAR *jar;
  int status = -1;

  jar = JAR_new();

  if (jar)
    {
    status = JAR_pass_archive (jar, jarArchGuess, filename, "");

    if (status == 0)
      status = jar->valid;

    JAR_destroy (jar);
    }

  return status;
  }

char *JAR_JAR_get_error (int status)
  {
  return JAR_get_error (status);
  }

/*
 *  S O B _ J A R _  h a s h
 *
 *  Hash algorithm interface for use by the Jartool. Since we really
 *  don't know the private sizes of the context, and Java does need to
 *  know this number, allocate 512 bytes for it.
 *
 *  In april 1997 hashes in this file were changed to call PKCS11,
 *  as FIPS requires that when a smartcard has failed validation, 
 *  hashes are not to be performed. But because of the difficulty of
 *  preserving pointer context between calls to the JAR_JAR hashing
 *  functions, the hash routines are called directly, though after
 *  checking to see if hashing is allowed.
 *
 */

void *JAR_JAR_new_hash (int alg)
  {
  void *context;

  MD5Context *md5;
  SHA1Context *sha1;

  /* this is a hack because this whole PORT_ZAlloc stuff looks scary */

  if (!PK11_HashOK (alg == 1 ? SEC_OID_MD5 : SEC_OID_SHA1))
    return NULL;

  context = PORT_ZAlloc (512);

  if (context)
    {
    switch (alg)
      {
      case 1:  /* MD5 */
               md5 = (MD5Context *) context;
               MD5_Begin (md5);
               break;

      case 2:  /* SHA1 */
               sha1 = (SHA1Context *) context;
               SHA1_Begin (sha1);
               break;
      }
    }

  return context;
  }

void *JAR_JAR_hash (int alg, void *cookie, int length, void *data)
  {
  MD5Context *md5;
  SHA1Context *sha1;

  /* this is a hack because this whole PORT_ZAlloc stuff looks scary */

  if (!PK11_HashOK (alg == 1 ? SEC_OID_MD5 : SEC_OID_SHA1))
    return NULL;

  if (length > 0)
    {
    switch (alg)
      {
      case 1:  /* MD5 */
               md5 = (MD5Context *) cookie;
               MD5_Update (md5, (unsigned char*)data, length);
               break;

      case 2:  /* SHA1 */
               sha1 = (SHA1Context *) cookie;
               SHA1_Update (sha1, (unsigned char*)data, length);
               break;
      }
    }

  return cookie;
  }

void *JAR_JAR_end_hash (int alg, void *cookie)
  {
  int length;
  unsigned char *data;
  char *ascii; 

  MD5Context *md5;
  SHA1Context *sha1;

  unsigned int md5_length;
  unsigned char md5_digest [MD5_LENGTH];

  unsigned int sha1_length;
  unsigned char sha1_digest [SHA1_LENGTH];

  /* this is a hack because this whole PORT_ZAlloc stuff looks scary */

  if (!PK11_HashOK (alg == 1 ? SEC_OID_MD5 : SEC_OID_SHA1)) 
    return NULL;

  switch (alg)
    {
    case 1:  /* MD5 */

             md5 = (MD5Context *) cookie;

             MD5_End (md5, md5_digest, &md5_length, MD5_LENGTH);
             /* MD5_DestroyContext (md5, PR_TRUE); */

             data = md5_digest;
             length = md5_length;

             break;

    case 2:  /* SHA1 */

             sha1 = (SHA1Context *) cookie;

             SHA1_End (sha1, sha1_digest, &sha1_length, SHA1_LENGTH);
             /* SHA1_DestroyContext (sha1, PR_TRUE); */

             data = sha1_digest;
             length = sha1_length;

             break;

    default: return NULL;
    }

  /* Instead of destroy context, since we created it */
  /* PORT_Free (cookie); */

  ascii = BTOA_DataToAscii(data, length);

  return ascii ? PORT_Strdup (ascii) : NULL;
  }

/*
 *  S O B _ J A R _ s i g n _ a r c h i v e
 *
 *  A simple API to sign a JAR archive.
 *
 */

int JAR_JAR_sign_archive 
      (char *nickname, char *password, char *sf, char *outsig)
  {
  char *out_fn;

  int status = JAR_ERR_GENERAL;
  JAR_FILE sf_fp; 
  JAR_FILE out_fp;

  CERTCertDBHandle *certdb;
  SECKEYKeyDBHandle *keydb;

  CERTCertificate *cert;

  /* open cert and key databases */

  certdb = JAR_open_database();
  if (certdb == NULL)
    return JAR_ERR_GENERAL;

  keydb = jar_open_key_database();
  if (keydb == NULL)
    return JAR_ERR_GENERAL;

  out_fn = PORT_Strdup (sf);

  if (out_fn == NULL || PORT_Strlen (sf) < 5)
    return JAR_ERR_GENERAL;

  sf_fp = JAR_FOPEN (sf, "rb");
  out_fp = JAR_FOPEN (outsig, "wb");

  cert = CERT_FindCertByNickname (certdb, nickname);

  if (cert && sf_fp && out_fp)
    {
    status = jar_create_pk7 (certdb, keydb, cert, password, sf_fp, out_fp);
    }

  /* remove password from prying eyes */
  PORT_Memset (password, 0, PORT_Strlen (password));

  JAR_FCLOSE (sf_fp);
  JAR_FCLOSE (out_fp);

  JAR_close_database (certdb);
  jar_close_key_database (keydb);

  return status;
  }
