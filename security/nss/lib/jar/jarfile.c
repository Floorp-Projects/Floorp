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
 *  JARFILE
 *
 *  Parsing of a Jar file
 */

#define JAR_SIZE 256

#include "jar.h"

#include "jarint.h"
#include "jarfile.h"

/* commercial compression */
#include "jzlib.h"

#ifdef XP_UNIX
#include "sys/stat.h"
#endif

#include "sechash.h"	/* for HASH_GetHashObject() */

/* extracting */

static int jar_guess_jar (char *filename, JAR_FILE fp);

static int jar_inflate_memory 
   (unsigned int method, long *length, long expected_out_len, char ZHUGEP **data);

static int jar_physical_extraction 
   (JAR_FILE fp, char *outpath, long offset, long length);

static int jar_physical_inflate
   (JAR_FILE fp, char *outpath, long offset, 
         long length, unsigned int method);

static int jar_verify_extract 
   (JAR *jar, char *path, char *physical_path);

static JAR_Physical *jar_get_physical (JAR *jar, char *pathname);

static int jar_extract_manifests (JAR *jar, jarArch format, JAR_FILE fp);

static int jar_extract_mf (JAR *jar, jarArch format, JAR_FILE fp, char *ext);


/* indexing */

static int jar_gen_index (JAR *jar, jarArch format, JAR_FILE fp);

static int jar_listtar (JAR *jar, JAR_FILE fp);

static int jar_listzip (JAR *jar, JAR_FILE fp);


/* conversions */

static int dosdate (char *date, char *s);

static int dostime (char *time, char *s);

static unsigned int xtoint (unsigned char *ii);

static unsigned long xtolong (unsigned char *ll);

static long atoo (char *s);

/*
 *  J A R _ p a s s _ a r c h i v e
 *
 *  For use by naive clients. Slam an entire archive file
 *  into this function. We extract manifests, parse, index
 *  the archive file, and do whatever nastiness.
 *
 */

int JAR_pass_archive
    (JAR *jar, jarArch format, char *filename, const char *url)
  {
  JAR_FILE fp;
  int status = 0;

  if (filename == NULL)
    return JAR_ERR_GENERAL;

  if ((fp = JAR_FOPEN (filename, "rb")) != NULL)
    {
    if (format == jarArchGuess)
      format = (jarArch)jar_guess_jar (filename, fp);

    jar->format = format;
    jar->url = url ? PORT_Strdup (url) : NULL;
    jar->filename = PORT_Strdup (filename);

    status = jar_gen_index (jar, format, fp);

    if (status == 0)
      status = jar_extract_manifests (jar, format, fp);

    JAR_FCLOSE (fp);

    if (status < 0)
      return status;

    /* people were expecting it this way */
    return jar->valid;
    }
  else
    {
    /* file not found */
    return JAR_ERR_FNF;
    }
  }

/*
 *  J A R _ p a s s _ a r c h i v e _ u n v e r i f i e d
 *
 * Same as JAR_pass_archive, but doesn't parse signatures.
 *
 */
int JAR_pass_archive_unverified
        (JAR *jar, jarArch format, char *filename, const char *url)
{
        JAR_FILE fp;
        int status = 0;

        if (filename == NULL) {
                return JAR_ERR_GENERAL;
        }

        if ((fp = JAR_FOPEN (filename, "rb")) != NULL) {
                if (format == jarArchGuess) {
                        format = (jarArch)jar_guess_jar (filename, fp);
                }

                jar->format = format;
                jar->url = url ? PORT_Strdup (url) : NULL;
                jar->filename = PORT_Strdup (filename);

                status = jar_gen_index (jar, format, fp);

                if (status == 0) {
                        status = jar_extract_mf(jar, format, fp, "mf");
                }

                JAR_FCLOSE (fp);

                if (status < 0) {
                        return status;
                }

                /* people were expecting it this way */
                return jar->valid;
        } else {
                /* file not found */
                return JAR_ERR_FNF;
        }
}

/*
 *  J A R _ v e r i f i e d _ e x t r a c t
 *
 *  Optimization: keep a file descriptor open
 *  inside the JAR structure, so we don't have to
 *  open the file 25 times to run java. 
 *
 */

int JAR_verified_extract
    (JAR *jar, char *path, char *outpath)
  {
  int status;

  status = JAR_extract (jar, path, outpath);

  if (status >= 0)
    return jar_verify_extract (jar, path, outpath);
  else
    return status;
  }

