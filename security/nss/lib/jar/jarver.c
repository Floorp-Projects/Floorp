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
 *  JARVER
 *
 *  Jarnature Parsing & Verification
 */

#define USE_MOZ_THREAD

#include "jar.h"
#include "jarint.h"

#ifdef USE_MOZ_THREAD
#include "jarevil.h"
#endif
#include "cdbhdl.h"
#include "secder.h"

/* to use huge pointers in win16 */

#if !defined(XP_WIN16)
#define xp_HUGE_MEMCPY PORT_Memcpy
#define xp_HUGE_STRCPY PORT_Strcpy
#define xp_HUGE_STRLEN PORT_Strlen
#define xp_HUGE_STRNCASECMP PORT_Strncasecmp
#else
#define xp_HUGE_MEMCPY hmemcpy
int xp_HUGE_STRNCASECMP (char ZHUGEP *buf, char *key, int len);
size_t xp_HUGE_STRLEN (char ZHUGEP *s);
char *xp_HUGE_STRCPY (char *to, char ZHUGEP *from);
#endif

/* from certdb.h */
#define CERTDB_USER (1<<6)

#if 0
/* from certdb.h */
extern PRBool SEC_CertNicknameConflict
   (char *nickname, CERTCertDBHandle *handle);
/* from certdb.h */
extern SECStatus SEC_AddTempNickname
   (CERTCertDBHandle *handle, char *nickname, SECItem *certKey);
/* from certdb.h */
typedef SECStatus (* PermCertCallback)(CERTCertificate *cert, SECItem *k, void *pdata);
#endif

/* from certdb.h */
SECStatus SEC_TraversePermCerts
   (CERTCertDBHandle *handle, PermCertCallback certfunc, void *udata);


#define SZ 512

static int jar_validate_pkcs7 
     (JAR *jar, JAR_Signer *signer, char *data, long length);

static void jar_catch_bytes
     (void *arg, const char *buf, unsigned long len);

static int jar_gather_signers
     (JAR *jar, JAR_Signer *signer, SEC_PKCS7ContentInfo *cinfo);

static char ZHUGEP *jar_eat_line 
     (int lines, int eating, char ZHUGEP *data, long *len);

static JAR_Digest *jar_digest_section 
     (char ZHUGEP *manifest, long length);

static JAR_Digest *jar_get_mf_digest (JAR *jar, char *path);

static int jar_parse_digital_signature 
     (char *raw_manifest, JAR_Signer *signer, long length, JAR *jar);

static int jar_add_cert
     (JAR *jar, JAR_Signer *signer, int type, CERTCertificate *cert);

static CERTCertificate *jar_get_certificate
            (JAR *jar, long keylen, void *key, int *result);

static char *jar_cert_element (char *name, char *tag, int occ);

static char *jar_choose_nickname (CERTCertificate *cert);

static char *jar_basename (const char *path);

static int jar_signal 
     (int status, JAR *jar, const char *metafile, char *pathname);

#ifdef DEBUG
static int jar_insanity_check (char ZHUGEP *data, long length);
#endif

int jar_parse_mf
    (JAR *jar, char ZHUGEP *raw_manifest, 
        long length, const char *path, const char *url);

int jar_parse_sf
    (JAR *jar, char ZHUGEP *raw_manifest, 
        long length, const char *path, const char *url);

int jar_parse_sig
    (JAR *jar, const char *path, char ZHUGEP *raw_manifest, long length);

int jar_parse_any
    (JAR *jar, int type, JAR_Signer *signer, char ZHUGEP *raw_manifest, 
        long length, const char *path, const char *url);

static int jar_internal_digest 
     (JAR *jar, const char *path, char *x_name, JAR_Digest *dig);

/*
 *  J A R _ p a r s e _ m a n i f e s t
 * 
 *  Pass manifest files to this function. They are
 *  decoded and placed into internal representations.
 * 
 *  Accepts both signature and manifest files. Use
 *  the same "jar" for both. 
 *
 */

int JAR_parse_manifest 
    (JAR *jar, char ZHUGEP *raw_manifest, 
        long length, const char *path, const char *url)
  {

#if defined(XP_WIN16)
    PORT_Assert( !IsBadHugeReadPtr(raw_manifest, length) );
#endif

  /* fill in the path, if supplied. This is a the location
     of the jar file on disk, if known */

  if (jar->filename == NULL && path)
    {
    jar->filename = PORT_Strdup (path);
    if (jar->filename == NULL)
      return JAR_ERR_MEMORY;
    }

  /* fill in the URL, if supplied. This is the place
     from which the jar file was retrieved. */

  if (jar->url == NULL && url)
    {
    jar->url = PORT_Strdup (url);
    if (jar->url == NULL)
      return JAR_ERR_MEMORY;
    }

  /* Determine what kind of file this is from the META-INF 
     directory. It could be MF, SF, or a binary RSA/DSA file */

  if (!xp_HUGE_STRNCASECMP (raw_manifest, "Manifest-Version:", 17))
    {
    return jar_parse_mf (jar, raw_manifest, length, path, url);
    }
  else if (!xp_HUGE_STRNCASECMP (raw_manifest, "Signature-Version:", 18))
    {
    return jar_parse_sf (jar, raw_manifest, length, path, url);
    }
  else
    {
    /* This is probably a binary signature */
    return jar_parse_sig (jar, path, raw_manifest, length);
    }
  }

/*
 *  j a r _ p a r s e _ s i g
 *
 *  Pass some manner of RSA or DSA digital signature
 *  on, after checking to see if it comes at an appropriate state.
 *
 */
 
