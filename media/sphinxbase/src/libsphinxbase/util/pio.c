/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 1999-2004 Carnegie Mellon University.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * This work was supported in part by funding from the Defense Advanced 
 * Research Projects Agency and the National Science Foundation of the 
 * United States of America, and the CMU Sphinx Speech Consortium.
 *
 * THIS SOFTWARE IS PROVIDED BY CARNEGIE MELLON UNIVERSITY ``AS IS'' AND 
 * ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY
 * NOR ITS EMPLOYEES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ====================================================================
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#if defined(_WIN32) && !defined(CYGWIN)
#include <direct.h>
#endif

#include "sphinxbase/pio.h"
#include "sphinxbase/filename.h"
#include "sphinxbase/err.h"
#include "sphinxbase/strfuncs.h"
#include "sphinxbase/ckd_alloc.h"

#ifndef EXEEXT
#define EXEEXT ""
#endif

enum {
    COMP_NONE,
    COMP_COMPRESS,
    COMP_GZIP,
    COMP_BZIP2
};

static void
guess_comptype(char const *file, int32 *ispipe, int32 *isgz)
{
    size_t k;

    k = strlen(file);
    *ispipe = 0;
    *isgz = COMP_NONE;
    if ((k > 2)
        && ((strcmp(file + k - 2, ".Z") == 0)
            || (strcmp(file + k - 2, ".z") == 0))) {
        *ispipe = 1;
        *isgz = COMP_COMPRESS;
    }
    else if ((k > 3) && ((strcmp(file + k - 3, ".gz") == 0)
                        || (strcmp(file + k - 3, ".GZ") == 0))) {
        *ispipe = 1;
        *isgz = COMP_GZIP;
    }
    else if ((k > 4) && ((strcmp(file + k - 4, ".bz2") == 0)
                        || (strcmp(file + k - 4, ".BZ2") == 0))) {
        *ispipe = 1;
        *isgz = COMP_BZIP2;
    }
}

FILE *
fopen_comp(const char *file, const char *mode, int32 * ispipe)
{
    FILE *fp;

#ifndef HAVE_POPEN
    *ispipe = 0; /* No popen() on WinCE */
#else /* HAVE_POPEN */
    int32 isgz;
    guess_comptype(file, ispipe, &isgz);
#endif /* HAVE_POPEN */

    if (*ispipe) {
#ifndef HAVE_POPEN
        /* Shouldn't get here, anyway */
        E_FATAL("No popen() on WinCE\n");
#else
        if (strcmp(mode, "r") == 0) {
            char *command;
            switch (isgz) {
            case COMP_GZIP:
                command = string_join("gunzip" EXEEXT, " -c ", file, NULL);
                break;
            case COMP_COMPRESS:
                command = string_join("zcat" EXEEXT, " ", file, NULL);
                break;
            case COMP_BZIP2:
                command = string_join("bunzip2" EXEEXT, " -c ", file, NULL);
                break;
            default:
                command = NULL; /* Make compiler happy. */
                E_FATAL("Unknown  compression type %d\n", isgz);
            }
            if ((fp = popen(command, mode)) == NULL) {
                E_ERROR_SYSTEM("Failed to open a pipe for a command '%s' mode '%s'", command, mode);
                ckd_free(command);
                return NULL;
            }
            ckd_free(command);
        }
        else if (strcmp(mode, "w") == 0) {
            char *command;
            switch (isgz) {
            case COMP_GZIP:
                command = string_join("gzip" EXEEXT, " > ", file, NULL);
                break;
            case COMP_COMPRESS:
                command = string_join("compress" EXEEXT, " -c > ", file, NULL);
                break;
            case COMP_BZIP2:
                command = string_join("bzip2" EXEEXT, " > ", file, NULL);
                break;
            default:
                command = NULL; /* Make compiler happy. */
                E_FATAL("Unknown compression type %d\n", isgz);
            }
            if ((fp = popen(command, mode)) == NULL) {
                E_ERROR_SYSTEM("Failed to open a pipe for a command '%s' mode '%s'", command, mode);
                ckd_free(command);
                return NULL;
            }
            ckd_free(command);
        }
        else {
            E_ERROR("Compressed file operation for mode %s is not supported", mode);
            return NULL;
        }
#endif /* HAVE_POPEN */
    }
    else {
        fp = fopen(file, mode);
    }

    return (fp);
}


