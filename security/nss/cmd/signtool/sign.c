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

#include "signtool.h"
#include "zip.h" 
#include "prmem.h"
#include "blapi.h"
#include "sechash.h"	/* for HASH_GetHashObject() */

static int create_pk7 (char *dir, char *keyName, int *keyType);
static int jar_find_key_type (CERTCertificate *cert);
static int manifesto (char *dirname, char *install_script, PRBool recurse);
static int manifesto_fn(char *relpath, char *basedir, char *reldir,
	char *filename, void *arg);
static int sign_all_arc_fn(char *relpath, char *basedir, char *reldir,
	char *filename, void *arg);
static int add_meta (FILE *fp, char *name);
static int SignFile (FILE *outFile, FILE *inFile, CERTCertificate *cert);
static int generate_SF_file (char *manifile, char *who);
static int calculate_MD5_range (FILE *fp, long r1, long r2, JAR_Digest *dig);
static void SignOut (void *arg, const char *buf, unsigned long len);

static char *metafile = NULL;
static int optimize = 0;
static FILE *mf;
static ZIPfile *zipfile=NULL;

/* 
 *  S i g n A r c h i v e
 *
 *  Sign an individual archive tree. A directory 
 *  called META-INF is created underneath this.
 *
 */
int
SignArchive(char *tree, char *keyName, char *zip_file, int javascript,
	char *meta_file, char *install_script, int _optimize, PRBool recurse)
{
  int status;
  char tempfn [FNSIZE], fullfn [FNSIZE];
	int keyType = rsaKey;

	metafile = meta_file;
	optimize = _optimize;

	if(zip_file) {
		zipfile = JzipOpen(zip_file, NULL /*no comment*/);
	}

  manifesto (tree, install_script, recurse);

  if (keyName)
    {
    status = create_pk7 (tree, keyName, &keyType);
    if (status < 0)
      {
      PR_fprintf(errorFD, "the tree \"%s\" was NOT SUCCESSFULLY SIGNED\n", tree);
		errorCount++;
      exit (ERRX);
      }
    }

  /* mf to zip */

  strcpy (tempfn, "META-INF/manifest.mf");
  sprintf (fullfn, "%s/%s", tree, tempfn);
  JzipAdd(fullfn, tempfn, zipfile, compression_level);

  /* sf to zip */

  sprintf (tempfn, "META-INF/%s.sf", base);
  sprintf (fullfn, "%s/%s", tree, tempfn);
  JzipAdd(fullfn, tempfn, zipfile, compression_level);

  /* rsa/dsa to zip */

  sprintf (tempfn, "META-INF/%s.%s", base, (keyType==dsaKey ? "dsa" : "rsa"));
  sprintf (fullfn, "%s/%s", tree, tempfn);
  JzipAdd(fullfn, tempfn, zipfile, compression_level);

  JzipClose(zipfile);

	if(verbosity >= 0) {
		if (javascript) {
			PR_fprintf(outputFD,"jarfile \"%s\" signed successfully\n",
				zip_file);
		} else {
			PR_fprintf(outputFD, "tree \"%s\" signed successfully\n", tree);
		}
	}

  return 0;
}

typedef struct {
	char *keyName;
	int javascript;
	char *metafile;
	char *install_script;
	int optimize;
} SignArcInfo;

/* 
 *  S i g n A l l A r c
 *
 *  Javascript may generate multiple .arc directories, one
 *  for each jar archive needed. Sign them all.
 *
 */
int
SignAllArc(char *jartree, char *keyName, int javascript, char *metafile,
	char *install_script, int optimize, PRBool recurse)
{
	SignArcInfo info;

	info.keyName = keyName;
	info.javascript = javascript;
	info.metafile = metafile;
	info.install_script = install_script;
	info.optimize = optimize;

	return foreach(jartree, "", sign_all_arc_fn, recurse,
		PR_TRUE /*include dirs*/, (void*)&info);
}