int jar_parse_sig
    (JAR *jar, const char *path, char ZHUGEP *raw_manifest, long length)
  {
  JAR_Signer *signer;
  int status = JAR_ERR_ORDER;

  if (length <= 128) 
    {
    /* signature is way too small */
    return JAR_ERR_SIG;
    }

  /* make sure that MF and SF have already been processed */

  if (jar->globalmeta == NULL)
    return JAR_ERR_ORDER;

#if 0
  /* XXX Turn this on to disable multiple signers */
  if (jar->digest == NULL)
    return JAR_ERR_ORDER;
#endif

  /* Determine whether or not this RSA file has
     has an associated SF file */

  if (path)
    {
    char *owner;
    owner = jar_basename (path);

    if (owner == NULL)
      return JAR_ERR_MEMORY;

    signer = jar_get_signer (jar, owner);

    PORT_Free (owner);
    }
  else
    signer = jar_get_signer (jar, "*");

  if (signer == NULL)
    return JAR_ERR_ORDER;


  /* Do not pass a huge pointer to this function,
     since the underlying security code is unaware. We will
     never pass >64k through here. */

  if (length > 64000)
    {
    /* this digital signature is way too big */
    return JAR_ERR_SIG;
    }

#ifdef XP_WIN16
  /*
   * For Win16, copy the portion of the raw_buffer containing the digital 
   * signature into another buffer...  This insures that the data will
   * NOT cross a segment boundary.  Therefore, 
   * jar_parse_digital_signature(...) does NOT need to deal with HUGE 
   * pointers...
   */

    {
    unsigned char *manifest_copy;

    manifest_copy = (unsigned char *) PORT_ZAlloc (length);
    if (manifest_copy)
      {
      xp_HUGE_MEMCPY (manifest_copy, raw_manifest, length);

      status = jar_parse_digital_signature 
                  (manifest_copy, signer, length, jar);

      PORT_Free (manifest_copy);
      }
    else
      {
      /* out of memory */
      return JAR_ERR_MEMORY;
      }
    }
#else
  /* don't expense unneeded calloc overhead on non-win16 */
  status = jar_parse_digital_signature 
                (raw_manifest, signer, length, jar);
#endif

  return status;
  }

/*
 *  j a r _ p a r s e _ m f
 *
 *  Parse the META-INF/manifest.mf file, whose
 *  information applies to all signers.
 *
 */

int jar_parse_mf
    (JAR *jar, char ZHUGEP *raw_manifest, 
        long length, const char *path, const char *url)
  {
  if (jar->globalmeta)
    {
    /* refuse a second manifest file, if passed for some reason */
    return JAR_ERR_ORDER;
    }


  /* remember a digest for the global section */

  jar->globalmeta = jar_digest_section (raw_manifest, length);

  if (jar->globalmeta == NULL)
    return JAR_ERR_MEMORY;


  return jar_parse_any 
    (jar, jarTypeMF, NULL, raw_manifest, length, path, url);
  }

/*
 *  j a r _ p a r s e _ s f
 *
 *  Parse META-INF/xxx.sf, a digitally signed file
 *  pointing to a subset of MF sections. 
 *
 */

int jar_parse_sf
    (JAR *jar, char ZHUGEP *raw_manifest, 
        long length, const char *path, const char *url)
  {
  JAR_Signer *signer = NULL;
  int status = JAR_ERR_MEMORY;

  if (jar->globalmeta == NULL)
    {
    /* It is a requirement that the MF file be passed before the SF file */
    return JAR_ERR_ORDER;
    }

  signer = JAR_new_signer();

  if (signer == NULL)
    goto loser;

  if (path)
    {
    signer->owner = jar_basename (path);
    if (signer->owner == NULL)
      goto loser;
    }


  /* check for priors. When someone doctors a jar file
     to contain identical path entries, prevent the second
     one from affecting JAR functions */

  if (jar_get_signer (jar, signer->owner))
    {
    /* someone is trying to spoof us */
    status = JAR_ERR_ORDER;
    goto loser;
    }


  /* remember its digest */

  signer->digest = JAR_calculate_digest (raw_manifest, length);

  if (signer->digest == NULL)
    goto loser;

  /* Add this signer to the jar */

  ADDITEM (jar->signers, jarTypeOwner, 
     signer->owner, signer, sizeof (JAR_Signer));


  return jar_parse_any 
    (jar, jarTypeSF, signer, raw_manifest, length, path, url);

loser:

  if (signer)
    JAR_destroy_signer (signer);

  return status;
  }

/* 
 *  j a r _ p a r s e _ a n y
 *
 *  Parse a MF or SF manifest file. 
 *
 */
 