int JAR_extract
    (JAR *jar, char *path, char *outpath)
  {
  int result;

  JAR_FILE fp;
  JAR_Physical *phy;

  if (jar->fp == NULL && jar->filename)
    {
    jar->fp = (FILE*)JAR_FOPEN (jar->filename, "rb");
    }

  if (jar->fp == NULL)
    {
    /* file not found */
    return JAR_ERR_FNF;
    }

  phy = jar_get_physical (jar, path);

  if (phy)
    {
    if (phy->compression != 0 && phy->compression != 8)
      {
      /* unsupported compression method */
      result = JAR_ERR_CORRUPT;
      }

    if (phy->compression == 0)
      {
      result = jar_physical_extraction 
         ((PRFileDesc*)jar->fp, outpath, phy->offset, phy->length);
      }
    else
      {
      result = jar_physical_inflate 
         ((PRFileDesc*)jar->fp, outpath, phy->offset, phy->length, 
            (unsigned int) phy->compression);
      }

#ifdef XP_UNIX
    if (phy->mode)
      chmod (outpath, 0400 | (mode_t) phy->mode);
#endif
    }
  else
    {
    /* pathname not found in archive */
    result = JAR_ERR_PNF;
    }

  return result;
  }

/* 
 *  p h y s i c a l _ e x t r a c t i o n
 *
 *  This needs to be done in chunks of say 32k, instead of
 *  in one bulk calloc. (Necessary under Win16 platform.)
 *  This is done for uncompressed entries only.
 *
 */

#define CHUNK 32768

static int jar_physical_extraction 
     (JAR_FILE fp, char *outpath, long offset, long length)
  {
  JAR_FILE out;

  char *buffer;
  long at, chunk;

  int status = 0;

  buffer = (char *) PORT_ZAlloc (CHUNK);

  if (buffer == NULL)
    return JAR_ERR_MEMORY;

  if ((out = JAR_FOPEN (outpath, "wb")) != NULL)
    {
    at = 0;

    JAR_FSEEK (fp, offset, (PRSeekWhence)0);

    while (at < length)
      {
      chunk = (at + CHUNK <= length) ? CHUNK : length - at;

      if (JAR_FREAD (fp, buffer, chunk) != chunk)
        {
        status = JAR_ERR_DISK;
        break;
        }

      at += chunk;

      if (JAR_FWRITE (out, buffer, chunk) < chunk)
        {
        /* most likely a disk full error */
        status = JAR_ERR_DISK;
        break;
        }
      }
    JAR_FCLOSE (out);
    }
  else
    {
    /* error opening output file */
    status = JAR_ERR_DISK;
    }

  PORT_Free (buffer);
  return status;
  }

/*
 *  j a r _ p h y s i c a l _ i n f l a t e
 *
 *  Inflate a range of bytes in a file, writing the inflated
 *  result to "outpath". Chunk based.
 *
 */

/* input and output chunks differ, assume 4x compression */

#define ICHUNK 8192
#define OCHUNK 32768

static int jar_physical_inflate
     (JAR_FILE fp, char *outpath, long offset, 
          long length, unsigned int method)
  {
  z_stream zs;

  JAR_FILE out;

  long at, chunk;
  char *inbuf, *outbuf;

  int status = 0;

  unsigned long prev_total, ochunk, tin;

  if ((inbuf = (char *) PORT_ZAlloc (ICHUNK)) == NULL)
    return JAR_ERR_MEMORY;

  if ((outbuf = (char *) PORT_ZAlloc (OCHUNK)) == NULL)
    {
    PORT_Free (inbuf);
    return JAR_ERR_MEMORY;
    }

  PORT_Memset (&zs, 0, sizeof (zs));
  status = inflateInit2 (&zs, -MAX_WBITS);

  if (status != Z_OK)
    return JAR_ERR_GENERAL;

  if ((out = JAR_FOPEN (outpath, "wb")) != NULL)
    {
    at = 0;

    JAR_FSEEK (fp, offset, (PRSeekWhence)0);

    while (at < length)
      {
      chunk = (at + ICHUNK <= length) ? ICHUNK : length - at;

      if (JAR_FREAD (fp, inbuf, chunk) != chunk)
        {
        /* incomplete read */
        return JAR_ERR_CORRUPT;
        }

      at += chunk;

      zs.next_in = (Bytef *) inbuf;
      zs.avail_in = chunk;
      zs.avail_out = OCHUNK;

      tin = zs.total_in;

      while ((zs.total_in - tin < chunk) || (zs.avail_out == 0))
        {
        prev_total = zs.total_out;

        zs.next_out = (Bytef *) outbuf;
        zs.avail_out = OCHUNK;

        status = inflate (&zs, Z_NO_FLUSH);

        if (status != Z_OK && status != Z_STREAM_END)
          {
          /* error during decompression */
          return JAR_ERR_CORRUPT;
          }

	ochunk = zs.total_out - prev_total;

        if (JAR_FWRITE (out, outbuf, ochunk) < ochunk)
          {
          /* most likely a disk full error */
          status = JAR_ERR_DISK;
          break;
          }

        if (status == Z_STREAM_END)
          break;
        }
      }

    JAR_FCLOSE (out);
    status = inflateEnd (&zs);
    }
  else
    {
    /* error opening output file */
    status = JAR_ERR_DISK;
    }

  PORT_Free (inbuf);
  PORT_Free (outbuf);

  return status;
  }