static int
sign_all_arc_fn(char *relpath, char *basedir, char *reldir, char *filename,
	void *arg)
{
	char *zipfile=NULL;
	char *arc=NULL, *archive=NULL;
	int retval=0;
	SignArcInfo *infop = (SignArcInfo*)arg;

	/* Make sure there is one and only one ".arc" in the relative path, 
	 * and that it is at the end of the path (don't sign .arcs within .arcs) */
	if ( (PL_strcaserstr(relpath, ".arc") == relpath + strlen(relpath) - 4) &&
		 (PL_strcasestr(relpath, ".arc") == relpath + strlen(relpath) - 4) ) {

		if(!infop) {
			PR_fprintf(errorFD, "%s: Internal failure\n", PROGRAM_NAME);
			errorCount++;
			retval = -1;
			goto finish;
		}
		archive = PR_smprintf("%s/%s", basedir, relpath);

		zipfile = PL_strdup(archive);
		arc = PORT_Strrchr (zipfile, '.');

		if (arc == NULL) {
			PR_fprintf(errorFD, "%s: Internal failure\n", PROGRAM_NAME);
			errorCount++;
			retval = -1;
			goto finish;
		}

		PL_strcpy (arc, ".jar");

		if(verbosity >= 0) {
			PR_fprintf(outputFD, "\nsigning: %s\n", zipfile);
		}
		retval = SignArchive(archive, infop->keyName, zipfile,
			infop->javascript, infop->metafile, infop->install_script,
			infop->optimize, PR_TRUE /* recurse */);
	}
finish:
	if(archive) PR_Free(archive);
	if(zipfile) PR_Free(zipfile);

	return retval;
}

/*********************************************************************
 *
 * c r e a t e _ p k 7
 */
static int
create_pk7 (char *dir, char *keyName, int *keyType)
{
  int status = 0;
	char *file_ext;

  CERTCertificate *cert;
  CERTCertDBHandle *db;

  FILE *in, *out;

  char sf_file [FNSIZE];
  char pk7_file [FNSIZE];


  /* open cert database */
  db = CERT_GetDefaultCertDB();

  if (db == NULL) 
    return -1;


  /* find cert */
	/*cert = CERT_FindCertByNicknameOrEmailAddr(db, keyName);*/
	cert = PK11_FindCertFromNickname(keyName, NULL /*wincx*/);

  if (cert == NULL) 
    {
    SECU_PrintError
      (
      PROGRAM_NAME,
      "the cert \"%s\" does not exist in the database",
      keyName
      );
    return -1;
    }


  /* determine the key type, which sets the extension for pkcs7 object */

  *keyType = jar_find_key_type (cert);
  file_ext = (*keyType == dsaKey) ? "dsa" : "rsa";

  sprintf (sf_file, "%s/META-INF/%s.sf", dir, base);
  sprintf (pk7_file, "%s/META-INF/%s.%s", dir, base, file_ext);

  if ((in = fopen (sf_file, "rb")) == NULL)
    {
    PR_fprintf(errorFD, "%s: Can't open %s for reading\n", PROGRAM_NAME, sf_file);
	errorCount++;
    exit (ERRX);
    }

  if ((out = fopen (pk7_file, "wb")) == NULL)
    {
    PR_fprintf(errorFD, "%s: Can't open %s for writing\n", PROGRAM_NAME, sf_file);
	errorCount++;
    exit (ERRX);
    }

  status = SignFile (out, in, cert);

  fclose (in);
  fclose (out);

  if (status)
    {
    PR_fprintf(errorFD, "%s: PROBLEM signing data (%s)\n",
		PROGRAM_NAME, SECU_ErrorString ((int16) PORT_GetError()));
	errorCount++;
    return -1;
    }

  return 0;
}

/*
 *  j a r _ f i n d _ k e y _ t y p e
 * 
 *  Determine the key type for a given cert, which 
 * should be rsaKey or dsaKey. Any error return 0.
 *
 */
static int
jar_find_key_type (CERTCertificate *cert)
{
  PK11SlotInfo *slot = NULL;
  SECKEYPrivateKey *privk = NULL;

  /* determine its type */
  PK11_FindObjectForCert (cert, /*wincx*/ NULL, &slot);

  if (slot == NULL)
    {
    PR_fprintf(errorFD, "warning - can't find slot for this cert\n");
	warningCount++;
    return 0;
    }

  privk = PK11_FindPrivateKeyFromCert (slot, cert, /*wincx*/ NULL);

  if (privk == NULL)
    {
    PR_fprintf(errorFD, "warning - can't find private key for this cert\n");
	warningCount++;
    return 0;
    }

  return privk->keyType;
  }


/*
 *  m a n i f e s t o
 *
 *  Run once for every subdirectory in which a 
 *  manifest is to be created -- usually exactly once.
 *
 */