int jar_parse_any
    (JAR *jar, int type, JAR_Signer *signer, char ZHUGEP *raw_manifest, 
        long length, const char *path, const char *url)
  {
  int status;

  long raw_len;

  JAR_Digest *dig, *mfdig = NULL;

  char line [SZ];
  char x_name [SZ], x_md5 [SZ], x_sha [SZ];

  char *x_info;

  char *sf_md5 = NULL, *sf_sha1 = NULL;

  *x_name = 0;
  *x_md5 = 0;
  *x_sha = 0;

  PORT_Assert( length > 0 );
  raw_len = length;

#ifdef DEBUG
  if ((status = jar_insanity_check (raw_manifest, raw_len)) < 0)
    return status;
#endif


  /* null terminate the first line */
  raw_manifest = jar_eat_line (0, PR_TRUE, raw_manifest, &raw_len);


  /* skip over the preliminary section */
  /* This is one section at the top of the file with global metainfo */

  while (raw_len)
    {
    JAR_Metainfo *met;

    raw_manifest = jar_eat_line (1, PR_TRUE, raw_manifest, &raw_len);
    if (!*raw_manifest) break;

    met = (JAR_Metainfo*)PORT_ZAlloc (sizeof (JAR_Metainfo));
    if (met == NULL)
      return JAR_ERR_MEMORY;

    /* Parse out the header & info */

    if (xp_HUGE_STRLEN (raw_manifest) >= SZ)
      {
      /* almost certainly nonsense */
      continue;
      }

    xp_HUGE_STRCPY (line, raw_manifest);
    x_info = line;

    while (*x_info && *x_info != ' ' && *x_info != '\t' && *x_info != ':')
      x_info++;

    if (*x_info) *x_info++ = 0;

    while (*x_info == ' ' || *x_info == '\t')
      x_info++;

    /* metainfo (name, value) pair is now (line, x_info) */

    met->header = PORT_Strdup (line);
    met->info = PORT_Strdup (x_info);

    if (type == jarTypeMF)
      {
      ADDITEM (jar->metainfo, jarTypeMeta, 
         /* pathname */ NULL, met, sizeof (JAR_Metainfo));
      }

    /* For SF files, this metadata may be the digests
       of the MF file, still in the "met" structure. */

    if (type == jarTypeSF)
      {
      if (!PORT_Strcasecmp (line, "MD5-Digest"))
        sf_md5 = (char *) met->info;

      if (!PORT_Strcasecmp (line, "SHA1-Digest") || !PORT_Strcasecmp (line, "SHA-Digest"))
        sf_sha1 = (char *) met->info;
      }
    }

  if (type == jarTypeSF && jar->globalmeta)
    {
    /* this is a SF file which may contain a digest of the manifest.mf's 
       global metainfo. */

    int match = 0;
    JAR_Digest *glob = jar->globalmeta;

    if (sf_md5)
      {
      unsigned int md5_length;
      unsigned char *md5_digest;

      md5_digest = ATOB_AsciiToData (sf_md5, &md5_length);
      PORT_Assert( md5_length == MD5_LENGTH );

      if (md5_length != MD5_LENGTH)
        return JAR_ERR_CORRUPT;

      match = PORT_Memcmp (md5_digest, glob->md5, MD5_LENGTH);
      }

    if (sf_sha1 && match == 0)
      {
      unsigned int sha1_length;
      unsigned char *sha1_digest;

      sha1_digest = ATOB_AsciiToData (sf_sha1, &sha1_length);
      PORT_Assert( sha1_length == SHA1_LENGTH );

      if (sha1_length != SHA1_LENGTH)
        return JAR_ERR_CORRUPT;

      match = PORT_Memcmp (sha1_digest, glob->sha1, SHA1_LENGTH);
      }

    if (match != 0)
      {
      /* global digest doesn't match, SF file therefore invalid */
      jar->valid = JAR_ERR_METADATA;
      return JAR_ERR_METADATA;
      }
    }

  /* done with top section of global data */


  while (raw_len)
    {
    *x_md5 = 0;
    *x_sha = 0;
    *x_name = 0;


    /* If this is a manifest file, attempt to get a digest of the following section, 
       without damaging it. This digest will be saved later. */

    if (type == jarTypeMF)
      {
      char ZHUGEP *sec;
      long sec_len = raw_len;

      if (!*raw_manifest || *raw_manifest == '\n')
        {     
        /* skip the blank line */ 
        sec = jar_eat_line (1, PR_FALSE, raw_manifest, &sec_len);
        }
      else
        sec = raw_manifest;

      if (!xp_HUGE_STRNCASECMP (sec, "Name:", 5))
        {
        if (type == jarTypeMF)
          mfdig = jar_digest_section (sec, sec_len);
        else
          mfdig = NULL;
        }
      }


    while (raw_len)
      {
      raw_manifest = jar_eat_line (1, PR_TRUE, raw_manifest, &raw_len);
      if (!*raw_manifest) break; /* blank line, done with this entry */

      if (xp_HUGE_STRLEN (raw_manifest) >= SZ)
        {
        /* almost certainly nonsense */
        continue;
        }


      /* Parse out the name/value pair */

      xp_HUGE_STRCPY (line, raw_manifest);
      x_info = line;

      while (*x_info && *x_info != ' ' && *x_info != '\t' && *x_info != ':')
        x_info++;

      if (*x_info) *x_info++ = 0;

      while (*x_info == ' ' || *x_info == '\t') 
        x_info++;


      if (!PORT_Strcasecmp (line, "Name"))
        PORT_Strcpy (x_name, x_info);

      else if (!PORT_Strcasecmp (line, "MD5-Digest"))
        PORT_Strcpy (x_md5, x_info);

      else if (!PORT_Strcasecmp (line, "SHA1-Digest") 
                  || !PORT_Strcasecmp (line, "SHA-Digest"))
        {
        PORT_Strcpy (x_sha, x_info);
        }

      /* Algorithm list is meta info we don't care about; keeping it out
         of metadata saves significant space for large jar files */

      else if (!PORT_Strcasecmp (line, "Digest-Algorithms")
                    || !PORT_Strcasecmp (line, "Hash-Algorithms"))
        {
        continue;
        }

      /* Meta info is only collected for the manifest.mf file,
         since the JAR_get_metainfo call does not support identity */

      else if (type == jarTypeMF)
        {
        JAR_Metainfo *met;

        /* this is meta-data */

        met = (JAR_Metainfo*)PORT_ZAlloc (sizeof (JAR_Metainfo));

        if (met == NULL)
          return JAR_ERR_MEMORY;

        /* metainfo (name, value) pair is now (line, x_info) */

        if ((met->header = PORT_Strdup (line)) == NULL)
          return JAR_ERR_MEMORY;

        if ((met->info = PORT_Strdup (x_info)) == NULL)
          return JAR_ERR_MEMORY;

        ADDITEM (jar->metainfo, jarTypeMeta, 
           x_name, met, sizeof (JAR_Metainfo));
        }
      }

	if(!x_name || !*x_name) {
		/* Whatever that was, it wasn't an entry, because we didn't get a name.
		 * We don't really have anything, so don't record this. */
		continue;
	}

    dig = (JAR_Digest*)PORT_ZAlloc (sizeof (JAR_Digest));
    if (dig == NULL)
      return JAR_ERR_MEMORY;

    if (*x_md5 ) 
      {
      unsigned int binary_length;
      unsigned char *binary_digest;

      binary_digest = ATOB_AsciiToData (x_md5, &binary_length);
      PORT_Assert( binary_length == MD5_LENGTH );

      if (binary_length != MD5_LENGTH)
        return JAR_ERR_CORRUPT;

      memcpy (dig->md5, binary_digest, MD5_LENGTH);
      dig->md5_status = jarHashPresent;
      }

    if (*x_sha ) 
      {
      unsigned int binary_length;
      unsigned char *binary_digest;

      binary_digest = ATOB_AsciiToData (x_sha, &binary_length);
      PORT_Assert( binary_length == SHA1_LENGTH );

      if (binary_length != SHA1_LENGTH)
        return JAR_ERR_CORRUPT;

      memcpy (dig->sha1, binary_digest, SHA1_LENGTH);
      dig->sha1_status = jarHashPresent;
      }

    PORT_Assert( type == jarTypeMF || type == jarTypeSF );


    if (type == jarTypeMF)
      {
      ADDITEM (jar->hashes, jarTypeMF, x_name, dig, sizeof (JAR_Digest));
      }
    else if (type == jarTypeSF)
      {
      ADDITEM (signer->sf, jarTypeSF, x_name, dig, sizeof (JAR_Digest));
      }
    else
      return JAR_ERR_ORDER;

    /* we're placing these calculated digests of manifest.mf 
       sections in a list where they can subsequently be forgotten */

    if (type == jarTypeMF && mfdig)
      {
      ADDITEM (jar->manifest, jarTypeSect, 
         x_name, mfdig, sizeof (JAR_Digest));

      mfdig = NULL;
      }


    /* Retrieve our saved SHA1 digest from saved copy and check digests.
       This is just comparing the digest of the MF section as indicated in
       the SF file with the one we remembered from parsing the MF file */

    if (type == jarTypeSF)
      {
      if ((status = jar_internal_digest (jar, path, x_name, dig)) < 0)
        return status;
      }
    }

  return 0;
  }