/*
 *  j a r _ i n f l a t e _ m e m o r y
 *
 *  Call zlib to inflate the given memory chunk. It is re-XP_ALLOC'd, 
 *  and thus appears to operate inplace to the caller.
 *
 */

static int jar_inflate_memory 
     (unsigned int method, long *length, long expected_out_len, char ZHUGEP **data)
  {
  int status;
  z_stream zs;

  long insz, outsz;

  char *inbuf, *outbuf;

  inbuf =  *data;
  insz = *length;

  outsz = expected_out_len;
  outbuf = (char*)PORT_ZAlloc (outsz);

  if (outbuf == NULL)
    return JAR_ERR_MEMORY;

  PORT_Memset (&zs, 0, sizeof (zs));

  status = inflateInit2 (&zs, -MAX_WBITS);

  if (status < 0)
    {
    /* error initializing zlib stream */
    return JAR_ERR_GENERAL;
    }

  zs.next_in = (Bytef *) inbuf;
  zs.next_out = (Bytef *) outbuf;

  zs.avail_in = insz;
  zs.avail_out = outsz;

  status = inflate (&zs, Z_FINISH);

  if (status != Z_OK && status != Z_STREAM_END)
    {
    /* error during deflation */
    return JAR_ERR_GENERAL; 
    }

  status = inflateEnd (&zs);

  if (status != Z_OK)
    {
    /* error during deflation */
    return JAR_ERR_GENERAL;
    }

  PORT_Free (*data);

  *data = outbuf;
  *length = zs.total_out;

  return 0;
  }

/*
 *  v e r i f y _ e x t r a c t
 *
 *  Validate signature on the freshly extracted file.
 *
 */

static int jar_verify_extract (JAR *jar, char *path, char *physical_path)
  {
  int status;
  JAR_Digest dig;

  PORT_Memset (&dig, 0, sizeof (JAR_Digest));
  status = JAR_digest_file (physical_path, &dig);

  if (!status)
    status = JAR_verify_digest (jar, path, &dig);

  return status;
  }

/*
 *  g e t _ p h y s i c a l
 *
 *  Let's get physical.
 *  Obtains the offset and length of this file in the jar file.
 *
 */

static JAR_Physical *jar_get_physical (JAR *jar, char *pathname)
  {
  JAR_Item *it;

  JAR_Physical *phy;

  ZZLink *link;
  ZZList *list;

  list = jar->phy;

  if (ZZ_ListEmpty (list))
    return NULL;

  for (link = ZZ_ListHead (list);
       !ZZ_ListIterDone (list, link);
       link = link->next)
    {
    it = link->thing;
    if (it->type == jarTypePhy 
          && it->pathname && !PORT_Strcmp (it->pathname, pathname))
      {
      phy = (JAR_Physical *) it->data;
      return phy;
      }
    }

  return NULL;
  }

/*
 *  j a r _ e x t r a c t _ m a n i f e s t s
 *
 *  Extract the manifest files and parse them,
 *  from an open archive file whose contents are known.
 *
 */

static int jar_extract_manifests (JAR *jar, jarArch format, JAR_FILE fp)
  {
  int status;

  if (format != jarArchZip && format != jarArchTar)
    return JAR_ERR_CORRUPT;

  if ((status = jar_extract_mf (jar, format, fp, "mf")) < 0)
    return status;

  if ((status = jar_extract_mf (jar, format, fp, "sf")) < 0)
    return status;

  if ((status = jar_extract_mf (jar, format, fp, "rsa")) < 0)
    return status;

  if ((status = jar_extract_mf (jar, format, fp, "dsa")) < 0)
    return status;

  return 0;
  }