void
fclose_comp(FILE * fp, int32 ispipe)
{
    if (ispipe) {
#ifdef HAVE_POPEN
#if defined(_WIN32) && (!defined(__SYMBIAN32__))
        _pclose(fp);
#else
        pclose(fp);
#endif
#endif
    }
    else
        fclose(fp);
}


FILE *
fopen_compchk(const char *file, int32 * ispipe)
{
#ifndef HAVE_POPEN
    *ispipe = 0; /* No popen() on WinCE */
    /* And therefore the rest of this function is useless. */
    return (fopen_comp(file, "r", ispipe));
#else /* HAVE_POPEN */
    int32 isgz;
    FILE *fh;

    /* First just try to fopen_comp() it */
    if ((fh = fopen_comp(file, "r", ispipe)) != NULL)
        return fh;
    else {
        char *tmpfile;
        size_t k;

        /* File doesn't exist; try other compressed/uncompressed form, as appropriate */
        guess_comptype(file, ispipe, &isgz);
        k = strlen(file);
        tmpfile = ckd_calloc(k+5, 1);
        strcpy(tmpfile, file);
        switch (isgz) {
        case COMP_GZIP:
            tmpfile[k - 3] = '\0';
            break;
        case COMP_BZIP2:
            tmpfile[k - 4] = '\0';
            break;
        case COMP_COMPRESS:
            tmpfile[k - 2] = '\0';
            break;
        case COMP_NONE:
            strcpy(tmpfile + k, ".gz");
            if ((fh = fopen_comp(tmpfile, "r", ispipe)) != NULL) {
                E_WARN("Using %s instead of %s\n", tmpfile, file);
                ckd_free(tmpfile);
                return fh;
            }
            strcpy(tmpfile + k, ".bz2");
            if ((fh = fopen_comp(tmpfile, "r", ispipe)) != NULL) {
                E_WARN("Using %s instead of %s\n", tmpfile, file);
                ckd_free(tmpfile);
                return fh;
            }
            strcpy(tmpfile + k, ".Z");
            if ((fh = fopen_comp(tmpfile, "r", ispipe)) != NULL) {
                E_WARN("Using %s instead of %s\n", tmpfile, file);
                ckd_free(tmpfile);
                return fh;
            }
            ckd_free(tmpfile);
            return NULL;
        }
        E_WARN("Using %s instead of %s\n", tmpfile, file);
        fh = fopen_comp(tmpfile, "r", ispipe);
        ckd_free(tmpfile);
        return NULL;
    }
#endif /* HAVE_POPEN */
}

lineiter_t *
lineiter_start(FILE *fh)
{
    lineiter_t *li;

    li = (lineiter_t *)ckd_calloc(1, sizeof(*li));
    li->buf = (char *)ckd_malloc(128);
    li->buf[0] = '\0';
    li->bsiz = 128;
    li->len = 0;
    li->fh = fh;

    li = lineiter_next(li);
    
    /* Strip the UTF-8 BOM */
    
    if (li && 0 == strncmp(li->buf, "\xef\xbb\xbf", 3)) {
	memmove(li->buf, li->buf + 3, strlen(li->buf + 1));
	li->len -= 3;
    }
    
    return li;
}

lineiter_t *
lineiter_start_clean(FILE *fh)
{
    lineiter_t *li;
    
    li = lineiter_start(fh);
    
    if (li == NULL)
	return li;
    
    li->clean = TRUE;
    
    if (li->buf && li->buf[0] == '#') {
	li = lineiter_next(li);
    } else {
	string_trim(li->buf, STRING_BOTH);
    }
    
    return li;
}


static lineiter_t *
lineiter_next_plain(lineiter_t *li)
{
    /* We are reading the next line */
    li->lineno++;
    
    /* Read a line and check for EOF. */
    if (fgets(li->buf, li->bsiz, li->fh) == NULL) {
        lineiter_free(li);
        return NULL;
    }
    /* If we managed to read the whole thing, then we are done
     * (this will be by far the most common result). */
    li->len = (int32)strlen(li->buf);
    if (li->len < li->bsiz - 1 || li->buf[li->len - 1] == '\n')
        return li;

    /* Otherwise we have to reallocate and keep going. */
    while (1) {
        li->bsiz *= 2;
        li->buf = (char *)ckd_realloc(li->buf, li->bsiz);
        /* If we get an EOF, we are obviously done. */
        if (fgets(li->buf + li->len, li->bsiz - li->len, li->fh) == NULL) {
            li->len += strlen(li->buf + li->len);
            return li;
        }
        li->len += strlen(li->buf + li->len);
        /* If we managed to read the whole thing, then we are done. */
        if (li->len < li->bsiz - 1 || li->buf[li->len - 1] == '\n')
            return li;
    }

    /* Shouldn't get here. */
    return li;
}


