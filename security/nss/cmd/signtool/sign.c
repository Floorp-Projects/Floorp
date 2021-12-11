/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "signtool.h"
#include "zip.h"
#include "prmem.h"
#include "blapi.h"
#include "sechash.h" /* for HASH_GetHashObject() */

static int create_pk7(char *dir, char *keyName, int *keyType);
static int jar_find_key_type(CERTCertificate *cert);
static int manifesto(char *dirname, char *install_script, PRBool recurse);
static int manifesto_fn(char *relpath, char *basedir, char *reldir,
                        char *filename, void *arg);
static int manifesto_xpi_fn(char *relpath, char *basedir, char *reldir,
                            char *filename, void *arg);
static int sign_all_arc_fn(char *relpath, char *basedir, char *reldir,
                           char *filename, void *arg);
static int add_meta(FILE *fp, char *name);
static int SignFile(FILE *outFile, FILE *inFile, CERTCertificate *cert);
static int generate_SF_file(char *manifile, char *who);
static int calculate_MD5_range(FILE *fp, long r1, long r2,
                               JAR_Digest *dig);
static void SignOut(void *arg, const char *buf, unsigned long len);

static char *metafile = NULL;
static int optimize = 0;
static FILE *mf;
static ZIPfile *zipfile = NULL;

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
    char tempfn[FNSIZE], fullfn[FNSIZE];
    int keyType = rsaKey;
    int count;

    metafile = meta_file;
    optimize = _optimize;

    /* To create XPI compatible Archive manifesto() must be run before
     * the zipfile is opened. This is so the signed files are not added
     * the archive before the crucial rsa/dsa file*/
    if (xpi_arc) {
        manifesto(tree, install_script, recurse);
    }

    if (zip_file) {
        zipfile = JzipOpen(zip_file, NULL /*no comment*/);
    }

    /*Sign and add files to the archive normally with manifesto()*/
    if (!xpi_arc) {
        manifesto(tree, install_script, recurse);
    }

    if (keyName) {
        status = create_pk7(tree, keyName, &keyType);
        if (status < 0) {
            PR_fprintf(errorFD, "the tree \"%s\" was NOT SUCCESSFULLY SIGNED\n",
                       tree);
            errorCount++;
            exit(ERRX);
        }
    }

    /* Add the rsa/dsa file as the first file in the archive. This is crucial
     * for a XPInstall compatible archive */
    if (xpi_arc) {
        if (verbosity >= 0) {
            PR_fprintf(outputFD, "%s \n", XPI_TEXT);
        }

        /* rsa/dsa to zip */
        count = snprintf(tempfn, sizeof(tempfn), "META-INF/%s.%s", base, (keyType == dsaKey ? "dsa" : "rsa"));
        if (count >= sizeof(tempfn)) {
            PR_fprintf(errorFD, "unable to write key metadata\n");
            errorCount++;
            exit(ERRX);
        }
        count = snprintf(fullfn, sizeof(fullfn), "%s/%s", tree, tempfn);
        if (count >= sizeof(fullfn)) {
            PR_fprintf(errorFD, "unable to write key metadata\n");
            errorCount++;
            exit(ERRX);
        }
        JzipAdd(fullfn, tempfn, zipfile, compression_level);

        /* Loop through all files & subdirectories, add to archive */
        foreach (tree, "", manifesto_xpi_fn, recurse, PR_FALSE /*include dirs */,
                 (void *)NULL)
            ;
    }
    /* mf to zip */
    strcpy(tempfn, "META-INF/manifest.mf");
    count = snprintf(fullfn, sizeof(fullfn), "%s/%s", tree, tempfn);
    if (count >= sizeof(fullfn)) {
        PR_fprintf(errorFD, "unable to write manifest\n");
        errorCount++;
        exit(ERRX);
    }
    JzipAdd(fullfn, tempfn, zipfile, compression_level);

    /* sf to zip */
    count = snprintf(tempfn, sizeof(tempfn), "META-INF/%s.sf", base);
    if (count >= sizeof(tempfn)) {
        PR_fprintf(errorFD, "unable to write sf metadata\n");
        errorCount++;
        exit(ERRX);
    }
    count = snprintf(fullfn, sizeof(fullfn), "%s/%s", tree, tempfn);
    if (count >= sizeof(fullfn)) {
        PR_fprintf(errorFD, "unable to write sf metadata\n");
        errorCount++;
        exit(ERRX);
    }
    JzipAdd(fullfn, tempfn, zipfile, compression_level);

    /* Add the rsa/dsa file to the zip archive normally */
    if (!xpi_arc) {
        /* rsa/dsa to zip */
        count = snprintf(tempfn, sizeof(tempfn), "META-INF/%s.%s", base, (keyType == dsaKey ? "dsa" : "rsa"));
        if (count >= sizeof(tempfn)) {
            PR_fprintf(errorFD, "unable to write key metadata\n");
            errorCount++;
            exit(ERRX);
        }
        count = snprintf(fullfn, sizeof(fullfn), "%s/%s", tree, tempfn);
        if (count >= sizeof(fullfn)) {
            PR_fprintf(errorFD, "unable to write key metadata\n");
            errorCount++;
            exit(ERRX);
        }
        JzipAdd(fullfn, tempfn, zipfile, compression_level);
    }

    JzipClose(zipfile);

    if (verbosity >= 0) {
        if (javascript) {
            PR_fprintf(outputFD, "jarfile \"%s\" signed successfully\n",
                       zip_file);
        } else {
            PR_fprintf(outputFD, "tree \"%s\" signed successfully\n",
                       tree);
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
SignAllArc(char *jartree, char *keyName, int javascript, char *metafilename,
           char *install_script, int optimize_level, PRBool recurse)
{
    SignArcInfo info;

    info.keyName = keyName;
    info.javascript = javascript;
    info.metafile = metafilename;
    info.install_script = install_script;
    info.optimize = optimize_level;

    return foreach (jartree, "", sign_all_arc_fn, recurse,
                    PR_TRUE /*include dirs*/, (void *)&info);
}

static int
sign_all_arc_fn(char *relpath, char *basedir, char *reldir, char *filename,
                void *arg)
{
    char *zipfilename = NULL;
    char *arc = NULL, *archive = NULL;
    int retval = 0;
    SignArcInfo *infop = (SignArcInfo *)arg;

    /* Make sure there is one and only one ".arc" in the relative path,
     * and that it is at the end of the path (don't sign .arcs within .arcs) */
    if ((PL_strcaserstr(relpath, ".arc") == relpath + strlen(relpath) - 4) &&
        (PL_strcasestr(relpath, ".arc") == relpath + strlen(relpath) - 4)) {

        if (!infop) {
            PR_fprintf(errorFD, "%s: Internal failure\n", PROGRAM_NAME);
            errorCount++;
            retval = -1;
            goto finish;
        }
        archive = PR_smprintf("%s/%s", basedir, relpath);

        zipfilename = PL_strdup(archive);
        arc = PORT_Strrchr(zipfilename, '.');

        if (arc == NULL) {
            PR_fprintf(errorFD, "%s: Internal failure\n", PROGRAM_NAME);
            errorCount++;
            retval = -1;
            goto finish;
        }

        PL_strcpy(arc, ".jar");

        if (verbosity >= 0) {
            PR_fprintf(outputFD, "\nsigning: %s\n", zipfilename);
        }
        retval = SignArchive(archive, infop->keyName, zipfilename,
                             infop->javascript, infop->metafile, infop->install_script,
                             infop->optimize, PR_TRUE /* recurse */);
    }
finish:
    if (archive)
        PR_Free(archive);
    if (zipfilename)
        PR_Free(zipfilename);

    return retval;
}

/*********************************************************************
 *
 * c r e a t e _ p k 7
 */
static int
create_pk7(char *dir, char *keyName, int *keyType)
{
    int status = 0;
    char *file_ext;

    CERTCertificate *cert;
    CERTCertDBHandle *db;

    FILE *in, *out;

    char sf_file[FNSIZE];
    char pk7_file[FNSIZE];

    /* open cert database */
    db = CERT_GetDefaultCertDB();

    if (db == NULL)
        return -1;

    /* find cert */
    /*cert = CERT_FindCertByNicknameOrEmailAddr(db, keyName);*/
    cert = PK11_FindCertFromNickname(keyName, &pwdata);

    if (cert == NULL) {
        SECU_PrintError(PROGRAM_NAME,
                        "Cannot find the cert \"%s\"", keyName);
        return -1;
    }

    /* determine the key type, which sets the extension for pkcs7 object */

    *keyType = jar_find_key_type(cert);
    file_ext = (*keyType == dsaKey) ? "dsa" : "rsa";

    sprintf(sf_file, "%s/META-INF/%s.sf", dir, base);
    sprintf(pk7_file, "%s/META-INF/%s.%s", dir, base, file_ext);

    if ((in = fopen(sf_file, "rb")) == NULL) {
        PR_fprintf(errorFD, "%s: Can't open %s for reading\n", PROGRAM_NAME,
                   sf_file);
        errorCount++;
        exit(ERRX);
    }

    if ((out = fopen(pk7_file, "wb")) == NULL) {
        PR_fprintf(errorFD, "%s: Can't open %s for writing\n", PROGRAM_NAME,
                   sf_file);
        errorCount++;
        exit(ERRX);
    }

    status = SignFile(out, in, cert);

    CERT_DestroyCertificate(cert);
    fclose(in);
    fclose(out);

    if (status) {
        PR_fprintf(errorFD, "%s: PROBLEM signing data (%s)\n",
                   PROGRAM_NAME, SECU_Strerror(PORT_GetError()));
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
jar_find_key_type(CERTCertificate *cert)
{
    SECKEYPrivateKey *privk = NULL;
    KeyType keyType;

    /* determine its type */
    privk = PK11_FindKeyByAnyCert(cert, &pwdata);
    if (privk == NULL) {
        PR_fprintf(errorFD, "warning - can't find private key for this cert\n");
        warningCount++;
        return 0;
    }

    keyType = privk->keyType;
    SECKEY_DestroyPrivateKey(privk);
    return keyType;
}

/*
 *  m a n i f e s t o
 *
 *  Run once for every subdirectory in which a
 *  manifest is to be created -- usually exactly once.
 *
 */
static int
manifesto(char *dirname, char *install_script, PRBool recurse)
{
    char metadir[FNSIZE], sfname[FNSIZE];

    /* Create the META-INF directory to hold signing info */

    if (PR_Access(dirname, PR_ACCESS_READ_OK)) {
        PR_fprintf(errorFD, "%s: unable to read your directory: %s\n",
                   PROGRAM_NAME, dirname);
        errorCount++;
        perror(dirname);
        exit(ERRX);
    }

    if (PR_Access(dirname, PR_ACCESS_WRITE_OK)) {
        PR_fprintf(errorFD, "%s: unable to write to your directory: %s\n",
                   PROGRAM_NAME, dirname);
        errorCount++;
        perror(dirname);
        exit(ERRX);
    }

    sprintf(metadir, "%s/META-INF", dirname);

    strcpy(sfname, metadir);

    PR_MkDir(metadir, 0777);

    strcat(metadir, "/");
    strcat(metadir, MANIFEST);

    if ((mf = fopen(metadir, "wb")) == NULL) {
        perror(MANIFEST);
        PR_fprintf(errorFD, "%s: Probably, the directory you are trying to"
                            " sign has\n",
                   PROGRAM_NAME);
        PR_fprintf(errorFD, "%s: permissions problems or may not exist.\n",
                   PROGRAM_NAME);
        errorCount++;
        exit(ERRX);
    }

    if (verbosity >= 0) {
        PR_fprintf(outputFD, "Generating %s file..\n", metadir);
    }

    fprintf(mf, "Manifest-Version: 1.0\n");
    fprintf(mf, "Created-By: %s\n", CREATOR);
    fprintf(mf, "Comments: %s\n", BREAKAGE);

    if (scriptdir) {
        fprintf(mf, "Comments: --\n");
        fprintf(mf, "Comments: --\n");
        fprintf(mf, "Comments: -- This archive signs Javascripts which may not necessarily\n");
        fprintf(mf, "Comments: -- be included in the physical jar file.\n");
        fprintf(mf, "Comments: --\n");
        fprintf(mf, "Comments: --\n");
    }

    if (install_script)
        fprintf(mf, "Install-Script: %s\n", install_script);

    if (metafile)
        add_meta(mf, "+");

    /* Loop through all files & subdirectories */
    foreach (dirname, "", manifesto_fn, recurse, PR_FALSE /*include dirs */,
             (void *)NULL)
        ;

    fclose(mf);

    strcat(sfname, "/");
    strcat(sfname, base);
    strcat(sfname, ".sf");

    if (verbosity >= 0) {
        PR_fprintf(outputFD, "Generating %s.sf file..\n", base);
    }
    generate_SF_file(metadir, sfname);

    return 0;
}

/*
 *  m a n i f e s t o _ x p i _ f n
 *
 *  Called by pointer from SignArchive(), once for
 *  each file within the directory. This function
 *  is only used for adding to XPI compatible archive
 *
 */
static int
manifesto_xpi_fn(char *relpath, char *basedir, char *reldir, char *filename, void *arg)
{
    char fullname[FNSIZE];
    int count;

    if (verbosity >= 0) {
        PR_fprintf(outputFD, "--> %s\n", relpath);
    }

    /* extension matching */
    if (extensionsGiven) {
        char *ext = PL_strrchr(relpath, '.');
        if (!ext)
            return 0;
        if (!PL_HashTableLookup(extensions, ext))
            return 0;
    }
    count = snprintf(fullname, sizeof(fullname), "%s/%s", basedir, relpath);
    if (count >= sizeof(fullname)) {
        return 1;
    }
    JzipAdd(fullname, relpath, zipfile, compression_level);

    return 0;
}

/*
 *  m a n i f e s t o _ f n
 *
 *  Called by pointer from manifesto(), once for
 *  each file within the directory.
 *
 */
static int
manifesto_fn(char *relpath, char *basedir, char *reldir, char *filename, void *arg)
{
    int use_js;
    char *md5, *sha1;

    JAR_Digest dig;
    char fullname[FNSIZE];

    if (verbosity >= 0) {
        PR_fprintf(outputFD, "--> %s\n", relpath);
    }

    /* extension matching */
    if (extensionsGiven) {
        char *ext = PL_strrchr(relpath, '.');
        if (!ext)
            return 0;
        if (!PL_HashTableLookup(extensions, ext))
            return 0;
    }

    sprintf(fullname, "%s/%s", basedir, relpath);

    fprintf(mf, "\n");

    use_js = 0;

    if (scriptdir && !PORT_Strcmp(scriptdir, reldir))
        use_js++;

    /* sign non-.js files inside .arc directories using the javascript magic */

    if ((PL_strcaserstr(filename, ".js") != filename + strlen(filename) - 3) &&
        (PL_strcaserstr(reldir, ".arc") == reldir + strlen(filename) - 4))
        use_js++;

    if (use_js) {
        fprintf(mf, "Name: %s\n", filename);
        fprintf(mf, "Magic: javascript\n");

        if (optimize == 0)
            fprintf(mf, "javascript.id: %s\n", filename);

        if (metafile)
            add_meta(mf, filename);
    } else {
        fprintf(mf, "Name: %s\n", relpath);
        if (metafile)
            add_meta(mf, relpath);
    }

    JAR_digest_file(fullname, &dig);

    if (optimize == 0) {
        fprintf(mf, "Digest-Algorithms: MD5 SHA1\n");

        md5 = BTOA_DataToAscii(dig.md5, MD5_LENGTH);
        fprintf(mf, "MD5-Digest: %s\n", md5);
        PORT_Free(md5);
    }

    sha1 = BTOA_DataToAscii(dig.sha1, SHA1_LENGTH);
    fprintf(mf, "SHA1-Digest: %s\n", sha1);
    PORT_Free(sha1);

    if (!use_js) {
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
static int
add_meta(FILE *fp, char *name)
{
    FILE *met;
    char buf[BUFSIZ];

    int place;
    char *pattern, *meta;

    int num = 0;

    if ((met = fopen(metafile, "r")) != NULL) {
        while (fgets(buf, BUFSIZ, met)) {
            char *s;

            for (s = buf; *s && *s != '\n' && *s != '\r'; s++)
                ;
            *s = 0;

            if (*buf == 0)
                continue;

            pattern = buf;

            /* skip to whitespace */
            for (s = buf; *s && *s != ' ' && *s != '\t'; s++)
                ;

            /* terminate pattern */
            if (*s == ' ' || *s == '\t')
                *s++ = 0;

            /* eat through whitespace */
            while (*s == ' ' || *s == '\t')
                s++;

            meta = s;

            /* this will eventually be regexp matching */

            place = 0;
            if (!PORT_Strcmp(pattern, name))
                place = 1;

            if (place) {
                num++;
                if (verbosity >= 0) {
                    PR_fprintf(outputFD, "[%s] %s\n", name, meta);
                }
                fprintf(fp, "%s\n", meta);
            }
        }
        fclose(met);
    } else {
        PR_fprintf(errorFD, "%s: can't open metafile: %s\n", PROGRAM_NAME,
                   metafile);
        errorCount++;
        exit(ERRX);
    }

    return num;
}

/**********************************************************************
 *
 * S i g n F i l e
 */
static int
SignFile(FILE *outFile, FILE *inFile, CERTCertificate *cert)
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

    hashcx = (*hashObj->create)();
    if (hashcx == NULL)
        return -1;

    (*hashObj->begin)(hashcx);

    for (;;) {
        if (feof(inFile))
            break;
        nb = fread(ibuf, 1, sizeof(ibuf), inFile);
        if (nb == 0) {
            if (ferror(inFile)) {
                PORT_SetError(SEC_ERROR_IO);
                (*hashObj->destroy)(hashcx, PR_TRUE);
                return -1;
            }
            /* eof */
            break;
        }
        (*hashObj->update)(hashcx, (unsigned char *)ibuf, nb);
    }

    (*hashObj->end)(hashcx, (unsigned char *)digestdata, &len, 32);
    (*hashObj->destroy)(hashcx, PR_TRUE);

    digest.data = (unsigned char *)digestdata;
    digest.len = len;

    cinfo = SEC_PKCS7CreateSignedData(cert, certUsageObjectSigner, NULL,
                                      SEC_OID_SHA1, &digest, NULL, NULL);

    if (cinfo == NULL)
        return -1;

    rv = SEC_PKCS7IncludeCertChain(cinfo, NULL);
    if (rv != SECSuccess) {
        SEC_PKCS7DestroyContentInfo(cinfo);
        return -1;
    }

    if (no_time == 0) {
        rv = SEC_PKCS7AddSigningTime(cinfo);
        if (rv != SECSuccess) {
            /* don't check error */
        }
    }

    rv = SEC_PKCS7Encode(cinfo, SignOut, outFile, NULL, NULL, &pwdata);

    SEC_PKCS7DestroyContentInfo(cinfo);

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
static int
generate_SF_file(char *manifile, char *who)
{
    FILE *sfFile;
    FILE *mfFile;
    long r1, r2, r3;
    char whofile[FNSIZE];
    char *buf, *name = NULL;
    char *md5, *sha1;
    JAR_Digest dig;
    int line = 0;

    strcpy(whofile, who);

    if ((mfFile = fopen(manifile, "rb")) == NULL) {
        perror(manifile);
        exit(ERRX);
    }

    if ((sfFile = fopen(whofile, "wb")) == NULL) {
        perror(who);
        exit(ERRX);
    }

    buf = (char *)PORT_ZAlloc(BUFSIZ);

    if (buf)
        name = (char *)PORT_ZAlloc(BUFSIZ);

    if (buf == NULL || name == NULL)
        out_of_memory();

    fprintf(sfFile, "Signature-Version: 1.0\n");
    fprintf(sfFile, "Created-By: %s\n", CREATOR);
    fprintf(sfFile, "Comments: %s\n", BREAKAGE);

    if (fgets(buf, BUFSIZ, mfFile) == NULL) {
        PR_fprintf(errorFD, "%s: empty manifest file!\n", PROGRAM_NAME);
        errorCount++;
        exit(ERRX);
    }

    if (strncmp(buf, "Manifest-Version:", 17)) {
        PR_fprintf(errorFD, "%s: not a manifest file!\n", PROGRAM_NAME);
        errorCount++;
        exit(ERRX);
    }

    fseek(mfFile, 0L, SEEK_SET);

    /* Process blocks of headers, and calculate their hashen */

    while (1) {
        /* Beginning range */
        r1 = ftell(mfFile);

        if (fgets(name, BUFSIZ, mfFile) == NULL)
            break;

        line++;

        if (r1 != 0 && strncmp(name, "Name:", 5)) {
            PR_fprintf(errorFD,
                       "warning: unexpected input in manifest file \"%s\" at line %d:\n",
                       manifile, line);
            PR_fprintf(errorFD, "%s\n", name);
            warningCount++;
        }

        r2 = r1;
        while (fgets(buf, BUFSIZ, mfFile)) {
            if (*buf == 0 || *buf == '\n' || *buf == '\r')
                break;

            line++;

            /* Ending range for hashing */
            r2 = ftell(mfFile);
        }

        r3 = ftell(mfFile);

        if (r1) {
            fprintf(sfFile, "\n");
            fprintf(sfFile, "%s", name);
        }

        calculate_MD5_range(mfFile, r1, r2, &dig);

        if (optimize == 0) {
            fprintf(sfFile, "Digest-Algorithms: MD5 SHA1\n");

            md5 = BTOA_DataToAscii(dig.md5, MD5_LENGTH);
            fprintf(sfFile, "MD5-Digest: %s\n", md5);
            PORT_Free(md5);
        }

        sha1 = BTOA_DataToAscii(dig.sha1, SHA1_LENGTH);
        fprintf(sfFile, "SHA1-Digest: %s\n", sha1);
        PORT_Free(sha1);

        /* restore normalcy after changing offset position */
        fseek(mfFile, r3, SEEK_SET);
    }

    PORT_Free(buf);
    PORT_Free(name);

    fclose(sfFile);
    fclose(mfFile);

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
calculate_MD5_range(FILE *fp, long r1, long r2, JAR_Digest *dig)
{
    int num;
    int range;
    unsigned char *buf;
    SECStatus rv;

    range = r2 - r1;

    /* position to the beginning of range */
    fseek(fp, r1, SEEK_SET);

    buf = (unsigned char *)PORT_ZAlloc(range);
    if (buf == NULL)
        out_of_memory();

    if ((num = fread(buf, 1, range, fp)) != range) {
        PR_fprintf(errorFD, "%s: expected %d bytes, got %d\n", PROGRAM_NAME,
                   range, num);
        errorCount++;
        exit(ERRX);
    }

    rv = PK11_HashBuf(SEC_OID_MD5, dig->md5, buf, range);
    if (rv == SECSuccess) {
        rv = PK11_HashBuf(SEC_OID_SHA1, dig->sha1, buf, range);
    }
    if (rv != SECSuccess) {
        PR_fprintf(errorFD, "%s: can't generate digest context\n",
                   PROGRAM_NAME);
        errorCount++;
        exit(ERRX);
    }

    PORT_Free(buf);

    return 0;
}

static void
SignOut(void *arg, const char *buf, unsigned long len)
{
    fwrite(buf, len, 1, (FILE *)arg);
}