/*
 *  j a r _ e x t r a c t _ m f
 *
 *  Extracts manifest files based on an extension, which 
 *  should be .MF, .SF, .RSA, etc. Order of the files is now no 
 *  longer important when zipping jar files.
 *
 */

static int jar_extract_mf (JAR *jar, jarArch format, JAR_FILE fp, char *ext)
  {
  JAR_Item *it;

  JAR_Physical *phy;

  ZZLink *link;
  ZZList *list;

  char *fn, *e;
  char ZHUGEP *manifest;

  long length;
  int status, ret = 0, num;

  list = jar->phy;

  if (ZZ_ListEmpty (list))
    return JAR_ERR_PNF;

  for (link = ZZ_ListHead (list);
       !ZZ_ListIterDone (list, link);
       link = link->next)
    {
    it = link->thing;
    if (it->type == jarTypePhy 
          && !PORT_Strncmp (it->pathname, "META-INF", 8))
      {
      phy = (JAR_Physical *) it->data;

      if (PORT_Strlen (it->pathname) < 8)
        continue;

      fn = it->pathname + 8;
      if (*fn == '/' || *fn == '\\') fn++;

      if (*fn == 0)
        {
        /* just a directory entry */
        continue;
        }

      /* skip to extension */
      for (e = fn; *e && *e != '.'; e++)
        /* yip */ ;

      /* and skip dot */
      if (*e == '.') e++;

      if (PORT_Strcasecmp (ext, e))
        {
        /* not the right extension */
        continue;
        }

      if (phy->length == 0)
        {
        /* manifest files cannot be zero length! */
        return JAR_ERR_CORRUPT;
        }

      /* Read in the manifest and parse it */
      /* FIX? Does this break on win16 for very very large manifest files? */

#ifdef XP_WIN16
      PORT_Assert( phy->length+1 < 0xFFFF );
#endif

      manifest = (char ZHUGEP *) PORT_ZAlloc (phy->length + 1);
      if (manifest)
        {
        JAR_FSEEK (fp, phy->offset, (PRSeekWhence)0);
        num = JAR_FREAD (fp, manifest, phy->length);

        if (num != phy->length)
          {
          /* corrupt archive file */
          return JAR_ERR_CORRUPT;
          }

        if (phy->compression == 8)
          {
          length = phy->length;

          status = jar_inflate_memory ((unsigned int) phy->compression, &length,  phy->uncompressed_length, &manifest);

          if (status < 0)
            return status;
          }
        else if (phy->compression)
          {
          /* unsupported compression method */
          return JAR_ERR_CORRUPT;
          }
        else
          length = phy->length;

        status = JAR_parse_manifest 
           (jar, manifest, length, it->pathname, "url");

        PORT_Free (manifest);

        if (status < 0 && ret == 0) ret = status;
        }
      else
        return JAR_ERR_MEMORY;
      }
    else if (it->type == jarTypePhy)
      {
      /* ordinary file */
      }
    }

  return ret;
  }

/*
 *  j a r _ g e n _ i n d e x
 *
 *  Generate an index for the various types of
 *  known archive files. Right now .ZIP and .TAR
 *
 */

static int jar_gen_index (JAR *jar, jarArch format, JAR_FILE fp)
  {
  int result = JAR_ERR_CORRUPT;
  JAR_FSEEK (fp, 0, (PRSeekWhence)0);

  switch (format)
    {
    case jarArchZip:
      result = jar_listzip (jar, fp);
      break;

    case jarArchTar:
      result = jar_listtar (jar, fp);
      break;
    }

  JAR_FSEEK (fp, 0, (PRSeekWhence)0);
  return result;
  }


/*
 *  j a r _ l i s t z i p
 *
 *  List the physical contents of a Phil Katz 
 *  style .ZIP file into the JAR linked list.
 *
 */

