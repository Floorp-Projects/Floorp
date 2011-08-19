/* ***** BEGIN LICENSE BLOCK *****
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
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/*
 *  JARVER
 *
 *  Jarnature Parsing & Verification
 */

#include "nssrenam.h"
#include "jar.h"
#include "jarint.h"
#include "certdb.h"
#include "certt.h"
#include "secpkcs7.h"

/*#include "cdbhdl.h" */
#include "secder.h"

/* from certdb.h */
#define CERTDB_USER (1<<6)

#define SZ 512

static int
jar_validate_pkcs7(JAR *jar, JAR_Signer *signer, char *data, long length);

static void
jar_catch_bytes(void *arg, const char *buf, unsigned long len);

static int
jar_gather_signers(JAR *jar, JAR_Signer *signer, SEC_PKCS7ContentInfo *cinfo);

static char *
jar_eat_line(int lines, int eating, char *data, long *len);

static JAR_Digest *
jar_digest_section(char *manifest, long length);

static JAR_Digest *jar_get_mf_digest(JAR *jar, char *path);

static int
jar_parse_digital_signature(char *raw_manifest, JAR_Signer *signer,
			    long length, JAR *jar);

static int
jar_add_cert(JAR *jar, JAR_Signer *signer, int type, CERTCertificate *cert);

static char *jar_basename(const char *path);

static int
jar_signal(int status, JAR *jar, const char *metafile, char *pathname);

#ifdef DEBUG
static int jar_insanity_check(char *data, long length);
#endif

int
jar_parse_mf(JAR *jar, char *raw_manifest, long length,
	     const char *path, const char *url);

int
jar_parse_sf(JAR *jar, char *raw_manifest, long length,
	     const char *path, const char *url);

int
jar_parse_sig(JAR *jar, const char *path, char *raw_manifest,
	      long length);

int
jar_parse_any(JAR *jar, int type, JAR_Signer *signer,
	      char *raw_manifest, long length, const char *path,
	  const char *url);

static int
jar_internal_digest(JAR *jar, const char *path, char *x_name, JAR_Digest *dig);

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
int
JAR_parse_manifest(JAR *jar, char *raw_manifest, long length,
		   const char *path, const char *url)
{
    int filename_free = 0;

    /* fill in the path, if supplied. This is the location
	 of the jar file on disk, if known */

    if (jar->filename == NULL && path) {
	jar->filename = PORT_Strdup(path);
	if (jar->filename == NULL)
	    return JAR_ERR_MEMORY;
	filename_free = 1;
    }

    /* fill in the URL, if supplied. This is the place
	 from which the jar file was retrieved. */

    if (jar->url == NULL && url) {
	jar->url = PORT_Strdup(url);
	if (jar->url == NULL) {
	    if (filename_free) {
		PORT_Free(jar->filename);
	    }
	    return JAR_ERR_MEMORY;
	}
    }

    /* Determine what kind of file this is from the META-INF
	 directory. It could be MF, SF, or a binary RSA/DSA file */

    if (!PORT_Strncasecmp (raw_manifest, "Manifest-Version:", 17)) {
	return jar_parse_mf(jar, raw_manifest, length, path, url);
    }
    else if (!PORT_Strncasecmp (raw_manifest, "Signature-Version:", 18))
    {
	return jar_parse_sf(jar, raw_manifest, length, path, url);
    } else {
	/* This is probably a binary signature */
	return jar_parse_sig(jar, path, raw_manifest, length);
    }
}

/*
 *  j a r _ p a r s e _ s i g
 *
 *  Pass some manner of RSA or DSA digital signature
 *  on, after checking to see if it comes at an appropriate state.
 *
 */
int
jar_parse_sig(JAR *jar, const char *path, char *raw_manifest,
	      long length)
{
    JAR_Signer *signer;
    int status = JAR_ERR_ORDER;

    if (length <= 128) {
	/* signature is way too small */
	return JAR_ERR_SIG;
    }

    /* make sure that MF and SF have already been processed */

    if (jar->globalmeta == NULL)
	return JAR_ERR_ORDER;

    /* Determine whether or not this RSA file has
	 has an associated SF file */

    if (path) {
	char *owner;
	owner = jar_basename(path);

	if (owner == NULL)
	    return JAR_ERR_MEMORY;

	signer = jar_get_signer(jar, owner);
	PORT_Free(owner);
    } else
	signer = jar_get_signer(jar, "*");

    if (signer == NULL)
	return JAR_ERR_ORDER;


    /* Do not pass a huge pointer to this function,
	 since the underlying security code is unaware. We will
	 never pass >64k through here. */

    if (length > 64000) {
	/* this digital signature is way too big */
	return JAR_ERR_SIG;
    }

    /* don't expense unneeded calloc overhead on non-win16 */
    status = jar_parse_digital_signature(raw_manifest, signer, length, jar);

    return status;
}