static int
manifesto (char *dirname, char *install_script, PRBool recurse)
{
  char metadir [FNSIZE], sfname [FNSIZE];

  /* Create the META-INF directory to hold signing info */

  if (PR_Access (dirname, PR_ACCESS_READ_OK))
    {
    PR_fprintf(errorFD, "%s: unable to read your directory: %s\n", PROGRAM_NAME,
		dirname);
	errorCount++;
    perror (dirname);
    exit (ERRX);
    }

	if (PR_Access (dirname, PR_ACCESS_WRITE_OK)) {
		PR_fprintf(errorFD, "%s: unable to write to your directory: %s\n",
			PROGRAM_NAME, dirname);
		errorCount++;
		perror(dirname);
		exit(ERRX);
	}

  sprintf (metadir, "%s/META-INF", dirname);

  strcpy (sfname, metadir);

  PR_MkDir (metadir, 0777);

  strcat (metadir, "/");
  strcat (metadir, MANIFEST);

  if ((mf = fopen (metadir, "wb")) == NULL)
    {
    perror (MANIFEST);
    PR_fprintf(errorFD, "%s: Probably, the directory you are trying to"

		" sign has\n", PROGRAM_NAME);
    PR_fprintf(errorFD, "%s: permissions problems or may not exist.\n",
		PROGRAM_NAME);
	errorCount++;
    exit (ERRX);
    }

	if(verbosity >= 0) {
		PR_fprintf(outputFD, "Generating %s file..\n", metadir);
	}

  fprintf(mf, "Manifest-Version: 1.0\n");
  fprintf (mf, "Created-By: %s\n", CREATOR);
  fprintf (mf, "Comments: %s\n", BREAKAGE);

  if (scriptdir)
    {
    fprintf (mf, "Comments: --\n");
    fprintf (mf, "Comments: --\n");
    fprintf (mf, "Comments: -- This archive signs Javascripts which may not necessarily\n");
    fprintf (mf, "Comments: -- be included in the physical jar file.\n");
    fprintf (mf, "Comments: --\n");
    fprintf (mf, "Comments: --\n");
    }

  if (install_script)
    fprintf (mf, "Install-Script: %s\n", install_script);

  if (metafile)
    add_meta (mf, "+");

  /* Loop through all files & subdirectories */
  foreach (dirname, "", manifesto_fn, recurse, PR_FALSE /*include dirs */,
		(void*)NULL);

  fclose (mf);

  strcat (sfname, "/");
  strcat (sfname, base);
  strcat (sfname, ".sf");

	if(verbosity >= 0) {
		PR_fprintf(outputFD, "Generating %s.sf file..\n", base);
	}
  generate_SF_file (metadir, sfname);

  return 0;
}

/*
 *  m a n i f e s t o _ f n
 *
 *  Called by pointer from manifesto(), once for
 *  each file within the directory.
 *
 */
static int manifesto_fn 
     (char *relpath, char *basedir, char *reldir, char *filename, void *arg)
{
  int use_js;

  JAR_Digest dig;
  char fullname [FNSIZE];

	if(verbosity >= 0) {
		PR_fprintf(outputFD, "--> %s\n", relpath);
	}

	/* extension matching */
	if(extensionsGiven) {
	    char *ext;

		ext = PL_strrchr(relpath, '.');
		if(!ext) {
			return 0;
		} else {
			if(!PL_HashTableLookup(extensions, ext)) {
				return 0;
			}
		}
	}

  sprintf (fullname, "%s/%s", basedir, relpath);

  fprintf (mf, "\n");

  use_js = 0;

  if (scriptdir && !PORT_Strcmp (scriptdir, reldir))
    use_js++;

  /* sign non-.js files inside .arc directories 
     using the javascript magic */

  if ( (PL_strcaserstr(filename, ".js") != filename + strlen(filename) - 3)
		&& (PL_strcaserstr(reldir, ".arc") == reldir + strlen(filename)-4))
    use_js++;

  if (use_js)
    {
    fprintf (mf, "Name: %s\n", filename);
    fprintf (mf, "Magic: javascript\n");

    if (optimize == 0)
      fprintf (mf, "javascript.id: %s\n", filename);

    if (metafile) 
      add_meta (mf, filename);
    }
  else
    {
    fprintf (mf, "Name: %s\n", relpath);
    if (metafile)
      add_meta (mf, relpath);
    }

  JAR_digest_file (fullname, &dig);


  if (optimize == 0)
    {
    fprintf (mf, "Digest-Algorithms: MD5 SHA1\n");
    fprintf (mf, "MD5-Digest: %s\n", BTOA_DataToAscii (dig.md5, MD5_LENGTH));
    }

  fprintf (mf, "SHA1-Digest: %s\n", BTOA_DataToAscii (dig.sha1, SHA1_LENGTH));

	if(!use_js) {
		JzipAdd(fullname, relpath, zipfile, compression_level);
	}

  return 0;
}