static int jar_internal_digest 
     (JAR *jar, const char *path, char *x_name, JAR_Digest *dig)
  {
  int cv;
  int status;

  JAR_Digest *savdig;

  savdig = jar_get_mf_digest (jar, x_name);

  if (savdig == NULL)
    {
    /* no .mf digest for this pathname */
    status = jar_signal (JAR_ERR_ENTRY, jar, path, x_name);
    if (status < 0) 
      return 0; /* was continue; */
    else 
      return status;
    }

  /* check for md5 consistency */
  if (dig->md5_status)
    {
    cv = PORT_Memcmp (savdig->md5, dig->md5, MD5_LENGTH);
    /* md5 hash of .mf file is not what expected */
    if (cv) 
      {
      status = jar_signal (JAR_ERR_HASH, jar, path, x_name);

      /* bad hash, man */

      dig->md5_status = jarHashBad;
      savdig->md5_status = jarHashBad;

      if (status < 0) 
        return 0; /* was continue; */
      else 
        return status;
      }
    }

  /* check for sha1 consistency */
  if (dig->sha1_status)
    {
    cv = PORT_Memcmp (savdig->sha1, dig->sha1, SHA1_LENGTH);
    /* sha1 hash of .mf file is not what expected */
    if (cv) 
      {
      status = jar_signal (JAR_ERR_HASH, jar, path, x_name);

      /* bad hash, man */

      dig->sha1_status = jarHashBad;
      savdig->sha1_status = jarHashBad;

      if (status < 0)
        return 0; /* was continue; */
      else
        return status;
      }
    }
	return 0;
  }

#ifdef DEBUG
/*
 *  j a r _ i n s a n i t y _ c h e c k
 *
 *  Check for illegal characters (or possibly so)
 *  in the manifest files, to detect potential memory
 *  corruption by our neighbors. Debug only, since
 *  not I18N safe.
 * 
 */

static int jar_insanity_check (char ZHUGEP *data, long length)
  {
  int c;
  long off;

  for (off = 0; off < length; off++)
    {
    c = data [off];

    if (c == '\n' || c == '\r' || (c >= ' ' && c <= 128))
      continue;

    return JAR_ERR_CORRUPT;
    }

  return 0;
  }
#endif

/*
 *  j a r _ p a r s e _ d i g i t a l _ s i g n a t u r e
 *
 *  Parse an RSA or DSA (or perhaps other) digital signature.
 *  Right now everything is PKCS7.
 *
 */

static int jar_parse_digital_signature 
     (char *raw_manifest, JAR_Signer *signer, long length, JAR *jar)
  {
#if defined(XP_WIN16)
  PORT_Assert( LOWORD(raw_manifest) + length < 0xFFFF );
#endif
  return jar_validate_pkcs7 (jar, signer, raw_manifest, length);
  }

/*
 *  j a r _ a d d _ c e r t
 * 
 *  Add information for the given certificate
 *  (or whatever) to the JAR linked list. A pointer
 *  is passed for some relevant reference, say
 *  for example the original certificate.
 *
 */

static int jar_add_cert
     (JAR *jar, JAR_Signer *signer, int type, CERTCertificate *cert)
  {
  JAR_Cert *fing;

  if (cert == NULL)
    return JAR_ERR_ORDER;

  fing = (JAR_Cert*)PORT_ZAlloc (sizeof (JAR_Cert));

  if (fing == NULL)
    goto loser;

#ifdef USE_MOZ_THREAD
  fing->cert = jar_moz_dup (cert);
#else
  fing->cert = CERT_DupCertificate (cert);
#endif

  /* get the certkey */

  fing->length = cert->certKey.len;

  fing->key = (char *) PORT_ZAlloc (fing->length);

  if (fing->key == NULL)
    goto loser;

  PORT_Memcpy (fing->key, cert->certKey.data, fing->length);

  ADDITEM (signer->certs, type, 
    /* pathname */ NULL, fing, sizeof (JAR_Cert));

  return 0;

loser:

  if (fing)
    {
    if (fing->cert) 
      CERT_DestroyCertificate (fing->cert);

    PORT_Free (fing);
    }

  return JAR_ERR_MEMORY;
  }

/*
 *  e a t _ l i n e 
 *
 *  Consume an ascii line from the top of a file kept
 *  in memory. This destroys the file in place. This function
 *  handles PC, Mac, and Unix style text files.
 *
 */

static char ZHUGEP *jar_eat_line 
    (int lines, int eating, char ZHUGEP *data, long *len)
  {
  char ZHUGEP *ret;

  ret = data;
  if (!*len) return ret;

  /* Eat the requisite number of lines, if any; 
     prior to terminating the current line with a 0. */

  for (/* yip */ ; lines; lines--)
    {
    while (*data && *data != '\n')
      data++;

    /* After the CR, ok to eat one LF */

    if (*data == '\n')
      data++;

    /* If there are zeros, we put them there */

    while (*data == 0 && data - ret < *len)
      data++;
    }

  *len -= data - ret;
  ret = data;

  if (eating)
    {
    /* Terminate this line with a 0 */ 

    while (*data && *data != '\n' && *data != '\r')
      data++;

    /* In any case we are allowed to eat CR */

    if (*data == '\r')
      *data++ = 0;

    /* After the CR, ok to eat one LF */

    if (*data == '\n')
      *data++ = 0;
    }

  return ret;
  }

/*
 *  j a r _ d i g e s t _ s e c t i o n
 *
 *  Return the digests of the next section of the manifest file.
 *  Does not damage the manifest file, unlike parse_manifest.
 * 
 */

static JAR_Digest *jar_digest_section 
    (char ZHUGEP *manifest, long length)
  {
  long global_len;
  char ZHUGEP *global_end;

  global_end = manifest;
  global_len = length;

  while (global_len)
    {
    global_end = jar_eat_line (1, PR_FALSE, global_end, &global_len);
    if (*global_end == 0 || *global_end == '\n')
      break;
    }

  return JAR_calculate_digest (manifest, global_end - manifest);
  }

/*
 *  J A R _ v e r i f y _ d i g e s t
 *
 *  Verifies that a precalculated digest matches the
 *  expected value in the manifest.
 *
 */

int PR_CALLBACK JAR_verify_digest
    (JAR *jar, const char *name, JAR_Digest *dig)
  {
  JAR_Item *it;

  JAR_Digest *shindig;

  ZZLink *link;
  ZZList *list;

  int result1, result2;

  list = jar->hashes;

  result1 = result2 = 0;

  if (jar->valid < 0)
    {
    /* signature not valid */
    return JAR_ERR_SIG;
    }

  if (ZZ_ListEmpty (list))
    {
    /* empty list */
    return JAR_ERR_PNF;
    }

  for (link = ZZ_ListHead (list); 
       !ZZ_ListIterDone (list, link); 
       link = link->next)
    {
    it = link->thing;
    if (it->type == jarTypeMF 
           && it->pathname && !PORT_Strcmp (it->pathname, name))
      {
      shindig = (JAR_Digest *) it->data;

      if (shindig->md5_status)
        {
        if (shindig->md5_status == jarHashBad)
          return JAR_ERR_HASH;
        else
          result1 = memcmp (dig->md5, shindig->md5, MD5_LENGTH);
        }

      if (shindig->sha1_status)
        {
        if (shindig->sha1_status == jarHashBad)
          return JAR_ERR_HASH;
        else
          result2 = memcmp (dig->sha1, shindig->sha1, SHA1_LENGTH);
        }

      return (result1 == 0 && result2 == 0) ? 0 : JAR_ERR_HASH;
      }
    }

  return JAR_ERR_PNF;
  }