/*
 *  j a r _ p a r s e _ m f
 *
 *  Parse the META-INF/manifest.mf file, whose
 *  information applies to all signers.
 *
 */
int
jar_parse_mf(JAR *jar, char *raw_manifest, long length,
	     const char *path, const char *url)
{
    if (jar->globalmeta) {
	/* refuse a second manifest file, if passed for some reason */
	return JAR_ERR_ORDER;
    }

    /* remember a digest for the global section */
    jar->globalmeta = jar_digest_section(raw_manifest, length);
    if (jar->globalmeta == NULL)
	return JAR_ERR_MEMORY;
    return jar_parse_any(jar, jarTypeMF, NULL, raw_manifest, length,
			 path, url);
}

/*
 *  j a r _ p a r s e _ s f
 *
 *  Parse META-INF/xxx.sf, a digitally signed file
 *  pointing to a subset of MF sections.
 *
 */
int
jar_parse_sf(JAR *jar, char *raw_manifest, long length,
	     const char *path, const char *url)
{
    JAR_Signer *signer = NULL;
    int status = JAR_ERR_MEMORY;

    if (jar->globalmeta == NULL) {
	/* It is a requirement that the MF file be passed before the SF file */
	return JAR_ERR_ORDER;
    }

    signer = JAR_new_signer();
    if (signer == NULL)
	goto loser;

    if (path) {
	signer->owner = jar_basename(path);
	if (signer->owner == NULL)
	    goto loser;
    }

    /* check for priors. When someone doctors a jar file
	 to contain identical path entries, prevent the second
	 one from affecting JAR functions */
    if (jar_get_signer(jar, signer->owner)) {
	/* someone is trying to spoof us */
	status = JAR_ERR_ORDER;
	goto loser;
    }

    /* remember its digest */
    signer->digest = JAR_calculate_digest (raw_manifest, length);
    if (signer->digest == NULL)
	goto loser;

    /* Add this signer to the jar */
    ADDITEM(jar->signers, jarTypeOwner, signer->owner, signer,
	    sizeof (JAR_Signer));

    return jar_parse_any(jar, jarTypeSF, signer, raw_manifest, length,
			 path, url);

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
int 
jar_parse_any(JAR *jar, int type, JAR_Signer *signer, 
              char *raw_manifest, long length, const char *path, 
	      const char *url)
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
    if ((status = jar_insanity_check(raw_manifest, raw_len)) < 0)
	return status;
#endif

    /* null terminate the first line */
    raw_manifest = jar_eat_line(0, PR_TRUE, raw_manifest, &raw_len);

    /* skip over the preliminary section */
    /* This is one section at the top of the file with global metainfo */
    while (raw_len > 0) {
	JAR_Metainfo *met;

	raw_manifest = jar_eat_line(1, PR_TRUE, raw_manifest, &raw_len);
	if (raw_len <= 0 || !*raw_manifest)
	    break;

	met = PORT_ZNew(JAR_Metainfo);
	if (met == NULL)
	    return JAR_ERR_MEMORY;

	/* Parse out the header & info */
	if (PORT_Strlen (raw_manifest) >= SZ) {
	    /* almost certainly nonsense */
	    PORT_Free(met);
	    continue;
	}

	PORT_Strcpy (line, raw_manifest);
	x_info = line;

	while (*x_info && *x_info != ' ' && *x_info != '\t' && *x_info != ':')
	    x_info++;

	if (*x_info)
	    *x_info++ = 0;

	while (*x_info == ' ' || *x_info == '\t')
	    x_info++;

	/* metainfo (name, value) pair is now (line, x_info) */
	met->header = PORT_Strdup(line);
	met->info = PORT_Strdup(x_info);

	if (type == jarTypeMF) {
	    ADDITEM (jar->metainfo, jarTypeMeta,
	    /* pathname */ NULL, met, sizeof (JAR_Metainfo));
	}

	/* For SF files, this metadata may be the digests
	       of the MF file, still in the "met" structure. */

	if (type == jarTypeSF) {
	    if (!PORT_Strcasecmp(line, "MD5-Digest"))
		sf_md5 = (char *) met->info;

	    if (!PORT_Strcasecmp(line, "SHA1-Digest") || 
	        !PORT_Strcasecmp(line, "SHA-Digest"))
		sf_sha1 = (char *) met->info;
	}

	if (type != jarTypeMF) {
	    PORT_Free(met->header);
	    if (type != jarTypeSF) {
		PORT_Free(met->info);
	    }
	    PORT_Free(met);
	}
    }

    if (type == jarTypeSF && jar->globalmeta) {
	/* this is a SF file which may contain a digest of the manifest.mf's
	       global metainfo. */

	int match = 0;
	JAR_Digest *glob = jar->globalmeta;

	if (sf_md5) {
	    unsigned int md5_length;
	    unsigned char *md5_digest;

	    md5_digest = ATOB_AsciiToData (sf_md5, &md5_length);
	    PORT_Assert( md5_length == MD5_LENGTH );

	    if (md5_length != MD5_LENGTH)
		return JAR_ERR_CORRUPT;

	    match = PORT_Memcmp(md5_digest, glob->md5, MD5_LENGTH);
	}

	if (sf_sha1 && match == 0) {
	    unsigned int sha1_length;
	    unsigned char *sha1_digest;

	    sha1_digest = ATOB_AsciiToData (sf_sha1, &sha1_length);
	    PORT_Assert( sha1_length == SHA1_LENGTH );

	    if (sha1_length != SHA1_LENGTH)
		return JAR_ERR_CORRUPT;

	    match = PORT_Memcmp(sha1_digest, glob->sha1, SHA1_LENGTH);
	}

	if (match != 0) {
	    /* global digest doesn't match, SF file therefore invalid */
	    jar->valid = JAR_ERR_METADATA;
	    return JAR_ERR_METADATA;
	}
    }

    /* done with top section of global data */
    while (raw_len > 0) {
	*x_md5 = 0;
	*x_sha = 0;
	*x_name = 0;

	/* If this is a manifest file, attempt to get a digest of the following
	   section, without damaging it. This digest will be saved later. */

	if (type == jarTypeMF) {
	    char *sec;
	    long sec_len = raw_len;

	    if (!*raw_manifest || *raw_manifest == '\n') {
		/* skip the blank line */
		sec = jar_eat_line(1, PR_FALSE, raw_manifest, &sec_len);
	    } else
		sec = raw_manifest;

	    if (sec_len > 0 && !PORT_Strncasecmp(sec, "Name:", 5)) {
		if (type == jarTypeMF)
		    mfdig = jar_digest_section(sec, sec_len);
		else
		    mfdig = NULL;
	    }
	}


	while (raw_len > 0) {
	    raw_manifest = jar_eat_line(1, PR_TRUE, raw_manifest, &raw_len);
	    if (raw_len <= 0 || !*raw_manifest)
		break; /* blank line, done with this entry */

	    if (PORT_Strlen(raw_manifest) >= SZ) {
		/* almost certainly nonsense */
		continue;
	    }

	    /* Parse out the name/value pair */
	    PORT_Strcpy(line, raw_manifest);
	    x_info = line;

	    while (*x_info && *x_info != ' ' && *x_info != '\t' && 
	           *x_info != ':')
		x_info++;

	    if (*x_info)
		*x_info++ = 0;

	    while (*x_info == ' ' || *x_info == '\t')
		x_info++;

	    if (!PORT_Strcasecmp(line, "Name"))
		PORT_Strcpy(x_name, x_info);
	    else if (!PORT_Strcasecmp(line, "MD5-Digest"))
		PORT_Strcpy(x_md5, x_info);
	    else if (!PORT_Strcasecmp(line, "SHA1-Digest")
		  || !PORT_Strcasecmp(line, "SHA-Digest"))
		PORT_Strcpy(x_sha, x_info);

	    /* Algorithm list is meta info we don't care about; keeping it out
		 of metadata saves significant space for large jar files */
	    else if (!PORT_Strcasecmp(line, "Digest-Algorithms")
		  || !PORT_Strcasecmp(line, "Hash-Algorithms"))
		continue;

	    /* Meta info is only collected for the manifest.mf file,
		 since the JAR_get_metainfo call does not support identity */
	    else if (type == jarTypeMF) {
		JAR_Metainfo *met;

		/* this is meta-data */
		met = PORT_ZNew(JAR_Metainfo);
		if (met == NULL)
		    return JAR_ERR_MEMORY;

		/* metainfo (name, value) pair is now (line, x_info) */
		if ((met->header = PORT_Strdup(line)) == NULL) {
		    PORT_Free(met);
		    return JAR_ERR_MEMORY;
		}

		if ((met->info = PORT_Strdup(x_info)) == NULL) {
		    PORT_Free(met->header);
		    PORT_Free(met);
		    return JAR_ERR_MEMORY;
		}

		ADDITEM (jar->metainfo, jarTypeMeta,
		x_name, met, sizeof (JAR_Metainfo));
	    }
	}

	if (!*x_name) {
	    /* Whatever that was, it wasn't an entry, because we didn't get a 
	       name. We don't really have anything, so don't record this. */
	    continue;
	}

	dig = PORT_ZNew(JAR_Digest);
	if (dig == NULL)
	    return JAR_ERR_MEMORY;

	if (*x_md5) {
	    unsigned int binary_length;
	    unsigned char *binary_digest;

	    binary_digest = ATOB_AsciiToData (x_md5, &binary_length);
	    PORT_Assert( binary_length == MD5_LENGTH );
	    if (binary_length != MD5_LENGTH) {
		PORT_Free(dig);
		return JAR_ERR_CORRUPT;
	    }
	    memcpy (dig->md5, binary_digest, MD5_LENGTH);
	    dig->md5_status = jarHashPresent;
	}

	if (*x_sha ) {
	    unsigned int binary_length;
	    unsigned char *binary_digest;

	    binary_digest = ATOB_AsciiToData (x_sha, &binary_length);
	    PORT_Assert( binary_length == SHA1_LENGTH );
	    if (binary_length != SHA1_LENGTH) {
		PORT_Free(dig);
		return JAR_ERR_CORRUPT;
	    }
	    memcpy (dig->sha1, binary_digest, SHA1_LENGTH);
	    dig->sha1_status = jarHashPresent;
	}

	PORT_Assert( type == jarTypeMF || type == jarTypeSF );
	if (type == jarTypeMF) {
	    ADDITEM (jar->hashes, jarTypeMF, x_name, dig, sizeof (JAR_Digest));
	} else if (type == jarTypeSF) {
	    ADDITEM (signer->sf, jarTypeSF, x_name, dig, sizeof (JAR_Digest));
	} else {
	    PORT_Free(dig);
	    return JAR_ERR_ORDER;
	}

	/* we're placing these calculated digests of manifest.mf
	   sections in a list where they can subsequently be forgotten */
	if (type == jarTypeMF && mfdig) {
	    ADDITEM (jar->manifest, jarTypeSect,
	    x_name, mfdig, sizeof (JAR_Digest));
	    mfdig = NULL;
	}

	/* Retrieve our saved SHA1 digest from saved copy and check digests.
	   This is just comparing the digest of the MF section as indicated in
	   the SF file with the one we remembered from parsing the MF file */

	if (type == jarTypeSF) {
	    if ((status = jar_internal_digest(jar, path, x_name, dig)) < 0)
		return status;
	}
    }

    return 0;
}