/*
 *  a d d _ m e t a
 *
 *  Parse the metainfo file, and add any details
 *  necessary to the manifest file. In most cases you
 *  should be using the -i option (ie, for SmartUpdate).
 *
 */
static int add_meta (FILE *fp, char *name)
{
  FILE *met;
  char buf [BUFSIZ];

  int place;
  char *pattern, *meta;

  int num = 0;

  if ((met = fopen (metafile, "r")) != NULL)
    {
    while (fgets (buf, BUFSIZ, met))
      {
      char *s;

      for (s = buf; *s && *s != '\n' && *s != '\r'; s++);
      *s = 0;

      if (*buf == 0)
        continue;

      pattern = buf;

      /* skip to whitespace */
      for (s = buf; *s && *s != ' ' && *s != '\t'; s++);

      /* terminate pattern */
      if (*s == ' ' || *s == '\t') *s++ = 0;

      /* eat through whitespace */
      while (*s == ' ' || *s == '\t') s++;

      meta = s;

      /* this will eventually be regexp matching */

      place = 0;
      if (!PORT_Strcmp (pattern, name))
        place = 1;

      if (place)
        {
        num++;
		if(verbosity >= 0) {
			PR_fprintf(outputFD, "[%s] %s\n", name, meta);
		}
        fprintf (fp, "%s\n", meta);
        }
      }
    fclose (met);
    }
  else
    {
    PR_fprintf(errorFD, "%s: can't open metafile: %s\n", PROGRAM_NAME, metafile);
	errorCount++;
    exit (ERRX);
    }

  return num;
}

/**********************************************************************
 *
 * S i g n F i l e
 */
static int
SignFile (FILE *outFile, FILE *inFile, CERTCertificate *cert)
{
  int nb;
  char ibuf[4096], digestdata[32];
  const SECHashObject *hashObj;
  void *hashcx;
  unsigned int len;

  SECItem digest;
  SEC_PKCS7ContentInfo *cinfo;
  SECStatus rv;

  if (outFile == NULL || inFile == NULL || cert == NULL)
    return -1;

  /* XXX probably want to extend interface to allow other hash algorithms */
  hashObj = HASH_GetHashObject(HASH_AlgSHA1);

  hashcx = (* hashObj->create)();
  if (hashcx == NULL)
    return -1;

  (* hashObj->begin)(hashcx);

  for (;;) 
    {
    if (feof(inFile)) break;
    nb = fread(ibuf, 1, sizeof(ibuf), inFile);
    if (nb == 0) 
      {
      if (ferror(inFile)) 
        {
        PORT_SetError(SEC_ERROR_IO);
	(* hashObj->destroy)(hashcx, PR_TRUE);
	return -1;
        }
      /* eof */
      break;
      }
    (* hashObj->update)(hashcx, (unsigned char *) ibuf, nb);
    }

  (* hashObj->end)(hashcx, (unsigned char *) digestdata, &len, 32);
  (* hashObj->destroy)(hashcx, PR_TRUE);

  digest.data = (unsigned char *) digestdata;
  digest.len = len;

  cinfo = SEC_PKCS7CreateSignedData 
              (cert, certUsageObjectSigner, NULL, 
                  SEC_OID_SHA1, &digest, NULL, NULL);

  if (cinfo == NULL)
    return -1;

  rv = SEC_PKCS7IncludeCertChain (cinfo, NULL);
  if (rv != SECSuccess) 
    {
    SEC_PKCS7DestroyContentInfo (cinfo);
    return -1;
    }

  if (no_time == 0)
    {
    rv = SEC_PKCS7AddSigningTime (cinfo);
    if (rv != SECSuccess)
      {
      /* don't check error */
      }
    }

	if(password) {
		rv = SEC_PKCS7Encode(cinfo, SignOut, outFile, NULL, password_hardcode,
			NULL);
	} else {
		rv = SEC_PKCS7Encode(cinfo, SignOut, outFile, NULL, NULL,
			NULL);
	}
		

  SEC_PKCS7DestroyContentInfo (cinfo);

  if (rv != SECSuccess)
    return -1;

  return 0;
}

/*
 *  g e n e r a t e _ S F _ f i l e 
 *
 *  From the supplied manifest file, calculates
 *  digests on the various sections, creating a .SF
 *  file in the process.
 * 
 */