lineiter_t *
lineiter_next(lineiter_t *li)
{
    if (!li->clean)
	return lineiter_next_plain(li);
    
    for (li = lineiter_next_plain(li); li; li = lineiter_next_plain(li)) {
	if (li->buf && li->buf[0] != '#') {
	    li->buf = string_trim(li->buf, STRING_BOTH);
	    break;
	}
    }
    return li;
}

int lineiter_lineno(lineiter_t *li)
{
    return li->lineno;
}

void
lineiter_free(lineiter_t *li)
{
    if (li == NULL)
        return;
    ckd_free(li->buf);
    ckd_free(li);
}

char *
fread_line(FILE *stream, size_t *out_len)
{
    char *output, *outptr;
    char buf[128];

    output = outptr = NULL;
    while (fgets(buf, sizeof(buf), stream)) {
        size_t len = strlen(buf);
        /* Append this data to the buffer. */
        if (output == NULL) {
            output = (char *)ckd_malloc(len + 1);
            outptr = output;
        }
        else {
            size_t cur = outptr - output;
            output = (char *)ckd_realloc(output, cur + len + 1);
            outptr = output + cur;
        }
        memcpy(outptr, buf, len + 1);
        outptr += len;
        /* Stop on a short read or end of line. */
        if (len < sizeof(buf)-1 || buf[len-1] == '\n')
            break;
    }
    if (out_len) *out_len = outptr - output;
    return output;
}

#define FREAD_RETRY_COUNT	60

int32
fread_retry(void *pointer, int32 size, int32 num_items, FILE * stream)
{
    char *data;
    size_t n_items_read;
    size_t n_items_rem;
    uint32 n_retry_rem;
    int32 loc;

    n_retry_rem = FREAD_RETRY_COUNT;

    data = (char *)pointer;
    loc = 0;
    n_items_rem = num_items;

    do {
        n_items_read = fread(&data[loc], size, n_items_rem, stream);

        n_items_rem -= n_items_read;

        if (n_items_rem > 0) {
            /* an incomplete read occurred */

            if (n_retry_rem == 0)
                return -1;

            if (n_retry_rem == FREAD_RETRY_COUNT) {
                E_ERROR_SYSTEM("fread() failed; retrying...\n");
            }

            --n_retry_rem;

            loc += n_items_read * size;
#if !defined(_WIN32) && defined(HAVE_UNISTD_H)
            sleep(1);
#endif
        }
    } while (n_items_rem > 0);

    return num_items;
}


#ifdef _WIN32_WCE /* No stat() on WinCE */
int32
stat_retry(const char *file, struct stat * statbuf)
{
    WIN32_FIND_DATAW file_data;
    HANDLE *h;
    wchar_t *wfile;
    size_t len;

    len = mbstowcs(NULL, file, 0) + 1;
    wfile = ckd_calloc(len, sizeof(*wfile));
    mbstowcs(wfile, file, len);
    if ((h = FindFirstFileW(wfile, &file_data)) == INVALID_HANDLE_VALUE) {
        ckd_free(wfile);
        return -1;
    }
    ckd_free(wfile);
    memset(statbuf, 0, sizeof(*statbuf));
    statbuf->st_mtime = file_data.ftLastWriteTime.dwLowDateTime;
    statbuf->st_size = file_data.nFileSizeLow;
    FindClose(h);

    return 0;
}