static int
jar_internal_digest(JAR *jar, const char *path, char *x_name, JAR_Digest *dig)
{
    int cv;
    int status;

    JAR_Digest *savdig;

    savdig = jar_get_mf_digest(jar, x_name);
    if (savdig == NULL) {
	/* no .mf digest for this pathname */
	status = jar_signal(JAR_ERR_ENTRY, jar, path, x_name);
	if (status < 0)
	    return 0; /* was continue; */
	return status;
    }

    /* check for md5 consistency */
    if (dig->md5_status) {
	cv = PORT_Memcmp(savdig->md5, dig->md5, MD5_LENGTH);
	/* md5 hash of .mf file is not what expected */
	if (cv) {
	    status = jar_signal(JAR_ERR_HASH, jar, path, x_name);

	    /* bad hash, man */
	    dig->md5_status = jarHashBad;
	    savdig->md5_status = jarHashBad;

	    if (status < 0)
		return 0; /* was continue; */
	    return status;
	}
    }

    /* check for sha1 consistency */
    if (dig->sha1_status) {
	cv = PORT_Memcmp(savdig->sha1, dig->sha1, SHA1_LENGTH);
	/* sha1 hash of .mf file is not what expected */
	if (cv) {
	    status = jar_signal(JAR_ERR_HASH, jar, path, x_name);

	    /* bad hash, man */
	    dig->sha1_status = jarHashBad;
	    savdig->sha1_status = jarHashBad;

	    if (status < 0)
		return 0; /* was continue; */
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
static int
jar_insanity_check(char *data, long length)
{
    int c;
    long off;

    for (off = 0; off < length; off++) {
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
static int
jar_parse_digital_signature(char *raw_manifest, JAR_Signer *signer,
			    long length, JAR *jar)
{
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
static int
jar_add_cert(JAR *jar, JAR_Signer *signer, int type, CERTCertificate *cert)
{
    JAR_Cert *fing;
    unsigned char *keyData;

    if (cert == NULL)
	return JAR_ERR_ORDER;

    fing = PORT_ZNew(JAR_Cert);
    if (fing == NULL)
	goto loser;

    fing->cert = CERT_DupCertificate (cert);

    /* get the certkey */
    fing->length = cert->derIssuer.len + 2 + cert->serialNumber.len;
    fing->key = keyData = (unsigned char *) PORT_ZAlloc(fing->length);
    if (fing->key == NULL)
	goto loser;
    keyData[0] = ((cert->derIssuer.len) >> 8) & 0xff;
    keyData[1] = ((cert->derIssuer.len) & 0xff);
    PORT_Memcpy(&keyData[2], cert->derIssuer.data, cert->derIssuer.len);
    PORT_Memcpy(&keyData[2+cert->derIssuer.len], cert->serialNumber.data,
		cert->serialNumber.len);

    ADDITEM (signer->certs, type, NULL, fing, sizeof (JAR_Cert));
    return 0;

loser:
    if (fing) {
	if (fing->cert)
	    CERT_DestroyCertificate (fing->cert);
	PORT_Free(fing);
    }
    return JAR_ERR_MEMORY;
}

/*
 *  e a t _ l i n e
 *
 * Reads and/or modifies input buffer "data" of length "*len".
 * This function does zero, one or two of the following tasks:
 * 1) if "lines" is non-zero, it reads and discards that many lines from
 *    the input.  NUL characters are treated as end-of-line characters,
 *    not as end-of-input characters.  The input is NOT NUL terminated.
 *    Note: presently, all callers pass either 0 or 1 for lines.
 * 2) After skipping the specified number of input lines, if "eating" is 
 *    non-zero, it finds the end of the next line of input and replaces
 *    the end of line character(s) with a NUL character.
 *  This function modifies the input buffer, containing the file, in place. 
 *  This function handles PC, Mac, and Unix style text files.
 *  On entry, *len contains the maximum number of characters that this
 *  function should ever examine, starting with the character in *data.
 *  On return, *len is reduced by the number of characters skipped by the
 *  first task, if any;
 *  If lines is zero and eating is false, this function returns
 *  the value in the data argument, but otherwise does nothing.
 */
static char *
jar_eat_line(int lines, int eating, char *data, long *len)
{
    char *start = data;
    long maxLen = *len;

    if (maxLen <= 0)
	return start;

#define GO_ON ((data - start) < maxLen)

    /* Eat the requisite number of lines, if any;
       prior to terminating the current line with a 0. */
    for (/* yip */ ; lines > 0; lines--) {
	while (GO_ON && *data && *data != '\r' && *data != '\n')
	    data++;

	/* Eat any leading CR */
	if (GO_ON && *data == '\r')
	    data++;

	/* After the CR, ok to eat one LF */
	if (GO_ON && *data == '\n')
	    data++;

	/* If there are NULs, this function probably put them there */
	while (GO_ON && !*data)
	    data++;
    }
    maxLen -= data - start;           /* we have this many characters left. */
    *len  = maxLen;
    start = data;                     /* now start again here.            */
    if (maxLen > 0 && eating) {
	/* Terminate this line with a 0 */
	while (GO_ON && *data && *data != '\n' && *data != '\r')
	    data++;

	/* If not past the end, we are allowed to eat one CR */
	if (GO_ON && *data == '\r')
	    *data++ = 0;

	/* After the CR (if any), if not past the end, ok to eat one LF */
	if (GO_ON && *data == '\n')
	    *data++ = 0;
    }
    return start;
}
#undef GO_ON

/*
 *  j a r _ d i g e s t _ s e c t i o n
 *
 *  Return the digests of the next section of the manifest file.
 *  Does not damage the manifest file, unlike parse_manifest.
 *
 */
static JAR_Digest *
jar_digest_section(char *manifest, long length)
{
    long global_len;
    char *global_end;

    global_end = manifest;
    global_len = length;

    while (global_len > 0) {
	global_end = jar_eat_line(1, PR_FALSE, global_end, &global_len);
	if (global_len > 0 && (*global_end == 0 || *global_end == '\n'))
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
int PR_CALLBACK
JAR_verify_digest(JAR *jar, const char *name, JAR_Digest *dig)
{
    JAR_Item *it;
    JAR_Digest *shindig;
    ZZLink *link;
    ZZList *list = jar->hashes;
    int result1 = 0;
    int result2 = 0;


    if (jar->valid < 0) {
	/* signature not valid */
	return JAR_ERR_SIG;
    }
    if (ZZ_ListEmpty (list)) {
	/* empty list */
	return JAR_ERR_PNF;
    }

    for (link = ZZ_ListHead (list);
	     !ZZ_ListIterDone (list, link);
	     link = link->next) {
	it = link->thing;
	if (it->type == jarTypeMF
	    && it->pathname && !PORT_Strcmp(it->pathname, name)) {
	    shindig = (JAR_Digest *) it->data;
	    if (shindig->md5_status) {
		if (shindig->md5_status == jarHashBad)
		    return JAR_ERR_HASH;
		result1 = memcmp (dig->md5, shindig->md5, MD5_LENGTH);
	    }
	    if (shindig->sha1_status) {
		if (shindig->sha1_status == jarHashBad)
		    return JAR_ERR_HASH;
		result2 = memcmp (dig->sha1, shindig->sha1, SHA1_LENGTH);
	    }
	    return (result1 == 0 && result2 == 0) ? 0 : JAR_ERR_HASH;
	}
    }
    return JAR_ERR_PNF;
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
CERTCertificate *
JAR_fetch_cert(long length, void *key)
{
    CERTIssuerAndSN issuerSN;
    CERTCertificate *cert = NULL;
    CERTCertDBHandle *certdb;

    certdb = JAR_open_database();
    if (certdb) {
	unsigned char *keyData = (unsigned char *)key;
	issuerSN.derIssuer.len = (keyData[0] << 8) + keyData[0];
	issuerSN.derIssuer.data = &keyData[2];
	issuerSN.serialNumber.len = length - (2 + issuerSN.derIssuer.len);
	issuerSN.serialNumber.data = &keyData[2+issuerSN.derIssuer.len];
	cert = CERT_FindCertByIssuerAndSN (certdb, &issuerSN);
	JAR_close_database (certdb);
    }
    return cert;
}

/*
 *  j a r _ g e t _ m f _ d i g e s t
 *
 *  Retrieve a corresponding saved digest over a section
 *  of the main manifest file.
 *
 */
static JAR_Digest *
jar_get_mf_digest(JAR *jar, char *pathname)
{
    JAR_Item *it;
    JAR_Digest *dig;
    ZZLink *link;
    ZZList *list = jar->manifest;

    if (ZZ_ListEmpty (list))
	return NULL;

    for (link = ZZ_ListHead (list);
	 !ZZ_ListIterDone (list, link);
	 link = link->next) {
	it = link->thing;
	if (it->type == jarTypeSect
	    && it->pathname && !PORT_Strcmp(it->pathname, pathname)) {
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
static char *
jar_basename(const char *path)
{
    char *pith, *e, *basename, *ext;

    if (path == NULL)
	return PORT_Strdup("");

    pith = PORT_Strdup(path);
    basename = pith;
    while (1) {
	for (e = basename; *e && *e != '/' && *e != '\\'; e++)
	    /* yip */ ;
	if (*e)
	    basename = ++e;
	else
	    break;
    }

    if ((ext = PORT_Strrchr(basename, '.')) != NULL)
	*ext = 0;

    /* We already have the space allocated */
    PORT_Strcpy(pith, basename);
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
static void
jar_catch_bytes(void *arg, const char *buf, unsigned long len)
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
static int 
jar_validate_pkcs7(JAR *jar, JAR_Signer *signer, char *data, long length)
{

    SEC_PKCS7ContentInfo *cinfo = NULL;
    SEC_PKCS7DecoderContext *dcx;
    PRBool goodSig;
    int status = 0;
    SECItem detdig;

    PORT_Assert( jar != NULL && signer != NULL );

    if (jar == NULL || signer == NULL)
	return JAR_ERR_ORDER;

    signer->valid = JAR_ERR_SIG;

    /* We need a context if we can get one */
    dcx = SEC_PKCS7DecoderStart(jar_catch_bytes, NULL /*cb_arg*/,
				NULL /*getpassword*/, jar->mw,
				NULL, NULL, NULL);
    if (dcx == NULL) {
	/* strange pkcs7 failure */
	return JAR_ERR_PK7;
    }

    SEC_PKCS7DecoderUpdate (dcx, data, length);
    cinfo = SEC_PKCS7DecoderFinish (dcx);
    if (cinfo == NULL) {
	/* strange pkcs7 failure */
	return JAR_ERR_PK7;
    }
    if (SEC_PKCS7ContentIsEncrypted (cinfo)) {
	/* content was encrypted, fail */
	return JAR_ERR_PK7;
    }
    if (SEC_PKCS7ContentIsSigned (cinfo) == PR_FALSE) {
	/* content was not signed, fail */
	return JAR_ERR_PK7;
    }

    PORT_SetError(0);

    /* use SHA1 only */
    detdig.len = SHA1_LENGTH;
    detdig.data = signer->digest->sha1;
    goodSig = SEC_PKCS7VerifyDetachedSignature(cinfo,
					       certUsageObjectSigner,
					       &detdig, HASH_AlgSHA1,
					       PR_FALSE);
    jar_gather_signers(jar, signer, cinfo);
    if (goodSig == PR_TRUE) {
	/* signature is valid */
	signer->valid = 0;
    } else {
	status = PORT_GetError();
	PORT_Assert( status < 0 );
	if (status >= 0)
	    status = JAR_ERR_SIG;
	jar->valid = status;
	signer->valid = status;
    }
    jar->pkcs7 = PR_TRUE;
    signer->pkcs7 = PR_TRUE;
    SEC_PKCS7DestroyContentInfo(cinfo);
    return status;
}

/*
 *  j a r _ g a t h e r _ s i g n e r s
 *
 *  Add the single signer of this signature to the
 *  certificate linked list.
 *
 */
static int
jar_gather_signers(JAR *jar, JAR_Signer *signer, SEC_PKCS7ContentInfo *cinfo)
{
    int result;
    CERTCertificate *cert;
    CERTCertDBHandle *certdb;
    SEC_PKCS7SignedData *sdp = cinfo->content.signedData;
    SEC_PKCS7SignerInfo **pksigners, *pksigner;

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

    result = jar_add_cert(jar, signer, jarTypeSign, cert);
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
CERTCertDBHandle *
JAR_open_database(void)
{
    return CERT_GetDefaultCertDB();
}

/*
 *  j a r _ c l o s e _ d a t a b a s e
 *
 *  Close the certificate database.
 *  For use by JAR functions.
 *
 */
int 
JAR_close_database(CERTCertDBHandle *certdb)
{
    return 0;
}


/*
 *  j a r _ s i g n a l
 *
 *  Nonfatal errors come here to callback Java.
 *
 */
static int
jar_signal(int status, JAR *jar, const char *metafile, char *pathname)
{
    char *errstring = JAR_get_error (status);
    if (jar->signal) {
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
int
jar_append(ZZList *list, int type, char *pathname, void *data, size_t size)
{
    JAR_Item *it = PORT_ZNew(JAR_Item);
    ZZLink *entity;

    if (it == NULL)
	goto loser;

    if (pathname) {
	it->pathname = PORT_Strdup(pathname);
	if (it->pathname == NULL)
	    goto loser;
    }

    it->type = (jarType)type;
    it->data = (unsigned char *) data;
    it->size = size;
    entity = ZZ_NewLink (it);
    if (entity) {
	ZZ_AppendLink (list, entity);
	return 0;
    }

loser:
    if (it) {
	if (it->pathname) 
	    PORT_Free(it->pathname);
	PORT_Free(it);
    }
    return JAR_ERR_MEMORY;
}