static int generate_SF_file (char *manifile, char *who)
{
  FILE *sf;
  FILE *mf;

  long r1, r2, r3;

  char whofile [FNSIZE];
  char *buf, *name;

  JAR_Digest dig;

  int line = 0;

  strcpy (whofile, who);

  if ((mf = fopen (manifile, "rb")) == NULL)
    {
    perror (manifile);
    exit (ERRX);
    }

  if ((sf = fopen (whofile, "wb")) == NULL)
    {
    perror (who);
    exit (ERRX);
    }

  buf = (char *) PORT_ZAlloc (BUFSIZ);

  if (buf)
    name = (char *) PORT_ZAlloc (BUFSIZ);

  if (buf == NULL || name == NULL)
    out_of_memory();

  fprintf (sf, "Signature-Version: 1.0\n");
  fprintf (sf, "Created-By: %s\n", CREATOR);
  fprintf (sf, "Comments: %s\n", BREAKAGE);

  if (fgets (buf, BUFSIZ, mf) == NULL)
    {
    PR_fprintf(errorFD, "%s: empty manifest file!\n", PROGRAM_NAME);
	errorCount++;
    exit (ERRX);
    }

  if (strncmp (buf, "Manifest-Version:", 17))
    {
    PR_fprintf(errorFD, "%s: not a manifest file!\n", PROGRAM_NAME);
	errorCount++;
    exit (ERRX);
    }

  fseek (mf, 0L, SEEK_SET);

  /* Process blocks of headers, and calculate their hashen */

  while (1)
    {
    /* Beginning range */
    r1 = ftell (mf);

    if (fgets (name, BUFSIZ, mf) == NULL)
      break;

    line++;

    if (r1 != 0 && strncmp (name, "Name:", 5))
      {
      PR_fprintf(errorFD, "warning: unexpected input in manifest file \"%s\" at line %d:\n", manifile, line);
      PR_fprintf(errorFD, "%s\n", name);
		warningCount++;
      }

    while (fgets (buf, BUFSIZ, mf))
      {
      if (*buf == 0 || *buf == '\n' || *buf == '\r')
        break;

      line++;

      /* Ending range for hashing */
      r2 = ftell (mf);
      }

    r3 = ftell (mf);

    if (r1)
      {
      fprintf (sf, "\n");
      fprintf (sf, "%s", name);
      }

    calculate_MD5_range (mf, r1, r2, &dig);

    if (optimize == 0)
      {
      fprintf (sf, "Digest-Algorithms: MD5 SHA1\n");
      fprintf (sf, "MD5-Digest: %s\n", 
         BTOA_DataToAscii (dig.md5, MD5_LENGTH));
      }

    fprintf (sf, "SHA1-Digest: %s\n", 
       BTOA_DataToAscii (dig.sha1, SHA1_LENGTH));

    /* restore normalcy after changing offset position */
    fseek (mf, r3, SEEK_SET);
    }

  PORT_Free (buf);
  PORT_Free (name);

  fclose (sf);
  fclose (mf);

  return 0;
}

/*
 *  c a l c u l a t e _ M D 5 _ r a n g e
 *
 *  Calculate the MD5 digest on a range of bytes in
 *  the specified fopen'd file. Returns base64.
 *
 */
static int
calculate_MD5_range (FILE *fp, long r1, long r2, JAR_Digest *dig)
{
    int num;
    int range;
    unsigned char *buf;

    MD5Context *md5 = 0;
    SHA1Context *sha1 = 0;

    unsigned int sha1_length, md5_length;

    range = r2 - r1;

    /* position to the beginning of range */
    fseek (fp, r1, SEEK_SET);

    buf = (unsigned char *) PORT_ZAlloc (range);
    if (buf == NULL)
      out_of_memory();
 
    if ((num = fread (buf, 1, range, fp)) != range)
      {
      PR_fprintf(errorFD, "%s: expected %d bytes, got %d\n", PROGRAM_NAME,
		range, num);
		errorCount++;
      exit (ERRX);
      }

    md5 = MD5_NewContext();
    sha1 = SHA1_NewContext();

    if (md5 == NULL || sha1 == NULL) 
      {
      PR_fprintf(errorFD, "%s: can't generate digest context\n", PROGRAM_NAME);
		errorCount++;
      exit (ERRX);
      }

    MD5_Begin (md5);
    SHA1_Begin (sha1);

    MD5_Update (md5, buf, range);
    SHA1_Update (sha1, buf, range);

    MD5_End (md5, dig->md5, &md5_length, MD5_LENGTH);
    SHA1_End (sha1, dig->sha1, &sha1_length, SHA1_LENGTH);

    MD5_DestroyContext (md5, PR_TRUE);
    SHA1_DestroyContext (sha1, PR_TRUE);

    PORT_Free (buf);

    return 0;
}

static void SignOut (void *arg, const char *buf, unsigned long len)
{
  fwrite (buf, len, 1, (FILE *) arg);
}
