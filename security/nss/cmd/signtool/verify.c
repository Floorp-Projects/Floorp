/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "signtool.h"

static int jar_cb(int status, JAR *jar, const char *metafile,
                  char *pathname, char *errortext);
static int verify_global(JAR *jar);

/*************************************************************************
 *
 * V e r i f y J a r
 */
int
VerifyJar(char *filename)
{
    FILE *fp;

    int ret;
    int status;
    int failed = 0;
    char *err;

    JAR *jar;
    JAR_Context *ctx;

    JAR_Item *it;

    jar = JAR_new();

    if ((fp = fopen(filename, "r")) == NULL) {
        perror(filename);
        exit(ERRX);
    } else
        fclose(fp);

    JAR_set_callback(JAR_CB_SIGNAL, jar, jar_cb);

    status = JAR_pass_archive(jar, jarArchGuess, filename, "some-url");

    if (status < 0 || jar->valid < 0) {
        failed = 1;
        PR_fprintf(outputFD,
                   "\nNOTE -- \"%s\" archive DID NOT PASS crypto verification.\n",
                   filename);
        if (status < 0) {
            const char *errtext;

            if (status >= JAR_BASE && status <= JAR_BASE_END) {
                errtext = JAR_get_error(status);
            } else {
                errtext = SECU_Strerror(PORT_GetError());
            }

            PR_fprintf(outputFD, "  (reported reason: %s)\n\n",
                       errtext);

            /* corrupt files should not have their contents listed */

            if (status == JAR_ERR_CORRUPT)
                return -1;
        }
        PR_fprintf(outputFD,
                   "entries shown below will have their digests checked only.\n");
        jar->valid = 0;
    } else
        PR_fprintf(outputFD,
                   "archive \"%s\" has passed crypto verification.\n", filename);

    if (verify_global(jar))
        failed = 1;

    PR_fprintf(outputFD, "\n");
    PR_fprintf(outputFD, "%16s   %s\n", "status", "path");
    PR_fprintf(outputFD, "%16s   %s\n", "------------", "-------------------");

    ctx = JAR_find(jar, NULL, jarTypeMF);

    while (JAR_find_next(ctx, &it) >= 0) {
        if (it && it->pathname) {
            rm_dash_r(TMP_OUTPUT);
            ret = JAR_verified_extract(jar, it->pathname, TMP_OUTPUT);
            /* if (ret < 0) printf ("error %d on %s\n", ret, it->pathname); */
            if (ret < 0)
                failed = 1;

            if (ret == JAR_ERR_PNF)
                err = "NOT PRESENT";
            else if (ret == JAR_ERR_HASH)
                err = "HASH FAILED";
            else
                err = "NOT VERIFIED";

            PR_fprintf(outputFD, "%16s   %s\n",
                       ret >= 0 ? "verified" : err, it->pathname);

            if (ret != 0 && ret != JAR_ERR_PNF && ret != JAR_ERR_HASH)
                PR_fprintf(outputFD, "      (reason: %s)\n",
                           JAR_get_error(ret));
        }
    }

    JAR_find_end(ctx);

    if (status < 0 || jar->valid < 0) {
        failed = 1;
        PR_fprintf(outputFD,
                   "\nNOTE -- \"%s\" archive DID NOT PASS crypto verification.\n",
                   filename);
        give_help(status);
    }

    JAR_destroy(jar);

    if (failed)
        return -1;
    return 0;
}

/***************************************************************************
 *
 * v e r i f y _ g l o b a l
 */