/* 
 *  J A R _ c e r t _ a t t r i b u t e
 *
 *  Return the named certificate attribute from the
 *  certificate specified by the given key.
 *
 */

int PR_CALLBACK JAR_cert_attribute
    (JAR *jar, jarCert attrib, long keylen, void *key, 
        void **result, unsigned long *length)
  {
  int status = 0;
  char *ret = NULL;

  CERTCertificate *cert;

  CERTCertDBHandle *certdb;

  JAR_Digest *dig;
  SECItem hexme;

  *length = 0;

  if (attrib == 0 || key == 0)
    return JAR_ERR_GENERAL;

  if (attrib == jarCertJavaHack)
    {
    cert = (CERTCertificate *) NULL;
    certdb = JAR_open_database();

    if (certdb)
      {
#ifdef USE_MOZ_THREAD
      cert = jar_moz_nickname (certdb, (char*)key);
#else
      cert = CERT_FindCertByNickname (certdb, key);
#endif

      if (cert)
        {
        *length = cert->certKey.len;

        *result = (void *) PORT_ZAlloc (*length);

        if (*result)
          PORT_Memcpy (*result, cert->certKey.data, *length);
        else
          return JAR_ERR_MEMORY;
        }
      JAR_close_database (certdb);
      }

    return cert ? 0 : JAR_ERR_GENERAL;
    }

  if (jar && jar->pkcs7 == 0)
    return JAR_ERR_GENERAL;

  cert = jar_get_certificate (jar, keylen, key, &status);

  if (cert == NULL || status < 0)
    return JAR_ERR_GENERAL;

#define SEP " <br> "
#define SEPLEN (PORT_Strlen(SEP))

  switch (attrib)
    {
    case jarCertCompany:

      ret = cert->subjectName;

      /* This is pretty ugly looking but only used
         here for this one purpose. */

      if (ret)
        {
        int retlen = 0;

        char *cer_ou1, *cer_ou2, *cer_ou3;
	char *cer_cn, *cer_e, *cer_o, *cer_l;

	cer_cn  = CERT_GetCommonName (&cert->subject);
        cer_e   = CERT_GetCertEmailAddress (&cert->subject);
        cer_ou3 = jar_cert_element (ret, "OU=", 3);
        cer_ou2 = jar_cert_element (ret, "OU=", 2);
        cer_ou1 = jar_cert_element (ret, "OU=", 1);
        cer_o   = CERT_GetOrgName (&cert->subject);
        cer_l   = CERT_GetCountryName (&cert->subject);

        if (cer_cn)  retlen += SEPLEN + PORT_Strlen (cer_cn);
        if (cer_e)   retlen += SEPLEN + PORT_Strlen (cer_e);
        if (cer_ou1) retlen += SEPLEN + PORT_Strlen (cer_ou1);
        if (cer_ou2) retlen += SEPLEN + PORT_Strlen (cer_ou2);
        if (cer_ou3) retlen += SEPLEN + PORT_Strlen (cer_ou3);
        if (cer_o)   retlen += SEPLEN + PORT_Strlen (cer_o);
        if (cer_l)   retlen += SEPLEN + PORT_Strlen (cer_l);

        ret = (char *) PORT_ZAlloc (1 + retlen);

        if (cer_cn)  { PORT_Strcpy (ret, cer_cn);  PORT_Strcat (ret, SEP); }
        if (cer_e)   { PORT_Strcat (ret, cer_e);   PORT_Strcat (ret, SEP); }
        if (cer_ou1) { PORT_Strcat (ret, cer_ou1); PORT_Strcat (ret, SEP); }
        if (cer_ou2) { PORT_Strcat (ret, cer_ou2); PORT_Strcat (ret, SEP); }
        if (cer_ou3) { PORT_Strcat (ret, cer_ou3); PORT_Strcat (ret, SEP); }
        if (cer_o)   { PORT_Strcat (ret, cer_o);   PORT_Strcat (ret, SEP); }
        if (cer_l)     PORT_Strcat (ret, cer_l);

	/* return here to avoid unsightly memory leak */

        *result = ret;
        *length = PORT_Strlen (ret);

        return 0;
        }
      break;

    case jarCertCA:

      ret = cert->issuerName;

      if (ret)
        {
        int retlen = 0;

        char *cer_ou1, *cer_ou2, *cer_ou3;
	char *cer_cn, *cer_e, *cer_o, *cer_l;

        /* This is pretty ugly looking but only used
           here for this one purpose. */

	cer_cn  = CERT_GetCommonName (&cert->issuer);
        cer_e   = CERT_GetCertEmailAddress (&cert->issuer);
        cer_ou3 = jar_cert_element (ret, "OU=", 3);
        cer_ou2 = jar_cert_element (ret, "OU=", 2);
        cer_ou1 = jar_cert_element (ret, "OU=", 1);
        cer_o   = CERT_GetOrgName (&cert->issuer);
        cer_l   = CERT_GetCountryName (&cert->issuer);

        if (cer_cn)  retlen += SEPLEN + PORT_Strlen (cer_cn);
        if (cer_e)   retlen += SEPLEN + PORT_Strlen (cer_e);
        if (cer_ou1) retlen += SEPLEN + PORT_Strlen (cer_ou1);
        if (cer_ou2) retlen += SEPLEN + PORT_Strlen (cer_ou2);
        if (cer_ou3) retlen += SEPLEN + PORT_Strlen (cer_ou3);
        if (cer_o)   retlen += SEPLEN + PORT_Strlen (cer_o);
        if (cer_l)   retlen += SEPLEN + PORT_Strlen (cer_l);

        ret = (char *) PORT_ZAlloc (1 + retlen);

        if (cer_cn)  { PORT_Strcpy (ret, cer_cn);  PORT_Strcat (ret, SEP); }
        if (cer_e)   { PORT_Strcat (ret, cer_e);   PORT_Strcat (ret, SEP); }
        if (cer_ou1) { PORT_Strcat (ret, cer_ou1); PORT_Strcat (ret, SEP); }
        if (cer_ou2) { PORT_Strcat (ret, cer_ou2); PORT_Strcat (ret, SEP); }
        if (cer_ou3) { PORT_Strcat (ret, cer_ou3); PORT_Strcat (ret, SEP); }
        if (cer_o)   { PORT_Strcat (ret, cer_o);   PORT_Strcat (ret, SEP); }
        if (cer_l)     PORT_Strcat (ret, cer_l);

	/* return here to avoid unsightly memory leak */

        *result = ret;
        *length = PORT_Strlen (ret);

        return 0;
        }

      break;

    case jarCertSerial:

      ret = CERT_Hexify (&cert->serialNumber, 1);
      break;

    case jarCertExpires:

      ret = DER_UTCDayToAscii (&cert->validity.notAfter);
      break;

    case jarCertNickname:

      ret = jar_choose_nickname (cert);
      break;

    case jarCertFinger:

      dig = JAR_calculate_digest 
         ((char *) cert->derCert.data, cert->derCert.len);

      if (dig)
        {
        hexme.len = sizeof (dig->md5);
        hexme.data = dig->md5;
        ret = CERT_Hexify (&hexme, 1);
        }
      break;

    default:

      return JAR_ERR_GENERAL;
    }

  *result = ret ? PORT_Strdup (ret) : NULL;
  *length = ret ? PORT_Strlen (ret) : 0;

  return 0;
  }