static int jar_listzip (JAR *jar, JAR_FILE fp)
  {
  int err = 0;

  long pos = 0L;
  char filename [JAR_SIZE];

  char date [9], time [9];
  char sig [4];

  unsigned int compression;
  unsigned int filename_len, extra_len;

  struct ZipLocal *Local;
  struct ZipCentral *Central;
  struct ZipEnd *End;

  /* phy things */

  ZZLink  *ent;
  JAR_Item *it;
  JAR_Physical *phy;

  Local = (struct ZipLocal *) PORT_ZAlloc (30);
  Central = (struct ZipCentral *) PORT_ZAlloc (46);
  End = (struct ZipEnd *) PORT_ZAlloc (22);

  if (!Local || !Central || !End)
    {
    /* out of memory */
    err = JAR_ERR_MEMORY;
    goto loser;
    }

  while (1)
    {
    JAR_FSEEK (fp, pos, (PRSeekWhence)0);

    if (JAR_FREAD (fp, (char *) sig, 4) != 4)
      {
      /* zip file ends prematurely */
      err = JAR_ERR_CORRUPT;
      goto loser;
      }

    JAR_FSEEK (fp, pos, (PRSeekWhence)0);

    if (xtolong ((unsigned char *)sig) == LSIG)
      {
      JAR_FREAD (fp, (char *) Local, 30);

      filename_len = xtoint ((unsigned char *) Local->filename_len);
      extra_len = xtoint ((unsigned char *) Local->extrafield_len);

      if (filename_len >= JAR_SIZE)
        {
        /* corrupt zip file */
        err = JAR_ERR_CORRUPT;
        goto loser;
        }

      if (JAR_FREAD (fp, filename, filename_len) != filename_len)
        {
        /* truncated archive file */
        err = JAR_ERR_CORRUPT;
        goto loser;
        }

      filename [filename_len] = 0;

      /* Add this to our jar chain */

      phy = (JAR_Physical *) PORT_ZAlloc (sizeof (JAR_Physical));

      if (phy == NULL)
        {
        err = JAR_ERR_MEMORY;
        goto loser;
        }

      /* We will index any file that comes our way, but when it comes
         to actually extraction, compression must be 0 or 8 */

      compression = xtoint ((unsigned char *) Local->method);
      phy->compression = compression >= 0 && 
              compression <= 255 ? compression : 222;

      phy->offset = pos + 30 + filename_len + extra_len;
      phy->length = xtolong ((unsigned char *) Local->size);
      phy->uncompressed_length = xtolong((unsigned char *) Local->orglen);

      dosdate (date, Local->date);
      dostime (time, Local->time);

      it = (JAR_Item*)PORT_ZAlloc (sizeof (JAR_Item));
      if (it == NULL)
        {
        err = JAR_ERR_MEMORY;
        goto loser;
        }

      it->pathname = PORT_Strdup (filename);

      it->type = jarTypePhy;

      it->data = (unsigned char *) phy;
      it->size = sizeof (JAR_Physical);

      ent = ZZ_NewLink (it);

      if (ent == NULL)
        {
        err = JAR_ERR_MEMORY;
        goto loser;
        }

      ZZ_AppendLink (jar->phy, ent);
 
      pos = phy->offset + phy->length;
      }
    else if (xtolong ( (unsigned char *)sig) == CSIG)
      {
      if (JAR_FREAD (fp, (char *) Central, 46) != 46)
        {
        /* apparently truncated archive */
        err = JAR_ERR_CORRUPT;
        goto loser;
        }

#ifdef XP_UNIX
      /* with unix we need to locate any bits from 
         the protection mask in the external attributes. */
        {
        unsigned int attr;

        /* determined empirically */
        attr = Central->external_attributes [2];

        if (attr)
          {
          /* we have to read the filename, again */

          filename_len = xtoint ((unsigned char *) Central->filename_len);

          if (filename_len >= JAR_SIZE)
            {
            /* corrupt in central directory */
            err = JAR_ERR_CORRUPT;
            goto loser;
            }

          if (JAR_FREAD (fp, filename, filename_len) != filename_len)
            {
            /* truncated in central directory */
            err = JAR_ERR_CORRUPT;
            goto loser;
            }

          filename [filename_len] = 0;

          /* look up this name again */
          phy = jar_get_physical (jar, filename);

          if (phy)
            {
            /* always allow access by self */
            phy->mode = 0400 | attr;
            }
          }
        }
#endif

      pos += 46 + xtoint ( (unsigned char *)Central->filename_len)
                + xtoint ( (unsigned char *)Central->commentfield_len)
                + xtoint ( (unsigned char *)Central->extrafield_len);
      }
    else if (xtolong ( (unsigned char *)sig) == ESIG)
      {
      if (JAR_FREAD (fp, (char *) End, 22) != 22)
        {
        err = JAR_ERR_CORRUPT;
        goto loser;
        }
      else
        break;
      }
    else
      {
      /* garbage in archive */
      err = JAR_ERR_CORRUPT;
      goto loser;
      }
    }

loser:

  if (Local) PORT_Free (Local);
  if (Central) PORT_Free (Central);
  if (End) PORT_Free (End);

  return err;
  }