int32
stat_mtime(const char *file)
{
    struct stat statbuf;

    if (stat_retry(file, &statbuf) != 0)
        return -1;

    return ((int32) statbuf.st_mtime);
}
#else
#define STAT_RETRY_COUNT	10
int32
stat_retry(const char *file, struct stat * statbuf)
{
    int32 i;

    for (i = 0; i < STAT_RETRY_COUNT; i++) {
#ifndef HAVE_SYS_STAT_H
	FILE *fp;

	if ((fp = (FILE *)fopen(file, "r")) != 0) {
	    fseek(fp, 0, SEEK_END);
	    statbuf->st_size = ftell(fp);
	    fclose(fp);
	    return 0;
	}
#else /* HAVE_SYS_STAT_H */
        if (stat(file, statbuf) == 0)
            return 0;
#endif
        if (i == 0) {
            E_ERROR_SYSTEM("Failed to stat file '%s'; retrying...", file);
        }
#ifdef HAVE_UNISTD_H
        sleep(1);
#endif
    }

    return -1;
}

int32
stat_mtime(const char *file)
{
    struct stat statbuf;

#ifdef HAVE_SYS_STAT_H
    if (stat(file, &statbuf) != 0)
        return -1;
#else /* HAVE_SYS_STAT_H */
    if (stat_retry(file, &statbuf) != 0)
        return -1;
#endif /* HAVE_SYS_STAT_H */

    return ((int32) statbuf.st_mtime);
}
#endif /* !_WIN32_WCE */

struct bit_encode_s {
    FILE *fh;
    unsigned char buf, bbits;
    int16 refcount;
};

bit_encode_t *
bit_encode_attach(FILE *outfh)
{
    bit_encode_t *be;

    be = (bit_encode_t *)ckd_calloc(1, sizeof(*be));
    be->refcount = 1;
    be->fh = outfh;
    return be;
}

bit_encode_t *
bit_encode_retain(bit_encode_t *be)
{
    ++be->refcount;
    return be;
}

int
bit_encode_free(bit_encode_t *be)
{
    if (be == NULL)
        return 0;
    if (--be->refcount > 0)
        return be->refcount;
    ckd_free(be);

    return 0;
}

int
bit_encode_write(bit_encode_t *be, unsigned char const *bits, int nbits)
{
    int tbits;

    tbits = nbits + be->bbits;
    if (tbits < 8)  {
        /* Append to buffer. */
        be->buf |= ((bits[0] >> (8 - nbits)) << (8 - tbits));
    }
    else {
        int i = 0;
        while (tbits >= 8) {
            /* Shift bits out of the buffer and splice with high-order bits */
            fputc(be->buf | ((bits[i]) >> be->bbits), be->fh);
            /* Put low-order bits back into buffer */
            be->buf = (bits[i] << (8 - be->bbits)) & 0xff;
            tbits -= 8;
            ++i;
        }
    }
    /* tbits contains remaining number of  bits. */
    be->bbits = tbits;

    return nbits;
}

int
bit_encode_write_cw(bit_encode_t *be, uint32 codeword, int nbits)
{
    unsigned char bits[4];
    codeword <<= (32 - nbits);
    bits[0] = (codeword >> 24) & 0xff;
    bits[1] = (codeword >> 16) & 0xff;
    bits[2] = (codeword >> 8) & 0xff;
    bits[3] = codeword & 0xff;
    return bit_encode_write(be, bits, nbits);
}

int
bit_encode_flush(bit_encode_t *be)
{
    if (be->bbits) {
        fputc(be->buf, be->fh);
        be->bbits = 0;
    }
    return 0;
}

int
build_directory(const char *path)
{
    int rv;

    /* Utterly failed... */
    if (strlen(path) == 0)
        return -1;

#if defined(_WIN32) && !defined(CYGWIN)
    else if ((rv = _mkdir(path)) == 0)
        return 0;
#elif defined(HAVE_SYS_STAT_H) /* Unix, Cygwin, doesn't work on MINGW */
    else if ((rv = mkdir(path, 0777)) == 0)
        return 0;
#endif

    /* Or, it already exists... */
    else if (errno == EEXIST)
        return 0;
    else if (errno != ENOENT) {
        E_ERROR_SYSTEM("Failed to create %s", path);
        return -1;
    }
    else {
        char *dirname = ckd_salloc(path);
        path2dirname(path, dirname);
        build_directory(dirname);
        ckd_free(dirname);

#if defined(_WIN32) && !defined(CYGWIN)
	return _mkdir(path);
#elif defined(HAVE_SYS_STAT_H) /* Unix, Cygwin, doesn't work on MINGW */
        return mkdir(path, 0777);
#endif
    }
}