/* 
 *  j a r  _ c e r t _ e l e m e n t
 *
 *  Retrieve an element from an x400ish ascii
 *  designator, in a hackish sort of way. The right
 *  thing would probably be to sort AVATags.
 *
 */

static char *jar_cert_element (char *name, char *tag, int occ)
  {
  if (name && tag)
    {
    char *s;
    int found = 0;

    while (occ--)
      {
      if (PORT_Strstr (name, tag))
        {
        name = PORT_Strstr (name, tag) + PORT_Strlen (tag);
        found = 1;
        }
      else
        {
        name = PORT_Strstr (name, "=");
        if (name == NULL) return NULL;
        found = 0;
        }
      }

    if (!found) return NULL;

    /* must mangle only the copy */
    name = PORT_Strdup (name);

    /* advance to next equal */
    for (s = name; *s && *s != '='; s++)
      /* yip */ ;

    /* back up to previous comma */
    while (s > name && *s != ',') s--;

    /* zap the whitespace and return */
    *s = 0;
    }

  return name;
  }

/* 
 *  j a r _ c h o o s e _ n i c k n a m e
 *
 *  Attempt to determine a suitable nickname for
 *  a certificate with a computer-generated "tmpcertxxx" 
 *  nickname. It needs to be something a user can
 *  understand, so try a few things.
 *
 */

static char *jar_choose_nickname (CERTCertificate *cert)
  {
  char *cert_cn;
  char *cert_o;
  char *cert_cn_o;

  int cn_o_length;

  /* is the existing name ok */

  if (cert->nickname && PORT_Strncmp (cert->nickname, "tmpcert", 7))
    return PORT_Strdup (cert->nickname);

  /* we have an ugly name here people */

  /* Try the CN */
  cert_cn = CERT_GetCommonName (&cert->subject);

  if (cert_cn)
    {
    /* check for duplicate nickname */

#ifdef USE_MOZ_THREAD
    if (jar_moz_nickname (CERT_GetDefaultCertDB(), cert_cn) == NULL)
#else
    if (CERT_FindCertByNickname (CERT_GetDefaultCertDB(), cert_cn) == NULL)
#endif
      return cert_cn;

    /* Try the CN plus O */
    cert_o = CERT_GetOrgName (&cert->subject);

    cn_o_length = PORT_Strlen (cert_cn) + 3 + PORT_Strlen (cert_o) + 20;
    cert_cn_o = (char*)PORT_ZAlloc (cn_o_length);

    PR_snprintf (cert_cn_o, cn_o_length, 
           "%s's %s Certificate", cert_cn, cert_o);

#ifdef USE_MOZ_THREAD
    if (jar_moz_nickname (CERT_GetDefaultCertDB(), cert_cn_o) == NULL)
#else
    if (CERT_FindCertByNickname (CERT_GetDefaultCertDB(), cert_cn_o) == NULL)
#endif
      return cert_cn;
    }

  /* If all that failed, use the ugly nickname */
  return cert->nickname ? PORT_Strdup (cert->nickname) : NULL;
  }

/*
 *  J A R _ c e r t _ h t m l 
 *
 *  Return an HTML representation of the certificate
 *  designated by the given fingerprint, in specified style.
 *
 *  JAR is optional, but supply it if you can in order
 *  to optimize.
 *
 */

char *JAR_cert_html
    (JAR *jar, int style, long keylen, void *key, int *result)
  {
  char *html;
  CERTCertificate *cert;

  *result = -1;

  if (style != 0)
    return NULL;

  cert = jar_get_certificate (jar, keylen, key, result);

  if (cert == NULL || *result < 0)
    return NULL;

  *result = 0;

  html = CERT_HTMLCertInfo (cert, /* show images */ PR_TRUE,
		/*show issuer*/PR_TRUE);

  if (html == NULL)
    *result = -1;

  return html;
  }

/*
 *  J A R _ s t a s h _ c e r t
 *
 *  Stash the certificate pointed to by this
 *  fingerprint, in persistent storage somewhere.
 *
 */

extern int PR_CALLBACK JAR_stash_cert
    (JAR *jar, long keylen, void *key)
  {
  int result = 0;

  char *nickname;
  CERTCertTrust trust;

  CERTCertDBHandle *certdb;
  CERTCertificate *cert, *newcert;

  cert = jar_get_certificate (jar, keylen, key, &result);

  if (result < 0)
    return result;

  if (cert == NULL)
    return JAR_ERR_GENERAL;

  if ((certdb = JAR_open_database()) == NULL)
    return JAR_ERR_GENERAL;

  /* Attempt to give a name to the newish certificate */
  nickname = jar_choose_nickname (cert);

#ifdef USE_MOZ_THREAD
  newcert = jar_moz_nickname (certdb, nickname);
#else
  newcert = CERT_FindCertByNickname (certdb, nickname);
#endif

  if (newcert && newcert->isperm) 
    {
    /* already in permanant database */
    return 0;
    }

  if (newcert) cert = newcert;

  /* FIX, since FindCert returns a bogus dbhandle
     set it ourselves */

  cert->dbhandle = certdb;

#if 0
  nickname = cert->subjectName;
  if (nickname)
    {
    /* Not checking for a conflict here. But this should
       be a new cert or it would have been found earlier. */

    nickname = jar_cert_element (nickname, "CN=", 1);

    if (SEC_CertNicknameConflict (nickname, cert->dbhandle))
      {
      /* conflict */
      nickname = PORT_Realloc (&nickname, PORT_Strlen (nickname) + 3);

      /* Beyond one copy, there are probably serious problems 
         so we will stop at two rather than counting.. */

      PORT_Strcat (nickname, " #2");
      }
    }
#endif

  if (nickname != NULL)
    {
    PORT_Memset ((void *) &trust, 0, sizeof(trust));

#ifdef USE_MOZ_THREAD
    if (jar_moz_perm (cert, nickname, &trust) != SECSuccess) 
#else
    if (CERT_AddTempCertToPerm (cert, nickname, &trust) != SECSuccess) 
#endif
      {
      /* XXX might want to call PORT_GetError here */
      result = JAR_ERR_GENERAL;
      }
    }

  JAR_close_database (certdb);

  return result;
  }