static int
verify_global(JAR *jar)
{
    FILE *fp;
    JAR_Context *ctx;
    JAR_Item *it;
    JAR_Digest *globaldig;
    char *ext;
    unsigned char *md5_digest, *sha1_digest;
    unsigned int sha1_length, md5_length;
    int retval = 0;
    char buf[BUFSIZ];

    ctx = JAR_find(jar, "*", jarTypePhy);

    while (JAR_find_next(ctx, &it) >= 0) {
        if (!PORT_Strncmp(it->pathname, "META-INF", 8)) {
            for (ext = it->pathname; *ext; ext++)
                ;
            while (ext > it->pathname && *ext != '.')
                ext--;

            if (verbosity >= 0) {
                if (!PORT_Strcasecmp(ext, ".rsa")) {
                    PR_fprintf(outputFD, "found a RSA signature file: %s\n",
                               it->pathname);
                }

                if (!PORT_Strcasecmp(ext, ".dsa")) {
                    PR_fprintf(outputFD, "found a DSA signature file: %s\n",
                               it->pathname);
                }

                if (!PORT_Strcasecmp(ext, ".mf")) {
                    PR_fprintf(outputFD,
                               "found a MF master manifest file: %s\n",
                               it->pathname);
                }
            }

            if (!PORT_Strcasecmp(ext, ".sf")) {
                if (verbosity >= 0) {
                    PR_fprintf(outputFD,
                               "found a SF signature manifest file: %s\n",
                               it->pathname);
                }

                rm_dash_r(TMP_OUTPUT);
                if (JAR_extract(jar, it->pathname, TMP_OUTPUT) < 0) {
                    PR_fprintf(errorFD, "%s: error extracting %s\n",
                               PROGRAM_NAME, it->pathname);
                    errorCount++;
                    retval = -1;
                    continue;
                }

                md5_digest = NULL;
                sha1_digest = NULL;

                if ((fp = fopen(TMP_OUTPUT, "rb")) != NULL) {
                    while (fgets(buf, BUFSIZ, fp)) {
                        char *s;

                        if (*buf == 0 || *buf == '\n' || *buf == '\r')
                            break;

                        for (s = buf; *s && *s != '\n' && *s != '\r'; s++)
                            ;
                        *s = 0;

                        if (!PORT_Strncmp(buf, "MD5-Digest: ", 12)) {
                            md5_digest =
                                ATOB_AsciiToData(buf + 12, &md5_length);
                        }
                        if (!PORT_Strncmp(buf, "SHA1-Digest: ", 13)) {
                            sha1_digest =
                                ATOB_AsciiToData(buf + 13, &sha1_length);
                        }
                        if (!PORT_Strncmp(buf, "SHA-Digest: ", 12)) {
                            sha1_digest =
                                ATOB_AsciiToData(buf + 12, &sha1_length);
                        }
                    }

                    globaldig = jar->globalmeta;

                    if (globaldig && md5_digest && verbosity >= 0) {
                        PR_fprintf(outputFD,
                                   "  md5 digest on global metainfo: %s\n",
                                   PORT_Memcmp(md5_digest, globaldig->md5, MD5_LENGTH)
                                       ? "no match"
                                       : "match");
                    }

                    if (globaldig && sha1_digest && verbosity >= 0) {
                        PR_fprintf(outputFD,
                                   "  sha digest on global metainfo: %s\n",
                                   PORT_Memcmp(sha1_digest, globaldig->sha1, SHA1_LENGTH)
                                       ? "no match"
                                       : "match");
                    }

                    if (globaldig == NULL && verbosity >= 0) {
                        PR_fprintf(outputFD,
                                   "global metadigest is not available, strange.\n");
                    }

                    PORT_Free(md5_digest);
                    PORT_Free(sha1_digest);
                    fclose(fp);
                }
            }
        }
    }

    JAR_find_end(ctx);

    return retval;
}

/************************************************************************
 *
 * J a r W h o
 */
int
JarWho(char *filename)
{
    FILE *fp;

    JAR *jar;
    JAR_Context *ctx;

    int status;
    int retval = 0;

    JAR_Item *it;
    JAR_Cert *fing;

    CERTCertificate *cert, *prev = NULL;

    jar = JAR_new();

    if ((fp = fopen(filename, "r")) == NULL) {
        perror(filename);
        exit(ERRX);
    }
    fclose(fp);

    status = JAR_pass_archive(jar, jarArchGuess, filename, "some-url");

    if (status < 0 || jar->valid < 0) {
        PR_fprintf(outputFD,
                   "NOTE -- \"%s\" archive DID NOT PASS crypto verification.\n",
                   filename);
        retval = -1;
        if (jar->valid < 0 || status != -1) {
            const char *errtext;

            if (status >= JAR_BASE && status <= JAR_BASE_END) {
                errtext = JAR_get_error(status);
            } else {
                errtext = SECU_Strerror(PORT_GetError());
            }

            PR_fprintf(outputFD, "  (reported reason: %s)\n\n", errtext);
        }
    }

    PR_fprintf(outputFD, "\nSigner information:\n\n");

    ctx = JAR_find(jar, NULL, jarTypeSign);

    while (JAR_find_next(ctx, &it) >= 0) {
        fing = (JAR_Cert *)it->data;
        cert = fing->cert;

        if (cert) {
            if (prev == cert)
                break;

            if (cert->nickname)
                PR_fprintf(outputFD, "nickname: %s\n", cert->nickname);
            if (cert->subjectName)
                PR_fprintf(outputFD, "subject name: %s\n",
                           cert->subjectName);
            if (cert->issuerName)
                PR_fprintf(outputFD, "issuer name: %s\n", cert->issuerName);
        } else {
            PR_fprintf(outputFD, "no certificate could be found\n");
            retval = -1;
        }

        prev = cert;
    }

    JAR_find_end(ctx);

    JAR_destroy(jar);
    return retval;
}

/************************************************************************
 * j a r _ c b
 */
static int
jar_cb(int status, JAR *jar, const char *metafile,
       char *pathname, char *errortext)
{
    PR_fprintf(errorFD, "error %d: %s IN FILE %s\n", status, errortext,
               pathname);
    errorCount++;
    return 0;
}