/*
 *  j a r _ l i s t t a r
 *
 *  List the physical contents of a Unix 
 *  .tar file into the JAR linked list.
 *
 */

static int jar_listtar (JAR *jar, JAR_FILE fp)
  {
  long pos = 0L;

  long sz, mode;
  time_t when;
  union TarEntry tarball;

  char *s;

  /* phy things */

  ZZLink  *ent;
  JAR_Item *it;
  JAR_Physical *phy;

  while (1)
    {
    JAR_FSEEK (fp, pos, (PRSeekWhence)0);

    if (JAR_FREAD (fp, (char *) &tarball, 512) < 512)
      break;

    if (!*tarball.val.filename)
      break;

    when = atoo (tarball.val.time);
    sz = atoo (tarball.val.size);
    mode = atoo (tarball.val.mode);


    /* Tag the end of filename */

    s = tarball.val.filename;
    while (*s && *s != ' ') s++;
    *s = 0;


    /* Add to our linked list */

    phy = (JAR_Physical *) PORT_ZAlloc (sizeof (JAR_Physical));

    if (phy == NULL)
      return JAR_ERR_MEMORY;

    phy->compression = 0;
    phy->offset = pos + 512;
    phy->length = sz;

    ADDITEM (jar->phy, jarTypePhy, 
       tarball.val.filename, phy, sizeof (JAR_Physical));


    /* Advance to next file entry */

    sz += 511;
    sz = (sz / 512) * 512;

    pos += sz + 512;
    }

  return 0;
  }

/*
 *  d o s d a t e
 *
 *  Not used right now, but keep it in here because
 *  it will be needed. 
 *
 */

static int dosdate (char *date, char *s)
  {
  int num = xtoint ( (unsigned char *)s);

  PR_snprintf (date, 9, "%02d-%02d-%02d",
     ((num >> 5) & 0x0F), (num & 0x1F), ((num >> 9) + 80));

  return 0;
  }

/*
 *  d o s t i m e
 *
 *  Not used right now, but keep it in here because
 *  it will be needed. 
 *
 */

static int dostime (char *time, char *s)
  {
  int num = xtoint ( (unsigned char *)s);

  PR_snprintf (time, 6, "%02d:%02d",
     ((num >> 11) & 0x1F), ((num >> 5) & 0x3F));

  return 0;
  }

/*
 *  x t o i n t
 *
 *  Converts a two byte ugly endianed integer
 *  to our platform's integer.
 *
 */

static unsigned int xtoint (unsigned char *ii)
  {
  return (int) (ii [0]) | ((int) ii [1] << 8);
  }

/*
 *  x t o l o n g
 *
 *  Converts a four byte ugly endianed integer
 *  to our platform's integer.
 *
 */

static unsigned long xtolong (unsigned char *ll)
  {
  unsigned long ret;

  ret =  (
         (((unsigned long) ll [0]) <<  0) |
         (((unsigned long) ll [1]) <<  8) |
         (((unsigned long) ll [2]) << 16) |
         (((unsigned long) ll [3]) << 24)
         );

  return ret;
  }

/*
 *  a t o o
 *
 *  Ascii octal to decimal.
 *  Used for integer encoding inside tar files.
 *
 */

static long atoo (char *s)
  {
  long num = 0L;

  while (*s == ' ') s++;

  while (*s >= '0' && *s <= '7')
    {
    num <<= 3;
    num += *s++ - '0';
    }

  return num;
  }

/*
 *  g u e s s _ j a r
 *
 *  Try to guess what kind of JAR file this is.
 *  Maybe tar, maybe zip. Look in the file for magic
 *  or at its filename.
 *
 */

static int jar_guess_jar (char *filename, JAR_FILE fp)
  {
  char *ext;

  ext = filename + PORT_Strlen (filename) - 4;

  if (!PORT_Strcmp (ext, ".tar"))
    return jarArchTar;

  return jarArchZip;
  }