/*
 *  J A R _ f e t c h _ c e r t
 *
 *  Given an opaque identifier of a certificate, 
 *  return the full certificate.
 *
 * The new function, which retrieves by key.
 *
 */

void *JAR_fetch_cert (long length, void *key)
  {
  SECItem seckey;
  CERTCertificate *cert = NULL;

  CERTCertDBHandle *certdb;

  certdb = JAR_open_database();

  if (certdb)
    {
    seckey.len = length;
    seckey.data = (unsigned char*)key;

#ifdef USE_MOZ_THREAD
    cert = jar_moz_certkey (certdb, &seckey);
#else
    cert = CERT_FindCertByKey (certdb, &seckey);
#endif

    JAR_close_database (certdb);
    }

  return (void *) cert;
  }

/*
 *  j a r _ g e t _ m f _ d i g e s t
 *
 *  Retrieve a corresponding saved digest over a section
 *  of the main manifest file. 
 *
 */

static JAR_Digest *jar_get_mf_digest (JAR *jar, char *pathname)
  {
  JAR_Item *it;

  JAR_Digest *dig;

  ZZLink *link;
  ZZList *list;

  list = jar->manifest;

  if (ZZ_ListEmpty (list))
    return NULL;

  for (link = ZZ_ListHead (list);
       !ZZ_ListIterDone (list, link);
       link = link->next)
    {
    it = link->thing;
    if (it->type == jarTypeSect 
          && it->pathname && !PORT_Strcmp (it->pathname, pathname))
      {
      dig = (JAR_Digest *) it->data;
      return dig;
      }
    }

  return NULL;
  }

/*
 *  j a r _ b a s e n a m e
 *
 *  Return the basename -- leading components of path stripped off,
 *  extension ripped off -- of a path.
 *
 */

static char *jar_basename (const char *path)
  {
  char *pith, *e, *basename, *ext;

  if (path == NULL)
    return PORT_Strdup ("");

  pith = PORT_Strdup (path);

  basename = pith;

  while (1)
    {
    for (e = basename; *e && *e != '/' && *e != '\\'; e++)
      /* yip */ ;
    if (*e) 
      basename = ++e; 
    else
      break;
    }

  if ((ext = PORT_Strrchr (basename, '.')) != NULL)
    *ext = 0;

  /* We already have the space allocated */
  PORT_Strcpy (pith, basename);

  return pith;
  }

/*
 *  + + + + + + + + + + + + + + +
 *
 *  CRYPTO ROUTINES FOR JAR
 *
 *  The following functions are the cryptographic 
 *  interface to PKCS7 for Jarnatures.
 *
 *  + + + + + + + + + + + + + + +
 *
 */

/*
 *  j a r _ c a t c h _ b y t e s
 *
 *  In the event signatures contain enveloped data, it will show up here.
 *  But note that the lib/pkcs7 routines aren't ready for it.
 *
 */

static void jar_catch_bytes
     (void *arg, const char *buf, unsigned long len)
  {
  /* Actually this should never be called, since there is
     presumably no data in the signature itself. */
  }

/*
 *  j a r _ v a l i d a t e _ p k c s 7
 *
 *  Validate (and decode, if necessary) a binary pkcs7
 *  signature in DER format.
 *
 */

static int jar_validate_pkcs7 
     (JAR *jar, JAR_Signer *signer, char *data, long length)
  {
  SECItem detdig;

  SEC_PKCS7ContentInfo *cinfo = NULL;
  SEC_PKCS7DecoderContext *dcx;

  int status = 0;
  char *errstring = NULL;

  PORT_Assert( jar != NULL && signer != NULL );

  if (jar == NULL || signer == NULL)
    return JAR_ERR_ORDER;

  signer->valid = JAR_ERR_SIG;

  /* We need a context if we can get one */

#ifdef MOZILLA_CLIENT_OLD
  if (jar->mw == NULL) {
    JAR_set_context (jar, NULL);
  }
#endif


  dcx = SEC_PKCS7DecoderStart
           (jar_catch_bytes, NULL /*cb_arg*/, NULL /*getpassword*/, jar->mw,
            NULL, NULL, NULL);

  if (dcx == NULL) 
    {
    /* strange pkcs7 failure */
    return JAR_ERR_PK7;
    }

  SEC_PKCS7DecoderUpdate (dcx, data, length);
  cinfo = SEC_PKCS7DecoderFinish (dcx);

  if (cinfo == NULL)
    {
    /* strange pkcs7 failure */
    return JAR_ERR_PK7;
    }

  if (SEC_PKCS7ContentIsEncrypted (cinfo))
    {
    /* content was encrypted, fail */
    return JAR_ERR_PK7;
    }

  if (SEC_PKCS7ContentIsSigned (cinfo) == PR_FALSE)
    {
    /* content was not signed, fail */
    return JAR_ERR_PK7;
    }

  PORT_SetError (0);

  /* use SHA1 only */

  detdig.len = SHA1_LENGTH;
  detdig.data = signer->digest->sha1;

#ifdef USE_MOZ_THREAD
  if (jar_moz_verify
        (cinfo, certUsageObjectSigner, &detdig, HASH_AlgSHA1, PR_FALSE)==
		SECSuccess)
#else
  if (SEC_PKCS7VerifyDetachedSignature 
        (cinfo, certUsageObjectSigner, &detdig, HASH_AlgSHA1, PR_FALSE)==
		PR_TRUE)
#endif
    {
    /* signature is valid */
    signer->valid = 0;
    jar_gather_signers (jar, signer, cinfo);
    }
  else
    {
    status = PORT_GetError();

    PORT_Assert( status < 0 );
    if (status >= 0) status = JAR_ERR_SIG;

    jar->valid = status;
    signer->valid = status;

    errstring = JAR_get_error (status);
    /*XP_TRACE(("JAR signature invalid (reason %d = %s)", status, errstring));*/
    }

  jar->pkcs7 = PR_TRUE;
  signer->pkcs7 = PR_TRUE;

  SEC_PKCS7DestroyContentInfo (cinfo);

  return status;
  }

/*
 *  j a r _ g a t h e r _ s i g n e r s
 *
 *  Add the single signer of this signature to the
 *  certificate linked list.
 *
 */

static int jar_gather_signers
     (JAR *jar, JAR_Signer *signer, SEC_PKCS7ContentInfo *cinfo)
  {
  int result;

  CERTCertificate *cert;
  CERTCertDBHandle *certdb;

  SEC_PKCS7SignedData *sdp;
  SEC_PKCS7SignerInfo **pksigners, *pksigner;

  sdp = cinfo->content.signedData;

  if (sdp == NULL)
    return JAR_ERR_PK7;

  pksigners = sdp->signerInfos;

  /* permit exactly one signer */

  if (pksigners == NULL || pksigners [0] == NULL || pksigners [1] != NULL)
    return JAR_ERR_PK7;

  pksigner = *pksigners;
  cert = pksigner->cert;

  if (cert == NULL)
    return JAR_ERR_PK7;

  certdb = JAR_open_database();

  if (certdb == NULL)
    return JAR_ERR_GENERAL;

  result = jar_add_cert (jar, signer, jarTypeSign, cert);

  JAR_close_database (certdb);

  return result;
  }

/*
 *  j a r _ o p e n _ d a t a b a s e
 *
 *  Open the certificate database,
 *  for use by JAR functions.
 *
 */

CERTCertDBHandle *JAR_open_database (void)
  {
  int keepcerts = 0;
  CERTCertDBHandle *certdb;

  /* local_certdb will only be used if calling from a command line tool */
  static CERTCertDBHandle local_certdb;

  certdb = CERT_GetDefaultCertDB();

  if (certdb == NULL) 
    {
    if (CERT_OpenCertDBFilename (&local_certdb, NULL, (PRBool)!keepcerts) != 
	                                                           SECSuccess)
      {
      return NULL;
      }
    certdb = &local_certdb;
    }

  return certdb;
  }

/*
 *  j a r _ c l o s e _ d a t a b a s e
 *
 *  Close the certificate database.
 *  For use by JAR functions.
 *
 */

int JAR_close_database (CERTCertDBHandle *certdb)
  {
  CERTCertDBHandle *defaultdb;

  /* This really just retrieves the handle, nothing more */
  defaultdb = CERT_GetDefaultCertDB();

  /* If there is no default db, it means we opened 
     the permanent database for some reason */

  if (defaultdb == NULL && certdb != NULL)
    CERT_ClosePermCertDB (certdb);

  return 0;
  }

/*
 *  j a r _ g e t _ c e r t i f i c a t e
 *
 *  Return the certificate referenced
 *  by a given fingerprint, or NULL if not found.
 *  Error code is returned in result.
 *
 */

static CERTCertificate *jar_get_certificate
      (JAR *jar, long keylen, void *key, int *result)
  {
  int found = 0;

  JAR_Item *it;
  JAR_Cert *fing = NULL;

  JAR_Context *ctx;

  if (jar == NULL) 
    {
    void *cert;
    cert = JAR_fetch_cert (keylen, key);
    *result = (cert == NULL) ? JAR_ERR_GENERAL : 0;
    return (CERTCertificate *) cert;
    }

  ctx = JAR_find (jar, NULL, jarTypeSign);

  while (JAR_find_next (ctx, &it) >= 0)
    {
    fing = (JAR_Cert *) it->data;

    if (keylen != fing->length)
      continue;

    PORT_Assert( keylen < 0xFFFF );
    if (!PORT_Memcmp (fing->key, key, keylen))
      {
      found = 1;
      break;
      }
    }

  JAR_find_end (ctx);

  if (found == 0)
    {
    *result = JAR_ERR_GENERAL;
    return NULL;
    }

  PORT_Assert(fing != NULL);
  *result = 0;
  return fing->cert;
  }

/*
 *  j a r _ s i g n a l
 *
 *  Nonfatal errors come here to callback Java.
 *  
 */

static int jar_signal 
     (int status, JAR *jar, const char *metafile, char *pathname)
  {
  char *errstring;

  errstring = JAR_get_error (status);

  if (jar->signal)
    {
    (*jar->signal) (status, jar, metafile, pathname, errstring);
    return 0;
    }

  return status;
  }

/*
 *  j a r _ a p p e n d
 *
 *  Tack on an element to one of a JAR's linked
 *  lists, with rudimentary error handling.
 *
 */

int jar_append (ZZList *list, int type, 
        char *pathname, void *data, size_t size)
  {
  JAR_Item *it;
  ZZLink *entity;

  it = (JAR_Item*)PORT_ZAlloc (sizeof (JAR_Item));

  if (it == NULL)
    goto loser;

  if (pathname)
    {
    it->pathname = PORT_Strdup (pathname);
    if (it->pathname == NULL)
      goto loser;
    }

  it->type = (jarType)type;
  it->data = (unsigned char *) data;
  it->size = size;

  entity = ZZ_NewLink (it);

  if (entity)
    {
    ZZ_AppendLink (list, entity);
    return 0;
    }

loser:

  if (it)
    {
    if (it->pathname) PORT_Free (it->pathname);
    PORT_Free (it);
    }

  return JAR_ERR_MEMORY;
  }

/* 
 *  W I N 1 6   s t u f f
 *
 *  These functions possibly belong in xp_mem.c, they operate 
 *  on huge string pointers for win16.
 *
 */

#if defined(XP_WIN16)
int xp_HUGE_STRNCASECMP (char ZHUGEP *buf, char *key, int len)
  {
  while (len--) 
    {
    char c1, c2;

    c1 = *buf++;
    c2 = *key++;

    if (c1 >= 'a' && c1 <= 'z') c1 -= ('a' - 'A');
    if (c2 >= 'a' && c2 <= 'z') c2 -= ('a' - 'A');

    if (c1 != c2) 
      return (c1 < c2) ? -1 : 1;
    }
  return 0;
  }

size_t xp_HUGE_STRLEN (char ZHUGEP *s)
  {
  size_t len = 0L;
  while (*s++) len++;
  return len;
  }

char *xp_HUGE_STRCPY (char *to, char ZHUGEP *from)
  {
  char *ret = to;

  while (*from)
    *to++ = *from++;
  *to = 0;

  return ret;
  }
#endif
